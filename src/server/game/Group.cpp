#include "Common.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Player.h"
#include "World.h"
#include "ObjectMgr.h"
#include "Group.h"
#include "Formulas.h"
#include "ObjectAccessor.h"
#include "BattleGround.h"
#include "MapManager.h"
#include "InstanceSaveMgr.h"
#include "MapInstanced.h"
#include "Util.h"
#include "Containers.h"
#include "CharacterCache.h"
#include "UpdateFieldFlags.h"

Group::Group() :
    m_masterLooterGuid(0),
    m_looterGuid(0),
    m_mainAssistant(0),
    m_leaderGuid(0),
    m_mainTank(0),
    m_bgGroup(nullptr),
    m_lootMethod((LootMethod)0),
    m_groupType((GroupType)0),
    m_lootThreshold(ITEM_QUALITY_UNCOMMON),
    m_subGroupsCounts(nullptr),
    m_leaderLogoutTime(0),
    m_raidDifficulty(RAID_DIFFICULTY_NORMAL),
    m_dungeonDifficulty(DUNGEON_DIFFICULTY_NORMAL)
{
    for(uint64 & m_targetIcon : m_targetIcons)
        m_targetIcon = 0;
}

Group::~Group()
{
    if(m_bgGroup)
    {
        if(m_bgGroup->GetBgRaid(ALLIANCE) == this) m_bgGroup->SetBgRaid(ALLIANCE, nullptr);
        else if(m_bgGroup->GetBgRaid(HORDE) == this) m_bgGroup->SetBgRaid(HORDE, nullptr);
        else TC_LOG_ERROR("FIXME","Group::~Group: battleground group is not linked to the correct battleground.");
    }
    Rolls::iterator itr;
    while(!RollId.empty())
    {
        itr = RollId.begin();
        Roll *r = *itr;
        RollId.erase(itr);
        delete(r);
    }

    // it is undefined whether objectmgr (which stores the groups) or instancesavemgr
    // will be unloaded first so we must be prepared for both cases
    // this may unload some instance saves
    for(auto & m_boundInstance : m_boundInstances)
        for(auto itr : m_boundInstance)
            itr.second.save->RemoveGroup(this);

    // Sub group counters clean up
    if (m_subGroupsCounts)
        delete[] m_subGroupsCounts;
}

bool Group::Create(const uint64 &guid, std::string const& name, SQLTransaction trans)
{
    m_leaderGuid = guid;
    m_leaderName = name;

    m_groupType  = isBGGroup() ? GROUPTYPE_RAID : GROUPTYPE_NORMAL;

    if (m_groupType == GROUPTYPE_RAID)
        _initRaidSubGroupsCounter();

    m_lootMethod = GROUP_LOOT;
    m_lootThreshold = ITEM_QUALITY_UNCOMMON;
    m_looterGuid = guid;
    m_masterLooterGuid = 0;

    m_dungeonDifficulty = REGULAR_DIFFICULTY;
    m_raidDifficulty = REGULAR_DIFFICULTY;

    if(!isBGGroup())
    {
        Player *leader = sObjectMgr->GetPlayer(guid);
        if(leader) 
            m_dungeonDifficulty = Difficulty(leader->GetDifficulty());

        Group::ConvertLeaderInstancesToGroup(leader, this, false);

        // store group in database
        trans->PAppend("DELETE FROM groups WHERE leaderGuid ='%u'", GUID_LOPART(m_leaderGuid));
        trans->PAppend("DELETE FROM group_member WHERE leaderGuid ='%u'", GUID_LOPART(m_leaderGuid));
        trans->PAppend("INSERT INTO groups(leaderGuid,mainTank,mainAssistant,lootMethod,looterGuid,lootThreshold,icon1,icon2,icon3,icon4,icon5,icon6,icon7,icon8,isRaid,difficulty) "
            "VALUES('%u','%u','%u','%u','%u','%u','" UI64FMTD "','" UI64FMTD "','" UI64FMTD "','" UI64FMTD "','" UI64FMTD "','" UI64FMTD "','" UI64FMTD "','" UI64FMTD "','%u','%u')",
            GUID_LOPART(m_leaderGuid), GUID_LOPART(m_mainTank), GUID_LOPART(m_mainAssistant), uint32(m_lootMethod),
            GUID_LOPART(m_looterGuid), uint32(m_lootThreshold), m_targetIcons[0], m_targetIcons[1], m_targetIcons[2], m_targetIcons[3], m_targetIcons[4], m_targetIcons[5], m_targetIcons[6], m_targetIcons[7], isRaidGroup(), m_dungeonDifficulty);
    }

    if(!AddMember(guid, name, trans))
        return false;

    return true;
}

bool Group::LoadGroupFromDB(const uint64 &leaderGuid, QueryResult result, bool loadMembers)
{
    if(isBGGroup())
        return false;

    if(!result)
    {
        //                                       0          1              2           3           4              5      6      7      8      9      10     11     12     13      14
        result = CharacterDatabase.PQuery("SELECT mainTank, mainAssistant, lootMethod, looterGuid, lootThreshold, icon1, icon2, icon3, icon4, icon5, icon6, icon7, icon8, isRaid, difficulty FROM groups WHERE leaderGuid ='%u'", GUID_LOPART(leaderGuid));
        if(!result)
            return false;
    }

    m_leaderGuid = leaderGuid;
    m_leaderLogoutTime = time(nullptr); // Give the leader a chance to keep his position after a server crash

    // group leader not exist
    if(!sCharacterCache->GetCharacterNameByGuid(m_leaderGuid, m_leaderName))
    {
        return false;
    }

    m_groupType  = (*result)[13].GetBool() ? GROUPTYPE_RAID : GROUPTYPE_NORMAL;

    if (m_groupType == GROUPTYPE_RAID)
        _initRaidSubGroupsCounter();

    m_dungeonDifficulty = Difficulty((*result)[14].GetUInt8());
    if (uint32(m_dungeonDifficulty) >= MAX_DIFFICULTY)
    {
        TC_LOG_ERROR("misc", "Group " UI64FMTD " has invalid dungeon difficulty %u, resetting to default value instead.", leaderGuid, m_dungeonDifficulty);
        m_dungeonDifficulty = REGULAR_DIFFICULTY;
    }

    m_mainTank = (*result)[0].GetUInt32();
    m_mainAssistant = (*result)[1].GetUInt64();
    m_lootMethod = (LootMethod)(*result)[2].GetUInt8();
    m_looterGuid = MAKE_NEW_GUID((*result)[3].GetUInt32(), 0, HighGuid::Player);
    m_lootThreshold = (ItemQualities)(*result)[4].GetUInt8();

    for(int i=0; i<TARGETICONCOUNT; i++)
        m_targetIcons[i] = (*result)[5+i].GetUInt32();

    if(loadMembers)
    {
        result = CharacterDatabase.PQuery("SELECT memberGuid, assistant, subgroup FROM group_member WHERE leaderGuid ='%u'", GUID_LOPART(leaderGuid));
        if(!result)
            return false;

        do
        {
            LoadMemberFromDB((*result)[0].GetUInt32(), (*result)[2].GetUInt16(), (*result)[1].GetBool());
        } while( result->NextRow() );

        // group too small
        if(GetMembersCount() < 2)
            return false;
    }

    return true;
}

bool Group::LoadMemberFromDB(uint32 guidLow, uint8 subgroup, bool assistant)
{
    MemberSlot member;
    member.guid      = MAKE_NEW_GUID(guidLow, 0, HighGuid::Player);

    // skip non-existed member
    if(!sCharacterCache->GetCharacterNameByGuid(member.guid, member.name))
        return false;

    member.group     = subgroup;
    member.assistant = assistant;
    m_memberSlots.push_back(member);

    SubGroupCounterIncrease(subgroup);

    return true;
}

void Group::ConvertToRaid()
{
    m_groupType = GROUPTYPE_RAID;

    _initRaidSubGroupsCounter();

    if(!isBGGroup()) CharacterDatabase.PExecute("UPDATE groups SET isRaid = 1 WHERE leaderGuid='%u'", GUID_LOPART(m_leaderGuid));
    SendUpdate();
}

bool Group::AddInvite(Player *player)
{
    if(!player || player->GetGroupInvite())
        return false;
        
    Group* group = player->GetGroup();
    if (group && group->isBGGroup())
        group = player->GetOriginalGroup();
    if (group)
        return false;

    RemoveInvite(player);

    m_invitees.insert(player);

    player->SetGroupInvite(this);

    return true;
}

bool Group::AddLeaderInvite(Player *player)
{
    if(!AddInvite(player))
        return false;

    m_leaderGuid = player->GetGUID();
    m_leaderName = player->GetName();
    return true;
}

void Group::RemoveInvite(Player *player)
{
    if (player) {
        m_invitees.erase(player);
        player->SetGroupInvite(nullptr);
    }
}

void Group::RemoveAllInvites()
{
    for(auto m_invitee : m_invitees)
        m_invitee->SetGroupInvite(nullptr);

    m_invitees.clear();
}

Player* Group::GetInvited(const uint64& guid) const
{
    for(auto m_invitee : m_invitees)
    {
        if(m_invitee->GetGUID() == guid)
            return m_invitee;
    }
    return nullptr;
}

Player* Group::GetInvited(const std::string& name) const
{
    for(auto m_invitee : m_invitees)
    {
        if(m_invitee->GetName() == name)
            return m_invitee;
    }
    return nullptr;
}

void Group::CleanInvited()
{
    for (auto itr = m_invitees.begin(); itr != m_invitees.end();)
    {
        if (!(*itr)->IsInWorld())
            m_invitees.erase(itr++);
        else
            ++itr;
    }
}

bool Group::AddMember(const uint64 &guid, std::string name, SQLTransaction trans)
{
    if(!_addMember(guid, name, trans))
        return false;
    SendUpdate();

    Player *player = sObjectMgr->GetPlayer(guid);
    if (!player)
        return false;
     
    if(!IsLeader(player->GetGUID()) && !isBGGroup())
    {
        // reset the new member's instances, unless he is currently in one of them
        // including raid/heroic instances that they are not permanently bound to!
        player->ResetInstances(INSTANCE_RESET_GROUP_JOIN);

        if(player->GetLevel() >= LEVELREQUIREMENT_HEROIC && player->GetDifficulty() != GetDifficulty() )
        {
            player->SetDifficulty(m_dungeonDifficulty, true, true);
        }
    }
    player->SetGroupUpdateFlag(GROUP_UPDATE_FULL);
    UpdatePlayerOutOfRange(player);

    // quest related GO state dependent from raid membership
    if (isRaidGroup())
        player->UpdateForQuestWorldObjects();

    {
        //sunstrider: ... Not sure this is BC, those UF_FLAG_PARTY_MEMBER are only quest fields, why are those shared to party members?
        // Broadcast new player group member fields to rest of the group
        player->SetFieldNotifyFlag(UF_FLAG_PARTY_MEMBER);

        UpdateData groupData;
        WorldPacket groupDataPacket;

        // Broadcast group members' fields to player
        for (GroupReference* itr = GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            if (itr->GetSource() == player)
                continue;

            if (Player* existingMember = itr->GetSource())
            {
                //send group members fields to new player
                if (player->HaveAtClient(existingMember))
                {
                    existingMember->SetFieldNotifyFlag(UF_FLAG_PARTY_MEMBER);
                    existingMember->BuildValuesUpdateBlockForPlayer(&groupData, player);
                    existingMember->RemoveFieldNotifyFlag(UF_FLAG_PARTY_MEMBER);
                }

                //send new player fields to existing member
                if (existingMember->HaveAtClient(player))
                {
                    UpdateData newData;
                    WorldPacket newDataPacket;
                    player->BuildValuesUpdateBlockForPlayer(&newData, existingMember); //will build UF_FLAG_PARTY_MEMBER, see notify before loop
                    if (newData.HasData())
                    {
                        newData.BuildPacket(&newDataPacket, false);
                        existingMember->SendDirectMessage(&newDataPacket);
                    }
                }
            }
        }

        if (groupData.HasData())
        {
            groupData.BuildPacket(&groupDataPacket, false);
            player->SendDirectMessage(&groupDataPacket);
        }

        player->RemoveFieldNotifyFlag(UF_FLAG_PARTY_MEMBER);
    }

    return true;
}

uint32 Group::RemoveMember(const uint64 &guid, const uint8 &method)
{
    BroadcastGroupUpdate();

    // remove member and change leader (if need) only if strong more 2 members _before_ member remove
    if(GetMembersCount() > (isBGGroup() ? 1 : 2))           // in BG group case allow 1 members group
    {
        bool leaderChanged = _removeMember(guid);

        if(Player *player = sObjectMgr->GetPlayer( guid ))
        {
            WorldPacket data;

            if(method == 1)
            {
                data.Initialize( SMSG_GROUP_UNINVITE, 0 );
                player->SendDirectMessage( &data );
            }

            // we already removed player from group and in player->GetGroup() is his original group!
            if (Group* group = player->GetGroup())
                group->SendUpdate();
            else {
                data.Initialize(SMSG_GROUP_LIST, 24);
                data << uint64(0) << uint64(0) << uint64(0);
                player->SendDirectMessage(&data);
            }

            _homebindIfInstance(player);
        }

        if(leaderChanged)
        {
            WorldPacket data(SMSG_GROUP_SET_LEADER, (m_memberSlots.front().name.size()+1));
            data << m_memberSlots.front().name;
            BroadcastPacket(&data, true);
        }

        SendUpdate();
    }
    // if group before remove <= 2 disband it
    else
        Disband(true);

    return m_memberSlots.size();
}

void Group::ChangeLeader(const uint64 &guid)
{
    auto slot = _getMemberCSlot(guid);

    if(slot==m_memberSlots.end())
        return;

    _setLeader(guid);

    WorldPacket data(SMSG_GROUP_SET_LEADER, slot->name.size()+1);
    data << slot->name;
    BroadcastPacket(&data, true);
    SendUpdate();
}

void Group::CheckLeader(const uint64 &guid, bool isLogout)
{

    if(IsLeader(guid))
    {
        if(isLogout)
        {
            m_leaderLogoutTime = time(nullptr);
        }
        else
        {
            m_leaderLogoutTime = 0;
        }
    }
    else
    {
        if(!isLogout && !m_leaderLogoutTime) //normal member logins
        {
            Player *leader = nullptr;
            
            //find the leader from group members
            for(GroupReference *itr = GetFirstMember(); itr != nullptr; itr = itr->next())
            {
                if(itr->GetSource()->GetGUID() == m_leaderGuid)
                {
                    leader = itr->GetSource();
                    break;
                }
            }

            if(!leader || !leader->IsInWorld())
            {
                m_leaderLogoutTime = time(nullptr);
            }
        }
    }
}

bool Group::ChangeLeaderToFirstOnlineMember()
{
    
    
    for (GroupReference *itr = GetFirstMember(); itr != nullptr; itr = itr->next()) {
        Player* player = itr->GetSource();

        if (player && player->IsInWorld() && player->GetGUID() != m_leaderGuid) {
            ChangeLeader(player->GetGUID());
            return true;
        }
    }
    
    return false;
}

void Group::Disband(bool hideDestroy)
{
    //TC sScriptMgr->OnGroupDisband(this);
    Player *player;

    for(member_citerator citr = m_memberSlots.begin(); citr != m_memberSlots.end(); ++citr)
    {
        player = sObjectMgr->GetPlayer(citr->guid);
        if (!player)
            continue;

        // we cannot call _removeMember because it would invalidate member iterator
        // if we are removing player from battleground raid
        if (isBGGroup())
            player->RemoveFromBattlegroundRaid();
        else
        {
            // we can remove player who is in battleground from his original group
            if (player->GetOriginalGroup() == this)
                player->SetOriginalGroup(nullptr);
            else
                player->SetGroup(nullptr);
        }


        // quest related GO state dependent from raid membership
      /* LK  if (isRaidGroup())
            player->UpdateForQuestWorldObjects();*/

        if(!player->GetSession())
            continue;

        WorldPacket data;
        if(!hideDestroy)
        {
            data.Initialize(SMSG_GROUP_DESTROYED, 0);
            player->SendDirectMessage(&data);
        }

        // we already removed player from group and in player->GetGroup() is his original group, send update
        if (Group* group = player->GetGroup())
            group->SendUpdate();
        else
        {
#ifndef LICH_KING
            data.Initialize(SMSG_GROUP_LIST, 24);
            data << uint64(0) << uint64(0) << uint64(0);
            player->SendDirectMessage(&data);
#else
            data.Initialize(SMSG_GROUP_LIST, 1 + 1 + 1 + 1 + 8 + 4 + 4 + 8);
            data << uint8(0x10) << uint8(0) << uint8(0) << uint8(0);
            data << uint64(m_guid) << uint32(m_counter) << uint32(0) << uint64(0);
            player->SendDirectMessage(&data);
#endif
        }

        _homebindIfInstance(player);
    }
    RollId.clear();
    m_memberSlots.clear();

    RemoveAllInvites();

    if (!isBGGroup() && !isBFGroup())
    {
        SQLTransaction trans = CharacterDatabase.BeginTransaction();
        trans->PAppend("DELETE FROM groups WHERE leaderGuid='%u'", GUID_LOPART(m_leaderGuid));
        trans->PAppend("DELETE FROM group_member WHERE leaderGuid='%u'", GUID_LOPART(m_leaderGuid));
        CharacterDatabase.CommitTransaction(trans);

        ResetInstances(INSTANCE_RESET_GROUP_DISBAND, false, nullptr);
        ResetInstances(INSTANCE_RESET_GROUP_DISBAND, true, nullptr);

        //TC sGroupMgr->FreeGroupDbStoreId(this);
    }

    m_leaderGuid = 0;
    m_leaderName = "";
}

/*********************************************************/
/***                   LOOT SYSTEM                     ***/
/*********************************************************/

void Group::SendLootStartRoll(uint32 CountDown, const Roll &r)
{
    WorldPacket data(SMSG_LOOT_START_ROLL, (8+4+4+4+4+4));
    data << uint64(r.itemGUID);                             // guid of rolled item
    data << uint32(r.totalPlayersRolling);                  // maybe the number of players rolling for it???
    data << uint32(r.itemid);                               // the itemEntryId for the item that shall be rolled for
    data << uint32(r.itemRandomSuffix);                     // randomSuffix
    data << uint32(r.itemRandomPropId);                     // item random property ID
    data << uint32(CountDown);                              // the countdown time to choose "need" or "greed"

    for (const auto & itr : r.playerVote)
    {
        Player *p = sObjectMgr->GetPlayer(itr.first);
        if(!p || !p->GetSession())
            continue;

        if(itr.second == NOT_EMITED_YET)
            p->SendDirectMessage( &data );
    }
}

void Group::SendLootRoll(const uint64& SourceGuid, const uint64& TargetGuid, uint8 RollNumber, uint8 RollType, const Roll &r)
{
    WorldPacket data(SMSG_LOOT_ROLL, (8+4+8+4+4+4+1+1));
    data << uint64(SourceGuid);                             // guid of the item rolled
    data << uint32(0);                                      // unknown, maybe amount of players
    data << uint64(TargetGuid);
    data << uint32(r.itemid);                               // the itemEntryId for the item that shall be rolled for
    data << uint32(r.itemRandomSuffix);                     // randomSuffix
    data << uint32(r.itemRandomPropId);                     // Item random property ID
    data << uint8(RollNumber);                              // 0: "Need for: [item name]" > 127: "you passed on: [item name]"      Roll number
    data << uint8(RollType);                                // 0: "Need for: [item name]" 0: "You have selected need for [item name] 1: need roll 2: greed roll
    data << uint8(0);                                       // 2.4.0

    for(const auto & itr : r.playerVote)
    {
        Player *p = sObjectMgr->GetPlayer(itr.first);
        if(!p || !p->GetSession())
            continue;

        if(itr.second != NOT_VALID)
            p->SendDirectMessage( &data );
    }
}

void Group::SendLootRollWon(const uint64& SourceGuid, const uint64& TargetGuid, uint8 RollNumber, uint8 RollType, const Roll &r)
{
    WorldPacket data(SMSG_LOOT_ROLL_WON, (8+4+4+4+4+8+1+1));
    data << uint64(SourceGuid);                             // guid of the item rolled
    data << uint32(0);                                      // unknown, maybe amount of players
    data << uint32(r.itemid);                               // the itemEntryId for the item that shall be rolled for
    data << uint32(r.itemRandomSuffix);                     // randomSuffix
    data << uint32(r.itemRandomPropId);                     // Item random property
    data << uint64(TargetGuid);                             // guid of the player who won.
    data << uint8(RollNumber);                              // rollnumber realted to SMSG_LOOT_ROLL
    data << uint8(RollType);                                // Rolltype related to SMSG_LOOT_ROLL

    for(const auto & itr : r.playerVote)
    {
        Player *p = sObjectMgr->GetPlayer(itr.first);
        if(!p || !p->GetSession())
            continue;

        if(itr.second != NOT_VALID)
            p->SendDirectMessage( &data );
    }
}

// notify group members which player is the allowed looter for the given creature
//NOT BC TESTED
void Group::SendLooter(Creature* creature, Player* groupLooter)
{
    ASSERT(creature);

    WorldPacket data(SMSG_LOOT_LIST, (8+8));
    data << uint64(creature->GetGUID());

    if (GetLootMethod() == MASTER_LOOT && creature->loot.hasOverThresholdItem())
        data << PackedGuid(GetMasterLooterGuid());
    else
        data << uint8(0);

    if (groupLooter)
        data << groupLooter->GetPackGUID();
    else
        data << uint8(0);

    BroadcastPacket(&data, false);
}

void Group::SendLootAllPassed(uint32 NumberOfPlayers, const Roll &r)
{
    WorldPacket data(SMSG_LOOT_ALL_PASSED, (8+4+4+4+4));
    data << uint64(r.itemGUID);                             // Guid of the item rolled
    data << uint32(NumberOfPlayers);                        // The number of players rolling for it???
    data << uint32(r.itemid);                               // The itemEntryId for the item that shall be rolled for
    data << uint32(r.itemRandomPropId);                     // Item random property ID
    data << uint32(r.itemRandomSuffix);                     // Item random suffix ID

    for(const auto & itr : r.playerVote)
    {
        Player *p = sObjectMgr->GetPlayer(itr.first);
        if(!p || !p->GetSession())
            continue;

        if(itr.second != NOT_VALID)
            p->SendDirectMessage( &data );
    }
}

void Group::GroupLoot(const uint64& playerGUID, Loot *loot, WorldObject* object)
{
    std::vector<LootItem>::iterator i;
    ItemTemplate const *item;
    uint8 itemSlot = 0;
    Player *player = sObjectMgr->GetPlayer(playerGUID);
    Group *group = player->GetGroup();

    for (i=loot->items.begin(); i != loot->items.end(); ++i, ++itemSlot)
    {
        item = sObjectMgr->GetItemTemplate(i->itemid);
        if (!item)
        {
            //TC_LOG_DEBUG("FIXME","Group::GroupLoot: missing item prototype for item with id: %d", i->itemid);
            continue;
        }

        //roll for over-threshold item if it's one-player loot
        if (item->Quality >= uint32(m_lootThreshold) && !i->freeforall)
        {
            uint64 newitemGUID = ObjectGuid::Create<HighGuid::Item>(sObjectMgr->GetGenerator<HighGuid::Item>().Generate()).GetRawValue();

            auto  r=new Roll(newitemGUID,*i);

            //a vector is filled with only near party members
            for(GroupReference *itr = GetFirstMember(); itr != nullptr; itr = itr->next())
            {
                Player *member = itr->GetSource();
                if(!member || !member->GetSession())
                    continue;
                if (member->IsAtGroupRewardDistance(object) && i->AllowedForPlayer(member) )
                {
                    ++r->totalPlayersRolling;
                        
                    if (member->GetPassOnGroupLoot()) {
                        r->playerVote[member->GetGUID()] = PASS;
                        r->totalPass++;
                        // can't broadcast the pass now. need to wait until all rolling players are known.
                    }
                    else
                        r->playerVote[member->GetGUID()] = NOT_EMITED_YET;
                }
            }

            r->setLoot(loot);
            r->itemSlot = itemSlot;

            // If there is any "auto pass", broadcast the pass now.
            if (r->totalPass) {
                for (Roll::PlayerVote::const_iterator itr=r->playerVote.begin(); itr != r->playerVote.end(); ++itr) {
                    Player *p = sObjectMgr->GetPlayer(itr->first);
                    if (!p || !p->GetSession())
                        continue;
                    
                    if (itr->second == PASS)
                        SendLootRoll(newitemGUID, p->GetGUID(), 128, ROLL_PASS, *r);
                }
            }
            
            group->SendLootStartRoll(60000, *r);

            loot->items[itemSlot].is_blocked = true;
            object->m_groupLootTimer = 60000;
            object->lootingGroupLeaderGUID = GetLeaderGUID();

            RollId.push_back(r);
        }
        else
            i->is_underthreshold=1;

    }
}

void Group::NeedBeforeGreed(const uint64& playerGUID, Loot *loot, WorldObject* object)
{
    ItemTemplate const *item;
    Player *player = sObjectMgr->GetPlayer(playerGUID);
    Group *group = player->GetGroup();

    uint8 itemSlot = 0;
    for(auto i=loot->items.begin(); i != loot->items.end(); ++i, ++itemSlot)
    {
        item = sObjectMgr->GetItemTemplate(i->itemid);

        //only roll for one-player items, not for ones everyone can get
        if (item->Quality >= uint32(m_lootThreshold) && !i->freeforall)
        {
            uint64 newitemGUID = ObjectGuid::Create<HighGuid::Item>(sObjectMgr->GetGenerator<HighGuid::Item>().Generate()).GetRawValue();
            auto  r=new Roll(newitemGUID,*i);

            for(GroupReference *itr = GetFirstMember(); itr != nullptr; itr = itr->next())
            {
                Player *playerToRoll = itr->GetSource();
                if(!playerToRoll || !playerToRoll->GetSession())
                    continue;

                if (playerToRoll->CanUseItem(item) && i->AllowedForPlayer(playerToRoll) && playerToRoll->IsAtGroupRewardDistance(object))
                {
                    ++r->totalPlayersRolling;
                        
                    if (playerToRoll->GetPassOnGroupLoot()) {
                        r->playerVote[playerToRoll->GetGUID()] = PASS;
                        r->totalPass++;
                        // can't broadcast the pass now. need to wait until all rolling players are known.
                    }
                    else
                        r->playerVote[playerToRoll->GetGUID()] = NOT_EMITED_YET;
                }
            }

            if (r->totalPlayersRolling > 0)
            {
                r->setLoot(loot);
                r->itemSlot = itemSlot;
                
                // If there is any "auto pass", broadcast the pass now.
                if (r->totalPass) {
                    for (Roll::PlayerVote::const_iterator itr=r->playerVote.begin(); itr != r->playerVote.end(); ++itr) {
                        Player *p = sObjectMgr->GetPlayer(itr->first);
                        if (!p || !p->GetSession())
                            continue;

                        if (itr->second == PASS)
                            SendLootRoll(newitemGUID, p->GetGUID(), 128, ROLL_PASS, *r);
                    }
                }

                group->SendLootStartRoll(60000, *r);

                loot->items[itemSlot].is_blocked = true;

                RollId.push_back(r);
            }
            else
            {
                delete r;
            }
        }
        else
            i->is_underthreshold=1;
    }
}

void Group::MasterLoot(const uint64& playerGUID, Loot* /*loot*/, WorldObject* object)
{
    Player *player = sObjectMgr->GetPlayer(playerGUID);
    if(!player)
        return;

    uint32 real_count = 0;

    WorldPacket data(SMSG_LOOT_MASTER_LIST, 330);
    data << (uint8)real_count; //place holder, will be overwritten later

    for(GroupReference *itr = GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        /* Do NOT exclude players that could become eligible later, client asks for this list only once and will never refresh it, if for example one player 
        was disconnected when this was sent, the master looter will never be able to assign him any loot */
        Player* member = itr->GetSource();
        //todo: exclude here members that will never be able to loot
        data << member->GetGUID();
        ++real_count;
    }

    data.put<uint8>(0,real_count);

    player->SendDirectMessage(&data);
}

void Group::CountRollVote(const uint64& playerGUID, const uint64& Guid, uint32 NumberOfPlayers, uint8 Choise)
{
    auto rollI = GetRoll(Guid);
    if (rollI == RollId.end())
        return;
    Roll* roll = *rollI;

    auto itr = roll->playerVote.find(playerGUID);
    // this condition means that player joins to the party after roll begins
    if (itr == roll->playerVote.end())
        return;

    if (roll->getLoot())
        if (roll->getLoot()->items.empty())
            return;

    switch (Choise)
    {
        case 0:                                             //Player choose pass
        {
            SendLootRoll(0, playerGUID, 128, 128, *roll);
            ++roll->totalPass;
            itr->second = PASS;
        }
        break;
        case 1:                                             //player choose Need
        {
            SendLootRoll(0, playerGUID, 0, 0, *roll);
            ++roll->totalNeed;
            itr->second = NEED;
        }
        break;
        case 2:                                             //player choose Greed
        {
            SendLootRoll(0, playerGUID, 128, 2, *roll);
            ++roll->totalGreed;
            itr->second = GREED;
        }
        break;
    }
    if (roll->totalPass + roll->totalGreed + roll->totalNeed >= roll->totalPlayersRolling)
    {
        CountTheRoll(rollI, NumberOfPlayers);
    }
}

//called when roll timer expires
void Group::EndRoll()
{
    Rolls::iterator itr;
    while(!RollId.empty())
    {
        //need more testing here, if rolls disappear
        itr = RollId.begin();
        CountTheRoll(itr, GetMembersCount());               //i don't have to edit player votes, who didn't vote ... he will pass
    }
}

void Group::CountTheRoll(Rolls::iterator rollI, uint32 NumberOfPlayers)
{
    Roll* roll = *rollI;
    if(!roll->isValid())                                    // is loot already deleted ?
    {
        RollId.erase(rollI);
        delete roll;
        return;
    }
    //end of the roll
    if (roll->totalNeed > 0)
    {
        if(!roll->playerVote.empty())
        {
            uint8 maxresul = 0;
            uint64 maxguid  = (*roll->playerVote.begin()).first;
            Player *player;

            for( Roll::PlayerVote::const_iterator itr=roll->playerVote.begin(); itr!=roll->playerVote.end(); ++itr)
            {
                if (itr->second != NEED)
                    continue;

                uint8 randomN = urand(1, 99);
                SendLootRoll(0, itr->first, randomN, 1, *roll);
                if (maxresul < randomN)
                {
                    maxguid  = itr->first;
                    maxresul = randomN;
                }
            }
            SendLootRollWon(0, maxguid, maxresul, 1, *roll);
            player = sObjectMgr->GetPlayer(maxguid);

            if(player && player->GetSession())
            {
                ItemPosCountVec dest;
                LootItem *item = &(roll->getLoot()->items[roll->itemSlot]);
                uint8 msg = player->CanStoreNewItem( NULL_BAG, NULL_SLOT, dest, roll->itemid, item->count );
                if ( msg == EQUIP_ERR_OK )
                {
                    item->is_looted = true;
                    roll->getLoot()->NotifyItemRemoved(roll->itemSlot);
                    --roll->getLoot()->unlootedCount;
                    player->StoreNewItem( dest, roll->itemid, true, item->randomPropertyId);
                }
                else
                {
                    item->is_blocked = false;
                    player->SendEquipError( msg, nullptr, nullptr );
                }
            }
        }
    }
    else if (roll->totalGreed > 0)
    {
        if(!roll->playerVote.empty())
        {
            uint8 maxresul = 0;
            uint64 maxguid = (*roll->playerVote.begin()).first;
            Player *player;

            Roll::PlayerVote::iterator itr;
            for (itr=roll->playerVote.begin(); itr!=roll->playerVote.end(); ++itr)
            {
                if (itr->second != GREED)
                    continue;

                uint8 randomN = urand(1, 99);
                SendLootRoll(0, itr->first, randomN, 2, *roll);
                if (maxresul < randomN)
                {
                    maxguid  = itr->first;
                    maxresul = randomN;
                }
            }
            SendLootRollWon(0, maxguid, maxresul, 2, *roll);
            player = sObjectMgr->GetPlayer(maxguid);

            if(player && player->GetSession())
            {
                ItemPosCountVec dest;
                LootItem *item = &(roll->getLoot()->items[roll->itemSlot]);
                uint8 msg = player->CanStoreNewItem( NULL_BAG, NULL_SLOT, dest, roll->itemid, item->count );
                if ( msg == EQUIP_ERR_OK )
                {
                    item->is_looted = true;
                    roll->getLoot()->NotifyItemRemoved(roll->itemSlot);
                    --roll->getLoot()->unlootedCount;
                    player->StoreNewItem( dest, roll->itemid, true, item->randomPropertyId);
                }
                else
                {
                    item->is_blocked = false;
                    player->SendEquipError( msg, nullptr, nullptr );
                }
            }
        }
    }
    else
    {
        SendLootAllPassed(NumberOfPlayers, *roll);
        LootItem *item = &(roll->getLoot()->items[roll->itemSlot]);
        if(item) item->is_blocked = false;
    }
    RollId.erase(rollI);
    delete roll;
}

void Group::SetTargetIcon(uint8 id, uint64 guid)
{
    if(id >= TARGETICONCOUNT)
        return;

    // clean other icons
    if( guid != 0 )
        for(int i=0; i<TARGETICONCOUNT; i++)
            if( m_targetIcons[i] == guid )
                SetTargetIcon(i, 0);

    m_targetIcons[id] = guid;

    WorldPacket data(MSG_RAID_TARGET_UPDATE, (2+8));
    data << (uint8)0;
    data << id;
    data << guid;
    BroadcastPacket(&data, true);
}

uint64 Group::GetTargetIcon(uint8 id) const
{
    if(id >= TARGETICONCOUNT)
    {
        TC_LOG_ERROR("misc", "Group::GetTargetIcon called with wrong argument %u", id);
        return 0;
    }

    return m_targetIcons[id];
}

void Group::GetDataForXPAtKill(Unit const* victim, uint32& count,uint32& sum_level, Player* & member_with_max_level, Player* & not_gray_member_with_max_level)
{
    for(GroupReference *itr = GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player* member = itr->GetSource();
        if(!member || !member->IsAlive())                   // only for alive
            continue;

        if(!member->IsAtGroupRewardDistance(victim))        // at req. distance
            continue;

        ++count;
        sum_level += member->GetLevel();
        // store maximum member level
        if(!member_with_max_level || member_with_max_level->GetLevel() < member->GetLevel())
            member_with_max_level = member;

        uint32 gray_level = Trinity::XP::GetGrayLevel(member->GetLevel());
        // if the victim is higher level than the gray level of the currently examined group member,
        // then set not_gray_member_with_max_level if needed.
        if( victim->GetLevel() > gray_level && (!not_gray_member_with_max_level
           || not_gray_member_with_max_level->GetLevel() < member->GetLevel()))
            not_gray_member_with_max_level = member;
    }
}

void Group::SendTargetIconList(WorldSession *session)
{
    if(!session)
        return;

    WorldPacket data(MSG_RAID_TARGET_UPDATE, (1+TARGETICONCOUNT*9));
    data << (uint8)1;

    for(int i=0; i<TARGETICONCOUNT; i++)
    {
        if(m_targetIcons[i] == 0)
            continue;

        data << (uint8)i;
        data << m_targetIcons[i];
    }

    session->SendPacket(&data);
}

void Group::SendUpdateToPlayer(uint64 playerGUID, MemberSlot* slot)
{
    Player* player = sObjectMgr->GetPlayer(playerGUID);
    if (!player || !player->GetSession() || player->GetGroup() != this)
        return;

    // if MemberSlot wasn't provided
    if (!slot)
    {
        member_witerator witr = _getMemberWSlot(playerGUID);

        if (witr == m_memberSlots.end()) // if there is no MemberSlot for such a player
            return;

        slot = &(*witr);
    }

    //LK OK                                             // guess size
    WorldPacket data(SMSG_GROUP_LIST, (1 + 1 + 1 + 1 + 8 + 4 + GetMembersCount() * 20));
    data << (uint8)m_groupType;                         // group type
#ifndef LICH_KING
    data << (uint8)(isBGGroup() ? 1 : 0);               // 2.0.x, isBattlegroundGroup?
#endif
    data << (uint8)(slot->group);                       // groupid
#ifdef LICH_KING
    data << uint8(slot->flags);
#endif
    data << (uint8)(slot->assistant ? 0x01 : 0);            // 0x2 main assist, 0x4 main tank
#ifdef LICH_KING
    if (isLFGGroup())
    {
        data << uint8(sLFGMgr->GetState(m_guid) == lfg::LFG_STATE_FINISHED_DUNGEON ? 2 : 0); // FIXME - Dungeon save status? 2 = done
        data << uint32(sLFGMgr->GetDungeon(m_guid));
    }
    data << uint64(m_guid);
    data << uint32(m_counter++);                        // 3.3, value increases every time this packet gets sent
#else
    data << uint64(0x50000000FFFFFFFELL);               // related to voice chat?
#endif
    data << uint32(GetMembersCount() - 1);
    for (member_citerator citr2 = m_memberSlots.begin(); citr2 != m_memberSlots.end(); ++citr2)
    {
        if (slot->guid == citr2->guid)
            continue;

        Player* member = sObjectMgr->GetPlayer(citr2->guid);
        uint8 onlineState = (member) ? MEMBER_STATUS_ONLINE : MEMBER_STATUS_OFFLINE;
        onlineState = onlineState | ((isBGGroup()) ? MEMBER_STATUS_PVP : 0);

        data << citr2->name;
        data << (uint64)citr2->guid;
        // online-state
        data << (uint8)(onlineState);
        data << (uint8)(citr2->group);                  // groupid
#ifdef LICH_KING
        data << uint8(citr->flags);                     // See enum GroupMemberFlags
#endif
        data << (uint8)(citr2->assistant ? 0x01 : 0);       // 0x2 main assist, 0x4 main tank
    }

    data << uint64(m_leaderGuid);                       // leader guid
    if (GetMembersCount() - 1)
    {
        data << (uint8)m_lootMethod;                    // loot method
        if (m_lootMethod == MASTER_LOOT)
            data << uint64(m_masterLooterGuid);         // master looter guid
        else
            data << uint64(0);
        data << (uint8)m_lootThreshold;                 // loot threshold
        data << (uint8)m_dungeonDifficulty;             // Heroic Mod Group
#ifdef LICH_KING
        data << uint8(m_raidDifficulty);                // Raid Difficulty
        data << uint8(m_raidDifficulty >= RAID_DIFFICULTY_10MAN_HEROIC);    // 3.3 Dynamic Raid Difficulty - 0 normal/1 heroic
#endif
    }
    player->SendDirectMessage(&data);
}

void Group::SendUpdate()
{
    for(member_citerator citr = m_memberSlots.begin(); citr != m_memberSlots.end(); ++citr)
        SendUpdateToPlayer(citr->guid);
}

// Automatic Update by World thread
void Group::Update(time_t diff)
{
    //change the leader if it has disconnect for a long time
    if (m_leaderLogoutTime) {
        time_t thisTime = time(nullptr);

        if (thisTime > m_leaderLogoutTime + sWorld->getConfig(CONFIG_GROUPLEADER_RECONNECT_PERIOD)) {
            ChangeLeaderToFirstOnlineMember();
            m_leaderLogoutTime = 0;
        }
    }
}

void Group::UpdatePlayerOutOfRange(Player* pPlayer)
{
    if(!pPlayer)
        return;

    Player *player;
    WorldPacket data;
    pPlayer->GetSession()->BuildPartyMemberStatsChangedPacket(pPlayer, &data);

    for(GroupReference *itr = GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        player = itr->GetSource();
        if (player && player != pPlayer && !pPlayer->HaveAtClient(player))
            player->SendDirectMessage(&data);
    }
}

void Group::BroadcastPacket(WorldPacket *packet, bool ignorePlayersInBGRaid, int group, uint64 _ignore)
{
    for(GroupReference *itr = GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player *pl = itr->GetSource();
        if(!pl || (_ignore != 0 && pl->GetGUID() == _ignore) || (ignorePlayersInBGRaid && pl->GetGroup() != this))
            continue;

        if (pl->GetSession() && (group==-1 || itr->getSubGroup()==group))
            pl->SendDirectMessage(packet);
    }
}

void Group::BroadcastReadyCheck(WorldPacket *packet)
{
    for(GroupReference *itr = GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player *pl = itr->GetSource();
        if(pl && pl->GetSession())
            if(IsLeader(pl->GetGUID()) || IsAssistant(pl->GetGUID()))
                pl->SendDirectMessage(packet);
    }
}

void Group::OfflineReadyCheck()
{
    for(member_citerator citr = m_memberSlots.begin(); citr != m_memberSlots.end(); ++citr)
    {
        Player *pl = sObjectMgr->GetPlayer(citr->guid);
        if (!pl || !pl->GetSession())
        {
            WorldPacket data(MSG_RAID_READY_CHECK_CONFIRM, 9);
            data << citr->guid;
            data << (uint8)0;
            BroadcastReadyCheck(&data);
        }
    }
}

bool Group::_addMember(const uint64 &guid, std::string name, SQLTransaction trans, bool isAssistant)
{
    // get first not-full group
    uint8 groupid = 0;
    if (m_subGroupsCounts)
    {
        bool groupFound = false;
        for (; groupid < MAXRAIDSIZE/MAXGROUPSIZE; ++groupid)
        {
            if (m_subGroupsCounts[groupid] < MAXGROUPSIZE)
            {
                groupFound = true;
                break;
            }
        }
        // We are raid group and no one slot is free
        if (!groupFound)
                return false;
    }

    return _addMember(guid, name, isAssistant, groupid, trans);
}

bool Group::_addMember(const uint64 &guid, std::string name, bool isAssistant, uint8 group, SQLTransaction trans)
{
    if(IsFull())
        return false;

    if(!guid)
        return false;

    Player *player = sObjectMgr->GetPlayer(guid);

    MemberSlot member;
    member.guid      = guid;
    member.name      = name;
    member.group     = group;
    member.assistant = isAssistant;
    m_memberSlots.push_back(member);

    SubGroupCounterIncrease(group);

    if(player)
    {
        player->SetGroupInvite(nullptr);
        // if player is in group and he is being added to BG raid group, then call SetBattlegroundRaid()
        if (player->GetGroup() && isBGGroup())
            player->SetBattlegroundRaid(this, group);
        // if player is in bg raid and we are adding him to normal group, then call SetOriginalGroup()
        else if (player->GetGroup())
            player->SetOriginalGroup(this, group);
        // if player is not in group, then call set group
        else
            player->SetGroup(this, group);

        // if the same group invites the player back, cancel the homebind timer
        InstanceGroupBind *bind = GetBoundInstance(Difficulty(player->GetDifficulty()), player->GetMapId());
        if(bind && bind->save->GetInstanceId() == player->GetInstanceId())
            player->m_InstanceValid = true;
    }

    if(!isRaidGroup())                                      // reset targetIcons for non-raid-groups
    {
        for(uint64 & m_targetIcon : m_targetIcons)
            m_targetIcon = 0;
    }

    if(!isBGGroup())
    {
        // insert into group table
        trans->PAppend("INSERT INTO group_member(leaderGuid,memberGuid,assistant,subgroup) VALUES('%u','%u','%u','%u')", GUID_LOPART(m_leaderGuid), GUID_LOPART(member.guid), ((member.assistant==1)?1:0), member.group);
    }

    return true;
}

bool Group::_removeMember(const uint64 &guid)
{
    Player *player = sObjectMgr->GetPlayer(guid);
    if (player)
    {
        // if we are removing player from battleground raid
        if (isBGGroup())
            player->RemoveFromBattlegroundRaid();
        else
        {
            // we can remove player who is in battleground from his original group
            if (player->GetOriginalGroup() == this)
                player->SetOriginalGroup(nullptr);
            else
                player->SetGroup(nullptr);
        }
    }

    _removeRolls(guid);

    auto slot = _getMemberWSlot(guid);
    if (slot != m_memberSlots.end())
    {
        SubGroupCounterDecrease(slot->group);

        m_memberSlots.erase(slot);
    }

    if(!isBGGroup())
        CharacterDatabase.PExecute("DELETE FROM group_member WHERE memberGuid='%u'", GUID_LOPART(guid));

    if(m_leaderGuid == guid)                                // leader was removed
    {
        if(GetMembersCount() > 0)
            _setLeader(m_memberSlots.front().guid);
        return true;
    }

    return false;
}

void Group::_setLeader(const uint64 &guid)
{
    
    
    auto slot = _getMemberCSlot(guid);
    if(slot==m_memberSlots.end())
        return;

    if(!isBGGroup())
    {
        // TODO: set a time limit to have this function run rarely cause it can be slow
        SQLTransaction trans = CharacterDatabase.BeginTransaction();

        // update the group's bound instances when changing leaders

        // remove all permanent binds from the group
        // in the DB also remove solo binds that will be replaced with permbinds
        // from the new leader
        trans->PAppend(
            "DELETE FROM group_instance WHERE leaderguid='%u' AND (permanent = 1 OR "
            "instance IN (SELECT instance FROM character_instance WHERE guid = '%u')"
            ")", GUID_LOPART(m_leaderGuid), GUID_LOPART(slot->guid)
        );

        Player *player = sObjectMgr->GetPlayer(slot->guid);
        if (!player)
            return; //do not allow switching leader to offline players

        for(auto & m_boundInstance : m_boundInstances)
        {
            for(auto itr = m_boundInstance.begin(); itr != m_boundInstance.end();)
            {
                if(itr->second.perm)
                {
                    itr->second.save->RemoveGroup(this);
                    itr = m_boundInstance.erase(itr);
                }
                else
                    ++itr;
            }
        }

        // update the group's solo binds to the new leader
        trans->PAppend("UPDATE group_instance SET leaderGuid='%u' WHERE leaderGuid = '%u'", GUID_LOPART(slot->guid), GUID_LOPART(m_leaderGuid));

        // copy the permanent binds from the new leader to the group
        // overwriting the solo binds with permanent ones if necessary
        // in the DB those have been deleted already
        Group::ConvertLeaderInstancesToGroup(player, this, true);

        // update the group leader
        trans->PAppend("UPDATE groups SET leaderGuid='%u' WHERE leaderGuid='%u'", GUID_LOPART(slot->guid), GUID_LOPART(m_leaderGuid));
        trans->PAppend("UPDATE group_member SET leaderGuid='%u' WHERE leaderGuid='%u'", GUID_LOPART(slot->guid), GUID_LOPART(m_leaderGuid));
        CharacterDatabase.CommitTransaction(trans);
    }

    m_leaderGuid = slot->guid;
    m_leaderName = slot->name;
}

/// convert the player's binds to the group
void Group::ConvertLeaderInstancesToGroup(Player *player, Group *group, bool switchLeader)
{
    

    // copy all binds to the group, when changing leader it's assumed the character
    // will not have any solo binds

    if (player)
    {
        for (uint8 i = 0; i < MAX_DIFFICULTY; i++)
        {
            for (auto itr = player->m_boundInstances[i].begin(); itr != player->m_boundInstances[i].end();)
            {
                if (!switchLeader || !group->GetBoundInstance(itr->second.save->GetDifficulty(), itr->first))
                    group->BindToInstance(itr->second.save, itr->second.perm, true);

                // permanent binds are not removed
                if (switchLeader && !itr->second.perm)
                {
                    player->UnbindInstance(itr, Difficulty(i), true);   // increments itr
                }
                else
                    ++itr;
            }
        }
    }

    /* if group leader is in a non-raid dungeon map and nobody is actually bound to this map then the group can "take over" the instance *
    * (example: two-player group disbanded by disconnect where the player reconnects within 60 seconds and the group is reformed)       */
    if (Map* playerMap = player->GetMap())
        if (!switchLeader && playerMap->IsNonRaidDungeon())
            if (InstanceSave* save = sInstanceSaveMgr->GetInstanceSave(playerMap->GetInstanceId()))
                if (save->GetGroupCount() == 0 && save->GetPlayerCount() == 0)
                {
                    TC_LOG_DEBUG("maps", "Group::ConvertLeaderInstancesToGroup: Group for player %s is taking over unbound instance map %d with Id %d", player->GetName().c_str(), playerMap->GetId(), playerMap->GetInstanceId());
                    // if nobody is saved to this, then the save wasn't permanent
                    group->BindToInstance(save, false, false);
                }
}

void Group::_removeRolls(const uint64 &guid)
{
    for (auto it = RollId.begin(); it < RollId.end(); ++it)
    {
        Roll* roll = *it;
        auto itr2 = roll->playerVote.find(guid);
        if(itr2 == roll->playerVote.end())
            continue;

        if (itr2->second == GREED) --roll->totalGreed;
        if (itr2->second == NEED) --roll->totalNeed;
        if (itr2->second == PASS) --roll->totalPass;
        if (itr2->second != NOT_VALID) --roll->totalPlayersRolling;

        roll->playerVote.erase(itr2);

        CountRollVote(guid, roll->itemGUID, GetMembersCount()-1, 3);
    }
}

bool Group::_setMembersGroup(const uint64 &guid, const uint8 &group)
{
    auto slot = _getMemberWSlot(guid);
    if(slot==m_memberSlots.end())
        return false;

    slot->group = group;

    SubGroupCounterIncrease(group);

    if(!isBGGroup()) 
        CharacterDatabase.PExecute("UPDATE group_member SET subgroup='%u' WHERE memberGuid='%u'", group, GUID_LOPART(guid));

    return true;
}

bool Group::_setAssistantFlag(const uint64 &guid, const bool &state)
{
    auto slot = _getMemberWSlot(guid);
    if(slot==m_memberSlots.end())
        return false;

    slot->assistant = state;
    if(!isBGGroup()) 
        CharacterDatabase.PExecute("UPDATE group_member SET assistant='%u' WHERE memberGuid='%u'", (state==true)?1:0, GUID_LOPART(guid));
    return true;
}

bool Group::_setMainTank(const uint64 &guid)
{
    auto slot = _getMemberCSlot(guid);
    if(slot==m_memberSlots.end())
        return false;

    if(m_mainAssistant == guid)
        _setMainAssistant(0);
    m_mainTank = guid;
    if(!isBGGroup()) CharacterDatabase.PExecute("UPDATE groups SET mainTank='%u' WHERE leaderGuid='%u'", GUID_LOPART(m_mainTank), GUID_LOPART(m_leaderGuid));
    return true;
}

bool Group::_setMainAssistant(const uint64 &guid)
{
    auto slot = _getMemberWSlot(guid);
    if(slot==m_memberSlots.end())
        return false;

    if(m_mainTank == guid)
        _setMainTank(0);
    m_mainAssistant = guid;
    if(!isBGGroup()) CharacterDatabase.PExecute("UPDATE groups SET mainAssistant='%u' WHERE leaderGuid='%u'", GUID_LOPART(m_mainAssistant), GUID_LOPART(m_leaderGuid));
    return true;
}

bool Group::SameSubGroup(Player const* member1, Player const* member2) const
{
    if(!member1 || !member2) return false;
    if (member1->GetGroup() != this || member2->GetGroup() != this) return false;
    else return member1->GetSubGroup() == member2->GetSubGroup();
}

// allows setting subgroup for offline members
void Group::ChangeMembersGroup(const uint64 &guid, const uint8 &group)
{
    if(!isRaidGroup())
        return;

    Player *player = sObjectMgr->GetPlayer(guid);

    if (!player)
    {
        /*uint8 prevSubGroup = player->GetSubGroup();
        if (player->GetGroup() == this)
            player->GetGroupRef().setSubGroup(group);
        // if player is in BG raid, it is possible that he is also in normal raid - and that normal raid is stored in m_originalGroup reference
        else
        {
            prevSubGroup = player->GetOriginalSubGroup();
            player->GetOriginalGroupRef().setSubGroup(group);
        }*/
        
        uint8 prevSubGroup;
        prevSubGroup = GetMemberGroup(guid);

        SubGroupCounterDecrease(prevSubGroup);

        if(_setMembersGroup(guid, group))
            SendUpdate();
    }
    else
        // This methods handles itself groupcounter decrease
        ChangeMembersGroup(player, group);
}

// only for online members
void Group::ChangeMembersGroup(Player *player, const uint8 &group)
{
    if(!player || !isRaidGroup())
        return;

    if(_setMembersGroup(player->GetGUID(), group))
    {
        uint8 prevSubGroup;
        prevSubGroup = player->GetSubGroup();

        SubGroupCounterDecrease(prevSubGroup);

        player->GetGroupRef().setSubGroup(group);
        SendUpdate();
    }
}

void Group::UpdateLooterGuid( WorldObject* object, bool ifneed )
{
    switch (GetLootMethod())
    {
        case MASTER_LOOT:
        case FREE_FOR_ALL:
            return;
        default:
            // round robin style looting applies for all low
            // quality items in each loot method except free for all and master loot
            break;
    }

    auto guid_itr = _getMemberCSlot(GetLooterGuid());
    if(guid_itr != m_memberSlots.end())
    {
        if(ifneed)
        {
            // not update if only update if need and ok
            Player* looter = ObjectAccessor::FindPlayer(guid_itr->guid);
            if (looter && looter->IsAtGroupRewardDistance(object))
                return;
        }
        ++guid_itr;
    }

    // search next after current
    if(guid_itr != m_memberSlots.end())
    {
        for(auto itr = guid_itr; itr != m_memberSlots.end(); ++itr)
        {
            if(Player* pl = ObjectAccessor::FindPlayer(itr->guid))
            {
                if (pl->IsAtGroupRewardDistance(object))
                {
                    bool refresh = pl->GetLootGUID()==object->GetGUID();

                    //if(refresh)                             // update loot for new looter
                    //    pl->GetSession()->DoLootRelease(pl->GetLootGUID());
                    SetLooterGuid(pl->GetGUID());
                    SendUpdate();
                    if(refresh)                             // update loot for new looter
                        pl->SendLoot(object->GetGUID(),LOOT_CORPSE);
                    return;
                }
            }
        }
    }

    // search from start
    for(member_citerator itr = m_memberSlots.begin(); itr != guid_itr; ++itr)
    {
        if(Player* pl = ObjectAccessor::FindPlayer(itr->guid))
        {
            if (pl->IsAtGroupRewardDistance(object))
            {
                bool refresh = pl->GetLootGUID()==object->GetGUID();

                //if(refresh)                               // update loot for new looter
                //    pl->GetSession()->DoLootRelease(pl->GetLootGUID());
                SetLooterGuid(pl->GetGUID());
                SendUpdate();
                if(refresh)                                 // update loot for new looter
                    pl->SendLoot(object->GetGUID(),LOOT_CORPSE);
                return;
            }
        }
    }

    SetLooterGuid(0);
    SendUpdate();
}

GroupJoinBattlegroundResult Group::CanJoinBattlegroundQueue(Battleground const* bgOrTemplate, BattlegroundQueueTypeId bgQueueTypeId, uint32 MinPlayerCount, uint32 /*MaxPlayerCount*/, bool isRated, uint32 arenaSlot)
{
#ifdef LICH_KING
    // check if this group is LFG group
    if (isLFGGroup())
        return ERR_LFG_CANT_USE_BATTLEGROUND;
#endif

    BattlemasterListEntry const* bgEntry = sBattlemasterListStore.LookupEntry(bgOrTemplate->GetTypeID());
    if (!bgEntry)
        return ERR_GROUP_JOIN_BATTLEGROUND_FAIL;            // shouldn't happen

                                                            // check for min / max count
    uint32 memberscount = GetMembersCount();

#ifdef LICH_KING
    if (memberscount > bgEntry->maxGroupSize)                // no MinPlayerCount for battlegrounds
#else
    if (memberscount > bgEntry->maxplayersperteam)
#endif
        return ERR_BATTLEGROUND_NONE;                        // ERR_GROUP_JOIN_BATTLEGROUND_TOO_MANY handled on client side

    // get a player as reference, to compare other players' stats to (arena team id, queue id based on level, etc.)
    Player* reference = ASSERT_NOTNULL(GetFirstMember())->GetSource();
    // no reference found, can't join this way
    if (!reference)
#ifdef LICH_KING
        return ERR_BATTLEGROUND_JOIN_FAILED;
#else
        return ERR_BATTLEGROUND_NONE;
#endif

    PvPDifficultyEntry const* bracketEntry = GetBattlegroundBracketByLevel(bgOrTemplate->GetMapId(), reference->GetLevel());
    if (!bracketEntry)
#ifdef LICH_KING
        return ERR_BATTLEGROUND_JOIN_FAILED;
#else
        return ERR_BATTLEGROUND_NONE;
#endif

    uint32 arenaTeamId = reference->GetArenaTeamId(arenaSlot);
    uint32 team = reference->GetTeam();

#ifdef LICH_KING
    BattlegroundQueueTypeId bgQueueTypeIdRandom = BattlegroundMgr::BGQueueTypeId(BATTLEGROUND_RB, 0);
#endif

    // check every member of the group to be able to join
    memberscount = 0;
    for (GroupReference* itr = GetFirstMember(); itr != nullptr; itr = itr->next(), ++memberscount)
    {
        Player* member = itr->GetSource();
        // offline member? don't let join
        if (!member)
#ifdef LICH_KING
            return ERR_BATTLEGROUND_JOIN_FAILED;
#else
            return FAKE_ERR_BATTLEGROUND_OFFLINE_MEMBER;
#endif
        // don't allow cross-faction join as group
        if (member->GetTeam() != team)
#ifdef LICH_KING
            return ERR_BATTLEGROUND_JOIN_TIMED_OUT;
#else
            return ERR_BATTLEGROUND_MIXED_TEAM;
#endif
        // not in the same battleground level braket, don't let join
        PvPDifficultyEntry const* memberBracketEntry = GetBattlegroundBracketByLevel(bracketEntry->mapId, member->GetLevel());
        if (memberBracketEntry != bracketEntry)
#ifdef LICH_KING
            return ERR_BATTLEGROUND_JOIN_RANGE_INDEX;
#else
            return FAKE_ERR_BATTLEGROUND_MIXED_LEVELS;
#endif
        // don't let join rated matches if the arena team id doesn't match
        if (isRated && member->GetArenaTeamId(arenaSlot) != arenaTeamId)
#ifdef LICH_KING
            return ERR_BATTLEGROUND_JOIN_FAILED;
#else
            return ERR_BATTLEGROUND_NONE;
#endif
        // don't let join if someone from the group is already in that bg queue
        if (member->InBattlegroundQueueForBattlegroundQueueType(bgQueueTypeId))
#ifdef LICH_KING
            return ERR_BATTLEGROUND_JOIN_FAILED;  // not blizz-like
#else
            return FAKE_ERR_BATTLEGROUND_ALREADY_IN_QUEUE;  // not blizz-like
#endif           
#ifdef LICH_KING
        // don't let join if someone from the group is in bg queue random
        if (member->InBattlegroundQueueForBattlegroundQueueType(bgQueueTypeIdRandom))
            return ERR_IN_RANDOM_BG;
        // don't let join to bg queue random if someone from the group is already in bg queue
        if (bgOrTemplate->GetTypeID() == BATTLEGROUND_RB && member->InBattlegroundQueue())
            return ERR_IN_NON_RANDOM_BG;
#endif
        // check for deserter debuff in case not arena queue
        if (bgOrTemplate->GetTypeID() != BATTLEGROUND_AA && !member->CanJoinToBattleground(bgOrTemplate))
            return ERR_GROUP_JOIN_BATTLEGROUND_DESERTERS;
        // check if member can join any more battleground queues
        if (!member->HasFreeBattlegroundQueueId())
            return ERR_BATTLEGROUND_TOO_MANY_QUEUES;        // not blizz-like
#ifdef LICH_KING
        // check if someone in party is using dungeon system
        if (member->isUsingLfg())
            return ERR_LFG_CANT_USE_BATTLEGROUND;
#endif
        // check Freeze debuff
        if (member->HasAura(9454))
#ifdef LICH_KING
            return ERR_BATTLEGROUND_JOIN_FAILED;  // not blizz-like
#else
            return FAKE_ERR_BATTLEGROUND_FROZEN;  // not blizz-like
#endif         
    }

    // only check for MinPlayerCount since MinPlayerCount == MaxPlayerCount for arenas...
    if (bgOrTemplate->IsArena() && memberscount != MinPlayerCount)
#ifdef LICH_KING
        return ERR_ARENA_TEAM_PARTY_SIZE;
#else
        return FAKE_ERR_BATTLEGROUND_TEAM_SIZE;
#endif

    return GroupJoinBattlegroundResult(bgOrTemplate->GetTypeID());
}

//===================================================
//============== Roll ===============================
//===================================================

void Roll::targetObjectBuildLink()
{
    // called from link()
    getTarget()->addLootValidatorRef(this);
}

void Group::SetDifficulty(Difficulty difficulty)
{
    ASSERT(uint32(difficulty) < MAX_DIFFICULTY);
    m_dungeonDifficulty = difficulty;
    if(!isBGGroup()) 
        CharacterDatabase.PExecute("UPDATE groups SET difficulty = %u WHERE leaderGuid ='%u'", m_dungeonDifficulty, GUID_LOPART(m_leaderGuid));

    for(GroupReference *itr = GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player *player = itr->GetSource();
        if(!player->GetSession() || player->GetLevel() < LEVELREQUIREMENT_HEROIC)
            continue;
        player->SetDifficulty(difficulty, true, true);
    }
}

bool Group::InCombatToInstance(uint32 instanceId)
{
    for(GroupReference *itr = GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player *pPlayer = itr->GetSource();
        if(pPlayer && pPlayer->GetAttackers().size() && pPlayer->GetInstanceId() == instanceId && (pPlayer->GetMap()->IsRaid() || pPlayer->GetMap()->IsHeroic()))
            for(auto i : pPlayer->GetAttackers())
                if(i && i->GetTypeId() == TYPEID_UNIT && (i->ToCreature())->GetCreatureTemplate()->flags_extra & CREATURE_FLAG_EXTRA_INSTANCE_BIND)
                    return true;
    }
    return false;
}

void Group::ResetInstances(uint8 method, bool isRaid, Player* SendMsgTo)
{
    if(isBGGroup())
        return;

    // method can be INSTANCE_RESET_ALL, INSTANCE_RESET_CHANGE_DIFFICULTY, INSTANCE_RESET_GROUP_DISBAND

    // we assume that when the difficulty changes, all instances that can be reset will be
    uint8 dif = GetDifficulty();

    for(auto itr = m_boundInstances[dif].begin(); itr != m_boundInstances[dif].end();)
    {
        InstanceSave* instanceSave = itr->second.save;
        const MapEntry* entry = sMapStore.LookupEntry(itr->first);
        if (!entry || entry->IsRaid() != isRaid || (!instanceSave->CanReset() && method != INSTANCE_RESET_GROUP_DISBAND) || entry->MapID == 580) //HACK
        {
            ++itr;
            continue;
        }

        if(method == INSTANCE_RESET_ALL)
        {
            // the "reset all instances" method can only reset normal maps
            if(dif == DUNGEON_DIFFICULTY_HEROIC || entry->map_type == MAP_RAID)
            {
                ++itr;
                continue;
            }
        }

        bool isEmpty = true;
        // if the map is loaded, reset it
        Map* map = sMapMgr->FindMap(instanceSave->GetMapId(), instanceSave->GetInstanceId());
        if (map && map->IsDungeon() && !(method == INSTANCE_RESET_GROUP_DISBAND && !instanceSave->CanReset()))
        {
            if(instanceSave->CanReset())
                isEmpty = ((InstanceMap*)map)->Reset(method);
            else
                isEmpty = !map->HavePlayers();
        }

        if(SendMsgTo)
        {
            if (!isEmpty)
                SendMsgTo->SendResetInstanceFailed(0, instanceSave->GetMapId());
            else
            {
                if (Group* group = SendMsgTo->GetGroup())
                {
                    for (GroupReference* groupRef = group->GetFirstMember(); groupRef != nullptr; groupRef = groupRef->next())
                        if (Player* player = groupRef->GetSource())
                            player->SendResetInstanceSuccess(instanceSave->GetMapId());
                }

                else
                    SendMsgTo->SendResetInstanceSuccess(instanceSave->GetMapId());
            }
        }

        if(isEmpty || method == INSTANCE_RESET_GROUP_DISBAND || method == INSTANCE_RESET_CHANGE_DIFFICULTY)
        {
            // do not reset the instance, just unbind if others are permanently bound to it
            if (isEmpty && instanceSave->CanReset())
                instanceSave->DeleteFromDB();
            else
            {
                PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_GROUP_INSTANCE_BY_INSTANCE);

                stmt->setUInt32(0, instanceSave->GetInstanceId());

                CharacterDatabase.Execute(stmt);
            }
            // i don't know for sure if hash_map iterators
            m_boundInstances[dif].erase(itr);
            itr = m_boundInstances[dif].begin();
            // this unloads the instance save unless online players are bound to it
            // (eg. permanent binds or GM solo binds)
            instanceSave->RemoveGroup(this);
        }
        else
            ++itr;
    }
}


InstanceGroupBind* Group::GetBoundInstance(Player* player)
{
    uint32 mapid = player->GetMapId();
    MapEntry const* mapEntry = sMapStore.LookupEntry(mapid);
    return GetBoundInstance(mapEntry);
}

InstanceGroupBind* Group::GetBoundInstance(Map* aMap)
{
    // Currently spawn numbering not different from map difficulty
    Difficulty difficulty = GetDifficulty(aMap->IsRaid());
    return GetBoundInstance(difficulty, aMap->GetId());
}

InstanceGroupBind* Group::GetBoundInstance(MapEntry const* mapEntry)
{
    if (!mapEntry || !mapEntry->IsDungeon())
        return nullptr;

    Difficulty difficulty = GetDifficulty(mapEntry->IsRaid());
    return GetBoundInstance(difficulty, mapEntry->MapID);
}

Difficulty Group::GetDifficulty(bool isRaid) const
{
    return isRaid ? m_raidDifficulty : m_dungeonDifficulty;
}

InstanceGroupBind* Group::GetBoundInstance(Difficulty difficulty, uint32 mapId)
{
    if (uint32(difficulty) >= MAX_DUNGEON_DIFFICULTY)
    {
        TC_LOG_ERROR("misc", "Group::GetBoundInstance called with invalid difficulty %u", uint32(difficulty));
        return nullptr;
    }

    // some instances only have one difficulty
    GetDownscaledMapDifficultyData(mapId, difficulty);

    auto itr = m_boundInstances[difficulty].find(mapId);
    if (itr != m_boundInstances[difficulty].end())
        return &itr->second;
    else
        return nullptr;
}

Group::BoundInstancesMap& Group::GetBoundInstances(Difficulty difficulty)
{
    return m_boundInstances[difficulty];
}

InstanceGroupBind* Group::BindToInstance(InstanceSave *save, bool permanent, bool load)
{
    if(save && !isBGGroup())
    {
        InstanceGroupBind& bind = m_boundInstances[save->GetDifficulty()][save->GetMapId()];
        if(bind.save)
        {
            // when a boss is killed or when copying the player's binds to the group
            if(permanent != bind.perm || save != bind.save)
                if(!load) CharacterDatabase.PExecute("UPDATE group_instance SET instance = '%u', permanent = '%u' WHERE leaderGuid = '%u' AND instance = '%u'", save->GetInstanceId(), permanent, GUID_LOPART(GetLeaderGUID()), bind.save->GetInstanceId());
        }
        else
            if(!load) CharacterDatabase.PExecute("INSERT INTO group_instance (leaderGuid, instance, permanent) VALUES ('%u', '%u', '%u')", GUID_LOPART(GetLeaderGUID()), save->GetInstanceId(), permanent);

        if(bind.save != save)
        {
            if(bind.save) bind.save->RemoveGroup(this);
            save->AddGroup(this);
        }

        bind.save = save;
        bind.perm = permanent;
        return &bind;
    }
    else
        return nullptr;
}

void Group::UnbindInstance(uint32 mapid, uint8 difficulty, bool unload)
{
    auto itr = m_boundInstances[difficulty].find(mapid);
    if(itr != m_boundInstances[difficulty].end())
    {
        if(!unload) CharacterDatabase.PExecute("DELETE FROM group_instance WHERE leaderGuid = '%u' AND instance = '%u'", GUID_LOPART(GetLeaderGUID()), itr->second.save->GetInstanceId());
        itr->second.save->RemoveGroup(this);                // save can become invalid
        m_boundInstances[difficulty].erase(itr);
    }
}

void Group::_homebindIfInstance(Player *player)
{
    if(player && !player->IsGameMaster() && sMapStore.LookupEntry(player->GetMapId())->IsDungeon())
    {
        // leaving the group in an instance, the homebind timer is started
        /*InstanceSave *save = sInstanceSaveMgr->GetInstanceSave(player->GetInstanceId());
        InstancePlayerBind *playerBind = save ? player->GetBoundInstance(save->GetMapId(), save->GetDifficulty()) : NULL;
        if(!playerBind || !playerBind->perm)*/
            player->m_InstanceValid = false;
    }
}

void Group::BroadcastGroupUpdate()
{
    // FG: HACK: force flags update on group leave - for values update hack
    // -- not very efficient but safe
    for(member_citerator citr = m_memberSlots.begin(); citr != m_memberSlots.end(); ++citr)
    {

        Player *pp = sObjectMgr->GetPlayer(citr->guid);
        if(pp && pp->IsInWorld())
        {
            pp->ForceValuesUpdateAtIndex(UNIT_FIELD_BYTES_2);
            pp->ForceValuesUpdateAtIndex(UNIT_FIELD_FACTIONTEMPLATE);
            TC_LOG_DEBUG("network.opcode","-- Forced group value update for '%s'", pp->GetName().c_str());
        }
    }
}

Player* Group::GetRandomMember()
{
    std::list<Player*> players;
    Player* player;
    
    for(member_citerator citr = m_memberSlots.begin(); citr != m_memberSlots.end(); ++citr)
    {
        player = sObjectMgr->GetPlayer(citr->guid);
        if (!player)
            continue;
        if (player->IsDead())
            continue;
            
        players.push_back(player);
    }
    
    Trinity::Containers::RandomResize(players, 1);
    
    return players.front();
}
