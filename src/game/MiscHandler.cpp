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
#include "Language.h"
#include "DatabaseEnv.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Log.h"
#include "Player.h"
#include "GossipDef.h"
#include "World.h"
#include "ObjectMgr.h"
#include "WorldSession.h"
#include "BigNumber.h"
#include "SHA1.h"
#include "UpdateData.h"
#include "LootMgr.h"
#include "Chat.h"
#include "ScriptCalls.h"
#include "MapManager.h"
#include "ObjectAccessor.h"
#include "Object.h"
#include "BattleGround.h"
#include "OutdoorPvP.h"
#include "SpellAuras.h"
#include "Pet.h"
#include "SocialMgr.h"
#include "CellImpl.h"
#include "AccountMgr.h"
#include "ScriptMgr.h"
#include "GameObjectAI.h"
#include "IRCMgr.h"

void WorldSession::HandleRepopRequestOpcode( WorldPacket & /*recvData*/ )
{
    TC_LOG_DEBUG("network", "WORLD: Recvd CMSG_REPOP_REQUEST Message");

    if(GetPlayer()->IsAlive()||GetPlayer()->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
        return;

    // the world update order is sessions, players, creatures
    // the netcode runs in parallel with all of these
    // creatures can kill players
    // so if the server is lagging enough the player can
    // release spirit after he's killed but before he is updated
    if(GetPlayer()->GetDeathState() == JUST_DIED)
    {
        TC_LOG_ERROR("network","HandleRepopRequestOpcode: got request after player %s(%d) was killed and before he was updated", GetPlayer()->GetName().c_str(), GetPlayer()->GetGUIDLow());
        GetPlayer()->KillPlayer();
    }

    //this is spirit release confirm?
    GetPlayer()->RemovePet(NULL,PET_SAVE_NOT_IN_SLOT, true);
    GetPlayer()->BuildPlayerRepop();
    GetPlayer()->RepopAtGraveyard();
}

void WorldSession::HandleGossipSelectOptionOpcode(WorldPacket& recvData)
{
    TC_LOG_DEBUG("network", "WORLD: CMSG_GOSSIP_SELECT_OPTION");

    uint32 gossipListId;
    uint32 menuId;
    uint64 guid;
    std::string code = "";

    recvData >> guid >> menuId >> gossipListId;

    if (!_player->PlayerTalkClass->GetGossipMenu().GetItem(gossipListId))
    {
        recvData.rfinish();
        return;
    }

    if (_player->PlayerTalkClass->IsGossipOptionCoded(gossipListId))
        recvData >> code;

    // Prevent cheating on C++ scripted menus
    if (_player->PlayerTalkClass->GetGossipMenu().GetSenderGUID() != guid)
        return;

    Creature* unit = NULL;
    GameObject* go = NULL;
    if (IS_CREATURE_OR_VEHICLE_GUID(guid))
    {
        unit = GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_GOSSIP);
        if (!unit)
        {
            TC_LOG_DEBUG("network", "WORLD: HandleGossipSelectOptionOpcode - " UI64FMTD " not found or you can't interact with him.", guid);
            return;
        }
    }
    else if (IS_GAMEOBJECT_GUID(guid))
    {
        go = _player->GetGameObjectIfCanInteractWith(guid);
        if (!go)
        {
            TC_LOG_DEBUG("network", "WORLD: HandleGossipSelectOptionOpcode - " UI64FMTD " not found.", guid);
            return;
        }
    }
    else
    {
        TC_LOG_DEBUG("network", "WORLD: HandleGossipSelectOptionOpcode - unsupported " UI64FMTD ".", guid);
        return;
    }

    // remove fake death
    if (GetPlayer()->HasUnitState(UNIT_STATE_DIED))
        GetPlayer()->RemoveAurasByType(SPELL_AURA_FEIGN_DEATH);

    if ((unit && unit->GetCreatureTemplate()->ScriptID != unit->LastUsedScriptID) || (go && go->GetGOInfo()->ScriptId != go->LastUsedScriptID))
    {
        TC_LOG_DEBUG("network", "WORLD: HandleGossipSelectOptionOpcode - Script reloaded while in use, ignoring and set new scipt id");
        if (unit)
            unit->LastUsedScriptID = unit->GetCreatureTemplate()->ScriptID;
        if (go)
            go->LastUsedScriptID = go->GetGOInfo()->ScriptId;
        _player->PlayerTalkClass->SendCloseGossip();
        return;
    }
    if (!code.empty())
    {
        if (unit)
        {
            unit->AI()->sGossipSelectCode(_player, menuId, gossipListId, code.c_str());
            if (!sScriptMgr->OnGossipSelectCode(_player, unit, _player->PlayerTalkClass->GetGossipOptionSender(gossipListId), _player->PlayerTalkClass->GetGossipOptionAction(gossipListId), code.c_str()))
                _player->OnGossipSelect(unit, gossipListId, menuId);
        }
        else
        {
            go->AI()->OnGossipSelectCode(_player, menuId, gossipListId, code.c_str());
            if (!sScriptMgr->OnGossipSelectCode(_player, go, _player->PlayerTalkClass->GetGossipOptionSender(gossipListId), _player->PlayerTalkClass->GetGossipOptionAction(gossipListId), code.c_str()))
                _player->OnGossipSelect(go, gossipListId, menuId);
        }
    }
    else
    {
        if (unit)
        {
            unit->AI()->sGossipSelect(_player, menuId, gossipListId);
            if (!sScriptMgr->OnGossipSelect(_player, unit, _player->PlayerTalkClass->GetGossipOptionSender(gossipListId), _player->PlayerTalkClass->GetGossipOptionAction(gossipListId)))
                _player->OnGossipSelect(unit, gossipListId, menuId);
        }
        else
        {
            go->AI()->OnGossipSelect(_player, menuId, gossipListId);
            if (!sScriptMgr->OnGossipSelect(_player, go, _player->PlayerTalkClass->GetGossipOptionSender(gossipListId), _player->PlayerTalkClass->GetGossipOptionAction(gossipListId)))
                _player->OnGossipSelect(go, gossipListId, menuId);
        }
    }
}

void WorldSession::HandleWhoOpcode( WorldPacket & recvData )
{
    TC_LOG_DEBUG("network", "WORLD: Recvd CMSG_WHO Message");

    uint32 level_min, level_max, racemask, classmask, zones_count, str_count;
    uint32 zoneids[10];                                     // 10 is client limit
    std::string player_name, guild_name;

    recvData >> level_min;                                 // maximal player level, default 0
    recvData >> level_max;                                 // minimal player level, default 100 (MAX_LEVEL)
    recvData >> player_name;                               // player name, case sensitive...

    recvData >> guild_name;                                // guild name, case sensitive...

    recvData >> racemask;                                  // race mask
    recvData >> classmask;                                 // class mask
    recvData >> zones_count;                               // zones count, client limit=10 (2.0.10)

    if(zones_count > 10)
        return;                                             // can't be received from real client or broken packet

    for(uint32 i = 0; i < zones_count; i++)
    {
        uint32 temp;
        recvData >> temp;                                  // zone id, 0 if zone is unknown...
        zoneids[i] = temp;
    }

    recvData >> str_count;                                 // user entered strings count, client limit=4 (checked on 2.0.10)

    if(str_count > 4)
        return;                                             // can't be received from real client or broken packet

    std::wstring str[4];                                    // 4 is client limit
    for(uint32 i = 0; i < str_count; i++)
    {
        std::string temp;
        recvData >> temp;                                  // user entered string, it used as universal search pattern(guild+player name)?

        if(!Utf8toWStr(temp,str[i]))
            continue;

        wstrToLower(str[i]);
    }

    std::wstring wplayer_name;
    std::wstring wguild_name;
    if(!(Utf8toWStr(player_name, wplayer_name) && Utf8toWStr(guild_name, wguild_name)))
        return;
    wstrToLower(wplayer_name);
    wstrToLower(wguild_name);

    // client send in case not set max level value 100 but mangos support 255 max level,
    // update it to show GMs with characters after 100 level
    if(level_max >= MAX_LEVEL)
        level_max = STRONG_MAX_LEVEL;

    uint32 team = _player->GetTeam();
    uint32 security = GetSecurity();
    bool allowTwoSideWhoList = sWorld->getConfig(CONFIG_ALLOW_TWO_SIDE_WHO_LIST);
    uint32 gmLevelInWhoList  = sWorld->getConfig(CONFIG_GM_LEVEL_IN_WHO_LIST);

    uint32 matchcount = 0;
    uint32 displaycount = 0;

    WorldPacket data( SMSG_WHO, 50 );                       // guess size
    data << uint32(matchcount); //placeholder, will be overriden later
    data << uint32(displaycount);

    boost::shared_lock<boost::shared_mutex> lock(*HashMapHolder<Player>::GetLock());
    HashMapHolder<Player>::MapType& m = ObjectAccessor::GetPlayers();
    for(HashMapHolder<Player>::MapType::iterator itr = m.begin(); itr != m.end(); ++itr)
    {
        if (security == SEC_PLAYER)
        {
            // player can see member of other team only if CONFIG_ALLOW_TWO_SIDE_WHO_LIST
            if (itr->second->GetTeam() != team && !allowTwoSideWhoList )
                continue;

            // player can see MODERATOR, GAME MASTER, ADMINISTRATOR only if CONFIG_GM_IN_WHO_LIST
            if ((itr->second->GetSession()->GetSecurity() > gmLevelInWhoList))
                continue;
        }

        // check if target is globally visible for player
        if (!(itr->second->IsVisibleGloballyFor(_player)))
            continue;

        // check if target's level is in level range
        uint32 lvl = itr->second->GetLevel();
        if (lvl < level_min || lvl > level_max)
            continue;

        // check if class matches classmask
        uint32 class_ = itr->second->GetClass();
        if (!(classmask & (1 << class_)))
            continue;

        // check if race matches racemask
        uint32 race = itr->second->GetRace();
        if (!(racemask & (1 << race)))
            continue;

        uint32 pzoneid = itr->second->GetZoneId();
        //do not show players in arenas
        if(pzoneid == 3698 || pzoneid == 3968 || pzoneid == 3702)
        {
            uint32 mapId = itr->second->GetBattlegroundEntryPointMap();
            Map * map = sMapMgr->FindBaseNonInstanceMap(mapId);
            if(map) 
            {
                float x = itr->second->GetBattlegroundEntryPointX();
                float y = itr->second->GetBattlegroundEntryPointY();
                float z = itr->second->GetBattlegroundEntryPointZ();
                pzoneid = map->GetZoneId(x,y,z);
            }
        }
        
        uint8 gender = itr->second->GetGender();

        bool z_show = true;
        for(uint32 i = 0; i < zones_count; i++)
        {
            if(zoneids[i] == pzoneid)
            {
                z_show = true;
                break;
            }

            z_show = false;
        }
        if (!z_show)
            continue;

        std::string pname = itr->second->GetName();
        std::wstring wpname;
        if(!Utf8toWStr(pname,wpname))
            continue;
        wstrToLower(wpname);

        if (!(wplayer_name.empty() || wpname.find(wplayer_name) != std::wstring::npos))
            continue;

        std::string gname = sObjectMgr->GetGuildNameById(itr->second->GetGuildId());
        std::wstring wgname;
        if(!Utf8toWStr(gname,wgname))
            continue;
        wstrToLower(wgname);

        if (!(wguild_name.empty() || wgname.find(wguild_name) != std::wstring::npos))
            continue;

        std::string aname;
        if(AreaTableEntry const* areaEntry = sAreaTableStore.LookupEntry(itr->second->GetAreaId()))
            aname = areaEntry->area_name[GetSessionDbcLocale()];

        bool s_show = true;
        for(uint32 i = 0; i < str_count; i++)
        {
            if (!str[i].empty())
            {
                if (wgname.find(str[i]) != std::wstring::npos ||
                    wpname.find(str[i]) != std::wstring::npos ||
                    Utf8FitTo(aname, str[i]) )
                {
                    s_show = true;
                    break;
                }
                s_show = false;
            }
        }
        if (!s_show)
            continue;


        ++matchcount;
        if (matchcount >= 50) // 49 is maximum player count sent to client - apparently can be overriden but is said unstable
            continue; //continue counting, just do not insert

        data << pname;                                    // player name
        data << gname;                                    // guild name
        data << uint32(lvl);                              // player level
        data << uint32(class_);                           // player class
        data << uint32(race);                             // player race
        data << uint8(gender);                            // new 2.4.0
        data << uint32(pzoneid);                          // player zone id

        ++displaycount;
    }

    data.put(0, displaycount);                             // insert right count, count of matches
    data.put(4, matchcount);                               // insert right count, count displayed

    SendPacket(&data);
}

void WorldSession::HandleLogoutRequestOpcode( WorldPacket & /*recvData*/ )
{
    TC_LOG_DEBUG("network", "WORLD: Recvd CMSG_LOGOUT_REQUEST Message, security - %u", GetSecurity());

    if (uint64 lguid = GetPlayer()->GetLootGUID())
        DoLootRelease(lguid);

	bool canLogoutInCombat = GetPlayer()->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING);

	bool instantLogout = canLogoutInCombat ||
		GetPlayer()->IsInFlight() || GetSecurity() >= sWorld->getConfig(CONFIG_INSTANT_LOGOUT);

	uint8 reason = 0;
	if (GetPlayer()->IsInCombat() && !canLogoutInCombat)
		reason = 1;
	else if (GetPlayer()->HasUnitMovementFlag(MOVEMENTFLAG_JUMPING_OR_FALLING | MOVEMENTFLAG_FALLING_FAR)) // is jumping or falling
		reason = 3;                                           
	else if (GetPlayer()->duel || GetPlayer()->HasAura(9454)) // is dueling or frozen by GM via freeze command
		reason = 0xC;  //not right id, need to get the correct value    


	WorldPacket data(SMSG_LOGOUT_RESPONSE, 4+1);
	data << uint32(reason);
	data << uint8(instantLogout);
	SendPacket(&data);

	if (reason)
	{
		LogoutRequest(0);
		return;
	}

    //instant logout in taverns/cities or on taxi or for admins, gm's, mod's if its enabled in mangosd.conf
    if (instantLogout)
    {
        LogoutPlayer(true);
        return;
    }

    // not set flags if player can't free move to prevent lost state at logout cancel
    if(GetPlayer()->CanFreeMove())
    {
        if (GetPlayer()->GetStandState() == UNIT_STAND_STATE_STAND)
           GetPlayer()->SetStandState(PLAYER_STATE_SIT);

        WorldPacket data( SMSG_FORCE_MOVE_ROOT, (8+4) );    // guess size
        data << GetPlayer()->GetPackGUID();
        data << (uint32)2;
        SendPacket( &data );
        GetPlayer()->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_STUNNED);
    }

	LogoutRequest(time(NULL));
}

void WorldSession::HandlePlayerLogoutOpcode( WorldPacket & /*recvData*/ )
{
    //TC_LOG_DEBUG("network", "WORLD: Recvd CMSG_PLAYER_LOGOUT Message");
}

void WorldSession::HandleLogoutCancelOpcode( WorldPacket & /*recvData*/ )
{
    //TC_LOG_DEBUG("network", "WORLD: Recvd CMSG_LOGOUT_CANCEL Message");

    // Player have already logged out serverside, too late to cancel
    if (!GetPlayer())
        return;

    LogoutRequest(0);

    WorldPacket data( SMSG_LOGOUT_CANCEL_ACK, 0 );
    SendPacket( &data );

    // not remove flags if can't free move - its not set in Logout request code.
    if(GetPlayer()->CanFreeMove())
    {
        //!we can move again
        data.Initialize( SMSG_FORCE_MOVE_UNROOT, 8 );       // guess size
        data << GetPlayer()->GetPackGUID();
        data << uint32(0);
        SendPacket( &data );

        //! Stand Up
        GetPlayer()->SetStandState(PLAYER_STATE_NONE);

        //! DISABLE_ROTATE
        GetPlayer()->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_STUNNED);
    }

    TC_LOG_DEBUG("network", "WORLD: Sent SMSG_LOGOUT_CANCEL_ACK Message");
}

void WorldSession::HandleTogglePvP( WorldPacket & recvData )
{
    // this opcode can be used in two ways: Either set explicit new status or toggle old status
    if(recvData.size() == 1)
    {
        bool newPvPStatus;
        recvData >> newPvPStatus;
        if(!newPvPStatus || !GetPlayer()->IsInDuelArea()) //can only be set active outside pvp zone
            GetPlayer()->ApplyModFlag(PLAYER_FLAGS, PLAYER_FLAGS_IN_PVP, newPvPStatus);
    }
    else
    {
        if(!GetPlayer()->IsInDuelArea())
            GetPlayer()->ToggleFlag(PLAYER_FLAGS, PLAYER_FLAGS_IN_PVP);
        else
            GetPlayer()->ApplyModFlag(PLAYER_FLAGS, PLAYER_FLAGS_IN_PVP, false);
    }

    if(GetPlayer()->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_IN_PVP))
    {
        if(!GetPlayer()->IsPvP() || GetPlayer()->pvpInfo.endTimer != 0)
            GetPlayer()->UpdatePvP(true, true);
    }
    else
    {
        if(!GetPlayer()->pvpInfo.inHostileArea && GetPlayer()->IsPvP())
            GetPlayer()->pvpInfo.endTimer = time(NULL);     // start toggle-off
    }

    if(OutdoorPvP * pvp = _player->GetOutdoorPvP())
    {
        pvp->HandlePlayerActivityChanged(_player);
    }
}

void WorldSession::HandleZoneUpdateOpcode( WorldPacket & recvData )
{
    

    uint32 newZone;
    recvData >> newZone;

    TC_LOG_DEBUG("network","WORLD: Recvd ZONE_UPDATE: %u", newZone);

    GetPlayer()->UpdateZone(newZone);

    GetPlayer()->SendInitWorldStates(true,newZone);
}

void WorldSession::HandleSetTargetOpcode( WorldPacket & recvData )
{
    // When this packet send?
    

    uint64 guid ;
    recvData >> guid;

    _player->SetUInt32Value(UNIT_FIELD_TARGET,guid);

    // update reputation list if need
    Unit* unit = ObjectAccessor::GetUnit(*_player, guid );
    if(!unit)
        return;

    _player->SetFactionVisibleForFactionTemplateId(unit->GetFaction());
}

void WorldSession::HandleSetSelectionOpcode( WorldPacket & recvData )
{
    

    uint64 guid;
    recvData >> guid;

    _player->SetSelection(guid);

    // update reputation list if need
    Unit* unit = ObjectAccessor::GetUnit(*_player, guid);
    if (_player->HaveSpectators())
    {
        if (Battleground *bg = _player->GetBattleground())
        {
            if (unit && bg->isSpectator(unit->GetGUID()))
                return;
        }
        SpectatorAddonMsg msg;
        msg.SetPlayer(_player->GetName());
        msg.SetTarget(unit ? unit->GetName() : "0");
        _player->SendSpectatorAddonMsgToBG(msg);
    }

    if (unit)
        _player->SetFactionVisibleForFactionTemplateId(unit->GetFaction());
}

void WorldSession::HandleStandStateChangeOpcode( WorldPacket & recvData )
{
    if(!_player->m_mover->IsAlive())
        return;

    

    uint8 animstate;
    recvData >> animstate;

    if (_player->m_mover->GetStandState() != animstate)
        _player->m_mover->SetStandState(animstate);
}

void WorldSession::HandleContactListOpcode( WorldPacket & recvData )
{
    uint32 unk;
    recvData >> unk;
    // TC_LOG_DEBUG("network", "WORLD: Received CMSG_CONTACT_LIST - Unk: %d", unk);
    _player->GetSocial()->SendSocialList();
}

void WorldSession::HandleAddFriendOpcode(WorldPacket& recvData)
{
    //TC_LOG_DEBUG("network", "WORLD: Received CMSG_ADD_FRIEND");

    std::string friendName = GetTrinityString(LANG_FRIEND_IGNORE_UNKNOWN);
    std::string friendNote;

    recvData >> friendName;

    // recheck
    

    recvData >> friendNote;

    if (!normalizePlayerName(friendName))
        return;

    TC_LOG_DEBUG("network", "WORLD: %s asked to add friend : '%s'",
        GetPlayer()->GetName().c_str(), friendName.c_str());

    PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_GUID_RACE_ACC_BY_NAME);

    stmt->setString(0, friendName);

    _addFriendCallback.SetParam(friendNote);
    _addFriendCallback.SetFutureResult(CharacterDatabase.AsyncQuery(stmt));
}

void WorldSession::HandleAddFriendOpcodeCallBack(PreparedQueryResult result, std::string const& friendNote)
{
    if (!GetPlayer())
        return;

    uint64 friendGuid;
    uint64 friendAcctid;
    uint32 team;
    FriendsResult friendResult;
 
    friendResult = FRIEND_NOT_FOUND;
    friendGuid = 0;

    if(result)
    {
        friendGuid = MAKE_NEW_GUID((*result)[0].GetUInt32(), 0, HIGHGUID_PLAYER);
        team = Player::TeamForRace((*result)[1].GetUInt8());
        friendAcctid = (*result)[2].GetUInt32();

        if ( GetSecurity() >= SEC_GAMEMASTER1 || sWorld->getConfig(CONFIG_ALLOW_GM_FRIEND) || sAccountMgr->GetSecurity(friendAcctid) < SEC_GAMEMASTER1)
            if(friendGuid)
            {
                if(friendGuid==GetPlayer()->GetGUID())
                    friendResult = FRIEND_SELF;
                else if(GetPlayer()->GetTeam() != team && !sWorld->getConfig(CONFIG_ALLOW_TWO_SIDE_ADD_FRIEND) && GetSecurity() < SEC_GAMEMASTER1)
                    friendResult = FRIEND_ENEMY;
                else if(GetPlayer()->GetSocial()->HasFriend(GUID_LOPART(friendGuid)))
                    friendResult = FRIEND_ALREADY;
                else
                {
                    Player* pFriend = ObjectAccessor::FindPlayer(friendGuid);
                    if( pFriend && pFriend->IsInWorld() && pFriend->IsVisibleGloballyFor(GetPlayer()))
                    friendResult = FRIEND_ADDED_ONLINE;
                    else
                        friendResult = FRIEND_ADDED_OFFLINE;
                    if(!GetPlayer()->GetSocial()->AddToSocialList(GUID_LOPART(friendGuid), false))
                    {
                        friendResult = FRIEND_LIST_FULL;
                        TC_LOG_DEBUG("network", "WORLD: %s's friend list is full.", GetPlayer()->GetName().c_str());
                    }
                }
                GetPlayer()->GetSocial()->SetFriendNote(GUID_LOPART(friendGuid), friendNote);
            }
    }

    sSocialMgr->SendFriendStatus(GetPlayer(), friendResult, GUID_LOPART(friendGuid), false);

    TC_LOG_DEBUG("network", "WORLD: Sent (SMSG_FRIEND_STATUS)");
}

void WorldSession::HandleDelFriendOpcode( WorldPacket & recvData )
{
    

    uint64 friendGUID;
    recvData >> friendGUID;

    _player->GetSocial()->RemoveFromSocialList(GUID_LOPART(friendGUID), false);

    sSocialMgr->SendFriendStatus(GetPlayer(), FRIEND_REMOVED, GUID_LOPART(friendGUID), false);
}

void WorldSession::HandleAddIgnoreOpcode( WorldPacket & recvData )
{
    

    TC_LOG_DEBUG("network", "WORLD: Received CMSG_ADD_IGNORE");

    std::string ignoreName = GetTrinityString(LANG_FRIEND_IGNORE_UNKNOWN);

    recvData >> ignoreName;

    if (!normalizePlayerName(ignoreName))
        return;

    TC_LOG_DEBUG("network", "WORLD: %s asked to Ignore: '%s'",
        GetPlayer()->GetName().c_str(), ignoreName.c_str());

    PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_GUID_BY_NAME);

    stmt->setString(0, ignoreName);

    _addIgnoreCallback = CharacterDatabase.AsyncQuery(stmt);
}

void WorldSession::HandleAddIgnoreOpcodeCallBack(PreparedQueryResult result)
{
    if (!GetPlayer())
        return;

    uint64 IgnoreGuid;
    FriendsResult ignoreResult;

    ignoreResult = FRIEND_IGNORE_NOT_FOUND;
    IgnoreGuid = 0;

    if (result)
    {
        IgnoreGuid = MAKE_NEW_GUID((*result)[0].GetUInt32(), 0, HIGHGUID_PLAYER);

        if (IgnoreGuid)
        {
            if (IgnoreGuid == GetPlayer()->GetGUID())              //not add yourself
                ignoreResult = FRIEND_IGNORE_SELF;
            else if (GetPlayer()->GetSocial()->HasIgnore(GUID_LOPART(IgnoreGuid)))
                ignoreResult = FRIEND_IGNORE_ALREADY;
            else
            {
                ignoreResult = FRIEND_IGNORE_ADDED;

                // ignore list full
                if (!GetPlayer()->GetSocial()->AddToSocialList(GUID_LOPART(IgnoreGuid), true))
                    ignoreResult = FRIEND_IGNORE_FULL;
            }
        }
    }

    sSocialMgr->SendFriendStatus(GetPlayer(), ignoreResult, GUID_LOPART(IgnoreGuid), false);

    TC_LOG_DEBUG("network", "WORLD: Sent (SMSG_FRIEND_STATUS)");
}

void WorldSession::HandleDelIgnoreOpcode( WorldPacket & recvData )
{
    
    
    

    TC_LOG_DEBUG("network", "WORLD: Received CMSG_DEL_IGNORE");

    uint64 ignoreGUID;

    recvData >> ignoreGUID;

    _player->GetSocial()->RemoveFromSocialList(GUID_LOPART(ignoreGUID), true);

    sSocialMgr->SendFriendStatus(GetPlayer(), FRIEND_IGNORE_REMOVED, GUID_LOPART(ignoreGUID), false);
}

void WorldSession::HandleSetContactNotesOpcode( WorldPacket & recvData )
{
    
    uint64 guid;
    std::string note;
    recvData >> guid >> note;
    _player->GetSocial()->SetFriendNote(guid, note);
}

void WorldSession::HandleBugOpcode( WorldPacket & recvData )
{
    

    uint32 suggestion, contentlen;
    std::string content;
    uint32 typelen;
    std::string type;

    recvData >> suggestion >> contentlen >> content;

    //recheck
    

    recvData >> typelen >> type;

    CharacterDatabase.EscapeString(type);
    CharacterDatabase.EscapeString(content);
    CharacterDatabase.PExecute ("INSERT INTO bugreport (type,content) VALUES('%s', '%s')", type.c_str( ), content.c_str( ));
}

void WorldSession::HandleReclaimCorpseOpcode(WorldPacket &recvData)
{
    

    TC_LOG_DEBUG("network","WORLD: Received CMSG_RECLAIM_CORPSE");
    if (GetPlayer()->IsAlive())
        return;

    // do not allow corpse reclaim in arena
    if (GetPlayer()->InArena())
        return;

    // body not released yet
    if(!GetPlayer()->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
        return;

    Corpse *corpse = GetPlayer()->GetCorpse();

    if (!corpse )
        return;

    // prevent resurrect before 30-sec delay after body release not finished
    if(GetPlayer()->GetDeathTime() + GetPlayer()->GetCorpseReclaimDelay(corpse->GetType()==CORPSE_RESURRECTABLE_PVP) > time(NULL))
        return;

    float dist = corpse->GetDistance2d(GetPlayer());
    if (dist > CORPSE_RECLAIM_RADIUS)
        return;

    uint64 guid;
    recvData >> guid;

    // resurrect
    GetPlayer()->ResurrectPlayer(GetPlayer()->InBattleground() ? 1.0f : 0.5f);

    // spawn bones
    GetPlayer()->SpawnCorpseBones();

    GetPlayer()->SaveToDB();
}

void WorldSession::HandleResurrectResponseOpcode(WorldPacket & recvData)
{
    

    TC_LOG_DEBUG("network","WORLD: Received CMSG_RESURRECT_RESPONSE");

    if(GetPlayer()->IsAlive())
        return;

    uint64 guid;
    uint8 status;
    recvData >> guid;
    recvData >> status;

    if(status == 0)
    {
        GetPlayer()->clearResurrectRequestData();           // reject
        return;
    }

    if(!GetPlayer()->isRessurectRequestedBy(guid))
        return;

    GetPlayer()->RessurectUsingRequestData();
    GetPlayer()->SaveToDB();
}

void WorldSession::HandleAreaTriggerOpcode(WorldPacket & recvData)
{
    

    uint32 triggerId;
    recvData >> triggerId;
    
    TC_LOG_DEBUG("network", "CMSG_AREATRIGGER. Trigger ID: %u", triggerId);

    if(GetPlayer()->IsGameMaster())
        SendAreaTriggerMessage("Entered areatrigger %u.", triggerId);

    if(GetPlayer()->IsInFlight())
        return;

    AreaTriggerEntry const* atEntry = sAreaTriggerStore.LookupEntry(triggerId);
    if(!atEntry)
    {
        TC_LOG_ERROR("network","Player '%s' (GUID: %u) send unknown (by DBC) Area Trigger ID:%u",GetPlayer()->GetName().c_str(),GetPlayer()->GetGUIDLow(), triggerId);
        return;
    }

    if (GetPlayer()->GetMapId()!=atEntry->mapid)
    {
        TC_LOG_ERROR("network","Player '%s' (GUID: %u) too far (trigger map: %u player map: %u), ignore Area Trigger ID: %u", GetPlayer()->GetName().c_str(), atEntry->mapid, GetPlayer()->GetMapId(), GetPlayer()->GetGUIDLow(), triggerId);
        return;
    }

    // delta is safe radius
    const float delta = 5.0f;
    // check if player in the range of areatrigger
    Player* pl = GetPlayer();

    if (atEntry->radius > 0)
    {
        // if we have radius check it
        float dist = pl->GetDistance(atEntry->x,atEntry->y,atEntry->z);
        if(dist > atEntry->radius + delta)
        {
            TC_LOG_ERROR("network","Player '%s' (GUID: %u) too far (radius: %f distance: %f), ignore Area Trigger ID: %u",
                pl->GetName().c_str(), pl->GetGUIDLow(), atEntry->radius, dist, triggerId);
            return;
        }
    }
    else if (atEntry->id != 4853)
    {
        // we have only extent
        float dx = pl->GetPositionX() - atEntry->x;
        float dy = pl->GetPositionY() - atEntry->y;
        float dz = pl->GetPositionZ() - atEntry->z;
        double es = sin(atEntry->box_orientation);
        double ec = cos(atEntry->box_orientation);
        // calc rotated vector based on extent axis
        double rotateDx = dx*ec - dy*es;
        double rotateDy = dx*es + dy*ec;

        if ((fabs(rotateDx) > atEntry->box_x / 2 + delta) ||
                (fabs(rotateDy) > atEntry->box_y / 2 + delta) ||
                (fabs(dz) > atEntry->box_z / 2 + delta)) {
            return;
        }
    }

    if(sScriptMgr->OnAreaTrigger(GetPlayer(), atEntry))
        return;

    uint32 quest_id = sObjectMgr->GetQuestForAreaTrigger( triggerId );
    if( quest_id && GetPlayer()->IsAlive() && GetPlayer()->IsActiveQuest(quest_id) )
    {
        Quest const* pQuest = sObjectMgr->GetQuestTemplate(quest_id);
        if( pQuest )
        {
            if(GetPlayer()->GetQuestStatus(quest_id) == QUEST_STATUS_INCOMPLETE)
                GetPlayer()->AreaExploredOrEventHappens( quest_id );
        }
    }

    if(sObjectMgr->IsTavernAreaTrigger(triggerId))
    {
        // set resting flag we are in the inn
        GetPlayer()->SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING);
        GetPlayer()->InnEnter(time(NULL), atEntry->mapid, atEntry->x, atEntry->y, atEntry->z);
        GetPlayer()->SetRestType(REST_TYPE_IN_TAVERN);

        if(sWorld->IsFFAPvPRealm())
            GetPlayer()->RemoveFlag(PLAYER_FLAGS,PLAYER_FLAGS_FFA_PVP);

        return;
    }

    if(GetPlayer()->InBattleground())
    {
        Battleground* bg = GetPlayer()->GetBattleground();
        if(bg)
            if(bg->GetStatus() == STATUS_IN_PROGRESS)
                bg->HandleAreaTrigger(GetPlayer(), triggerId);

        return;
    }

    if(OutdoorPvP * pvp = GetPlayer()->GetOutdoorPvP())
    {
        if(pvp->HandleAreaTrigger(_player, triggerId))
            return;
    }

    // NULL if all values default (non teleport trigger)
    AreaTrigger const* at = sObjectMgr->GetAreaTrigger(triggerId);
    if(!at)
        return;

    if(!GetPlayer()->Satisfy(sObjectMgr->GetAccessRequirement(at->access_id), at->target_mapId, true))
        return;

    GetPlayer()->TeleportTo(at->target_mapId,at->target_X,at->target_Y,at->target_Z,at->target_Orientation,TELE_TO_NOT_LEAVE_TRANSPORT);
}

void WorldSession::HandleUpdateAccountData(WorldPacket &/*recvData*/)
{
    TC_LOG_DEBUG("network", "WORLD: Received CMSG_UPDATE_ACCOUNT_DATA");
    // TODO
    /* TC CODE

    uint32 type, timestamp, decompressedSize;
    recvData >> type >> timestamp >> decompressedSize;

    TC_LOG_DEBUG("network", "UAD: type %u, time %u, decompressedSize %u", type, timestamp, decompressedSize);

    if (type > NUM_ACCOUNT_DATA_TYPES)
    return;

    if (decompressedSize == 0)                               // erase
    {
    SetAccountData(AccountDataType(type), 0, "");

    WorldPacket data(SMSG_UPDATE_ACCOUNT_DATA_COMPLETE, 4+4);
    data << uint32(type);
    data << uint32(0);
    SendPacket(&data);

    return;
    }

    if (decompressedSize > 0xFFFF)
    {
    recvData.rfinish();                   // unnneded warning spam in this case
    TC_LOG_ERROR("network", "UAD: Account data packet too big, size %u", decompressedSize);
    return;
    }

    ByteBuffer dest;
    dest.resize(decompressedSize);

    uLongf realSize = decompressedSize;
    if (uncompress(dest.contents(), &realSize, recvData.contents() + recvData.rpos(), recvData.size() - recvData.rpos()) != Z_OK)
    {
    recvData.rfinish();                   // unnneded warning spam in this case
    TC_LOG_ERROR("network", "UAD: Failed to decompress account data");
    return;
    }

    recvData.rfinish();                       // uncompress read (recvData.size() - recvData.rpos())

    std::string adata;
    dest >> adata;

    SetAccountData(AccountDataType(type), timestamp, adata);

    WorldPacket data(SMSG_UPDATE_ACCOUNT_DATA_COMPLETE, 4+4);
    data << uint32(type);
    data << uint32(0);
    SendPacket(&data);

    */
}

void WorldSession::HandleRequestAccountData(WorldPacket& /*recvData*/)
{
    TC_LOG_DEBUG("network", "WORLD: Received CMSG_REQUEST_ACCOUNT_DATA");
    // TODO

    /* TC CODE

    uint32 type;
    recvData >> type;

    TC_LOG_DEBUG("network", "RAD: type %u", type);

    if (type >= NUM_ACCOUNT_DATA_TYPES)
    return;

    AccountData* adata = GetAccountData(AccountDataType(type));

    uint32 size = adata->Data.size();

    uLongf destSize = compressBound(size);

    ByteBuffer dest;
    dest.resize(destSize);

    if (size && compress(dest.contents(), &destSize, (uint8 const*)adata->Data.c_str(), size) != Z_OK)
    {
    TC_LOG_DEBUG("network", "RAD: Failed to compress account data");
    return;
    }

    dest.resize(destSize);

    WorldPacket data(SMSG_UPDATE_ACCOUNT_DATA, 8+4+4+4+destSize);
    data << uint64(_player ? _player->GetGUID() : ObjectGuid::Empty);
    data << uint32(type);                                   // type (0-7)
    data << uint32(adata->Time);                            // unix time
    data << uint32(size);                                   // decompressed length
    data.append(dest);                                      // compressed data
    SendPacket(&data);
    */
}

void WorldSession::HandleSetActionButtonOpcode(WorldPacket& recvData)
{
    

    uint8 button, misc, type;
    uint16 action;
    recvData >> button >> action >> misc >> type;

    TC_LOG_DEBUG("network", "CMSG_SET_ACTION_BUTTON Button: %u", button);

    if (action == 0)
        GetPlayer()->removeActionButton(button);
    else
{
        if (type == ACTION_BUTTON_MACRO || type == ACTION_BUTTON_CMACRO) {
            GetPlayer()->addActionButton(button, action, type, misc);
        }
        else if(type==ACTION_BUTTON_SPELL)
        {
            GetPlayer()->addActionButton(button,action,type,misc);
        }
        else if(type==ACTION_BUTTON_ITEM)
        {
            GetPlayer()->addActionButton(button,action,type,misc);
        }
        else
            TC_LOG_ERROR( "network", "MISC: Unknown action button type %u for action %u into button %u", type, action, button );
    }
}

void WorldSession::HandleCompleteCinematic( WorldPacket & /*recvData*/ )
{
    TC_LOG_DEBUG("network", "WORLD: Received CMSG_COMPLETE_CINEMATIC");
    //SO WHAT?
}

void WorldSession::HandleNextCinematicCamera( WorldPacket & /*recvData*/ )
{
    TC_LOG_DEBUG("network", "WORLD: Received CMSG_NEXT_CINEMATIC_CAMERA");
}

void WorldSession::HandleMoveTimeSkippedOpcode( WorldPacket & recvData )
{
    //TC_LOG_DEBUG("network", "WORLD: Received CMSG_MOVE_TIME_SKIPPED");

    uint64 guid;
    uint32 time_skipped;
#ifdef BUILD_335_SUPPORT
    if(GetClientBuild() == BUILD_335)
        recvData.readPackGUID(guid);
    else
#endif
        recvData >> guid;

    recvData >> time_skipped;

    // ignore updates for not us
    if (_player == NULL || guid != _player->GetGUID())
        return;

    WorldPacket data(MSG_MOVE_TIME_SKIPPED, 12);
#ifdef BUILD_335_SUPPORT
    if(GetClientBuild() == BUILD_335)
        data << _player->GetPackGUID();
    else
#endif
        data << guid;

    data << time_skipped;

    _player->SendMessageToSet(&data, false);
}

void WorldSession::HandleFeatherFallAck(WorldPacket & recvData)
{
    TC_LOG_DEBUG("network", "WORLD: CMSG_MOVE_FEATHER_FALL_ACK");

    // not used
    recvData.rfinish();
}

void WorldSession::HandleMoveUnRootAck(WorldPacket& recvData)
{
    recvData.rfinish();                       // prevent warnings spam

    TC_LOG_DEBUG("network", "WORLD: CMSG_FORCE_MOVE_UNROOT_ACK");
    /*
        

        TC_LOG_DEBUG("network", "WORLD: CMSG_FORCE_MOVE_UNROOT_ACK" );
        recvData.hexlike();
        uint64 guid;
        uint64 unknown1;
        uint32 unknown2;
        float PositionX;
        float PositionY;
        float PositionZ;
        float Orientation;

        recvData >> guid;
        MovementInfo movementInfo;
        recvData >> movementInfo;

        // TODO for later may be we can use for anticheat
        TC_LOG_DEBUG("network","Guid " UI64FMTD,guid);
        TC_LOG_DEBUG("network","unknown1 " UI64FMTD,unknown1);
        TC_LOG_DEBUG("network","unknown2 %u",unknown2);
        TC_LOG_DEBUG("network","X %f",PositionX);
        TC_LOG_DEBUG("network","Y %f",PositionY);
        TC_LOG_DEBUG("network","Z %f",PositionZ);
        TC_LOG_DEBUG("network","O %f",Orientation);
    */
}

void WorldSession::HandleMoveRootAck(WorldPacket& recvData)
{
    

    // no used
    recvData.rfinish();                       // prevent warnings spam
    /*
        

        TC_LOG_DEBUG("network", "WORLD: CMSG_FORCE_MOVE_ROOT_ACK" );
        recvData.hexlike();
        uint64 guid;
        uint64 unknown1;
        uint32 unknown2;
        float PositionX;
        float PositionY;
        float PositionZ;
        float Orientation;

        recvData >> guid;
        MovementInfo movementInfo;
        recvData >> movementInfo;

        // for later may be we can use for anticheat
        TC_LOG_DEBUG("network","Guid " UI64FMTD,guid);
        TC_LOG_DEBUG("network","unknown1 " UI64FMTD,unknown1);
        TC_LOG_DEBUG("network","unknown1 %u",unknown2);
        TC_LOG_DEBUG("network","X %f",PositionX);
        TC_LOG_DEBUG("network","Y %f",PositionY);
        TC_LOG_DEBUG("network","Z %f",PositionZ);
        TC_LOG_DEBUG("network","O %f",Orientation);
    */
}

void WorldSession::HandleSetActionBarToggles(WorldPacket& recvData)
{
    

    uint8 ActionBar;
    recvData >> ActionBar;

    if(!GetPlayer())                                        // ignore until not logged (check needed because STATUS_AUTHED)
    {
        if(ActionBar!=0)
            TC_LOG_ERROR("network","WorldSession::HandleSetActionBar in not logged state with value: %u, ignored",uint32(ActionBar));
        return;
    }

    GetPlayer()->SetByteValue(PLAYER_FIELD_BYTES, 2, ActionBar);
}

void WorldSession::HandlePlayedTime(WorldPacket& /*recvData*/)
{
    uint32 TotalTimePlayed = GetPlayer()->GetTotalPlayedTime();
    uint32 LevelPlayedTime = GetPlayer()->GetLevelPlayedTime();

    WorldPacket data(SMSG_PLAYED_TIME, 8);
    data << TotalTimePlayed;
    data << LevelPlayedTime;
    SendPacket(&data);
}

void WorldSession::HandleInspectOpcode(WorldPacket& recvData)
{
    if (GetClientBuild() == BUILD_335)
        return;  //no support yet

    

    uint64 guid;
    recvData >> guid;

    _player->SetSelection(guid);

    Player *plr = sObjectMgr->GetPlayer(guid);
    if(!plr)                                                // wrong player
        return;
    
    uint32 talent_points = 0x3D; //bc talent count
    uint32 guid_size = plr->GetPackGUID().size();
    WorldPacket data(SMSG_INSPECT_TALENT, guid_size+4+talent_points);
    data << plr->GetPackGUID();
    data << uint32(talent_points);

    // fill by 0 talents array
    for(uint32 i = 0; i < talent_points; ++i)
        data << uint8(0);

    if(sWorld->getConfig(CONFIG_TALENTS_INSPECTING) || _player->IsGameMaster())
    {
        // find class talent tabs (all players have 3 talent tabs)
        uint32 const* talentTabIds = GetTalentTabPages(plr->GetClass());

        uint32 talentTabPos = 0;                            // pos of first talent rank in tab including all prev tabs
        for(uint32 i = 0; i < 3; ++i)
        {
            uint32 talentTabId = talentTabIds[i];

            // fill by real data
            for(uint32 talentId = 0; talentId < sTalentStore.GetNumRows(); ++talentId)
            {
                TalentEntry const* talentInfo = sTalentStore.LookupEntry(talentId);
                if(!talentInfo)
                    continue;

                // skip another tab talents
                if(talentInfo->TalentTab != talentTabId)
                    continue;

                // find talent rank
                uint32 curtalent_maxrank = 0;
                for(uint32 k = 5; k > 0; --k)
                {
                    if(talentInfo->RankID[k-1] && plr->HasSpell(talentInfo->RankID[k-1]))
                    {
                        curtalent_maxrank = k;
                        break;
                    }
                }

                // not learned talent
                if(!curtalent_maxrank)
                    continue;

                // 1 rank talent bit index
                uint32 curtalent_index = talentTabPos + GetTalentInspectBitPosInTab(talentId);

                uint32 curtalent_rank_index = curtalent_index+curtalent_maxrank-1;

                // slot/offset in 7-bit bytes
                uint32 curtalent_rank_slot7   = curtalent_rank_index / 7;
                uint32 curtalent_rank_offset7 = curtalent_rank_index % 7;

                // rank pos with skipped 8 bit
                uint32 curtalent_rank_index2 = curtalent_rank_slot7 * 8 + curtalent_rank_offset7;

                // slot/offset in 8-bit bytes with skipped high bit
                uint32 curtalent_rank_slot = curtalent_rank_index2 / 8;
                uint32 curtalent_rank_offset =  curtalent_rank_index2 % 8;

                // apply mask
                uint32 val = data.read<uint8>(guid_size + 4 + curtalent_rank_slot);
                val |= (1 << curtalent_rank_offset);
                data.put<uint8>(guid_size + 4 + curtalent_rank_slot, val & 0xFF);
            }

            talentTabPos += GetTalentTabInspectBitSize(talentTabId);
        }
    }

    SendPacket(&data);
}

void WorldSession::HandleInspectHonorStatsOpcode(WorldPacket& recvData)
{
    uint64 guid;
    recvData >> guid;

    Player *player = sObjectMgr->GetPlayer(guid);

    if(!player)
    {
        TC_LOG_ERROR("network","InspectHonorStats: WTF, player not found...");
        return;
    }

    WorldPacket data(MSG_INSPECT_HONOR_STATS, 8+1+4*4);
    data << uint64(player->GetGUID());
	data << uint8(player->GetHonorPoints()); //mangos has GetHighestPvPRankIndex here
    data << uint32(player->GetUInt32Value(PLAYER_FIELD_KILLS));
    data << uint32(player->GetUInt32Value(PLAYER_FIELD_TODAY_CONTRIBUTION));
    data << uint32(player->GetUInt32Value(PLAYER_FIELD_YESTERDAY_CONTRIBUTION));
    data << uint32(player->GetUInt32Value(PLAYER_FIELD_LIFETIME_HONORABLE_KILLS));
    SendPacket(&data);
}

void WorldSession::HandleWorldTeleportOpcode(WorldPacket& recvData)
{
    // write in client console: worldport 469 452 6454 2536 180 or /console worldport 469 452 6454 2536 180
    // Received opcode CMSG_WORLD_TELEPORT
    // Time is ***, map=469, x=452.000000, y=6454.000000, z=2536.000000, orient=3.141593

    TC_LOG_DEBUG("network","Received opcode CMSG_WORLD_TELEPORT");

    if (GetPlayer()->IsInFlight())
    {
        TC_LOG_DEBUG("network", "Player '%s' (GUID: %u) in flight, ignore worldport command.",
            GetPlayer()->GetName().c_str(), GetPlayer()->GetGUIDLow());
        return;
    }

    uint32 time;
    uint32 mapid;
    float PositionX;
    float PositionY;
    float PositionZ;
    float Orientation;

    recvData >> time;                                      // time in m.sec.
    recvData >> mapid;
    recvData >> PositionX;
    recvData >> PositionY;
    recvData >> PositionZ;
    recvData >> Orientation;                               // o (3.141593 = 180 degrees)

    /* TC_LOG_DEBUG("network", "CMSG_WORLD_TELEPORT: Player = %s, Time = %u, map = %u, x = %f, y = %f, z = %f, o = %f",
        GetPlayer()->GetName().c_str(), time, mapid, PositionX, PositionY, PositionZ, Orientation); */

    if (GetSecurity() >= SEC_GAMEMASTER3)
        GetPlayer()->TeleportTo(mapid,PositionX,PositionY,PositionZ,Orientation);
    else
        SendNotification(LANG_YOU_NOT_HAVE_PERMISSION);
}

void WorldSession::HandleWhoisOpcode(WorldPacket& recvData)
{
    // TC_LOG_DEBUG("network", "Received opcode CMSG_WHOIS");

    std::string charname;
    recvData >> charname;

    if (GetSecurity() < SEC_GAMEMASTER3)
    {
        SendNotification(LANG_YOU_NOT_HAVE_PERMISSION);
        return;
    }

    if(charname.empty() || !normalizePlayerName (charname))
    {
        SendNotification(LANG_NEED_CHARACTER_NAME);
        return;
    }

    Player *plr = sObjectAccessor->FindConnectedPlayerByName(charname.c_str());

    if(!plr)
    {
        SendNotification(LANG_PLAYER_NOT_EXIST_OR_OFFLINE, charname.c_str());
        return;
    }

    uint32 accid = plr->GetSession()->GetAccountId();

    QueryResult result = LoginDatabase.PQuery("SELECT username,email,last_ip FROM account WHERE id=%u", accid);
    if(!result)
    {
        SendNotification(LANG_ACCOUNT_FOR_PLAYER_NOT_FOUND, charname.c_str());
        return;
    }

    Field *fields = result->Fetch();
    std::string acc = fields[0].GetString();
    if(acc.empty())
        acc = "Unknown";
    std::string email = fields[1].GetString();
    if(email.empty())
        email = "Unknown";
    std::string lastip = fields[2].GetString();
    if(lastip.empty())
        lastip = "Unknown";

    std::string msg = charname + "'s " + "account is " + acc + ", e-mail: " + email + ", last ip: " + lastip;

    WorldPacket data(SMSG_WHOIS, msg.size()+1);
    data << msg;
    _player->SendDirectMessage(&data);

    TC_LOG_DEBUG("network", "Received whois command from player %s for character %s",
        GetPlayer()->GetName().c_str(), charname.c_str());
}

void WorldSession::HandleComplainOpcode( WorldPacket & recvData )
{
    

    TC_LOG_DEBUG("network", "WORLD: CMSG_COMPLAIN");

    uint8 ComplaintType;                                        // 0 - mail, 1 - chat
    uint64 spammer_guid;
    uint32 unk1 = 0, unk2 = 0, unk3 = 0, unk4 = 0;
    std::string description = "";
    recvData >> ComplaintType;                                 // unk 0x01 const, may be spam type (mail/chat)
    recvData >> spammer_guid;                              // player guid
    switch (ComplaintType)
    {
        case 0:
            
            recvData >> unk1;                              // const 0
            recvData >> unk2;                              // probably mail id
            recvData >> unk3;                              // const 0
            break;
        case 1:
            
            recvData >> unk1;                              // probably language
            recvData >> unk2;                              // message type?
            recvData >> unk3;                              // probably channel id
            recvData >> unk4;                              // unk random value
            recvData >> description;                       // spam description string (messagetype, channel name, player name, message)
            break;
    }

    // NOTE: all chat messages from this spammer automatically ignored by spam reporter until logout in case chat spam.
    // if it's mail spam - ALL mails from this spammer automatically removed by client

    // Trigger "Complaint Registred" message at client
    WorldPacket data(SMSG_COMPLAIN_RESULT, 1);
    data << uint8(0);
    SendPacket(&data);
    
    if (ComplaintType == 1) {
        if (Player* spammer = sObjectMgr->GetPlayer(spammer_guid))
            spammer->addSpamReport(_player->GetGUID(), description.c_str());
    }

    TC_LOG_DEBUG("network", "REPORT SPAM: type %u, " UI64FMTD ", unk1 %u, unk2 %u, unk3 %u, unk4 %u, message %s",
        ComplaintType, spammer_guid, unk1, unk2, unk3, unk4, description.c_str());
}

void WorldSession::HandleRealmSplitOpcode( WorldPacket & recvData )
{
    

    TC_LOG_DEBUG("network", "CMSG_REALM_SPLIT");

    uint32 Decision;
    std::string split_date = "01/01/01";
    recvData >> Decision;

    WorldPacket data(SMSG_REALM_SPLIT, 4+4+split_date.size()+1);
    data << Decision;
    data << uint32(0x00000000);                             // realm split state
    // split states:
    // 0x0 realm normal
    // 0x1 realm split
    // 0x2 realm split pending
    data << split_date;
    SendPacket(&data);
}

void WorldSession::HandleFarSightOpcode( WorldPacket & recvData )
{
    
    
    

    TC_LOG_DEBUG("network", "WORLD: CMSG_FAR_SIGHT");

    uint8 apply;
    recvData >> apply;

    _player->SetFarsightVision(apply);
    if(apply)
    {
        TC_LOG_DEBUG("network", "Added FarSight " UI64FMTD " to player %u", _player->GetGuidValue(PLAYER_FARSIGHT), _player->GetGUIDLow());

        //set the target to notify, so it updates visibility for shared visions (see PlayerRelocationNotifier and CreatureRelocationNotifier)
        WorldObject* farSightTarget = _player->GetFarsightTarget();
        if(farSightTarget && farSightTarget->isType(TYPEMASK_UNIT))
            farSightTarget->ToUnit()->SetToNotify(); 
        else
            TC_LOG_ERROR("network", "Player %s (%u) requests non-existing seer " UI64FMTD, _player->GetName().c_str(), _player->GetGUIDLow(), _player->GetGuidValue(PLAYER_FARSIGHT));
    }
    GetPlayer()->SetToNotify();
}

void WorldSession::HandleSetTitleOpcode( WorldPacket & recvData )
{
    

    TC_LOG_DEBUG("network", "CMSG_SET_TITLE");

    int32 title;
    recvData >> title;

    // -1 at none
    if(title > 0 && title < MAX_TITLE_INDEX)
    {
       if(!GetPlayer()->HasTitle(title))
            return;
    }
    else
        title = 0;

    GetPlayer()->SetUInt32Value(PLAYER_CHOSEN_TITLE, title);
}

void WorldSession::HandleAllowMoveAckOpcod( WorldPacket & recvData )
{
    
    
    

    uint32 counter, time_;
    recvData >> counter >> time_;

    // time_ seems always more than GetMSTime()
    // uint32 diff = GetMSTimeDiff(GetMSTime(),time_);
}

void WorldSession::HandleResetInstancesOpcode( WorldPacket & /*recvData*/ )
{
    Group *pGroup = _player->GetGroup();
    if(pGroup)
    {
        if(pGroup->IsLeader(_player->GetGUID()))
            pGroup->ResetInstances(INSTANCE_RESET_ALL, false, _player);
    }
    else
        _player->ResetInstances(INSTANCE_RESET_ALL, false);
}

void WorldSession::HandleSetDungeonDifficultyOpcode( WorldPacket & recvData )
{
    

    uint32 mode;
    recvData >> mode;

    if(mode == _player->GetDifficulty())
        return;

    if(mode >= MAX_DIFFICULTY)
    {
        TC_LOG_ERROR("network","WorldSession::HandleSetDungeonDifficultyOpcode: player %d sent an invalid instance mode %d!", _player->GetGUIDLow(), mode);
        return;
    }

    // cannot reset while in an instance
    Map *map = _player->GetMap();
    if(map && map->IsDungeon())
    {
        TC_LOG_ERROR("network","WorldSession::HandleSetDungeonDifficultyOpcode: player %d tried to reset the instance while inside!", _player->GetGUIDLow());
        return;
    }

    if(_player->GetLevel() < LEVELREQUIREMENT_HEROIC)
        return;

    Group *group = _player->GetGroup();
    if(group)
    {
        if(group->IsLeader(_player->GetGUID()))
        {
            for (GroupReference* itr = group->GetFirstMember(); itr != NULL; itr = itr->next())
            {
                Player* groupGuy = itr->GetSource();
                if (!groupGuy)
                    continue;

                if (!groupGuy->IsInMap(groupGuy))
                {
                    TC_LOG_DEBUG("network", "WorldSession::HandleSetDungeonDifficultyOpcode: player %d tried to reset the instance while group member (Name: %s, GUID: %u) is inside!",
                        _player->GetGUIDLow(), groupGuy->GetName().c_str(), groupGuy->GetGUIDLow());
                    return;
                }
            }

            // the difficulty is set even if the instances can't be reset
            group->ResetInstances(INSTANCE_RESET_CHANGE_DIFFICULTY, false, _player);
            group->SetDifficulty(Difficulty(mode));
        }
    }
    else
    {
        _player->ResetInstances(INSTANCE_RESET_CHANGE_DIFFICULTY, false);
        _player->SetDifficulty(Difficulty(mode), false, false); //client changes it without needing to be told
    }
}

void WorldSession::HandleCancelMountAuraOpcode( WorldPacket & /*recvData*/ )
{
    //If player is not mounted, so go out :)
    if (!_player->IsMounted())                              // not blizz like; no any messages on blizz
    {
        ChatHandler(this).SendSysMessage(LANG_CHAR_NON_MOUNTED);
        return;
    }

    if(_player->IsInFlight())                               // not blizz like; no any messages on blizz
    {
        ChatHandler(this).SendSysMessage(LANG_YOU_IN_FLIGHT);
        return;
    }

    _player->Dismount();
    _player->RemoveAurasByType(SPELL_AURA_MOUNTED);
}

void WorldSession::HandleMoveSetCanFlyAckOpcode( WorldPacket & recvData )
{
    

    uint64 guid = 0;                                            // guid - unused
    recvData >> guid;

    recvData.read_skip<uint32>();                          // unk

    uint32 flags;
    recvData >> flags;

    _player->m_mover->m_movementInfo.flags = flags;
}

void WorldSession::HandleRequestPetInfoOpcode( WorldPacket & /*recvData */)
{
    /*
        TC_LOG_DEBUG("network","WORLD: CMSG_REQUEST_PET_INFO");
        recvData.hexlike();
    */
}

void WorldSession::HandleSetTaxiBenchmarkOpcode( WorldPacket & recvData )
{
    TC_LOG_DEBUG("network", "WORLD: CMSG_SET_TAXI_BENCHMARK_MODE");

    uint8 mode;
    recvData >> mode;

    mode ? _player->SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_TAXI_BENCHMARK) : _player->RemoveFlag(PLAYER_FLAGS, PLAYER_FLAGS_TAXI_BENCHMARK);

    TC_LOG_DEBUG("network", "Client used \"/timetest %d\" command", mode);
}

