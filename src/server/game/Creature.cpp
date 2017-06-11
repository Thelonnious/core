
#include "Common.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "World.h"
#include "ObjectMgr.h"
#include "SpellMgr.h"
#include "Creature.h"
#include "QuestDef.h"
#include "GossipDef.h"
#include "Player.h"
#include "Opcodes.h"
#include "Log.h"
#include "LootMgr.h"
#include "MapManager.h"
#include "CreatureAI.h"
#include "CreatureAISelector.h"
#include "Formulas.h"
#include "SpellAuras.h"
#include "WaypointMovementGenerator.h"
#include "InstanceScript.h"
#include "BattleGround.h"
#include "Util.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "OutdoorPvPMgr.h"
#include "GameEventMgr.h"
#include "CreatureGroups.h"
#include "ScriptMgr.h"
#include "TemporarySummon.h"
#include "MoveSpline.h"
#include "Spell.h"
#include "InstanceScript.h"
#include "Transport.h"

void TrainerSpellData::Clear()
{
    for (auto & itr : spellList)
        delete itr;
    spellList.empty();
}

TrainerSpell const* TrainerSpellData::Find(uint32 spell_id) const
{
    for(auto itr : spellList)
        if(itr->spell == spell_id)
            return itr;

    return nullptr;
}

bool VendorItemData::RemoveItem( uint32 item_id )
{
    for(auto i = m_items.begin(); i != m_items.end(); ++i )
    {
        if((*i)->proto->ItemId==item_id)
        {
            m_items.erase(i);
            return true;
        }
    }
    return false;
}

size_t VendorItemData::FindItemSlot(uint32 item_id) const
{
    for(size_t i = 0; i < m_items.size(); ++i )
        if(m_items[i]->proto->ItemId==item_id)
            return i;
    return m_items.size();
}

VendorItem const* VendorItemData::FindItem(uint32 item_id) const
{
    for(auto m_item : m_items)
        if(m_item->proto->ItemId==item_id)
            return m_item;
    return nullptr;
}

uint32 CreatureTemplate::GetRandomValidModelId() const
{
    uint32 c = 0;
    uint32 modelIDs[4];

    if (Modelid1) modelIDs[c++] = Modelid1;
    if (Modelid2) modelIDs[c++] = Modelid2;
    if (Modelid3) modelIDs[c++] = Modelid3;
    if (Modelid4) modelIDs[c++] = Modelid4;

    return ((c>0) ? modelIDs[urand(0,c-1)] : 0);
}

uint32 CreatureTemplate::GetFirstValidModelId() const
{
    if(Modelid1) return Modelid1;
    if(Modelid2) return Modelid2;
    if(Modelid3) return Modelid3;
    if(Modelid4) return Modelid4;
    return 0;
}

uint32 CreatureTemplate::GetFirstInvisibleModel() const
{
    /* TC, not implemented 
    CreatureModelInfo const* modelInfo = sObjectMgr->GetCreatureModelInfo(Modelid1);
    if (modelInfo && modelInfo->is_trigger)
        return Modelid1;

    modelInfo = sObjectMgr->GetCreatureModelInfo(Modelid2);
    if (modelInfo && modelInfo->is_trigger)
        return Modelid2;

    modelInfo = sObjectMgr->GetCreatureModelInfo(Modelid3);
    if (modelInfo && modelInfo->is_trigger)
        return Modelid3;

    modelInfo = sObjectMgr->GetCreatureModelInfo(Modelid4);
    if (modelInfo && modelInfo->is_trigger)
        return Modelid4;
    */

    return 11686;
}

CreatureBaseStats const* CreatureBaseStats::GetBaseStats(uint8 level, uint8 unitClass)
{
    return sObjectMgr->GetCreatureBaseStats(level, unitClass);
}

bool AssistDelayEvent::Execute(uint64 /*e_time*/, uint32 /*p_time*/)
{
    Unit* victim = ObjectAccessor::GetUnit(m_owner, m_victim);
    if (victim)
    {
        while (!m_assistants.empty())
        {
            Creature* assistant = ObjectAccessor::GetCreature(m_owner, *m_assistants.begin());
            m_assistants.pop_front();

            if (assistant && assistant->CanAssistTo(&m_owner, victim))
            {
                assistant->SetNoCallAssistance(true);
                assistant->CombatStart(victim);
                if (assistant->IsAIEnabled) {
                    assistant->AI()->EnterCombat(victim);
                    assistant->AI()->AttackStart(victim);
                }

                if (InstanceScript* instance = ((InstanceScript*)assistant->GetInstanceScript()))
                    instance->MonsterPulled(assistant, victim);
            }
        }
    }
    return true;
}

bool ForcedDespawnDelayEvent::Execute(uint64 /*e_time*/, uint32 /*p_time*/)
{
    m_owner.ForcedDespawn();
    return true;
}

Creature::Creature(bool isWorldObject) : Unit(isWorldObject), MapObject(),
    lootForPickPocketed(false), 
    lootForBody(false), 
    m_lootMoney(0), 
    m_lootRecipient(0), 
    m_lootRecipientGroup(0),
    m_corpseRemoveTime(0), m_respawnTime(0), m_respawnDelay(25),
    m_corpseDelay(60), 
    m_respawnradius(0.0f),
    m_reactState(REACT_AGGRESSIVE), 
    m_transportCheckTimer(1000),
    m_regenTimer(2000),
    m_defaultMovementType(IDLE_MOTION_TYPE), 
    m_equipmentId(0), 
    m_areaCombatTimer(0), 
    m_relocateTimer(60000),
    m_AlreadyCallAssistance(false), 
    m_regenHealth(true), 
    m_AI_locked(false),
    m_meleeDamageSchoolMask(SPELL_SCHOOL_MASK_NORMAL),
    m_creatureInfo(nullptr), 
    m_creatureData(nullptr),
    m_creatureInfoAddon(nullptr), 
    m_spawnId(0), 
    m_formation(nullptr),
    m_PlayerDamageReq(0),
    m_timeSinceSpawn(0), 
    m_creaturePoolId(0), 
    _focusSpell(nullptr),
    m_isBeingEscorted(false), 
    m_path_id(0), 
    m_unreachableTargetTime(0), 
    m_evadingAttacks(false), 
    m_canFly(false),
    m_stealthAlertCooldown(0), 
    m_keepActiveTimer(0), 
    m_homeless(false)
{
    m_valuesCount = UNIT_END;

    for(uint32 & m_spell : m_spells)
        m_spell = 0;

	m_SightDistance = sWorld->getFloatConfig(CONFIG_SIGHT_MONSTER);
	m_CombatDistance = 0;//MELEE_RANGE;

    m_CreatureSpellCooldowns.clear();
    m_CreatureCategoryCooldowns.clear();
    m_GlobalCooldown = 0;
    DisableReputationGain = false;
    TriggerJustRespawned = false;
}

Creature::~Creature()
{
    m_vendorItemCounts.clear();

    if(i_AI)
    {
        delete i_AI;
        i_AI = nullptr;
    }

    if(m_uint32Values)
        TC_LOG_DEBUG("entities.unit","Deconstruct Creature Entry = %u", GetEntry());
}

void Creature::AddToWorld()
{
    ///- Register the creature for guid lookup
    if(!IsInWorld())
    {
        if (Map *map = FindMap())
            if (map->IsDungeon() && ((InstanceMap*)map)->GetInstanceScript())
                ((InstanceMap*)map)->GetInstanceScript()->OnCreatureCreate(this);

		GetMap()->GetObjectsStore().Insert<Creature>(GetGUID(), this);
        if (m_spawnId)
            GetMap()->GetCreatureBySpawnIdStore().insert(std::make_pair(m_spawnId, this));

        Unit::AddToWorld();
        SearchFormation();

        if(uint32 guid = GetSpawnId())
        {
            if (CreatureData const* data = sObjectMgr->GetCreatureData(guid))
                m_creaturePoolId = data->poolId;
            if (m_creaturePoolId)
                FindMap()->AddCreatureToPool(this, m_creaturePoolId);
        }

        AIM_Initialize();

#ifdef LICH_KING
        if (IsVehicle())
            GetVehicleKit()->Install();
#endif
    }
}

void Creature::RemoveFromWorld()
{
    ///- Remove the creature from the accessor
    if(IsInWorld())
    {
        if(Map *map = FindMap())
            if(map->IsDungeon() && ((InstanceMap*)map)->GetInstanceScript())
                ((InstanceMap*)map)->GetInstanceScript()->OnCreatureRemove(this);

        if(m_formation)
            sCreatureGroupMgr->RemoveCreatureFromGroup(m_formation, this);

        if (Transport* transport = GetTransport())
            transport->RemovePassenger(this, true);

        if (m_creaturePoolId)
            FindMap()->RemoveCreatureFromPool(this, m_creaturePoolId);

        Unit::RemoveFromWorld();

        if (m_spawnId)
            Trinity::Containers::MultimapErasePair(GetMap()->GetCreatureBySpawnIdStore(), m_spawnId, this);

		GetMap()->GetObjectsStore().Remove<Creature>(GetGUID());
    }
}

void Creature::DisappearAndDie()
{
    ForcedDespawn(0);
}

void Creature::SearchFormation()
{
    if(IsPet())
        return;

    uint32 lowguid = GetSpawnId();
    if(!lowguid)
        return;

    auto CreatureGroupMap = sCreatureGroupMgr->GetGroupMap();
    auto frmdata = CreatureGroupMap.find(lowguid);
    if(frmdata != CreatureGroupMap.end())
        sCreatureGroupMgr->AddCreatureToGroup(frmdata->second->leaderGUID, this);

}

void Creature::RemoveCorpse(bool setSpawnTime, bool destroyForNearbyPlayers)
{
    if( GetDeathState() != CORPSE )
        return;

    m_corpseRemoveTime = time(nullptr);
    SetDeathState(DEAD);
    RemoveAllAuras();
    loot.clear();

    if (IsAIEnabled)
        AI()->CorpseRemoved(m_respawnDelay);

    if (destroyForNearbyPlayers)
        DestroyForNearbyPlayers();

    // Should get removed later, just keep "compatibility" with scripts
    if(setSpawnTime)
        m_respawnTime = std::max<time_t>(time(NULL) + m_respawnDelay, m_respawnTime);

    // if corpse was removed during falling, the falling will continue and override relocation to respawn position
    if (IsFalling())
        StopMoving();

    float x,y,z,o;
    GetRespawnPosition(x, y, z, &o);

    // We were spawned on transport, calculate real position
    if (IsSpawnedOnTransport())
    {
        Position& pos = m_movementInfo.transport.pos;
        pos.m_positionX = x;
        pos.m_positionY = y;
        pos.m_positionZ = z;
        pos.SetOrientation(o);

        if (Transport* transport = GetTransport())
            transport->CalculatePassengerPosition(x, y, z, &o);
    }

    SetHomePosition(x,y,z,o);
    GetMap()->CreatureRelocation(this,x,y,z,o);
}

/**
 * change the entry of creature until respawn
 */
bool Creature::InitEntry(uint32 Entry, const CreatureData *data )
{
    CreatureTemplate const *normalInfo = sObjectMgr->GetCreatureTemplate(Entry);
    if(!normalInfo)
    {
        TC_LOG_ERROR("FIXME","Creature::UpdateEntry creature entry %u does not exist.", Entry);
        return false;
    }

    // get heroic mode entry
    uint32 actualEntry = Entry;
    CreatureTemplate const *cinfo = normalInfo;
    if(normalInfo->difficulty_entry_1)
    {
        Map *map = sMapMgr->FindMap(GetMapId(), GetInstanceId());
        if(map && map->IsHeroic())
        {
            cinfo = sObjectMgr->GetCreatureTemplate(normalInfo->difficulty_entry_1);
            if(!cinfo)
            {
                TC_LOG_ERROR("FIXME","Creature::UpdateEntry creature heroic entry %u does not exist.", actualEntry);
                return false;
            }
        }
    }

    SetEntry(Entry);                                        // normal entry always
    m_creatureInfo = cinfo;                                 // map mode related always

    // equal to player Race field, but creature does not have race
    SetByteValue(UNIT_FIELD_BYTES_0, UNIT_BYTES_0_OFFSET_RACE, 0);

    // known valid are: CLASS_WARRIOR, CLASS_PALADIN, CLASS_ROGUE, CLASS_MAGE
    SetByteValue(UNIT_FIELD_BYTES_0, UNIT_BYTES_0_OFFSET_CLASS, uint8(cinfo->unit_class));

    // Cancel load if no model defined
    if (!(cinfo->GetFirstValidModelId()))
    {
        TC_LOG_ERROR("sql.sql","Creature (Entry: %u) has no model defined in table `creature_template`, can't load. ",Entry);
        return false;
    }

    uint32 display_id = sObjectMgr->ChooseDisplayId(GetCreatureTemplate(), data);
    CreatureModelInfo const *minfo = sObjectMgr->GetCreatureModelRandomGender(display_id);
    if (!minfo)
    {
        TC_LOG_ERROR("sql.sql","Creature (Entry: %u) has model %u not found in table `creature_model_info`, can't load. ", Entry, display_id);
        return false;
    }

    SetDisplayId(display_id);
    SetNativeDisplayId(display_id);

    // equal to player Race field, but creature does not have race
    SetByteValue(UNIT_FIELD_BYTES_0, UNIT_BYTES_0_OFFSET_GENDER, minfo->gender);

    LastUsedScriptID = GetScriptId();

    m_homeless = m_creatureInfo->flags_extra & CREATURE_FLAG_EXTRA_HOMELESS;

    // Load creature equipment
    if(!data || data->equipmentId == 0)
    {                                                       // use default from the template
        LoadEquipment(cinfo->equipmentId);
    }
    else if(data && data->equipmentId != -1)
    {                                                       // override, -1 means no equipment
        LoadEquipment(data->equipmentId);
    }

    SetName(normalInfo->Name);                              // at normal entry always

    SetFloatValue(UNIT_FIELD_BOUNDINGRADIUS,minfo->bounding_radius);
    SetFloatValue(UNIT_FIELD_COMBATREACH,minfo->combat_reach );

    SetFloatValue(UNIT_MOD_CAST_SPEED, 1.0f);

    SetSpeedRate(MOVE_WALK,     cinfo->speed );
    SetSpeedRate(MOVE_RUN,      cinfo->speed );
    SetSpeedRate(MOVE_SWIM,     cinfo->speed );

    SetFloatValue(OBJECT_FIELD_SCALE_X, cinfo->scale);

    SetCanFly(GetCreatureTemplate()->InhabitType & INHABIT_AIR);
    // checked at loading
    m_defaultMovementType = MovementGeneratorType(cinfo->MovementType);
    if(!m_respawnradius && m_defaultMovementType==RANDOM_MOTION_TYPE)
        m_defaultMovementType = IDLE_MOTION_TYPE;

    for (uint8 i=0; i < CREATURE_MAX_SPELLS; ++i)
        m_spells[i] = GetCreatureTemplate()->spells[i];

    SetQuestPoolId(normalInfo->QuestPoolId);

    return true;
}

bool Creature::UpdateEntry(uint32 Entry, const CreatureData *data )
{
    if(!InitEntry(Entry,data))
        return false;

    CreatureTemplate const* cInfo = GetCreatureTemplate();
    m_regenHealth = GetCreatureTemplate()->RegenHealth;

    if(GetCreatureTemplate()->flags_extra & CREATURE_FLAG_EXTRA_CIVILIAN)
        SetSheath(SHEATH_STATE_UNARMED);
    else
        SetSheath(SHEATH_STATE_MELEE);

    SetByteValue(UNIT_FIELD_BYTES_2, UNIT_BYTES_2_OFFSET_1_UNK, UNIT_BYTE2_FLAG_AURAS );

    SelectLevel();
    SetUInt32Value(UNIT_FIELD_FACTIONTEMPLATE, cInfo->faction);

    if(GetCreatureTemplate()->flags_extra & CREATURE_FLAG_EXTRA_WORLDEVENT)
        SetUInt32Value(UNIT_NPC_FLAGS,GetCreatureTemplate()->npcflag | sGameEventMgr->GetNPCFlag(this));
    else
        SetUInt32Value(UNIT_NPC_FLAGS,GetCreatureTemplate()->npcflag);


    SetAttackTime(BASE_ATTACK,  cInfo->baseattacktime);
    SetAttackTime(OFF_ATTACK,   cInfo->baseattacktime);
    SetAttackTime(RANGED_ATTACK,cInfo->rangeattacktime);

    uint32 unit_flags = cInfo->unit_flags;
    // if unit is in combat, keep this flag
    unit_flags &= ~UNIT_FLAG_IN_COMBAT;
    if (IsInCombat())
        unit_flags |= UNIT_FLAG_IN_COMBAT;

    SetUInt32Value(UNIT_FIELD_FLAGS, unit_flags);
    SetUInt32Value(UNIT_DYNAMIC_FLAGS,cInfo->dynamicflags);

    SetMeleeDamageSchool(SpellSchools(cInfo->dmgschool));
    CreatureBaseStats const* stats = sObjectMgr->GetCreatureBaseStats(GetLevel(), cInfo->unit_class);
    float armor = (float)stats->GenerateArmor(cInfo); /// @todo Why is this treated as uint32 when it's a float?
    SetStatFlatModifier(UNIT_MOD_ARMOR,             BASE_VALUE, armor);
    SetStatFlatModifier(UNIT_MOD_RESISTANCE_HOLY,   BASE_VALUE, float(cInfo->resistance[SPELL_SCHOOL_HOLY-1])); //shifted by 1 because SPELL_SCHOOL_NORMAL is not in array
    SetStatFlatModifier(UNIT_MOD_RESISTANCE_FIRE,   BASE_VALUE, float(cInfo->resistance[SPELL_SCHOOL_FIRE-1]));
    SetStatFlatModifier(UNIT_MOD_RESISTANCE_NATURE, BASE_VALUE, float(cInfo->resistance[SPELL_SCHOOL_NATURE-1]));
    SetStatFlatModifier(UNIT_MOD_RESISTANCE_FROST,  BASE_VALUE, float(cInfo->resistance[SPELL_SCHOOL_FROST-1]));
    SetStatFlatModifier(UNIT_MOD_RESISTANCE_SHADOW, BASE_VALUE, float(cInfo->resistance[SPELL_SCHOOL_SHADOW-1]));
    SetStatFlatModifier(UNIT_MOD_RESISTANCE_ARCANE, BASE_VALUE, float(cInfo->resistance[SPELL_SCHOOL_ARCANE-1]));

    if(GetCreatureTemplate()->flags_extra & CREATURE_FLAG_EXTRA_DUAL_WIELD)
        SetCanDualWield(true);

    SetCanModifyStats(true);
    UpdateAllStats();

    FactionTemplateEntry const* factionTemplate = sFactionTemplateStore.LookupEntry(GetCreatureTemplate()->faction);
    if (factionTemplate)                                    // check and error show at loading templates
    {
        FactionEntry const* factionEntry = sFactionStore.LookupEntry(factionTemplate->faction);
        if (factionEntry)
            if( !IsCivilian() &&
                (factionEntry->team == ALLIANCE || factionEntry->team == HORDE) )
                SetPvP(true);
    }

    // HACK: trigger creature is always not selectable
    if(IsTrigger())
    {
        SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
        SetDisableGravity(true);
    }

    if(IsTotem() || IsTrigger() || GetCreatureType() == CREATURE_TYPE_CRITTER)
        SetReactState(REACT_PASSIVE);
    /*else if(IsCivilian())
        SetReactState(REACT_DEFENSIVE);*/
    else
        SetReactState(REACT_AGGRESSIVE);

    if(cInfo->flags_extra & CREATURE_FLAG_EXTRA_NO_TAUNT)
    {
        ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_MOD_TAUNT, true);
        ApplySpellImmune(0, IMMUNITY_EFFECT,SPELL_EFFECT_ATTACK_ME, true);
    }
    if(GetCreatureTemplate()->flags_extra & CREATURE_FLAG_EXTRA_NO_SPELL_SLOW)
        ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_HASTE_SPELLS, true);

    //TrinityCore has this LoadCreatureAddon();
    UpdateMovementFlags();
    return true;
}

void Creature::Update(uint32 diff)
{
    if (IsAIEnabled && TriggerJustRespawned)
    {
        TriggerJustRespawned = false;
        AI()->JustRespawned();
            
        Map *map = FindMap();
        if (map && map->IsDungeon() && ((InstanceMap*)map)->GetInstanceScript())
            ((InstanceMap*)map)->GetInstanceScript()->OnCreatureRespawn(this);
    }

    switch(m_deathState)
    {
        case JUST_RESPAWNED:
            // Must not be called, see Creature::setDeathState JUST_RESPAWNED -> ALIVE promoting.
            TC_LOG_ERROR("entities.unit","Creature (GUIDLow: %u Entry: %u ) in wrong state: JUST_RESPAWNED (4)",GetGUIDLow(),GetEntry());
            break;
        case JUST_DIED:
            // Must not be called, see Creature::setDeathState JUST_DIED -> CORPSE promoting.
            TC_LOG_ERROR("entities.unit","Creature (GUIDLow: %u Entry: %u ) in wrong state: JUST_DEAD (1)",GetGUIDLow(),GetEntry());
            break;
        case DEAD:
        {
            if( m_respawnTime <= time(nullptr) )
            {
                Map* map = FindMap();
                uint32 eventId = getInstanceEventId();
                if (eventId != -1 && map && map->IsRaid() && ((InstanceMap*)map)->GetInstanceScript()) {
                    if ((((InstanceMap*)map)->GetInstanceScript())->GetData(eventId) == NOT_STARTED)
                        Respawn(); // Respawn immediately
                    else if ((((InstanceMap*)map)->GetInstanceScript())->GetData(eventId) == IN_PROGRESS)
                        SetRespawnTime(5*MINUTE); // Delay next respawn check (5 minutes)
                    else // event is DONE or SPECIAL, don't respawn until tag reset
                        SetRespawnTime(7*DAY);
                }
                else if (!GetLinkedCreatureRespawnTime()) // Can respawn
                    Respawn();
                else { // the master is dead
                    if(uint32 targetGuid = sObjectMgr->GetLinkedRespawnGuid(m_spawnId)) {
                        if(targetGuid == m_spawnId) // if linking self, never respawn (check delayed to next day)
                            SetRespawnTime(DAY);
                        else
                            m_respawnTime = (time(nullptr)>GetLinkedCreatureRespawnTime()? time(nullptr):GetLinkedCreatureRespawnTime())+urand(5,MINUTE); // else copy time from master and add a little
                        SaveRespawnTime(); // also save to DB immediately
                    }
                    else
                        Respawn();
                }
            }
            break;
        }
        case CORPSE:
        {
            Unit::Update(diff);

            if (m_corpseRemoveTime <= time(nullptr))
            {
                RemoveCorpse(false);
                TC_LOG_DEBUG("entities.unit","Removing corpse... %u ", GetUInt32Value(OBJECT_FIELD_ENTRY));
            }
            else
            {
                if (m_groupLootTimer && lootingGroupLeaderGUID)
                {
                    if(diff <= m_groupLootTimer)
                    {
                        m_groupLootTimer -= diff;
                    }
                    else
                    {
                        Group* group = sObjectMgr->GetGroupByLeader(lootingGroupLeaderGUID);
                        if (group)
                            group->EndRoll();
                        m_groupLootTimer = 0;
                        lootingGroupLeaderGUID = 0;
                    }
                }
            }

            break;
        }
        case ALIVE:
        {
            Unit::Update(diff);
    
            // creature can be dead after Unit::Update call
            // CORPSE/DEAD state will processed at next tick (in other case death timer will be updated unexpectedly)
            if (!IsAlive())
                break;

            if (m_GlobalCooldown <= diff)
                m_GlobalCooldown = 0;
            else
                m_GlobalCooldown -= diff;

            m_timeSinceSpawn += diff;

            UpdateProhibitedSchools(diff);
            DecreaseTimer(m_stealthAlertCooldown, diff);

            //From TC. Removed as this is VERY costly in cpu time for little to no gain
            //UpdateMovementFlags();

            if (m_corpseRemoveTime <= time(nullptr))
            {
                RemoveCorpse(false);
                TC_LOG_DEBUG("entities.unit","Removing alive corpse... %u ", GetUInt32Value(OBJECT_FIELD_ENTRY));
            }

            if(IsInCombat() && 
                (IsWorldBoss() || GetCreatureTemplate()->flags_extra & CREATURE_FLAG_EXTRA_INSTANCE_BIND) &&
                GetMap() && GetMap()->IsDungeon())
            {
                if(m_areaCombatTimer < diff)
                {
                    std::list<HostileReference *> t_list = getThreatManager().getThreatList();
                    for(auto & i : t_list)
                        if(i && IS_PLAYER_GUID(i->getUnitGuid()))
                        {
                            AreaCombat();
                            break;
                        }

                    m_areaCombatTimer = 5000;
                }else m_areaCombatTimer -= diff;
            }

            // if creature is charmed, switch to charmed AI
            if(NeedChangeAI)
            {
                UpdateCharmAI();
                NeedChangeAI = false;
                IsAIEnabled = true;

                // sunwell: update combat state, if npc is not in combat - return to spawn correctly by calling EnterEvadeMode
                SelectVictim();
            }

            if(IsAIEnabled)
            {
                // do not allow the AI to be changed during update
                m_AI_locked = true;
                if (!IsInEvadeMode())
                {
                    i_AI->UpdateAI(diff);

                    HandleUnreachableTarget(diff);
                }
                m_AI_locked = false;
            }

            // creature can be dead after UpdateAI call
            // CORPSE/DEAD state will processed at next tick (in other case death timer will be updated unexpectedly)
            if(!IsAlive())
                break;

            if(!IsInCombat() && GetCreatureTemplate()->flags_extra & CREATURE_FLAG_EXTRA_PERIODIC_RELOC)
            {
                if(m_relocateTimer < diff)
                {
                     m_relocateTimer = 60000;
					 // forced recreate creature object at clients
					 DestroyForNearbyPlayers();
					 UpdateObjectVisibility();
                } else m_relocateTimer -= diff;
            }
            
            if ((GetCreatureTemplate()->flags_extra & CREATURE_FLAG_EXTRA_NO_HEALTH_RESET) == 0)
                if(m_regenTimer > 0)
                {
                    if(diff >= m_regenTimer)
                        m_regenTimer = 0;
                    else
                        m_regenTimer -= diff;
                }
            
            if (m_regenTimer == 0)
            {
                if (!IsInCombat())
                {
                    if(HasFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_TAPPED))
                        SetUInt32Value(UNIT_DYNAMIC_FLAGS, GetCreatureTemplate()->dynamicflags);
                        
                    RegenerateHealth();
                }
                else if(IsPolymorphed() || IsEvadingAttacks())
                    RegenerateHealth();

                Regenerate(POWER_MANA);

                m_regenTimer = CREATURE_REGEN_INTERVAL;
            }

            //remove keep active if a timer was defined
            if (m_keepActiveTimer != 0)
            {
                if (m_keepActiveTimer <= diff)
                {
                    SetKeepActive(false);
                    m_keepActiveTimer = 0;
                }
                else
                    m_keepActiveTimer -= diff;
            }
            
            break;
        }
        default:
            break;
    }

    if (IsInWorld())
    {
        // sunwell:
        if (IS_PLAYER_GUID(GetOwnerGUID()))
        {
            if (m_transportCheckTimer <= diff)
            {
                m_transportCheckTimer = 1000;
                Transport* newTransport = GetMap()->GetTransportForPos(GetPhaseMask(), GetPositionX(), GetPositionY(), GetPositionZ(), this);
                if (newTransport != GetTransport())
                {
                    if (GetTransport())
                        GetTransport()->RemovePassenger(this, true);
                    if (newTransport)
                        newTransport->AddPassenger(this, true);
                    this->StopMovingOnCurrentPos();
                    //SendMovementFlagUpdate();
                }
            }
            else
                m_transportCheckTimer -= diff;
        }

        sScriptMgr->OnCreatureUpdate(this, diff);
    }
}

void Creature::Regenerate(Powers power)
{
    uint32 curValue = GetPower(power);
    uint32 maxValue = GetMaxPower(power);

#ifdef LICH_KING
    if (!HasFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_REGENERATE_POWER))
        return;
#endif

    if (curValue >= maxValue)
        return;

    float addvalue = 0.0f;

    switch (power)
    {
        case POWER_FOCUS:
        {
            addvalue = 24 * sWorld->GetRate(RATE_POWER_FOCUS);

            AuraList const& ModPowerRegenPCTAuras = GetAurasByType(SPELL_AURA_MOD_POWER_REGEN_PERCENT);
            for (auto ModPowerRegenPCTAura : ModPowerRegenPCTAuras)
                if (ModPowerRegenPCTAura->GetModifier()->m_miscvalue == POWER_FOCUS)
                    addvalue *= (ModPowerRegenPCTAura->GetModifierValue() + 100) / 100.0f;
            break;
        }
        case POWER_ENERGY:
        {
            // For deathknight's ghoul.
            addvalue = 20;
            break;
        }
        case POWER_MANA:
        {
            // Combat and any controlled creature
            if (IsInCombat() || GetCharmerOrOwnerGUID())
            {
                if (!IsUnderLastManaUseEffect())
                {
                    float ManaIncreaseRate = sWorld->GetRate(RATE_POWER_MANA);
                    float Spirit = GetStat(STAT_SPIRIT);

                    addvalue = uint32((Spirit / 5.0f + 17.0f) * ManaIncreaseRate);
                }
            }
            else
                addvalue = maxValue / 3;

            break;
        }
        default:
            return;
    }

    // Apply modifiers (if any).
    addvalue *= GetTotalAuraMultiplierByMiscValue(SPELL_AURA_MOD_POWER_REGEN_PERCENT, power);

    addvalue += GetTotalAuraModifierByMiscValue(SPELL_AURA_MOD_POWER_REGEN, power) * (IsHunterPet() ? PET_FOCUS_REGEN_INTERVAL : CREATURE_REGEN_INTERVAL) / (5 * IN_MILLISECONDS);

    ModifyPower(power, addvalue);
}

void Creature::RegenerateHealth()
{
    if (!isRegeneratingHealth())
        return;

    uint32 curValue = GetHealth();
    uint32 maxValue = GetMaxHealth();

    if (curValue >= maxValue)
        return;

    uint32 addvalue = 0;

    // Not only pet, but any controlled creature (and not polymorphed)
    if(GetCharmerOrOwnerGUID() && !IsPolymorphed())
    {
        float HealthIncreaseRate = sWorld->GetRate(RATE_HEALTH);
        float Spirit = GetStat(STAT_SPIRIT);

        if( GetPower(POWER_MANA) > 0 )
            addvalue = uint32(Spirit * 0.25 * HealthIncreaseRate);
        else
            addvalue = uint32(Spirit * 0.80 * HealthIncreaseRate);
    }
    else
        addvalue = maxValue/3;

    ModifyHealth(addvalue);
}

bool Creature::AIM_Destroy()
{
    if (m_AI_locked)
    {
        TC_LOG_DEBUG("scripts", "AIM_Destroy: failed to destroy, locked.");
        return false;
    }

    ASSERT(!i_disabledAI,
        "The disabled AI wasn't cleared!");

    delete i_AI;
    i_AI = nullptr;

    IsAIEnabled = false;
    return true;
}

bool Creature::AIM_Initialize(CreatureAI* ai)
{
    // make sure nothing can change the AI during AI update
    if(m_AI_locked)
    {
        TC_LOG_ERROR("scripts","AIM_Initialize: failed to init for creature entry %u (DB GUID: %u), locked.", GetEntry(), GetSpawnId());
        return false;
    }

    Motion_Initialize();

    AIM_Destroy();

    i_AI = ai ? ai : FactorySelector::selectAI(this);
    IsAIEnabled = true;     // Keep this when getting rid of old system
    i_AI->InitializeAI();
    
#ifdef LICH_KING
    // Initialize vehicle
    if (GetVehicleKit())
        GetVehicleKit()->Reset();
#endif
    
    return true;
}

bool Creature::Create(uint32 guidlow, Map *map, uint32 phaseMask, uint32 entry, float x, float y, float z, float ang, const CreatureData *data)
{
	ASSERT(map);
	SetMap(map);
	SetPhaseMask(phaseMask, false);

    //m_spawnId = guidlow;

	CreatureTemplate const* cinfo = sObjectMgr->GetCreatureTemplate(entry);
	if (!cinfo)
	{
		TC_LOG_ERROR("sql.sql", "Creature::Create(): creature template (guidlow: %u, entry: %u) does not exist.", guidlow, entry);
		return false;
	}

	//! Relocate before CreateFromProto, to initialize coords and allow
	//! returning correct zone id for selecting OutdoorPvP/Battlefield script
	Relocate(x, y, z, ang);

	// Check if the position is valid before calling CreateFromProto(), otherwise we might add Auras to Creatures at
	// invalid position, triggering a crash about Auras not removed in the destructor
	if (!IsPositionValid())
	{
		TC_LOG_ERROR("entities.unit", "Creature::Create(): given coordinates for creature (guidlow %d, entry %d) are not valid (X: %f, Y: %f, Z: %f, O: %f)", guidlow, entry, x, y, z, ang);
		return false;
	}

    //oX = x;     oY = y;    dX = x;    dY = y;    m_moveTime = 0;    m_startMove = 0;
    if (!CreateFromProto(guidlow, entry, data))
        return false;

	// Allow players to see those units while dead, do it here (may be altered by addon auras)
	if (cinfo->type_flags & CREATURE_TYPE_FLAG_GHOST_VISIBLE)
		m_serverSideVisibility.SetValue(SERVERSIDE_VISIBILITY_GHOST, GHOST_VISIBILITY_ALIVE | GHOST_VISIBILITY_GHOST);

    /* TC, would be nice to have
    if (GetCreatureTemplate()->flags_extra & CREATURE_FLAG_EXTRA_DUNGEON_BOSS && map->IsDungeon())
        m_respawnDelay = 0; // special value, prevents respawn for dungeon bosses unless overridden
        */

    switch (GetCreatureTemplate()->rank)
    {
        case CREATURE_ELITE_RARE:
            m_corpseDelay = sWorld->getConfig(CONFIG_CORPSE_DECAY_RARE);
            break;
        case CREATURE_ELITE_ELITE:
            m_corpseDelay = sWorld->getConfig(CONFIG_CORPSE_DECAY_ELITE);
            break;
        case CREATURE_ELITE_RAREELITE:
            m_corpseDelay = sWorld->getConfig(CONFIG_CORPSE_DECAY_RAREELITE);
            break;
        case CREATURE_ELITE_WORLDBOSS:
            m_corpseDelay = sWorld->getConfig(CONFIG_CORPSE_DECAY_WORLDBOSS);
            break;
        default:
            m_corpseDelay = sWorld->getConfig(CONFIG_CORPSE_DECAY_NORMAL);
            break;
    }
    LoadCreatureAddon();
    InitCreatureAddon();

#ifdef LICH_KING
	//! Need to be called after LoadCreaturesAddon - MOVEMENTFLAG_HOVER is set there
	if (HasUnitMovementFlag(MOVEMENTFLAG_HOVER))
	{
		z += GetFloatValue(UNIT_FIELD_HOVERHEIGHT);

		//! Relocate again with updated Z coord
		Relocate(x, y, z, ang);
	}
#endif

	//LastUsedScriptID = GetScriptId(); Moved to InitEntry

	if (IsSpiritHealer() || IsSpiritGuide() || (GetCreatureTemplate()->flags_extra & CREATURE_FLAG_EXTRA_GHOST_VISIBILITY))
	{
		m_serverSideVisibility.SetValue(SERVERSIDE_VISIBILITY_GHOST, GHOST_VISIBILITY_GHOST);
		m_serverSideVisibilityDetect.SetValue(SERVERSIDE_VISIBILITY_GHOST, GHOST_VISIBILITY_GHOST);
	}

	/* TC
	if (GetCreatureTemplate()->flags_extra & CREATURE_FLAG_EXTRA_IGNORE_PATHFINDING)
		AddUnitState(UNIT_STATE_IGNORE_PATHFINDING);


	if (GetCreatureTemplate()->flags_extra & CREATURE_FLAG_EXTRA_IMMUNITY_KNOCKBACK)
	{
		ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
		ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK_DEST, true);
	}
	*/

    return true;
}

bool Creature::isTrainerFor(Player* pPlayer, bool msg) const
{
    if(!IsTrainer())
        return false;

    TrainerSpellData const* trainer_spells = GetTrainerSpells();

    if(!trainer_spells || trainer_spells->spellList.empty())
    {
        TC_LOG_ERROR("sql.sql","Creature %u (Entry: %u) have UNIT_NPC_FLAG_TRAINER but have empty trainer spell list.",
            GetGUIDLow(),GetEntry());
        return false;
    }

    switch(GetCreatureTemplate()->trainer_type)
    {
        case TRAINER_TYPE_CLASS:
            if(pPlayer->GetClass()!=GetCreatureTemplate()->trainer_class)
            {
                if(msg)
                {
                    pPlayer->PlayerTalkClass->ClearMenus();
                    switch(GetCreatureTemplate()->trainer_class)
                    {
                        case CLASS_DRUID:  pPlayer->PlayerTalkClass->SendGossipMenuTextID( 4913,GetGUID()); break;
                        case CLASS_HUNTER: pPlayer->PlayerTalkClass->SendGossipMenuTextID(10090,GetGUID()); break;
                        case CLASS_MAGE:   pPlayer->PlayerTalkClass->SendGossipMenuTextID(  328,GetGUID()); break;
                        case CLASS_PALADIN:pPlayer->PlayerTalkClass->SendGossipMenuTextID( 1635,GetGUID()); break;
                        case CLASS_PRIEST: pPlayer->PlayerTalkClass->SendGossipMenuTextID( 4436,GetGUID()); break;
                        case CLASS_ROGUE:  pPlayer->PlayerTalkClass->SendGossipMenuTextID( 4797,GetGUID()); break;
                        case CLASS_SHAMAN: pPlayer->PlayerTalkClass->SendGossipMenuTextID( 5003,GetGUID()); break;
                        case CLASS_WARLOCK:pPlayer->PlayerTalkClass->SendGossipMenuTextID( 5836,GetGUID()); break;
                        case CLASS_WARRIOR:pPlayer->PlayerTalkClass->SendGossipMenuTextID( 4985,GetGUID()); break;
                    }
                }
                return false;
            }
            break;
        case TRAINER_TYPE_PETS:
            if(pPlayer->GetClass()!=CLASS_HUNTER)
            {
                pPlayer->PlayerTalkClass->ClearMenus();
                pPlayer->PlayerTalkClass->SendGossipMenuTextID(3620,GetGUID());
                return false;
            }
            break;
        case TRAINER_TYPE_MOUNTS:
            if(GetCreatureTemplate()->trainer_race && pPlayer->GetRace() != GetCreatureTemplate()->trainer_race)
            {
                if(msg)
                {
                    pPlayer->PlayerTalkClass->ClearMenus();
                    switch(GetCreatureTemplate()->trainer_race)
                    {
                        case RACE_DWARF:        pPlayer->PlayerTalkClass->SendGossipMenuTextID(5865,GetGUID()); break;
                        case RACE_GNOME:        pPlayer->PlayerTalkClass->SendGossipMenuTextID(4881,GetGUID()); break;
                        case RACE_HUMAN:        pPlayer->PlayerTalkClass->SendGossipMenuTextID(5861,GetGUID()); break;
                        case RACE_NIGHTELF:     pPlayer->PlayerTalkClass->SendGossipMenuTextID(5862,GetGUID()); break;
                        case RACE_ORC:          pPlayer->PlayerTalkClass->SendGossipMenuTextID(5863,GetGUID()); break;
                        case RACE_TAUREN:       pPlayer->PlayerTalkClass->SendGossipMenuTextID(5864,GetGUID()); break;
                        case RACE_TROLL:        pPlayer->PlayerTalkClass->SendGossipMenuTextID(5816,GetGUID()); break;
                        case RACE_UNDEAD_PLAYER:pPlayer->PlayerTalkClass->SendGossipMenuTextID( 624,GetGUID()); break;
                        case RACE_BLOODELF:     pPlayer->PlayerTalkClass->SendGossipMenuTextID(5862,GetGUID()); break;
                        case RACE_DRAENEI:      pPlayer->PlayerTalkClass->SendGossipMenuTextID(5864,GetGUID()); break;
                    }
                }
                return false;
            }
            break;
        case TRAINER_TYPE_TRADESKILLS:
            if(GetCreatureTemplate()->trainer_spell && !pPlayer->HasSpell(GetCreatureTemplate()->trainer_spell))
            {
                if(msg)
                {
                    pPlayer->PlayerTalkClass->ClearMenus();
                    pPlayer->PlayerTalkClass->SendGossipMenuTextID(11031,GetGUID());
                }
                return false;
            }
            break;
        default:
            return false;                                   // checked and error output at creature_template loading
    }
    return true;
}

bool Creature::isCanInteractWithBattleMaster(Player* pPlayer, bool msg) const
{
    if(!IsBattleMaster())
        return false;

    uint32 bgTypeId = sObjectMgr->GetBattleMasterBG(GetEntry());
    if(!msg)
        return pPlayer->GetBGAccessByLevel(bgTypeId);

    if(!pPlayer->GetBGAccessByLevel(bgTypeId))
    {
        pPlayer->PlayerTalkClass->ClearMenus();
        switch(bgTypeId)
        {
            case BATTLEGROUND_AV:  pPlayer->PlayerTalkClass->SendGossipMenuTextID(7616,GetGUID()); break;
            case BATTLEGROUND_WS:  pPlayer->PlayerTalkClass->SendGossipMenuTextID(7599,GetGUID()); break;
            case BATTLEGROUND_AB:  pPlayer->PlayerTalkClass->SendGossipMenuTextID(7642,GetGUID()); break;
            case BATTLEGROUND_EY:
            case BATTLEGROUND_NA:
            case BATTLEGROUND_BE:
            case BATTLEGROUND_AA:
            case BATTLEGROUND_RL:  pPlayer->PlayerTalkClass->SendGossipMenuTextID(10024,GetGUID()); break;
            break;
        }
        return false;
    }
    return true;
}

bool Creature::canResetTalentsOf(Player* pPlayer) const
{
    return pPlayer->GetLevel() >= 10
        && GetCreatureTemplate()->trainer_type == TRAINER_TYPE_CLASS
        && pPlayer->GetClass() == GetCreatureTemplate()->trainer_class;
}

Player* Creature::GetLootRecipient() const
{
    if (!m_lootRecipient) 
        return nullptr;

    return ObjectAccessor::FindConnectedPlayer(m_lootRecipient);
}

Group* Creature::GetLootRecipientGroup() const
{
    if (!m_lootRecipient)
        return nullptr;

    return sObjectMgr->GetGroupByLeader(m_lootRecipientGroup);
}

// return true if this creature is tapped by the player or by a member of his group.
bool Creature::isTappedBy(Player const* player) const
{
    if (player->GetGUID() == m_lootRecipient)
        return true;

    Group const* playerGroup = player->GetGroup();
    if (!playerGroup || playerGroup != GetLootRecipientGroup()) // if we dont have a group we arent the recipient
        return false;                                           // if creature doesnt have group bound it means it was solo killed by someone else

    return true;
}

void Creature::SetLootRecipient(Unit *unit)
{
    // set the player whose group should receive the right
    // to loot the creature after it dies
    // should be set to NULL after the loot disappears

    if (!unit)
    {
        m_lootRecipient = 0;
        m_lootRecipientGroup = 0;
        RemoveFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_LOOTABLE|UNIT_DYNFLAG_TAPPED);
        return;
    }

    Player* player = unit->GetCharmerOrOwnerPlayerOrPlayerItself();
    if(!player)                                             // normal creature, no player involved
        return;

    m_lootRecipient = player->GetGUID();
    if (Group* group = player->GetGroup())
        m_lootRecipientGroup = group->GetLeaderGUID();

    SetFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_TAPPED);
}

void Creature::SaveToDB()
{
    // this should only be used when the creature has already been loaded
    // preferably after adding to map, because mapid may not be valid otherwise
    CreatureData const *data = sObjectMgr->GetCreatureData(m_spawnId);
    if(!data)
    {
        TC_LOG_ERROR("entities.unit","Creature::SaveToDB failed, cannot get creature data!");
        return;
    }

    SaveToDB(GetMapId(), data->spawnMask);
}

void Creature::SaveToDB(uint32 mapid, uint8 spawnMask)
{
    // update in loaded data
	if (!m_spawnId)
		m_spawnId = sObjectMgr->GenerateCreatureSpawnId();

    CreatureData& data = sObjectMgr->NewOrExistCreatureData(m_spawnId);

    uint32 displayId = GetNativeDisplayId();

    CreatureTemplate const *cinfo = GetCreatureTemplate();
    if(cinfo)
    {
        // check if it's a custom model and if not, use 0 for displayId
        if(displayId == cinfo->Modelid1 || displayId == cinfo->Modelid2 ||
            displayId == cinfo->Modelid3 || displayId == cinfo->Modelid4) displayId = 0;
    }

    // data->guid = guid must not be update at save
    data.id = GetEntry();
    data.mapid = mapid;
    data.displayid = displayId;
    data.equipmentId = GetEquipmentId();
    if(!GetTransport())
        GetPosition(data.posX,data.posY,data.posZ,data.orientation);
    else {
        data.posX = GetTransOffsetX();
        data.posY = GetTransOffsetY();
        data.posZ = GetTransOffsetZ();
        data.orientation = GetTransOffsetO();
    }
    data.spawntimesecs = m_respawnDelay;
    // prevent add data integrity problems
    data.spawndist = GetDefaultMovementType()==IDLE_MOTION_TYPE ? 0 : m_respawnradius;
    data.currentwaypoint = 0;
    data.curhealth = GetHealth();
    data.curmana = GetPower(POWER_MANA);
    // prevent add data integrity problems
    data.movementType = !m_respawnradius && GetDefaultMovementType()==RANDOM_MOTION_TYPE
        ? IDLE_MOTION_TYPE : GetDefaultMovementType();
    data.spawnMask = spawnMask;
    data.poolId = m_creaturePoolId;

    // updated in DB
    SQLTransaction trans = WorldDatabase.BeginTransaction();

    trans->PAppend("DELETE FROM creature WHERE guid = '%u'", m_spawnId);

    std::ostringstream ss;
    ss << "INSERT INTO creature (guid,id,map,spawnMask,modelid,equipment_id,position_x,position_y,position_z,orientation,spawntimesecs,spawndist,currentwaypoint,curhealth,curmana,MovementType, pool_id) VALUES ("
        << m_spawnId << ","
        << GetEntry() << ","
        << mapid <<","
        << (uint32)spawnMask << ","
        << displayId <<","
        << GetEquipmentId() <<","
        << data.posX << ","
        << data.posY << ","
        << data.posZ << ","
        << data.orientation << ","
        << m_respawnDelay << ","                            //respawn time
        << (float) m_respawnradius << ","                   //spawn distance (float)
        << (uint32) (0) << ","                              //currentwaypoint
        << GetHealth() << ","                               //curhealth
        << GetPower(POWER_MANA) << ","                      //curmana
        << GetDefaultMovementType() << ","                  //default movement generator type
        << m_creaturePoolId << ")";                          //creature pool id

    trans->Append( ss.str( ).c_str( ) );

    WorldDatabase.CommitTransaction(trans);
}

void Creature::UpdateLevelDependantStats()
{
    CreatureTemplate const* cInfo = GetCreatureTemplate();
    uint32 rank = IsPet() ? 0 : cInfo->rank;
    CreatureBaseStats const* stats = sObjectMgr->GetCreatureBaseStats(GetLevel(), cInfo->unit_class);


    // health
    uint32 health = stats->GenerateHealth(cInfo);

    SetCreateHealth(health);
    SetMaxHealth(health);
    SetHealth(health);
    ResetPlayerDamageReq();

    // mana
    uint32 mana = stats->GenerateMana(cInfo);

    SetCreateMana(mana);
    SetMaxPower(POWER_MANA, mana); // MAX Mana
    SetPower(POWER_MANA, mana);

    /// @todo set UNIT_FIELD_POWER*, for some creature class case (energy, etc)

    SetStatFlatModifier(UNIT_MOD_HEALTH, BASE_VALUE, (float)health);
    SetStatFlatModifier(UNIT_MOD_MANA, BASE_VALUE, (float)mana);

    // damage

    float basedamage = stats->GenerateBaseDamage(cInfo);

    float weaponBaseMinDamage = basedamage;
    float weaponBaseMaxDamage = basedamage * (1.0f + GetCreatureTemplate()->BaseVariance);

    SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, weaponBaseMinDamage);
    SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, weaponBaseMaxDamage);

    SetBaseWeaponDamage(OFF_ATTACK, MINDAMAGE, weaponBaseMinDamage);
    SetBaseWeaponDamage(OFF_ATTACK, MAXDAMAGE, weaponBaseMaxDamage);

    SetBaseWeaponDamage(RANGED_ATTACK, MINDAMAGE, weaponBaseMinDamage);
    SetBaseWeaponDamage(RANGED_ATTACK, MAXDAMAGE, weaponBaseMaxDamage);

    SetStatFlatModifier(UNIT_MOD_ATTACK_POWER, BASE_VALUE, stats->AttackPower);
    SetStatFlatModifier(UNIT_MOD_ATTACK_POWER_RANGED, BASE_VALUE, stats->RangedAttackPower);

    float armor = (float)stats->GenerateArmor(cInfo); /// @todo Why is this treated as uint32 when it's a float?
    SetStatFlatModifier(UNIT_MOD_ARMOR, BASE_VALUE, armor);
}

void Creature::SelectLevel()
{
    const CreatureTemplate* cInfo = GetCreatureTemplate();
    if(!cInfo)
        return;

    // level
    uint32 minlevel = std::min(cInfo->maxlevel, cInfo->minlevel);
    uint32 maxlevel = std::max(cInfo->maxlevel, cInfo->minlevel);
    uint32 level = minlevel == maxlevel ? minlevel : urand(minlevel, maxlevel);
    SetLevel(level);

    UpdateLevelDependantStats();
}

bool Creature::CreateFromProto(uint32 guidlow, uint32 Entry, const CreatureData *data)
{
    CreatureTemplate const *cinfo = sObjectMgr->GetCreatureTemplate(Entry);
    if(!cinfo)
    {
        TC_LOG_ERROR("FIXME","Error: creature entry %u does not exist.", Entry);
        return false;
    }
    m_originalEntry = Entry;

    Object::_Create(guidlow, Entry, HighGuid::Unit);

    if(!UpdateEntry(Entry, data))
        return false;

    //Notify the map's instance data.
    //Only works if you create the object in it, not if it is moves to that map.
    //Normally non-players do not teleport to other maps.
    Map *map = FindMap();
    if(map && map->IsDungeon() && ((InstanceMap*)map)->GetInstanceScript())
    {
        // Workaround, I need position_x in OnCreatureCreate for Felmyst. I'll rewrite the hook with data as third parameter later
        if (data)
            m_positionX = data->posX;
        ((InstanceMap*)map)->GetInstanceScript()->OnCreatureCreate(this);
    }

    return true;
}

bool Creature::LoadCreatureFromDB(uint32 spawnId, Map *map, bool addToMap, bool allowDuplicate)
{
    if (!allowDuplicate)
    {
        // If an alive instance of this spawnId is already found, skip creation
        // If only dead instance(s) exist, despawn them and spawn a new (maybe also dead) version
        const auto creatureBounds = map->GetCreatureBySpawnIdStore().equal_range(spawnId);
        std::vector <Creature*> despawnList;

        if (creatureBounds.first != creatureBounds.second)
        {
            for (auto itr = creatureBounds.first; itr != creatureBounds.second; ++itr)
            {
                if (itr->second->IsAlive())
                {
                    TC_LOG_DEBUG("maps", "Would have spawned %u but it already exists", spawnId);
                    return false;
                }
                else
                {
                    despawnList.push_back(itr->second);
                    TC_LOG_DEBUG("maps", "Despawned dead instance of spawn %u", spawnId);
                }
            }

            for (Creature* despawnCreature : despawnList)
                despawnCreature->AddObjectToRemoveList();
        }
    }

    CreatureData const* data = sObjectMgr->GetCreatureData(spawnId);

    if(!data)
    {
        TC_LOG_ERROR("FIXME","Creature (GUID: %u) not found in table `creature`, can't load. ", spawnId);
        return false;
    }
    
    // Rare creatures in dungeons have 15% chance to spawn
    CreatureTemplate const *cinfo = sObjectMgr->GetCreatureTemplate(data->id);
    if (cinfo && map->GetInstanceId() != 0 && (cinfo->rank == CREATURE_ELITE_RAREELITE || cinfo->rank == CREATURE_ELITE_RARE)) {
        if (rand()%5 != 0)
            return false;
    }

    m_spawnId = spawnId;
    m_creatureData = data;

    m_respawnradius = data->spawndist;
    m_respawnDelay = data->spawntimesecs;
    if(!Create(map->GenerateLowGuid<HighGuid::Unit>(), map, PHASEMASK_NORMAL /*data->phaseMask*/, data->id, data->posX, data->posY, data->posZ, data->orientation, data))
        return false;

    if(!IsPositionValid())
    {
        TC_LOG_ERROR("FIXME","ERROR: Creature (guidlow %d, entry %d) not loaded. Suggested coordinates isn't valid (X: %f Y: %f)",GetGUIDLow(),GetEntry(),GetPositionX(),GetPositionY());
        return false;
    }
    //We should set first home position, because then AI calls home movement
    SetHomePosition(data->posX,data->posY,data->posZ,data->orientation);

    m_deathState = ALIVE;

    m_respawnTime  = sObjectMgr->GetCreatureRespawnTime(m_spawnId,GetInstanceId());
    if(m_respawnTime)                          // respawn on Update
    {
        m_deathState = DEAD;
        if(CanFly())
        {
            float tz = GetMap()->GetHeight(data->posX,data->posY,data->posZ,false);
            if(data->posZ - tz > 0.1)
                Relocate(data->posX,data->posY,tz);
        }
    }

    SetSpawnHealth();

    // checked at creature_template loading
    m_defaultMovementType = MovementGeneratorType(data->movementType);

    return true;
}


void Creature::LoadEquipment(uint32 equip_entry, bool force)
{
    if(equip_entry == 0)
    {
        if (force)
        {
            for (uint8 i = WEAPON_SLOT_MAINHAND; i <= WEAPON_SLOT_RANGED; i++)
            {
                SetUInt32Value( UNIT_VIRTUAL_ITEM_SLOT_DISPLAY + i, 0);
                SetUInt32Value( UNIT_VIRTUAL_ITEM_INFO + (i * 2), 0);
                SetUInt32Value( UNIT_VIRTUAL_ITEM_INFO + (i * 2) + 1, 0);
            }
            m_equipmentId = 0;
        }
        return;
    }

    EquipmentInfo const *einfo = sObjectMgr->GetEquipmentInfo(equip_entry);
    if (!einfo)
        return;

    m_equipmentId = equip_entry;
    for (uint8 i = WEAPON_SLOT_MAINHAND; i <= WEAPON_SLOT_RANGED; i++)
    {
        SetUInt32Value( UNIT_VIRTUAL_ITEM_SLOT_DISPLAY + i, einfo->equipmodel[i]);
        SetUInt32Value( UNIT_VIRTUAL_ITEM_INFO + (i * 2), einfo->equipinfo[i]);
        SetUInt32Value( UNIT_VIRTUAL_ITEM_INFO + (i * 2) + 1, einfo->equipslot[i]);
    }
}

void Creature::SetSpawnHealth()
{
    uint32 curhealth;
    if (m_creatureData && !m_regenHealth)
    {
        curhealth = m_creatureData->curhealth;
        if (curhealth)
        {
            curhealth = uint32(curhealth /* * _GetHealthMod(GetCreatureTemplate()->rank)*/);
            if (curhealth < 1)
                curhealth = 1;
        }
        SetPower(POWER_MANA, m_creatureData->curmana);
    }
    else
    {
        curhealth = GetMaxHealth();
        SetPower(POWER_MANA, GetMaxPower(POWER_MANA));
    }

    SetHealth((m_deathState == ALIVE || m_deathState == JUST_RESPAWNED) ? curhealth : 0);
}

void Creature::SetWeapon(WeaponSlot slot, uint32 displayid, ItemSubclassWeapon subclass, InventoryType inventoryType)
{
    SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_DISPLAY+slot, displayid);
    SetUInt32Value(UNIT_VIRTUAL_ITEM_INFO + slot*2, ITEM_CLASS_WEAPON + (subclass << 8));
    SetUInt32Value(UNIT_VIRTUAL_ITEM_INFO + (slot*2)+1, inventoryType);
}

ItemSubclassWeapon Creature::GetWeaponSubclass(WeaponSlot slot)
{
    uint32 itemInfo = GetUInt32Value(UNIT_VIRTUAL_ITEM_INFO + slot * 2);
    ItemSubclassWeapon subclass = ItemSubclassWeapon((itemInfo & 0xFF00) >> 8);
    if (subclass > ITEM_SUBCLASS_WEAPON_FISHING_POLE)
        TC_LOG_ERROR("entities.creature", "Creature (table guid %u) appears to have broken weapon info for slot %u", GetSpawnId(), uint32(slot));

    return subclass;
}

bool Creature::HasMainWeapon() const
{
#ifdef LICH_KING
    return GetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_ID);
#else
    return GetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_DISPLAY);
#endif
}

bool Creature::HasQuest(uint32 quest_id) const
{
    QuestRelations const& qr = sObjectMgr->mCreatureQuestRelations;
    for(auto itr = qr.lower_bound(GetEntry()); itr != qr.upper_bound(GetEntry()); ++itr)
    {
        if(itr->second==quest_id)
            return true;
    }
    return false;
}

bool Creature::HasInvolvedQuest(uint32 quest_id) const
{
    QuestRelations const& qr = sObjectMgr->mCreatureQuestInvolvedRelations;
    for(auto itr = qr.lower_bound(GetEntry()); itr != qr.upper_bound(GetEntry()); ++itr)
    {
        if(itr->second==quest_id)
            return true;
    }
    return false;
}

void Creature::DeleteFromDB()
{
    if (!m_spawnId)
        return;

    sObjectMgr->SaveCreatureRespawnTime(m_spawnId, GetMapId(), GetInstanceId(),0);
    sObjectMgr->DeleteCreatureData(m_spawnId);

    SQLTransaction trans = WorldDatabase.BeginTransaction();
    trans->PAppend("DELETE FROM creature WHERE guid = '%u'", m_spawnId);
    trans->PAppend("DELETE FROM creature_addon WHERE guid = '%u'", m_spawnId);
    trans->PAppend("DELETE FROM game_event_creature WHERE guid = '%u'", m_spawnId);
    trans->PAppend("DELETE FROM game_event_model_equip WHERE guid = '%u'", m_spawnId);
    WorldDatabase.CommitTransaction(trans);
}

/*
bool Creature::CanSeeOrDetect(Unit const* u, bool detect, bool inVisibleList, bool is3dDistance) const
{
    // not in world
    if(!IsInWorld() || !u->IsInWorld())
        return false;

    // all dead creatures/players not visible for any creatures
    if(!u->IsAlive() || !IsAlive())
        return false;

    // Always can see self
    if (u == this)
        return true;

    // always seen by owner
    if(GetGUID() == u->GetCharmerOrOwnerGUID())
        return true;

    if(u->GetVisibility() == VISIBILITY_OFF) //GM
        return false;

    // invisible aura
    if((m_invisibilityMask || u->m_invisibilityMask) && !CanDetectInvisibilityOf(u))
        return false;

    // GM invisibility checks early, invisibility if any detectable, so if not stealth then visible
    if(u->GetVisibility() == VISIBILITY_GROUP_STEALTH)
    {
        //do not know what is the use of this detect
        if(!detect || CanDetectStealthOf(u, GetDistance(u) != DETECTED_STATUS_DETECTED))
            return false;
    }

    return true;
}
*/

bool Creature::IsWithinSightDist(Unit const* u) const
{
    return IsWithinDistInMap(u, sWorld->getConfig(CONFIG_SIGHT_MONSTER));
}

/**
Hostile target is in stealth and in warn range
*/
void Creature::StartStealthAlert(Unit const* target)
{
    m_stealthAlertCooldown = STEALTH_ALERT_COOLDOWN;

    GetMotionMaster()->MoveStealthAlert(target, STEALTH_ALERT_DURATINON);
    SendAIReaction(AI_REACTION_ALERT);
}

bool Creature::CanDoStealthAlert(Unit const* target) const
{
    if(   IsInCombat()
       || IsWorldBoss()
       || IsTotem()
      )
        return false;

    // cooldown not ready
    if(m_stealthAlertCooldown > 0)
        return false;

    // If this unit isn't an NPC, is already distracted, is in combat, is confused, stunned or fleeing, do nothing
    if (HasUnitState(UNIT_STATE_CONFUSED | UNIT_STATE_STUNNED | UNIT_STATE_FLEEING | UNIT_STATE_DISTRACTED)
        || IsCivilian() || HasReactState(REACT_PASSIVE) || !IsHostileTo(target)
       )
        return false;

    //target is not a player
    if(target->GetTypeId() != TYPEID_PLAYER)
        return false;

    //target not stealthed
	if(CanSeeOrDetect(target, false, true, true))
        return false;

    //only with those movement generators, we don't want to start warning when fleeing, chasing, ...
    MovementGeneratorType currentMovementType = GetMotionMaster()->GetCurrentMovementGeneratorType();
    if(currentMovementType != IDLE_MOTION_TYPE
       && currentMovementType != RANDOM_MOTION_TYPE
       && currentMovementType != WAYPOINT_MOTION_TYPE)
       return false;

    return true;
}

bool Creature::IsInvisibleDueToDespawn() const
{
	if (Unit::IsInvisibleDueToDespawn())
		return true;

	if (IsAlive() || IsDying() || m_corpseRemoveTime > time(NULL))
		return false;

	return true;
}

bool Creature::CanAlwaysSee(WorldObject const* obj) const
{
	if (IsAIEnabled && AI()->CanSeeAlways(obj))
		return true;

	return false;
}

//Trinity CanAttackStart // Sunwell CanCreatureAttack
CanAttackResult Creature::CanAggro(Unit const* who, bool assistAggro /* = false */) const
{
    if (!who->IsInMap(this))
        return CAN_ATTACK_RESULT_OTHER_MAP;

    if(IsInEvadeMode())
        return CAN_ATTACK_RESULT_SELF_EVADE;

    if(IsCivilian())
        return CAN_ATTACK_RESULT_CIVILIAN;

    // This set of checks is should be done only for creatures
    if ((HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_NPC) && who->GetTypeId() != TYPEID_PLAYER)                                   // flag is valid only for non player characters
        || (HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_PC) && who->GetTypeId() == TYPEID_PLAYER)                                 // immune to PC and target is a player, return false
        || (who->GetOwner() && who->GetOwner()->GetTypeId() == TYPEID_PLAYER && HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_PC))) // player pets are immune to pc as well
        return CAN_ATTACK_RESULT_IMMUNE_TO_TARGET;

    // Do not attack non-combat pets
    if (who->GetTypeId() == TYPEID_UNIT && who->GetCreatureType() == CREATURE_TYPE_NON_COMBAT_PET)
        return CAN_ATTACK_RESULT_OTHERS;

    if(Creature const* c = who->ToCreature())
    {
        if(c->IsInEvadeMode())
            return CAN_ATTACK_RESULT_TARGET_EVADE;
    }

    if(!CanFly() && GetDistanceZ(who) > CREATURE_Z_ATTACK_RANGE + m_CombatDistance)
        return CAN_ATTACK_RESULT_TOO_FAR_Z;

    if(assistAggro)
    {
        if(!IsWithinSightDist(who))
            return CAN_ATTACK_RESULT_TOO_FAR;
    } else {
        if(!IsWithinDistInMap(who, GetAggroRange(who) + m_CombatDistance)) //m_CombatDistance is usually 0 for melee. Ranged creatures will aggro from further, is this correct?
            return CAN_ATTACK_RESULT_TOO_FAR;
    }

    CanAttackResult result = CanAttack(who, false);
    if(result != CAN_ATTACK_RESULT_OK)
        return result;

    //ignore LoS for assist
    if(!assistAggro && !IsWithinLOSInMap(who))
        return CAN_ATTACK_RESULT_NOT_IN_LOS;

    if (!who->isInAccessiblePlaceFor(this))
        return CAN_ATTACK_RESULT_NOT_ACCESSIBLE;

    return CAN_ATTACK_RESULT_OK;
}

float Creature::GetAggroRange(Unit const* pl) const
{
    float aggroRate = sWorld->GetRate(RATE_CREATURE_AGGRO);
    if(aggroRate==0)
        return 0.0f;

    int32 playerlevel   = pl->GetLevelForTarget(this);
    int32 creaturelevel = GetLevelForTarget(pl);

    int32 leveldif       = playerlevel - creaturelevel;

    // "The maximum Aggro Radius has a cap of 25 levels under. Example: A level 30 char has the same Aggro Radius of a level 5 char on a level 60 mob."
    if ( leveldif < - 25)
        leveldif = -25;

    // "The aggro radius of a mob having the same level as the player is roughly 20 yards"
    float RetDistance = 20;

    // "Aggro Radius varies with level difference at a rate of roughly 1 yard/level"
    // radius grow if playlevel < creaturelevel
    RetDistance -= (float)leveldif;

	if (creaturelevel + 5 <= sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL))
	{
		// detect range auras
		RetDistance += GetTotalAuraModifier(SPELL_AURA_MOD_STEALTH_DETECT_RANGE);

		// detected range auras
		RetDistance += pl->GetTotalAuraModifier(SPELL_AURA_MOD_STEALTH_DETECTED_RANGE);
	}

    //minimal distance
    if(RetDistance < 3.0f)
        RetDistance = 3.0f;

    return (RetDistance * aggroRate);
}

void Creature::SetDeathState(DeathState s)
{
    Unit::SetDeathState(s);
    if (s == JUST_DIED)
    {
        m_corpseRemoveTime = time(nullptr) + m_corpseDelay;
        m_respawnTime = time(nullptr) + m_respawnDelay + m_corpseDelay;
        /* TC
        if (IsDungeonBoss() && !m_respawnDelay)
            m_respawnTime = std::numeric_limits<time_t>::max(); // never respawn in this instance
        else
            m_respawnTime = time(NULL) + m_respawnDelay + m_corpseDelay;
        */
        // always save boss respawn time at death to prevent crash cheating
        if(sWorld->getConfig(CONFIG_SAVE_RESPAWN_TIME_IMMEDIATELY) || IsWorldBoss())
            SaveRespawnTime();
            
        Map *map = FindMap();
        if(map && map->IsDungeon() && ((InstanceMap*)map)->GetInstanceScript())
            ((InstanceMap*)map)->GetInstanceScript()->OnCreatureDeath(this);

        SetUInt64Value (UNIT_FIELD_TARGET,0);               // remove target selection in any cases (can be set at aura remove in Unit::setDeathState)
        SetUInt32Value(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_NONE);

        SetUInt32Value(UNIT_FIELD_MOUNTDISPLAYID, 0);       // if creature is mounted on a virtual mount, remove it at death

        //if(!IsPet())
            SetKeepActive(false);

        if(!IsPet() && GetCreatureTemplate()->SkinLootId)
            if ( LootTemplates_Skinning.HaveLootFor(GetCreatureTemplate()->SkinLootId) )
                SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SKINNABLE);

        //Dismiss group if is leader
        if (m_formation && m_formation->getLeader() == this)
            m_formation->FormationReset(true);

        if ((CanFly() || IsFlying()) && !HasUnitMovementFlag(MOVEMENTFLAG_SWIMMING))
            GetMotionMaster()->MoveFall();

        Unit::SetDeathState(CORPSE);
    }
    if(s == JUST_RESPAWNED)
    {
        if (IsPet())
            SetFullHealth();
        else
            SetSpawnHealth();

        SetLootRecipient(nullptr);
        ResetPlayerDamageReq();

		SetCannotReachTarget(false);
        UpdateMovementFlags();

        RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SKINNABLE);
        AddUnitMovementFlag(MOVEMENTFLAG_WALKING);
        SetUInt32Value(UNIT_NPC_FLAGS, GetCreatureTemplate()->npcflag);
        ClearUnitState(uint32(UNIT_STATE_ALL_STATE & ~UNIT_STATE_IGNORE_PATHFINDING));
        Motion_Initialize();
        SetMeleeDamageSchool(SpellSchools(GetCreatureTemplate()->dmgschool));
        Unit::SetDeathState(ALIVE);
        InitCreatureAddon(true);

		/* TC
		if (creatureData && GetPhaseMask() != creatureData->phaseMask)
			SetPhaseMask(creatureData->phaseMask, false);
			*/
    }
}

void Creature::Respawn(bool force /* = false */)
{
	DestroyForNearbyPlayers();

    if (force)
    {
        if (IsAlive())
            SetDeathState(JUST_DIED);
        else if (GetDeathState() != CORPSE)
            SetDeathState(CORPSE);
    }

    RemoveCorpse(false, false);

    if(!IsInWorld())
        AddToWorld();

    this->loot.ClearRemovedItemsList();

    if(GetDeathState()==DEAD)
    {
        if (m_spawnId)
            sObjectMgr->SaveCreatureRespawnTime(m_spawnId,GetMapId(), GetInstanceId(),0);

        TC_LOG_DEBUG("entities.creature","Respawning...");
        m_respawnTime = 0;
        lootForPickPocketed = false;
        lootForBody         = false;

        if(m_originalEntry != GetEntry())
            UpdateEntry(m_originalEntry);

        SelectLevel();

        SetDeathState( JUST_RESPAWNED );

        GetMotionMaster()->InitDefault();

        //re rand level & model
        SelectLevel();

        uint32 displayID = GetNativeDisplayId();
        CreatureModelInfo const* minfo = sObjectMgr->GetCreatureModelRandomGender(displayID);
        if (minfo && !IsTotem())                               // Cancel load if no model defined or if totem
        {
            SetNativeDisplayId(displayID);

            Unit::AuraEffectList const& transformAuras = GetAuraEffectsByType(SPELL_AURA_TRANSFORM);
            Unit::AuraEffectList const& shapeshiftAuras = GetAuraEffectsByType(SPELL_AURA_MOD_SHAPESHIFT);
            if (transformAuras.empty() && shapeshiftAuras.empty())
                SetDisplayId(displayID);
        }
    }

    //Call AI respawn virtual function
    if (IsAIEnabled)
        TriggerJustRespawned = true;//delay event to next tick so all creatures are created on the map before processing

    m_timeSinceSpawn = 0;

	UpdateObjectVisibility();
}

void Creature::DespawnOrUnsummon(uint32 msTimeToDespawn /*= 0*/)
{
    if (TempSummon* summon = this->ToTempSummon())
        summon->UnSummon(msTimeToDespawn); 
    else
        ForcedDespawn(msTimeToDespawn);
}

void Creature::ForcedDespawn(uint32 timeMSToDespawn)
{
    if (timeMSToDespawn)
    {
        auto  pEvent = new ForcedDespawnDelayEvent(*this);

        m_Events.AddEvent(pEvent, m_Events.CalculateTime(timeMSToDespawn));
        return;
    }

    // do it before killing creature
    DestroyForNearbyPlayers();

    if (IsAlive())
        SetDeathState(JUST_DIED);

    RemoveCorpse(false, false);
}

bool Creature::IsImmunedToSpell(SpellInfo const* spellInfo, bool useCharges)
{
    if (!spellInfo)
        return false;

    if(spellInfo->Mechanic)
        if (GetCreatureTemplate()->MechanicImmuneMask & (1 << (spellInfo->Mechanic - 1)))
            return true;

    return Unit::IsImmunedToSpell(spellInfo, useCharges);
}

bool Creature::IsImmunedToSpellEffect(SpellInfo const* spellInfo, uint32 index) const
{
    if (GetCreatureTemplate()->MechanicImmuneMask & (1 << (spellInfo->Effects[index].Mechanic - 1)))
        return true;

    if (GetCreatureTemplate()->type == CREATURE_TYPE_MECHANICAL && spellInfo->Effects[index].Effect == SPELL_EFFECT_HEAL)
        return true;

    return Unit::IsImmunedToSpellEffect(spellInfo, index);
}

void Creature::ProhibitSpellSchool(SpellSchoolMask idSchoolMask, uint32 unTimeMs)
{
    if (idSchoolMask & SPELL_SCHOOL_MASK_NORMAL)
        m_prohibitedSchools[SPELL_SCHOOL_NORMAL] = unTimeMs;
    if (idSchoolMask & SPELL_SCHOOL_MASK_HOLY)
        m_prohibitedSchools[SPELL_SCHOOL_HOLY] = unTimeMs;
    if (idSchoolMask & SPELL_SCHOOL_MASK_FIRE)
        m_prohibitedSchools[SPELL_SCHOOL_FIRE] = unTimeMs;
    if (idSchoolMask & SPELL_SCHOOL_MASK_NATURE)
        m_prohibitedSchools[SPELL_SCHOOL_NATURE] = unTimeMs;
    if (idSchoolMask & SPELL_SCHOOL_MASK_FROST)
        m_prohibitedSchools[SPELL_SCHOOL_FROST] = unTimeMs;
    if (idSchoolMask & SPELL_SCHOOL_MASK_SHADOW)
        m_prohibitedSchools[SPELL_SCHOOL_ARCANE] = unTimeMs;
    if (idSchoolMask & SPELL_SCHOOL_MASK_ARCANE)
        m_prohibitedSchools[SPELL_SCHOOL_ARCANE] = unTimeMs;
}

void Creature::UpdateProhibitedSchools(uint32 const diff)
{
    for (uint32 & m_prohibitedSchool : m_prohibitedSchools) {
        if (m_prohibitedSchool == 0)
            continue;

        if (m_prohibitedSchool <= diff) {
            m_prohibitedSchool = 0;
            continue;
        }

        m_prohibitedSchool -= diff;
    }
}

bool Creature::IsSpellSchoolMaskProhibited(SpellSchoolMask idSchoolMask)
{
    bool prohibited = false;

    for (int i = 0; i < MAX_SPELL_SCHOOL; i++) {
        if ((idSchoolMask & (1 << i)) && m_prohibitedSchools[i] > 0) {
            prohibited = true;
            break;
        }
    }

    return prohibited;
}

SpellInfo const *Creature::reachWithSpellAttack(Unit *pVictim)
{
    if(!pVictim)
        return nullptr;

    for(uint32 m_spell : m_spells)
    {
        if(!m_spell)
            continue;
        SpellInfo const *spellInfo = sSpellMgr->GetSpellInfo(m_spell );
        if(!spellInfo)
        {
            TC_LOG_ERROR("FIXME","WORLD: unknown spell id %i\n", m_spell);
            continue;
        }

        bool bcontinue = true;
        for(const auto & Effect : spellInfo->Effects)
        {
            if( (Effect.Effect == SPELL_EFFECT_SCHOOL_DAMAGE )       ||
                (Effect.Effect == SPELL_EFFECT_INSTAKILL)            ||
                (Effect.Effect == SPELL_EFFECT_ENVIRONMENTAL_DAMAGE) ||
                (Effect.Effect == SPELL_EFFECT_HEALTH_LEECH )
                )
            {
                bcontinue = false;
                break;
            }
        }
        if(bcontinue) continue;

        if(spellInfo->ManaCost > GetPower(POWER_MANA))
            continue;
        float range = spellInfo->GetMaxRange(false, this);
        float minrange = spellInfo->GetMinRange(false);
        float dist = GetDistance(pVictim);
        //if(!isInFront( pVictim, range ) && spellInfo->AttributesEx )
        //    continue;
        if( dist > range || dist < minrange )
            continue;
        if(HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SILENCED))
            continue;
        return spellInfo;
    }
    return nullptr;
}

SpellInfo const *Creature::reachWithSpellCure(Unit *pVictim)
{
    if(!pVictim)
        return nullptr;

    for(uint32 m_spell : m_spells)
    {
        if(!m_spell)
            continue;
        SpellInfo const *spellInfo = sSpellMgr->GetSpellInfo(m_spell );
        if(!spellInfo)
        {
            TC_LOG_ERROR("FIXME","WORLD: unknown spell id %i\n", m_spell);
            continue;
        }

        bool bcontinue = true;
        for(const auto & Effect : spellInfo->Effects)
        {
            if( (Effect.Effect == SPELL_EFFECT_HEAL ) )
            {
                bcontinue = false;
                break;
            }
        }
        if(bcontinue) continue;

        if(spellInfo->ManaCost > GetPower(POWER_MANA))
            continue;
        float range = spellInfo->GetMaxRange(true, this);
        float minrange = spellInfo->GetMinRange();
        float dist = GetDistance(pVictim);
        //if(!isInFront( pVictim, range ) && spellInfo->AttributesEx )
        //    continue;
        if( dist > range || dist < minrange )
            continue;
        if(HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SILENCED))
            continue;
        return spellInfo;
    }
    return nullptr;
}

/*
bool Creature::IsVisibleInGridForPlayer(Player const* pl) const
{
    // gamemaster in GM mode see all, including ghosts
    if(pl->IsGameMaster() || pl->isSpectator())
        return true;

    // CREATURE_FLAG_EXTRA_GHOST_VISIBILITY handling
    if(GetCreatureTemplate()->flags_extra & CREATURE_FLAG_EXTRA_GHOST_VISIBILITY)
        return pl->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST);

    // Live player (or with not release body see live creatures or death creatures with corpse disappearing time > 0
    if(pl->IsAlive() || pl->GetDeathTimer() > 0)
    {
        if( GetEntry() == VISUAL_WAYPOINT && !pl->IsGameMaster() )
            return false;
        return IsAlive() || m_corpseRemoveTime > time(nullptr);
    }

    // Dead player see creatures near own corpse
    Corpse *corpse = pl->GetCorpse();
    if (corpse) {
        // 20 - aggro distance for same level, 25 - max additional distance if player level less that creature level
        if (corpse->IsWithinDistInMap(this,(20+25)*sWorld->GetRate(RATE_CREATURE_AGGRO)))
            return true;
    }

    // Dead player see Spirit Healer or Spirit Guide
    if(IsSpiritService())
        return true;

    // and not see any other
    return false;
}
*/

void Creature::DoFleeToGetAssistance(float radius) // Optional parameter
{
    if (!GetVictim())
        return;
        
    if (HasUnitState(UNIT_STATE_STUNNED))
        return;

    if (HasAuraType(SPELL_AURA_PREVENTS_FLEEING))
        return;

    if (radius <= 0)
        return;
        
    if (IsNonMeleeSpellCast(false))
        InterruptNonMeleeSpells(true);

    Creature* pCreature = nullptr;

    Trinity::NearestAssistCreatureInCreatureRangeCheck u_check(this,GetVictim(),radius);
    Trinity::CreatureLastSearcher<Trinity::NearestAssistCreatureInCreatureRangeCheck> searcher(pCreature, u_check);
    VisitNearbyGridObject(radius, searcher);

    if(!pCreature)
        SetControlled(true, UNIT_STATE_FLEEING);
    else
        GetMotionMaster()->MoveSeekAssistance(pCreature->GetPositionX(), pCreature->GetPositionY(), pCreature->GetPositionZ());
}

Unit* Creature::SelectNearestTarget(float dist, bool playerOnly /* = false */, bool furthest /* = false */) const
{
    Unit *target = nullptr;

    {
        if (dist == 0.0f)
            dist = MAX_SEARCHER_DISTANCE;

        Trinity::NearestHostileUnitInAggroRangeCheck u_check(this, dist, playerOnly, furthest);
        Trinity::UnitLastSearcher<Trinity::NearestHostileUnitInAggroRangeCheck> searcher(this, target, u_check);
        VisitNearbyObject(dist, searcher);
    }

    return target;
}


// select nearest hostile unit within the given attack distance (i.e. distance is ignored if > than ATTACK_DISTANCE), regardless of threat list.
Unit* Creature::SelectNearestTargetInAttackDistance(float dist) const
{
    CellCoord p(Trinity::ComputeCellCoord(GetPositionX(), GetPositionY()));
    Cell cell(p);
    cell.SetNoCreate();

    Unit* target = nullptr;

    if (dist < ATTACK_DISTANCE)
        dist = ATTACK_DISTANCE;
    if (dist > MAX_SEARCHER_DISTANCE)
        dist = MAX_SEARCHER_DISTANCE;

    {
        Trinity::NearestHostileUnitInAggroRangeCheck u_check(this, dist);
        Trinity::UnitLastSearcher<Trinity::NearestHostileUnitInAggroRangeCheck> searcher(this, target, u_check);

        TypeContainerVisitor<Trinity::UnitLastSearcher<Trinity::NearestHostileUnitInAggroRangeCheck>, WorldTypeMapContainer > world_unit_searcher(searcher);
        TypeContainerVisitor<Trinity::UnitLastSearcher<Trinity::NearestHostileUnitInAggroRangeCheck>, GridTypeMapContainer >  grid_unit_searcher(searcher);

        cell.Visit(p, world_unit_searcher, *GetMap(), *this, dist);
        cell.Visit(p, grid_unit_searcher, *GetMap(), *this, dist);
    }

    return target;
}

Unit* Creature::SelectNearestHostileUnitInAggroRange(bool useLOS) const
{
	// Selects nearest hostile target within creature's aggro range. Used primarily by
	//  pets set to aggressive. Will not return neutral or friendly targets.

	Unit* target = NULL;

	{
		Trinity::NearestHostileUnitInAggroRangeCheck u_check(this, useLOS);
		Trinity::UnitSearcher<Trinity::NearestHostileUnitInAggroRangeCheck> searcher(this, target, u_check);

		VisitNearbyGridObject(MAX_AGGRO_RADIUS, searcher);
	}

	return target;
}

void Creature::CallAssistance()
{
    if( !m_AlreadyCallAssistance && GetVictim() && !IsPet() && !IsCharmed())
    {
        SetNoCallAssistance(true);

        float radius = sWorld->getConfig(CONFIG_CREATURE_FAMILY_ASSISTANCE_RADIUS);
        if(radius > 0)
        {
            std::list<Creature*> assistList;

            {
                CellCoord p(Trinity::ComputeCellCoord(GetPositionX(), GetPositionY()));
                Cell cell(p);
                cell.SetNoCreate();
                cell.data.Part.reserved = ALL_DISTRICT;

                Trinity::AnyAssistCreatureInRangeCheck u_check(this, GetVictim(), radius);
                Trinity::CreatureListSearcher<Trinity::AnyAssistCreatureInRangeCheck> searcher(this, assistList, u_check);

                TypeContainerVisitor<Trinity::CreatureListSearcher<Trinity::AnyAssistCreatureInRangeCheck>, GridTypeMapContainer >  grid_creature_searcher(searcher);

                cell.Visit(p, grid_creature_searcher, *GetMap(), *this, radius);
            }

            // Add creatures from linking DB system
            if (m_creaturePoolId) {
                std::list<Creature*> allCreatures = FindMap()->GetAllCreaturesFromPool(m_creaturePoolId);
                for (auto itr : allCreatures) 
                {
                    if (itr->IsAlive() && itr->IsInWorld())
                        assistList.push_back(itr);
                }
                if(allCreatures.size() == 0)
                    TC_LOG_ERROR("sql.sql","Broken data in table creature_pool_relations for creature pool %u.", m_creaturePoolId);
            }

            if (!assistList.empty())
            {
                uint32 count = 0;
                auto e = new AssistDelayEvent(GetVictim()->GetGUID(), *this);
                while (!assistList.empty())
                {
                    ++count;
                    // Pushing guids because in delay can happen some creature gets despawned => invalid pointer
//                    TC_LOG_INFO("FIXME","ASSISTANCE: calling creature at %p", *assistList.begin());
                    Creature *cr = *assistList.begin();
                    if (!cr->IsInWorld()) {
                        TC_LOG_ERROR("FIXME","ASSISTANCE: ERROR: creature is not in world");
                        assistList.pop_front();
                        continue;
                    }
                    e->AddAssistant((*assistList.begin())->GetGUID());
                    assistList.pop_front();
                    if (GetInstanceId() == 0 && count >= 4)
                        break;
                }
                m_Events.AddEvent(e, m_Events.CalculateTime(sWorld->getConfig(CONFIG_CREATURE_FAMILY_ASSISTANCE_DELAY)));
            }
        }
    }
}

void Creature::CallForHelp(float radius)
{
    if (radius <= 0.0f || !GetVictim() || IsPet() || IsCharmed())
        return;

    CellCoord p(Trinity::ComputeCellCoord(GetPositionX(), GetPositionY()));
    Cell cell(p);
    cell.SetNoCreate();

    Trinity::CallOfHelpCreatureInRangeDo u_do(this, GetVictim(), radius);
    Trinity::CreatureWorker<Trinity::CallOfHelpCreatureInRangeDo> worker(u_do);

    TypeContainerVisitor<Trinity::CreatureWorker<Trinity::CallOfHelpCreatureInRangeDo>, GridTypeMapContainer >  grid_creature_searcher(worker);

    cell.Visit(p, grid_creature_searcher, *GetMap(), *this, radius);
}

bool Creature::CanAssistTo(const Unit* u, const Unit* enemy, bool checkFaction /* = true */) const
{
    // is it true?
    if(!HasReactState(REACT_AGGRESSIVE) || (HasJustRespawned() && !m_summoner)) //ignore justrespawned if summoned
        return false;

    // we don't need help from zombies :)
    if( !IsAlive() )
        return false;

    // we cannot assist in evade mode
    if (IsInEvadeMode())
        return false;

    // or if enemy is in evade mode
    if (enemy->GetTypeId() == TYPEID_UNIT && enemy->ToCreature()->IsInEvadeMode())
        return false;

    // skip fighting creature
    if( IsInCombat() )
        return false;

    // only from same creature faction
    if (checkFaction && GetFaction() != u->GetFaction())
        return false;

    // only free creature
    if( GetCharmerOrOwnerGUID() )
        return false;

    // skip non hostile to caster enemy creatures
    if( !IsHostileTo(enemy) )
        return false;

    // don't assist to slay innocent critters
    if( enemy->GetTypeId() == CREATURE_TYPE_CRITTER )
        return false;

    return true;
}

void Creature::SaveRespawnTime()
{
    if(IsPet() || !m_spawnId)
        return;

    sObjectMgr->SaveCreatureRespawnTime(m_spawnId, GetMapId(), GetInstanceId(), m_respawnTime);
}

bool Creature::IsOutOfThreatArea(Unit* pVictim) const
{
    if(!pVictim)
        return true;

    if(!pVictim->IsInMap(this))
        return true;

    if(CanAttack(pVictim) != CAN_ATTACK_RESULT_OK)
        return true;

    if(!pVictim->isInAccessiblePlaceFor(this))
        return true;

    if (((Creature*)this)->IsCombatStationary() && !CanReachWithMeleeAttack(pVictim))
        return true;

    if(sMapStore.LookupEntry(GetMapId())->IsDungeon())
        return false;

    float length;
    if (IsHomeless() || ((Creature*)this)->IsBeingEscorted())
        length = pVictim->GetDistance(GetPositionX(), GetPositionY(), GetPositionZ());
    else {
        float x,y,z,o;
        GetHomePosition(x,y,z,o);
        length = pVictim->GetDistance(x, y, z);
    }
    float AttackDist = GetAggroRange(pVictim);
    uint32 ThreatRadius = sWorld->getConfig(CONFIG_THREAT_RADIUS);

    //Use AttackDistance in distance check if threat radius is lower. This prevents creature bounce in and out of combat every update tick.
    return ( length > ((ThreatRadius > AttackDist) ? ThreatRadius : AttackDist));
}

void Creature::LoadCreatureAddon()
{
    if (m_spawnId)
    {
        if((m_creatureInfoAddon = sObjectMgr->GetCreatureAddon(m_spawnId)))
            return;
    }

    m_creatureInfoAddon = sObjectMgr->GetCreatureTemplateAddon(GetCreatureTemplate()->Entry);
}

//creature_addon table
bool Creature::InitCreatureAddon(bool reload)
{
    if(!m_creatureInfoAddon)
        return false;

    if (m_creatureInfoAddon->mount != 0)
        Mount(m_creatureInfoAddon->mount);

    if (m_creatureInfoAddon->bytes0 != 0)
        SetUInt32Value(UNIT_FIELD_BYTES_0, m_creatureInfoAddon->bytes0);

    if (m_creatureInfoAddon->bytes1 != 0)
        SetUInt32Value(UNIT_FIELD_BYTES_1, m_creatureInfoAddon->bytes1);

    if (m_creatureInfoAddon->bytes2 != 0)
        SetUInt32Value(UNIT_FIELD_BYTES_2, m_creatureInfoAddon->bytes2);

    if (m_creatureInfoAddon->emote != 0)
        SetEmoteState(m_creatureInfoAddon->emote);

    if (m_creatureInfoAddon->move_flags != 0)
        SetUnitMovementFlags(m_creatureInfoAddon->move_flags);

    //Load Path
    if (m_creatureInfoAddon->path_id != 0)
        m_path_id = m_creatureInfoAddon->path_id;

    for(auto id : m_creatureInfoAddon->auras)
    {
        SpellInfo const *AdditionalSpellInfo = sSpellMgr->GetSpellInfo(id);
        if (!AdditionalSpellInfo)
        {
            TC_LOG_ERROR("sql.sql","Creature (GUIDLow: %u Entry: %u ) has wrong spell %u defined in `auras` field.", GetSpawnId(),GetEntry(),id);
            continue;
        }

        // skip already applied aura
        if(HasAuraEffect(id))
        {
            if(!reload)
                TC_LOG_ERROR("sql.sql","Creature (GUIDLow: %u Entry: %u ) has duplicate aura (spell %u) in `auras` field.",GetSpawnId(),GetEntry(),id);

            continue;
        }

        AddAura(id,this);
        TC_LOG_DEBUG("entities.unit", "Spell: %u added to creature (GUID: %u Entry: %u)", id, GetSpawnId(), GetEntry());
    }
    return true;
}

/// Send a message to LocalDefense channel for players opposition team in the zone
void Creature::SendZoneUnderAttackMessage(Player* attacker)
{
    uint32 enemy_team = attacker->GetTeam();
    sWorld->SendZoneUnderAttack(GetZoneId(), (enemy_team==ALLIANCE ? HORDE : ALLIANCE));
}

void Creature::_AddCreatureSpellCooldown(uint32 spell_id, time_t end_time)
{
    m_CreatureSpellCooldowns[spell_id] = end_time;
}

void Creature::_AddCreatureCategoryCooldown(uint32 category, time_t apply_time)
{
    m_CreatureCategoryCooldowns[category] = apply_time;
}

void Creature::AddCreatureSpellCooldown(uint32 spellid)
{
    SpellInfo const *spellInfo = sSpellMgr->GetSpellInfo(spellid);
    if(!spellInfo)
        return;

    uint32 cooldown = spellInfo->GetRecoveryTime();
    if(cooldown)
        _AddCreatureSpellCooldown(spellid, time(nullptr) + cooldown/1000);

    if(spellInfo->GetCategory())
        _AddCreatureCategoryCooldown(spellInfo->GetCategory(), time(nullptr));

    m_GlobalCooldown = spellInfo->StartRecoveryTime;
}

bool Creature::HasCategoryCooldown(uint32 spell_id) const
{
    SpellInfo const *spellInfo = sSpellMgr->GetSpellInfo(spell_id);
    if(!spellInfo)
        return false;

    // check global cooldown if spell affected by it
    if (spellInfo->StartRecoveryCategory > 0 && m_GlobalCooldown > 0)
        return true;

    auto itr = m_CreatureCategoryCooldowns.find(spellInfo->GetCategory());
    return(itr != m_CreatureCategoryCooldowns.end() && time_t(itr->second + (spellInfo->CategoryRecoveryTime / 1000)) > time(nullptr));
}

bool Creature::HasSpellCooldown(uint32 spell_id) const
{
    auto itr = m_CreatureSpellCooldowns.find(spell_id);
    return (itr != m_CreatureSpellCooldowns.end() && itr->second > time(nullptr)) || HasCategoryCooldown(spell_id);
}

bool Creature::HasSpell(uint32 spellID) const
{
    uint8 i;
    for(i = 0; i < CREATURE_MAX_SPELLS; ++i)
        if(spellID == m_spells[i])
            break;
    return i < CREATURE_MAX_SPELLS;                         //broke before end of iteration of known spells
}

time_t Creature::GetRespawnTimeEx() const
{
    time_t now = time(nullptr);
    if (m_respawnTime > now)
        return m_respawnTime;
    else
        return now;
}

bool Creature::IsSpawnedOnTransport() const 
{
    return m_creatureData && m_creatureData->mapid != GetMapId();
}

void Creature::GetRespawnPosition( float &x, float &y, float &z, float* ori, float* dist ) const
{
    if (m_spawnId)
    {
        // for npcs on transport, this will return transport offset
        if (CreatureData const* data = sObjectMgr->GetCreatureData(GetSpawnId()))
        {
            x = data->posX;
            y = data->posY;
            z = data->posZ;
            if(ori)
                *ori = data->orientation;
            if(dist)
                *dist = data->spawndist;

            return;
        }
    }

    Position homePos = GetHomePosition();
    x = homePos.GetPositionX();
    y = homePos.GetPositionY();
    z = homePos.GetPositionZ();
    if(ori)
        *ori = homePos.GetOrientation();
    if(dist)
        *dist = 0;
}

void Creature::AllLootRemovedFromCorpse()
{
    if (!HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SKINNABLE))
    {
        time_t now = time(nullptr);
        // Do not reset corpse remove time if corpse is already removed
        if(m_corpseRemoveTime <= now)
            return;
            
        float decayRate = sWorld->GetRate(RATE_CORPSE_DECAY_LOOTED);
        CreatureTemplate const *cinfo = GetCreatureTemplate();

        // corpse skinnable, but without skinning flag, and then skinned, corpse will despawn next update
        if (cinfo && cinfo->SkinLootId)
            m_corpseRemoveTime = now;
        else
            m_corpseRemoveTime = now + uint32(m_corpseDelay * decayRate);

        m_respawnTime = std::max<time_t>(m_corpseRemoveTime + m_respawnDelay, m_respawnTime);
    }
}

uint8 Creature::GetLevelForTarget( WorldObject const* target ) const
{
    if(!IsWorldBoss() || !target->ToUnit())
        return Unit::GetLevelForTarget(target);

    //bosses are always considered 3 level higher
    uint32 level = target->ToUnit()->GetLevel() + 3;
    if(level < 1)
        return 1;
    if(level > 255)
        return 255;
	return uint8(level);
}

std::string Creature::GetScriptName()
{
    return sObjectMgr->GetScriptName(GetScriptId());
}

uint32 Creature::getInstanceEventId()
{
    if (CreatureData const* myData = sObjectMgr->GetCreatureData(m_spawnId))
        return myData->instanceEventId;
        
    return 0;
}   

uint32 Creature::GetScriptId()
{
    if (CreatureData const* creatureData = GetCreatureData())
        if (uint32 scriptId = creatureData->scriptId)
            return scriptId;

    return sObjectMgr->GetCreatureTemplate(GetEntry())->ScriptID;
}

std::string Creature::GetAIName() const
{
    return sObjectMgr->GetCreatureTemplate(GetEntry())->AIName;
}

VendorItemData const* Creature::GetVendorItems() const
{
    return sObjectMgr->GetNpcVendorItemList(GetEntry());
}

uint32 Creature::GetVendorItemCurrentCount(VendorItem const* vItem)
{
    if(!vItem->maxcount)
        return vItem->maxcount;

    auto itr = m_vendorItemCounts.begin();
    for(; itr != m_vendorItemCounts.end(); ++itr)
        if(itr->itemId==vItem->proto->ItemId)
            break;

    if(itr == m_vendorItemCounts.end())
        return vItem->maxcount;

    VendorItemCount* vCount = &*itr;

    time_t ptime = time(nullptr);

    if( vCount->lastIncrementTime + vItem->incrtime <= ptime )
    {
        ItemTemplate const* pProto = sObjectMgr->GetItemTemplate(vItem->proto->ItemId);

        uint32 diff = uint32((ptime - vCount->lastIncrementTime)/vItem->incrtime);
        if((vCount->count + diff * pProto->BuyCount) >= vItem->maxcount )
        {
            m_vendorItemCounts.erase(itr);
            return vItem->maxcount;
        }

        vCount->count += diff * pProto->BuyCount;
        vCount->lastIncrementTime = ptime;
    }

    return vCount->count;
}

uint32 Creature::UpdateVendorItemCurrentCount(VendorItem const* vItem, uint32 used_count)
{
    if(!vItem->maxcount)
        return 0;

    auto itr = m_vendorItemCounts.begin();
    for(; itr != m_vendorItemCounts.end(); ++itr)
        if(itr->itemId==vItem->proto->ItemId)
            break;

    if(itr == m_vendorItemCounts.end())
    {
        uint32 new_count = vItem->maxcount > used_count ? vItem->maxcount-used_count : 0;
        m_vendorItemCounts.push_back(VendorItemCount(vItem->proto->ItemId,new_count));
        return new_count;
    }

    VendorItemCount* vCount = &*itr;

    time_t ptime = time(nullptr);

    if( vCount->lastIncrementTime + vItem->incrtime <= ptime )
    {
        uint32 diff = uint32((ptime - vCount->lastIncrementTime)/vItem->incrtime);
        if((vCount->count + diff * vItem->proto->BuyCount) < vItem->maxcount )
            vCount->count += diff * vItem->proto->BuyCount;
        else
            vCount->count = vItem->maxcount;
    }

    vCount->count = vCount->count > used_count ? vCount->count-used_count : 0;
    vCount->lastIncrementTime = ptime;
    return vCount->count;
}

TrainerSpellData const* Creature::GetTrainerSpells() const
{
    return sObjectMgr->GetNpcTrainerSpells(GetEntry());
}

// overwrite WorldObject function for proper name localization
std::string const& Creature::GetNameForLocaleIdx(LocaleConstant loc_idx) const
{
    if (loc_idx >= 0)
    {
        CreatureLocale const *cl = sObjectMgr->GetCreatureLocale(GetEntry());
        if (cl)
        {
            if (cl->Name.size() > loc_idx && !cl->Name[loc_idx].empty())
                return cl->Name[loc_idx];
        }
    }

    return GetName();
}

const CreatureData* Creature::GetLinkedRespawnCreatureData() const
{
    if(!m_spawnId) // only hard-spawned creatures from DB can have a linked master
        return nullptr;

    if(uint32 targetGuid = sObjectMgr->GetLinkedRespawnGuid(m_spawnId))
        return sObjectMgr->GetCreatureData(targetGuid);

    return nullptr;
}

// returns master's remaining respawn time if any
time_t Creature::GetLinkedCreatureRespawnTime() const
{
    if(!m_spawnId) // only hard-spawned creatures from DB can have a linked master
        return 0;

    if(uint32 targetGuid = sObjectMgr->GetLinkedRespawnGuid(m_spawnId))
    {
        Map* targetMap = nullptr;
        if(const CreatureData* data = sObjectMgr->GetCreatureData(targetGuid))
        {
            if(data->mapid == GetMapId())   // look up on the same map
                targetMap = GetMap();
            else                            // it shouldn't be instanceable map here
                targetMap = sMapMgr->FindBaseNonInstanceMap(data->mapid);
        }
        if(targetMap)
            return sObjectMgr->GetCreatureRespawnTime(targetGuid,targetMap->GetInstanceId());
    }

    return 0;
}

void Creature::AreaCombat()
{
    if(Map* map = GetMap())
    {
        float range = 0.0f;
        if(GetCreatureTemplate()->flags_extra & CREATURE_FLAG_EXTRA_INSTANCE_BIND)
            range += 100.0f;
        if(IsWorldBoss())
            range += 100.0f;

        Map::PlayerList const &PlayerList = map->GetPlayers();
        for(const auto & i : PlayerList)
        {
            if (Player* i_pl = i.GetSource())
                if (i_pl->IsAlive() && IsWithinCombatRange(i_pl, range) && CanAttack(i_pl, false) == CAN_ATTACK_RESULT_OK)
                {
                    SetInCombatWith(i_pl);
                    i_pl->SetInCombatWith(this);
                    AddThreat(i_pl, 0.0f);
               }
        }
    }
}

bool Creature::HadPlayerInThreatListAtDeath(uint64 guid) const
{
    auto itr = m_playerInThreatListAtDeath.find(GUID_LOPART(guid));
    return itr == m_playerInThreatListAtDeath.end();
}

void Creature::ConvertThreatListIntoPlayerListAtDeath()
{
    auto threatList = getThreatManager().getThreatList();
    for(auto itr : threatList)
    {
        if(itr->getThreat() > 0.0f && itr->getSourceUnit()->GetTypeId() == TYPEID_PLAYER)
            m_playerInThreatListAtDeath.insert(itr->GetSource()->GetOwner()->GetGUIDLow());
    }
}

bool Creature::IsBelowHPPercent(float percent)
{
    float healthAtPercent = (GetMaxHealth() / 100.0f) * percent;
    
    return GetHealth() < healthAtPercent;
}

bool Creature::IsAboveHPPercent(float percent)
{
    float healthAtPercent = (GetMaxHealth() / 100.0f) * percent;
    
    return GetHealth() > healthAtPercent;
}

bool Creature::IsBetweenHPPercent(float minPercent, float maxPercent)
{
    float minHealthAtPercent = (GetMaxHealth() / 100.0f) * minPercent;
    float maxHealthAtPercent = (GetMaxHealth() / 100.0f) * maxPercent;

    return GetHealth() > minHealthAtPercent && GetHealth() < maxHealthAtPercent;
}

bool Creature::isMoving()
{
    float x, y ,z;
    return GetMotionMaster()->GetDestination(x,y,z);
}

bool AIMessageEvent::Execute(uint64 /*e_time*/, uint32 /*p_time*/) 
{ 
    if(owner.AI())
        owner.AI()->message(id,data);

    return true; 
}

void Creature::AddMessageEvent(uint64 timer, uint32 messageId, uint64 data)
{
    auto  messageEvent = new AIMessageEvent(*this,messageId,data);
    m_Events.AddEvent(messageEvent, m_Events.CalculateTime(timer), false);
}

float Creature::GetDistanceFromHome() const
{
    return GetDistance(GetHomePosition());
}

void Creature::Motion_Initialize()
{
    if (!m_formation)
        GetMotionMaster()->Initialize();
    else if (m_formation->getLeader() == this)
    {
        m_formation->FormationReset(false);
        GetMotionMaster()->Initialize();
    }
    else if (m_formation->isFormed())
        GetMotionMaster()->MoveIdle(); //wait the order of leader
    else
        GetMotionMaster()->Initialize();
}

void Creature::SetTarget(uint64 guid)
{
    if (!_focusSpell)
        SetUInt64Value(UNIT_FIELD_TARGET, guid);
}

void Creature::FocusTarget(Spell const* focusSpell, WorldObject const* target)
{
    // already focused
    if (_focusSpell)
        return;

    _focusSpell = focusSpell;
    SetUInt64Value(UNIT_FIELD_TARGET, target->GetGUID());
    if (focusSpell->GetSpellInfo()->HasAttribute(SPELL_ATTR5_DONT_TURN_DURING_CAST))
        AddUnitState(UNIT_STATE_ROTATING);

    // Set serverside orientation if needed (needs to be after attribute check)
    SetInFront(target);
}

void Creature::ReleaseFocus(Spell const* focusSpell)
{
    // focused to something else
    if (focusSpell != _focusSpell)
        return;

    _focusSpell = nullptr;
    if (Unit* victim = GetVictim())
        SetUInt64Value(UNIT_FIELD_TARGET, victim->GetGUID());
    else
        SetUInt64Value(UNIT_FIELD_TARGET, 0);

    if (focusSpell->GetSpellInfo()->HasAttribute(SPELL_ATTR5_DONT_TURN_DURING_CAST))
        ClearUnitState(UNIT_STATE_ROTATING);
}



bool Creature::IsFocusing(Spell const* focusSpell, bool withDelay)
{
    if (!IsAlive()) // dead creatures cannot focus
    {
        ReleaseFocus(nullptr/*, false*/);
        return false;
    }

    if (focusSpell && (focusSpell != _focusSpell))
        return false;

    /* TC 
    if (!_focusSpell)
    {
        if (!withDelay || !m_focusDelay)
            return false;
        if (GetMSTimeDiffToNow(m_focusDelay) > 1000) // @todo figure out if we can get rid of this magic number somehow
        {
            m_focusDelay = 0; // save checks in the future
            return false;
        }
    }
    */

    return true;
}

bool Creature::SetDisableGravity(bool disable, bool packetOnly/*=false*/)
{
    //! It's possible only a packet is sent but moveflags are not updated
    //! Need more research on this
    if (!packetOnly && !Unit::SetDisableGravity(disable))
        return false;

#ifdef LICH_KING
     if (!movespline->Initialized())
        return true;

    WorldPacket data(disable ? SMSG_SPLINE_MOVE_GRAVITY_DISABLE : SMSG_SPLINE_MOVE_GRAVITY_ENABLE, 9);
    data << GetPackGUID();
    SendMessageToSet(&data, false);
#endif
    return true;
}

uint32 Creature::GetPetAutoSpellOnPos(uint8 pos) const
{
	if (pos >= CREATURE_MAX_SPELLS || m_charmInfo->GetCharmSpell(pos)->active != ACT_ENABLED)
		return 0;
	else
		return m_charmInfo->GetCharmSpell(pos)->spellId;
}

float Creature::GetPetChaseDistance() const
{
	float range = MELEE_RANGE;

	for (uint8 i = 0; i < GetPetAutoSpellSize(); ++i)
	{
		uint32 spellID = GetPetAutoSpellOnPos(i);
		if (!spellID)
			continue;

		if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellID))
		{
			if (spellInfo->GetRecoveryTime() == 0 &&  // No cooldown
				spellInfo->RangeEntry->ID != 1 /*Self*/ && spellInfo->RangeEntry->ID != 2 /*Combat Range*/ &&
				spellInfo->GetMinRange() > range)
				range = spellInfo->GetMinRange();
		}
	}

	return range;
}

bool Creature::SetWalk(bool enable)
{
    if (!Unit::SetWalk(enable))
        return false;

    WorldPacket data(enable ? SMSG_SPLINE_MOVE_SET_WALK_MODE : SMSG_SPLINE_MOVE_SET_RUN_MODE, 9);
    data << GetPackGUID();
    SendMessageToSet(&data, false);
    return true;
}


bool Creature::SetSwim(bool enable)
{
    if (!Unit::SetSwim(enable))
        return false;

    if (!movespline->Initialized())
        return true;

    WorldPacket data(enable ? SMSG_SPLINE_MOVE_START_SWIM : SMSG_SPLINE_MOVE_STOP_SWIM);
    data << GetPackGUID();
    SendMessageToSet(&data, true);
    return true;
}

bool Creature::SetFlying(bool enable, bool packetOnly /* = false */)
{
    if (!Unit::SetFlying(enable))
        return false;

    if (!movespline->Initialized())
        return true;

    //also mark creature as able to fly to avoid getting fly mode removed
    if(enable)
        SetCanFly(enable, false);

    WorldPacket data(enable ? SMSG_SPLINE_MOVE_SET_FLYING : SMSG_SPLINE_MOVE_UNSET_FLYING, 9);
    data << GetPackGUID();
    SendMessageToSet(&data, false);
    return true;
}

void Creature::SetCanFly(bool enable, bool updateMovementFlags /* = true */) 
{ 
    m_canFly = enable; 
    if (updateMovementFlags)
        UpdateMovementFlags();
}

bool Creature::SetWaterWalking(bool enable, bool packetOnly /* = false */)
{
    if (!packetOnly && !Unit::SetWaterWalking(enable))
        return false;

    if (!movespline->Initialized())
        return true;

    WorldPacket data(enable ? SMSG_SPLINE_MOVE_WATER_WALK : SMSG_SPLINE_MOVE_LAND_WALK);
    data << GetPackGUID();
    SendMessageToSet(&data, true);
    return true;
}

bool Creature::SetFeatherFall(bool enable, bool packetOnly /* = false */)
{
    if (!packetOnly && !Unit::SetFeatherFall(enable))
        return false;

    if (!movespline->Initialized())
        return true;

    WorldPacket data(enable ? SMSG_SPLINE_MOVE_FEATHER_FALL : SMSG_SPLINE_MOVE_NORMAL_FALL);
    data << GetPackGUID();
    SendMessageToSet(&data, true);
    return true;
}

bool Creature::SetHover(bool enable, bool packetOnly /*= false*/)
{
    if (!packetOnly && !Unit::SetHover(enable))
        return false;

    //! Unconfirmed for players:
    if (enable)
        SetByteFlag(UNIT_FIELD_BYTES_1, UNIT_BYTES_1_OFFSET_ANIM_TIER, UNIT_BYTE1_FLAG_HOVER);
    else
        RemoveByteFlag(UNIT_FIELD_BYTES_1, UNIT_BYTES_1_OFFSET_ANIM_TIER, UNIT_BYTE1_FLAG_HOVER);

    if (!movespline->Initialized())
        return true;

    //! Not always a packet is sent
    WorldPacket data(enable ? SMSG_SPLINE_MOVE_SET_HOVER : SMSG_SPLINE_MOVE_UNSET_HOVER, 9);
    data << GetPackGUID();
    SendMessageToSet(&data, false);
    return true;
}

void Creature::UpdateMovementFlags()
{
    // Do not update movement flags if creature is controlled by a player (charm/vehicle)
    if (m_playerMovingMe)
        return;

    // Set the movement flags if the creature is in that mode. (Only fly if actually in air, only swim if in water, etc)
    float ground = GetFloorZ();

    bool isInAir = (G3D::fuzzyGt(GetPositionZMinusOffset(), ground + 0.05f) || G3D::fuzzyLt(GetPositionZMinusOffset(), ground - 0.05f)); // Can be underground too, prevent the falling
    
    if (CanFly() && isInAir && !IsFalling())
    {
        if (CanWalk())
        {
            SetFlying(true);
            SetDisableGravity(true);
        } 
        else
            SetDisableGravity(true);
    }
    else
    {
        SetFlying(false);
        SetDisableGravity(false);
    }

    if (!isInAir || CanFly())
        RemoveUnitMovementFlag(MOVEMENTFLAG_JUMPING_OR_FALLING);

    SetSwim(CanSwim() && IsInWater());
}

bool Creature::IsInEvadeMode() const 
{ 
    return HasUnitState(UNIT_STATE_EVADE); 
}

void Creature::SetCannotReachTarget(bool cannotReach)
{
    if (cannotReach)
    {
        //this mark creature as unreachable + start counting
        if (!m_unreachableTargetTime)
            m_unreachableTargetTime = 1;
    }
    else {
        m_unreachableTargetTime = 0;
        m_evadingAttacks = false;
    }
}

bool Creature::CannotReachTarget() const
{
    return m_unreachableTargetTime > 0;
}

void Creature::HandleUnreachableTarget(uint32 diff)
{
    if(!AI() || !IsInCombat() || m_unreachableTargetTime == 0 || IsInEvadeMode())
        return;

    if(GetMotionMaster()->GetCurrentMovementGeneratorType() == CHASE_MOTION_TYPE)
        m_unreachableTargetTime += diff;

    if (!m_evadingAttacks)
    {
        uint32 maxTimeBeforeEvadingAttacks = sWorld->getConfig(CONFIG_CREATURE_UNREACHABLE_TARGET_EVADE_ATTACKS_TIME);
        if (maxTimeBeforeEvadingAttacks)
            if (m_unreachableTargetTime > maxTimeBeforeEvadingAttacks)
                m_evadingAttacks = true;
    }

    //evading to home shouldn't happen in instance (not 100% sure in which case this is true on retail but this seems like a good behavior for now)
    if (GetMap()->IsDungeon())
        return;

    uint32 maxTimeBeforeEvadingHome = sWorld->getConfig(CONFIG_CREATURE_UNREACHABLE_TARGET_EVADE_TIME);
    if (maxTimeBeforeEvadingHome)
        if (m_unreachableTargetTime > maxTimeBeforeEvadingHome)
            AI()->EnterEvadeMode(CreatureAI::EVADE_REASON_NO_PATH);
}

void Creature::ResetCreatureEmote()
{
    if(CreatureAddon const* cAddon = GetCreatureAddon())
        SetEmoteState(cAddon->emote); 
    else
        SetEmoteState(0); 

    SetStandState(UNIT_STAND_STATE_STAND);
}

void Creature::WarnDeathToFriendly()
{
    std::list< std::pair<Creature*,float> > warnList;

    // Check near creatures for assistance
    CellCoord p(Trinity::ComputeCellCoord(GetPositionX(), GetPositionY()));
    Cell cell(p);
    cell.SetNoCreate();

    Trinity::AnyFriendlyUnitInObjectRangeCheckWithRangeReturned u_check(this, this, CREATURE_MAX_DEATH_WARN_RANGE);
    Trinity::CreatureListSearcherWithRange<Trinity::AnyFriendlyUnitInObjectRangeCheckWithRangeReturned> searcher(this, warnList, u_check);
    VisitNearbyGridObject(CREATURE_MAX_DEATH_WARN_RANGE, searcher);

    for(auto itr : warnList) 
        if(itr.first->IsAIEnabled)
            itr.first->AI()->FriendlyKilled(this, itr.second);    
}

void Creature::SetTextRepeatId(uint8 textGroup, uint8 id)
{
    CreatureTextRepeatIds& repeats = m_textRepeat[textGroup];
    if (std::find(repeats.begin(), repeats.end(), id) == repeats.end())
        repeats.push_back(id);
    else
        TC_LOG_ERROR("sql.sql", "CreatureTextMgr: TextGroup %u for Creature(%s) GuidLow %u Entry %u, id %u already added", uint32(textGroup), GetName().c_str(), GetGUIDLow(), GetEntry(), uint32(id));
}

CreatureTextRepeatIds Creature::GetTextRepeatGroup(uint8 textGroup)
{
    CreatureTextRepeatIds ids;

    CreatureTextRepeatGroup::const_iterator groupItr = m_textRepeat.find(textGroup);
    if (groupItr != m_textRepeat.end())
        ids = groupItr->second;

    return ids;
}

void Creature::ClearTextRepeatGroup(uint8 textGroup)
{
    auto groupItr = m_textRepeat.find(textGroup);
    if (groupItr != m_textRepeat.end())
        groupItr->second.clear();
}

void Creature::SetKeepActiveTimer(uint32 timerMS)
{
    if (timerMS == 0)
        return;

    if (GetTypeId() == TYPEID_PLAYER)
        return;

    if (!IsInWorld())
        return;

    SetKeepActive(true);
    m_keepActiveTimer = timerMS;
}

void Creature::SetObjectScale(float scale)
{
    Unit::SetObjectScale(scale);

    if (CreatureModelInfo const* minfo = sObjectMgr->GetCreatureModelInfo(GetDisplayId()))
    {
        SetFloatValue(UNIT_FIELD_BOUNDINGRADIUS, (IsPet() ? 1.0f : minfo->bounding_radius) * scale);
        SetFloatValue(UNIT_FIELD_COMBATREACH, (IsPet() ? DEFAULT_PLAYER_COMBAT_REACH : minfo->combat_reach) * scale);
    }
}

void Creature::SetDisplayId(uint32 modelId)
{
    Unit::SetDisplayId(modelId);

    if (CreatureModelInfo const* minfo = sObjectMgr->GetCreatureModelInfo(modelId))
    {
        SetFloatValue(UNIT_FIELD_BOUNDINGRADIUS, (IsPet() ? 1.0f : minfo->bounding_radius) * GetObjectScale());
        SetFloatValue(UNIT_FIELD_COMBATREACH, (IsPet() ? DEFAULT_PLAYER_COMBAT_REACH : minfo->combat_reach) * GetObjectScale());

        // Set Gender by modelId. Note: TC has this in Unit::SetDisplayId but this seems wrong for BC
         SetByteValue(UNIT_FIELD_BYTES_0, UNIT_BYTES_0_OFFSET_GENDER, minfo->gender);
    }

    if (Pet *pet = this->ToPet())
    {
        if (pet->isControlled())
        {
            Unit *owner = GetOwner();
            if (owner && (owner->GetTypeId() == TYPEID_PLAYER) && (owner->ToPlayer())->GetGroup())
                (owner->ToPlayer())->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_PET_MODEL_ID);
        }
    }

}
