/*
 * Copyright (C) 2005-2008 MaNGOS <http://www.mangosproject.org/>
 *
 * Copyright (C) 2008 Trinity <http://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "Common.h"
#include "Log.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "World.h"
#include "Opcodes.h"
#include "ObjectMgr.h"
#include "Chat.h"
#include "Database/DatabaseEnv.h"
#include "ChannelMgr.h"
#include "Group.h"
#include "Guild.h"
#include "MapManager.h"
#include "ObjectAccessor.h"
#include "ScriptCalls.h"
#include "Player.h"
#include "SpellAuras.h"
#include "Language.h"
#include "Util.h"
#include "ScriptMgr.h"
#include "IRCMgr.h"

void WorldSession::HandleMessagechatOpcode( WorldPacket & recvData )
{
    PROFILE;
    
    CHECK_PACKET_SIZE(recvData,4+4+1);

    uint32 type;
    uint32 lang;

    recvData >> type;
    recvData >> lang;

    if(type >= MAX_CHAT_MSG_TYPE)
    {
        TC_LOG_ERROR("FIXME","CHAT: Wrong message type received: %u", type);
        return;
    }

    //TC_LOG_DEBUG("FIXME","CHAT: packet received. type %u, lang %u", type, lang );

    // prevent talking at unknown language (cheating)
    LanguageDesc const* langDesc = GetLanguageDescByID(lang);
    if(!langDesc)
    {
        SendNotification(LANG_UNKNOWN_LANGUAGE);
        return;
    }
    if(langDesc->skill_id != 0 && !_player->HasSkill(langDesc->skill_id))
    {
        // also check SPELL_AURA_COMPREHEND_LANGUAGE (client offers option to speak in that language)
        Unit::AuraList const& langAuras = _player->GetAurasByType(SPELL_AURA_COMPREHEND_LANGUAGE);
        bool foundAura = false;
        for(Unit::AuraList::const_iterator i = langAuras.begin();i != langAuras.end(); ++i)
        {
            if((*i)->GetModifier()->m_miscvalue == lang)
            {
                foundAura = true;
                break;
            }
        }
        if(!foundAura)
        {
            SendNotification(LANG_NOT_LEARNED_LANGUAGE);
            return;
        }
    }

    if(lang == LANG_ADDON)
    {
        // Disabled addon channel?
        if(!sWorld->getConfig(CONFIG_ADDON_CHANNEL))
            return;
    }
    // LANG_ADDON should not be changed nor be affected by flood control
    else
    {
        // send in universal language if player in .gmon mode (ignore spell effects)
        if (_player->IsGameMaster())
            lang = LANG_UNIVERSAL;
        else
        {
            // send in universal language in two side iteration allowed mode
            if (sWorld->getConfig(CONFIG_ALLOW_TWO_SIDE_INTERACTION_CHAT))
                lang = LANG_UNIVERSAL;
            else
            {
                switch(type)
                {
                    case CHAT_MSG_PARTY:
                    case CHAT_MSG_RAID:
                    case CHAT_MSG_RAID_LEADER:
                    case CHAT_MSG_RAID_WARNING:
                        // allow two side chat at group channel if two side group allowed
                        if(sWorld->getConfig(CONFIG_ALLOW_TWO_SIDE_INTERACTION_GROUP))
                            lang = LANG_UNIVERSAL;
                        break;
                    case CHAT_MSG_GUILD:
                    case CHAT_MSG_OFFICER:
                        // allow two side chat at guild channel if two side guild allowed
                        if(sWorld->getConfig(CONFIG_ALLOW_TWO_SIDE_INTERACTION_GUILD))
                            lang = LANG_UNIVERSAL;
                        break;
                }
            }

            // but overwrite it by SPELL_AURA_MOD_LANGUAGE auras (only single case used)
            Unit::AuraList const& ModLangAuras = _player->GetAurasByType(SPELL_AURA_MOD_LANGUAGE);
            if(!ModLangAuras.empty())
                lang = ModLangAuras.front()->GetModifier()->m_miscvalue;
        }

        //Flood control for these channels only
        if (type == CHAT_MSG_SAY
        || type == CHAT_MSG_YELL
        || type == CHAT_MSG_WHISPER
        || type == CHAT_MSG_EMOTE
        || type == CHAT_MSG_TEXT_EMOTE
        || type == CHAT_MSG_CHANNEL
        || type == CHAT_MSG_BG_SYSTEM_NEUTRAL
        || type == CHAT_MSG_BG_SYSTEM_ALLIANCE
        || type == CHAT_MSG_BG_SYSTEM_HORDE
        || type == CHAT_MSG_BATTLEGROUND
        || type == CHAT_MSG_BATTLEGROUND_LEADER)
        {
            GetPlayer()->UpdateSpeakTime();
            if ( !_player->CanSpeak() )
            {
                std::string timeStr = secsToTimeString(m_muteTime - time(NULL));
                SendNotification(GetTrinityString(LANG_WAIT_BEFORE_SPEAKING),timeStr.c_str());
                return;
            }
        }
    }

   if (GetPlayer()->HasAura(1852,0) && type != CHAT_MSG_WHISPER)
    {
        std::string msg="";
        recvData >> msg;
        if (ChatHandler(this).ParseCommands(msg.c_str()) == 0)
        {
            SendNotification(GetTrinityString(LANG_GM_SILENCE), GetPlayer()->GetName().c_str());
            return;
        }
    }

    switch(type)
    {
        case CHAT_MSG_SAY:
        case CHAT_MSG_EMOTE:
        case CHAT_MSG_YELL:
        {
            std::string msg = "";
            recvData >> msg;

            if(msg.empty())
                break;

            if (ChatHandler(this).ParseCommands(msg.c_str()) > 0)
                break;
            
            if (GetPlayer()->isSpectator())
            {
                SendNotification("Vous ne pouvez pas effectuer cette action lorsque vous êtes spectateur");
                return;
            }

            // strip invisible characters for non-addon messages
            if (lang != LANG_ADDON && sWorld->getConfig(CONFIG_CHAT_FAKE_MESSAGE_PREVENTING))
                stripLineInvisibleChars(msg);

            if(msg.empty())
                break;
                
            if (strncmp(msg.c_str(), "|cff", 4) == 0) {
                char* cEntry = ChatHandler(GetPlayer()).extractKeyFromLink(((char*)msg.c_str()), "Hitem");
                if (cEntry) {
                    if (uint32 entry = atoi(cEntry)) {
                        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(entry);
                        if (!proto)
                            break;
                    }
                    else
                        break;
                }
            }

            if (sWorld->IsPhishing(msg)) {
                sWorld->LogPhishing(GetPlayer()->GetGUIDLow(), 0, msg);
                break;
            }

            if(type == CHAT_MSG_SAY)
                GetPlayer()->Say(msg, Language(lang));
            else if(type == CHAT_MSG_EMOTE)
                GetPlayer()->TextEmote(msg);
            else if(type == CHAT_MSG_YELL)
                GetPlayer()->Yell(msg, Language(lang));
        } break;

        case CHAT_MSG_WHISPER:
        {
            std::string to, msg;
            recvData >> to;
            CHECK_PACKET_SIZE(recvData,4+4+(to.size()+1)+1);
            recvData >> msg;

            // strip invisible characters for non-addon messages
            if (lang != LANG_ADDON && sWorld->getConfig(CONFIG_CHAT_FAKE_MESSAGE_PREVENTING))
                stripLineInvisibleChars(msg);

            if(msg.empty())
                break;
                
            if (ChatHandler(this).ParseCommands(msg.c_str()) > 0)
                break;

            if (strncmp(msg.c_str(), "|cff", 4) == 0) {
                char* cEntry = ChatHandler(GetPlayer()).extractKeyFromLink(((char*)msg.c_str()), "Hitem");
                if (cEntry) {
                    if (uint32 entry = atoi(cEntry)) {
                        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(entry);
                        if (!proto)
                            break;
                    }
                    else
                        break;
                }
            }

            if(!normalizePlayerName(to))
            {
                WorldPacket data(SMSG_CHAT_PLAYER_NOT_FOUND, (to.size()+1));
                data<<to;
                SendPacket(&data);
                break;
            }

            Player* toPlayer = sObjectAccessor->FindConnectedPlayerByName(to.c_str());
            uint32 tSecurity = GetSecurity();
            uint32 pSecurity = toPlayer ? toPlayer->GetSession()->GetSecurity() : 0;
            if(!toPlayer || tSecurity == SEC_PLAYER && pSecurity > SEC_PLAYER && !toPlayer->IsAcceptWhispers())
            {
                WorldPacket data(SMSG_CHAT_PLAYER_NOT_FOUND, (to.size()+1));
                data<<to;
                SendPacket(&data);
                return;
            }

            
            // gm shoudln't send whisper addon message while invisible
            if (lang == LANG_ADDON && GetPlayer()->GetVisibility() == VISIBILITY_OFF && !toPlayer->IsGameMaster())
                break;

            // can't whisper others players before CONFIG_WHISPER_MINLEVEL but can still whisper GM's
            if (pSecurity == SEC_PLAYER && 
                GetPlayer()->GetSession()->GetSecurity() <= SEC_PLAYER && 
                GetPlayer()->GetLevel() < sWorld->getConfig(CONFIG_WHISPER_MINLEVEL) && 
                lang != LANG_ADDON &&
                GetPlayer()->GetTotalPlayedTime() < DAY)
            {
                ChatHandler(this).PSendSysMessage("Vous devez atteindre le niveau %u ou avoir un temps de jeu total de 24h pour pouvoir chuchoter aux autres joueurs.", sWorld->getConfig(CONFIG_WHISPER_MINLEVEL));
                break;
            }

            if (sWorld->IsPhishing(msg)) {
                sWorld->LogPhishing(GetPlayer()->GetGUIDLow(), toPlayer->GetGUIDLow(), msg);
                break;
            }

            if (!sWorld->getConfig(CONFIG_ALLOW_TWO_SIDE_INTERACTION_CHAT) && tSecurity == SEC_PLAYER && pSecurity == SEC_PLAYER )
            {
                uint32 sidea = GetPlayer()->GetTeam();
                uint32 sideb = toPlayer->GetTeam();
                if( sidea != sideb )
                {
                    WorldPacket data(SMSG_CHAT_PLAYER_NOT_FOUND, (to.size()+1));
                    data<<to;
                    SendPacket(&data);
                    return;
                }
            }

            if (GetPlayer()->HasAura(1852,0) && !toPlayer->IsGameMaster())
            {
                SendNotification(GetTrinityString(LANG_GM_SILENCE), GetPlayer()->GetName().c_str());
                return;
            }

            GetPlayer()->Whisper(msg, Language(lang),toPlayer);
        } break;

        case CHAT_MSG_PARTY:
#ifdef LICH_KING
        case CHAT_MSG_PARTY_LEADER:
#endif
        {
            std::string msg = "";
            recvData >> msg;

            if(msg.empty())
                break;

            if (ChatHandler(this).ParseCommands(msg.c_str()) > 0)
                break;

            // strip invisible characters for non-addon messages
            if (lang != LANG_ADDON && sWorld->getConfig(CONFIG_CHAT_FAKE_MESSAGE_PREVENTING))
                stripLineInvisibleChars(msg);

            if(msg.empty())
                break;

            if (sWorld->IsPhishing(msg)) {
                sWorld->LogPhishing(GetPlayer()->GetGUIDLow(), 0, msg);
                break;
            }

            // if player is in battleground, he cannot say to battleground members by /p
            Group *group = GetPlayer()->GetOriginalGroup();
            // so if player hasn't OriginalGroup and his player->GetGroup() is BG raid, then return
            if (!group && (!(group = GetPlayer()->GetGroup()) || group->isBGGroup()))
                return;

            WorldPacket data;
            ChatHandler::BuildChatPacket(data, ChatMsg(type), Language(lang), GetPlayer(), nullptr, msg);
            group->BroadcastPacket(&data, false, group->GetMemberGroup(GetPlayer()->GetGUID()));
        }
        break;
        case CHAT_MSG_GUILD:
        {
            std::string msg = "";
            recvData >> msg;

            if(msg.empty())
                break;

            if (ChatHandler(this).ParseCommands(msg.c_str()) > 0)
                break;

            // strip invisible characters for non-addon messages
            if (lang != LANG_ADDON && sWorld->getConfig(CONFIG_CHAT_FAKE_MESSAGE_PREVENTING))
                stripLineInvisibleChars(msg);

            if(msg.empty())
                break;

            if (GetPlayer()->GetGuildId())
            {
                Guild *guild = sObjectMgr->GetGuildById(GetPlayer()->GetGuildId());
                if (guild) {
                    guild->BroadcastToGuild(this, msg, lang == LANG_ADDON ? LANG_ADDON : LANG_UNIVERSAL);
                    if (sWorld->getConfig(CONFIG_IRC_ENABLED) && lang != LANG_ADDON)
                        sIRCMgr->onIngameGuildMessage(guild->GetId(), _player->GetName(), msg.c_str());
                }
            }

            break;
        }
        case CHAT_MSG_OFFICER:
        {
            std::string msg = "";
            recvData >> msg;

            if(msg.empty())
                break;

            if (ChatHandler(this).ParseCommands(msg.c_str()) > 0)
                break;

            // strip invisible characters for non-addon messages
            if (lang != LANG_ADDON && sWorld->getConfig(CONFIG_CHAT_FAKE_MESSAGE_PREVENTING))
                stripLineInvisibleChars(msg);

            if(msg.empty())
                break;

            if (GetPlayer()->GetGuildId())
            {
                Guild *guild = sObjectMgr->GetGuildById(GetPlayer()->GetGuildId());
                if (guild)
                    guild->BroadcastToOfficers(this, msg, lang == LANG_ADDON ? LANG_ADDON : LANG_UNIVERSAL);
            }
            break;
        }
        case CHAT_MSG_RAID:
        {
            std::string msg="";
            recvData >> msg;

            if(msg.empty())
                break;

            if (ChatHandler(this).ParseCommands(msg.c_str()) > 0)
                break;

            // strip invisible characters for non-addon messages
            if (lang != LANG_ADDON && sWorld->getConfig(CONFIG_CHAT_FAKE_MESSAGE_PREVENTING))
                stripLineInvisibleChars(msg);

            if(msg.empty())
                break;

            // if player is in battleground, he cannot say to battleground members by /raid
            Group *group = GetPlayer()->GetOriginalGroup();
            // so if player hasn't OriginalGroup and his player->GetGroup() is BG raid or his group isn't raid, then return
            if (!group && !(group = GetPlayer()->GetGroup()) || group->isBGGroup() || !group->isRaidGroup())
                return;

            WorldPacket data;
            ChatHandler::BuildChatPacket(data, ChatMsg(type), Language(lang), GetPlayer(), nullptr, msg);
            group->BroadcastPacket(&data, false);
        } break;
        case CHAT_MSG_RAID_LEADER:
        {
            std::string msg="";
            recvData >> msg;

            if(msg.empty())
                break;

            if (ChatHandler(this).ParseCommands(msg.c_str()) > 0)
                break;

            // strip invisible characters for non-addon messages
            if (lang != LANG_ADDON && sWorld->getConfig(CONFIG_CHAT_FAKE_MESSAGE_PREVENTING))
                stripLineInvisibleChars(msg);

            if(msg.empty())
                break;

            // if player is in battleground, he cannot say to battleground members by /raid
            Group *group = GetPlayer()->GetOriginalGroup();
            if (!group && !(group = GetPlayer()->GetGroup()) || group->isBGGroup() || !group->isRaidGroup() || !group->IsLeader(GetPlayer()->GetGUID()))
                return;

            WorldPacket data;
            ChatHandler::BuildChatPacket(data, ChatMsg(type), Language(lang), GetPlayer(), nullptr, msg);
            group->BroadcastPacket(&data, false);
        } break;
        case CHAT_MSG_RAID_WARNING:
        {
            std::string msg="";
            recvData >> msg;

            // strip invisible characters for non-addon messages
            if (lang != LANG_ADDON && sWorld->getConfig(CONFIG_CHAT_FAKE_MESSAGE_PREVENTING))
                stripLineInvisibleChars(msg);

            if(msg.empty())
                break;

            if (ChatHandler(this).ParseCommands(msg.c_str()) > 0)
                break;

            Group *group = GetPlayer()->GetGroup();
            if(!group || !group->isRaidGroup() || !(group->IsLeader(GetPlayer()->GetGUID()) || group->IsAssistant(GetPlayer()->GetGUID())) || group->isBGGroup())
                return;

            WorldPacket data;
            // in battleground, raid warning is sent only to players in battleground - code is ok
            ChatHandler::BuildChatPacket(data, ChatMsg(type), Language(lang), GetPlayer(), nullptr, msg);
            group->BroadcastPacket(&data, false);
        } break;
        
        case CHAT_MSG_BATTLEGROUND_LEADER:
        {
            //battleground raid is always in Player->GetGroup(), never in GetOriginalGroup()
            Group* group = GetPlayer()->GetGroup();
            if (!group || !group->isBGGroup() || !group->IsLeader(GetPlayer()->GetGUID()))
                return;
        }
        case CHAT_MSG_BATTLEGROUND:
        {
            std::string msg="";
            recvData >> msg;

            // strip invisible characters for non-addon messages
            if (lang != LANG_ADDON && sWorld->getConfig(CONFIG_CHAT_FAKE_MESSAGE_PREVENTING))
                stripLineInvisibleChars(msg);

            if(msg.empty())
                break;

            if (ChatHandler(this).ParseCommands(msg.c_str()) > 0)
                break;

            //battleground raid is always in Player->GetGroup(), never in GetOriginalGroup()
            Group* _group = GetPlayer()->GetGroup();
            if(!_group || !_group->isBGGroup())
                return;

            WorldPacket data;
            ChatHandler::BuildChatPacket(data, ChatMsg(type), Language(lang), GetPlayer(), nullptr, msg);
            _group->BroadcastPacket(&data, false);
        } break;

        case CHAT_MSG_CHANNEL:
        {
            std::string channel = "", msg = "";
            recvData >> channel;

            // recheck
            CHECK_PACKET_SIZE(recvData,4+4+(channel.size()+1)+1);

            recvData >> msg;

            // strip invisible characters for non-addon messages
            if (lang != LANG_ADDON && sWorld->getConfig(CONFIG_CHAT_FAKE_MESSAGE_PREVENTING))
                stripLineInvisibleChars(msg);

            if(msg.empty())
                break;

            if (ChatHandler(this).ParseCommands(msg.c_str()) > 0)
                break;
                
            if (strncmp(msg.c_str(), "|cff", 4) == 0) {
                char* cEntry = ChatHandler(GetPlayer()).extractKeyFromLink(((char*)msg.c_str()), "Hitem");
                if (cEntry) {
                    if (uint32 entry = atoi(cEntry)) {
                        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(entry);
                        if (!proto)
                            break;
                    }
                    else
                        break;
                }
            }

            if (sWorld->IsPhishing(msg)) {
                sWorld->LogPhishing(GetPlayer()->GetGUIDLow(), 0, msg);
                break;
            }

            if(ChannelMgr* cMgr = channelMgr(_player->GetTeam()))
            {
                if(Channel *chn = cMgr->GetChannel(channel,_player))
                {
                    chn->Say(_player->GetGUID(),msg.c_str(), Language(lang));
                    if (sWorld->getConfig(CONFIG_IRC_ENABLED) && lang != LANG_ADDON)
                    {
                        ChannelFaction faction = _player->GetTeam() == TEAM_ALLIANCE ? CHAN_FACTION_ALLIANCE : CHAN_FACTION_HORDE;
                        sIRCMgr->onIngameChannelMessage(faction,channel.c_str(),_player->GetName(), msg.c_str());
                    }
                }
            }
        } break;

        case CHAT_MSG_AFK:
        {
            std::string msg;
            recvData >> msg;

            if((msg.empty() || !_player->IsAFK()) && !_player->IsInCombat() )
            {
                if(!_player->IsAFK())
                {
                    if(msg.empty())
                        msg  = GetTrinityString(LANG_PLAYER_AFK_DEFAULT);
                    _player->afkMsg = msg;
                }
                _player->ToggleAFK();
                if(_player->IsAFK() && _player->IsDND())
                    _player->ToggleDND();
            }
        } break;

        case CHAT_MSG_DND:
        {
            std::string msg;
            recvData >> msg;

            if(msg.empty() || !_player->IsDND())
            {
                if(!_player->IsDND())
                {
                    if(msg.empty())
                        msg  = GetTrinityString(LANG_PLAYER_DND_DEFAULT);
                    _player->dndMsg = msg;
                }
                _player->ToggleDND();
                if(_player->IsDND() && _player->IsAFK())
                    _player->ToggleAFK();
            }
        } break;

        default:
            TC_LOG_ERROR("FIXME","CHAT: unknown message type %u, lang: %u", type, lang);
            break;
    }
}

void WorldSession::HandleEmoteOpcode( WorldPacket & recvData )
{
    PROFILE;
    
    if(!GetPlayer()->IsAlive())
        return;
    CHECK_PACKET_SIZE(recvData,4);

    uint32 emote;
    recvData >> emote;
    GetPlayer()->HandleEmoteCommand(emote);
}

void WorldSession::HandleTextEmoteOpcode( WorldPacket & recvData )
{
    PROFILE;
    
    if(!_player->m_mover->IsAlive())
        return;

    GetPlayer()->UpdateSpeakTime();
    if (!GetPlayer()->CanSpeak())
    {
        std::string timeStr = secsToTimeString(m_muteTime - time(NULL));
        SendNotification(GetTrinityString(LANG_WAIT_BEFORE_SPEAKING),timeStr.c_str());
        return;
    }

    CHECK_PACKET_SIZE(recvData,4+4+8);

    uint32 text_emote, emoteNum;
    uint64 guid;

    recvData >> text_emote;
    recvData >> emoteNum;
    recvData >> guid;
    
    Unit* i_target = ObjectAccessor::GetUnit(*_player, guid);

    LocaleConstant loc_idx = _player->GetSession()->GetSessionDbcLocale();
    std::string const name(i_target ? i_target->GetNameForLocaleIdx(loc_idx) : "");
    uint32 namlen = name.size();

    EmotesTextEntry const *em = sEmotesTextStore.LookupEntry(text_emote);
    if (em)
    {
        uint32 emote_anim = em->textid;

        WorldPacket data;

        switch(emote_anim)
        {
            case EMOTE_STATE_SLEEP:
            case EMOTE_STATE_SIT:
            case EMOTE_STATE_KNEEL:
            case EMOTE_ONESHOT_NONE:
                break;
            default:
                _player->m_mover->HandleEmoteCommand(emote_anim);
                break;
        }

        if(_player->m_mover->ToPlayer()) //SMSG_TEXT_EMOTE is for player only
        {
            data.Initialize(SMSG_TEXT_EMOTE, (20+namlen));
            data << _player->m_mover->GetGUID();
            data << uint32(text_emote);
            data << uint32(emoteNum);
            data << uint32(namlen);
            if( namlen > 1 )
                data << name;
            else
                data << (uint8)0x00;
        
            _player->m_mover->SendMessageToSetInRange(&data,sWorld->getConfig(CONFIG_LISTEN_RANGE_TEXTEMOTE),true);
        }

        //Send scripted event call
        if (Creature *pCreature = dynamic_cast<Creature *>(i_target)) {
            sScriptMgr->ReceiveEmote(GetPlayer(),pCreature,text_emote);
            pCreature->AI()->ReceiveEmote(GetPlayer(), text_emote);
        }
    }
}

void WorldSession::HandleChatIgnoredOpcode(WorldPacket& recvData )
{
    PROFILE;
    
    CHECK_PACKET_SIZE(recvData, 8+1);

    uint64 iguid;
    uint8 unk;
    //TC_LOG_DEBUG("FIXME","WORLD: Received CMSG_CHAT_IGNORED");

    recvData >> iguid;
    recvData >> unk;                                       // probably related to spam reporting

    Player *player = sObjectMgr->GetPlayer(iguid);
    if(!player || !player->GetSession())
        return;

    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_IGNORED, LANG_UNIVERSAL, nullptr,  GetPlayer(), GetPlayer()->GetName().c_str());
    player->GetSession()->SendPacket(&data);
}

void WorldSession::HandleChannelDeclineInvite(WorldPacket &recvPacket)
{
    // TODO
}

