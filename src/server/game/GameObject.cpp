

#include "Common.h"
#include "QuestDef.h"
#include "GameObject.h"
#include "ObjectMgr.h"
#include "SpellMgr.h"
#include "Spell.h"
#include "GameTime.h"
#include "UpdateMask.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "World.h"
#include "Database/DatabaseEnv.h"
#include "MapManager.h"
#include "LootMgr.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "InstanceScript.h"
#include "BattleGround.h"
#include "Util.h"
#include "OutdoorPvPMgr.h"
#include "BattleGroundAV.h"
#include "CreatureAISelector.h"
#include "TransportMgr.h"
#include "Transport.h"
#include "UpdateFieldFlags.h"
#include "ScriptMgr.h"

#include "Models/GameObjectModel.h"
#include "DynamicTree.h"

GameObject::GameObject() : WorldObject(false), MapObject(),
    m_AI(nullptr), 
    m_model(nullptr), 
    m_goValue(),
    m_respawnTime(0),
    m_respawnDelayTime(25),
    m_despawnTime(0),
    m_lootState(GO_NOT_READY),
    m_spawnedByDefault(true),
    m_usetimes(0),
    m_spellId(0),
    m_charges(5),
    m_cooldownTime(0),
    m_inactive(false),
    m_goInfo(nullptr),
    m_goData(nullptr),
    m_spawnId(0),
    manual_unlock(false),
    m_prevGoState(GO_STATE_ACTIVE)
{
    m_objectType |= TYPEMASK_GAMEOBJECT;
    m_objectTypeId = TYPEID_GAMEOBJECT;

    m_updateFlag = (UPDATEFLAG_LOWGUID | UPDATEFLAG_HIGHGUID | UPDATEFLAG_STATIONARY_POSITION); // 2.3.2 - 0x58
    m_updateFlagLK = (LK_UPDATEFLAG_LOWGUID | LK_UPDATEFLAG_STATIONARY_POSITION | LK_UPDATEFLAG_POSITION | LK_UPDATEFLAG_ROTATION);

    m_valuesCount = GAMEOBJECT_END;

    m_stationaryPosition.Relocate(0.0f, 0.0f, 0.0f, 0.0f);
}

GameObject::~GameObject()
{
    delete m_AI;
    delete m_model;
}

void GameObject::CleanupsBeforeDelete(bool finalCleanup)
{
	WorldObject::CleanupsBeforeDelete(finalCleanup);

	if (m_uint32Values)                                      // field array can be not exist if GameOBject not loaded
		RemoveFromOwner();
}

void GameObject::RemoveFromOwner()
{
	ObjectGuid ownerGUID = GetOwnerGUID();
	if (!ownerGUID)
		return;

	if (Unit* owner = ObjectAccessor::GetUnit(*this, ownerGUID))
	{
		owner->RemoveGameObject(this, false);
		ASSERT(!GetOwnerGUID());
		return;
	}

	// This happens when a mage portal is despawned after the caster changes map (for example using the portal)
	TC_LOG_DEBUG("misc", "Removed GameObject (GUID: %u Entry: %u SpellId: %u LinkedGO: %u) that just lost any reference to the owner (%s) GO list",
		ObjectGuid(GetGUID()).GetCounter(), GetGOInfo()->entry, m_spellId, GetGOInfo()->GetLinkedGameObjectEntry(), ownerGUID.ToString().c_str());
	SetOwnerGUID(0 /*ObjectGuid::Empty*/);
}

std::string GameObject::GetAIName() const
{
    return sObjectMgr->GetGameObjectTemplate(GetEntry())->AIName;
}

void GameObject::BuildValuesUpdate(uint8 updateType, ByteBuffer* data, Player* target) const
{
    if (!target)
        return;

    bool forcedFlags = GetGoType() == GAMEOBJECT_TYPE_CHEST && GetGOInfo()->chest.groupLootRules; //todo loot && HasLootRecipient();
    bool targetIsGM = target->IsGameMaster();

    ByteBuffer fieldBuffer;

    UpdateMask updateMask;
    updateMask.SetCount(m_valuesCount);

    uint32* flags = GameObjectUpdateFieldFlags;
    uint32 visibleFlag = UF_FLAG_PUBLIC;
    if (GetOwnerGUID() == target->GetGUID())
        visibleFlag |= UF_FLAG_OWNER;

    for (uint16 index = 0; index < m_valuesCount; ++index)
    {
        if (_fieldNotifyFlags & flags[index] ||
            ((updateType == UPDATETYPE_VALUES ? _changesMask.GetBit(index) : m_uint32Values[index]) && (flags[index] & visibleFlag)) ||
            (index == GAMEOBJECT_FLAGS && forcedFlags))
        {
            updateMask.SetBit(index);

            //LK if (index == GAMEOBJECT_DYNAMIC)
            if (index == GAMEOBJECT_DYN_FLAGS)
            {
                uint16 dynFlags = 0;
#ifdef LICH_KING
               //LK int16 pathProgress = -1;
#endif
                switch (GetGoType())
                {
                    case GAMEOBJECT_TYPE_QUESTGIVER:
                        if (ActivateToQuest(target))
                            dynFlags |= GO_DYNFLAG_LO_ACTIVATE;
                        break;
                    case GAMEOBJECT_TYPE_CHEST:
                    case GAMEOBJECT_TYPE_GOOBER:
                        if (ActivateToQuest(target))
                            dynFlags |= GO_DYNFLAG_LO_ACTIVATE | GO_DYNFLAG_LO_SPARKLE;
                        else if (targetIsGM)
                            dynFlags |= GO_DYNFLAG_LO_ACTIVATE;
                        break;
                    case GAMEOBJECT_TYPE_GENERIC:
                        if (ActivateToQuest(target))
                            dynFlags |= GO_DYNFLAG_LO_SPARKLE;
                        break;
#ifdef LICH_KING
                    case GAMEOBJECT_TYPE_TRANSPORT:
                        if (const StaticTransport* t = ToStaticTransport())
                            if (t->GetPauseTime())
                            {
                                if (GetGoState() == GO_STATE_READY)
                                {
                                    if (t->GetPathProgress() >= t->GetPauseTime()) // if not, send 100% progress
                                        pathProgress = int16(float(t->GetPathProgress() - t->GetPauseTime()) / float(t->GetPeriod() - t->GetPauseTime()) * 65535.0f);
                                }
                                else
                                {
                                    if (t->GetPathProgress() <= t->GetPauseTime()) // if not, send 100% progress
                                        pathProgress = int16(float(t->GetPathProgress()) / float(t->GetPauseTime()) * 65535.0f);
                                }
                            }
                        // else it's ignored
                        break;
                    case GAMEOBJECT_TYPE_MO_TRANSPORT:
                        if (const MotionTransport* t = ToMotionTransport())
                            pathProgress = int16(float(t->GetPathProgress()) / float(t->GetPeriod()) * 65535.0f);
                        break;
#endif
                    default:
                        break;
                }

#ifdef LICH_KING
                fieldBuffer << uint16(dynFlags);
                fieldBuffer << int16(pathProgress);
#else
                fieldBuffer << uint32(dynFlags);
#endif
            }
            else if (index == GAMEOBJECT_FLAGS)
            {
                uint32 _flags = m_uint32Values[GAMEOBJECT_FLAGS];
          /*TODO LOOT      if (GetGoType() == GAMEOBJECT_TYPE_CHEST)
                    if (GetGOInfo()->chest.groupLootRules && !IsLootAllowedFor(target))
                        flags |= GO_FLAG_LOCKED | GO_FLAG_NOT_SELECTABLE; */

                fieldBuffer << _flags;
            }
            else
                fieldBuffer << m_uint32Values[index];                // other cases
        }
    }

    *data << uint8(updateMask.GetBlockCount());
    updateMask.AppendToPacket(data);
    data->append(fieldBuffer);
}

void GameObject::AddToWorld()
{
    if(!IsInWorld())
    ///- Register the gameobject for guid lookup
    {
		if (m_zoneScript)
			m_zoneScript->OnGameObjectCreate(this);

		GetMap()->GetObjectsStore().Insert<GameObject>(GetGUID(), this);

        if (m_spawnId)
            GetMap()->GetGameObjectBySpawnIdStore().insert(std::make_pair(m_spawnId, this));

        // The state can be changed after GameObject::Create but before GameObject::AddToWorld
        bool toggledState = GetGoType() == GAMEOBJECT_TYPE_CHEST ? getLootState() == GO_READY : (GetGoState() == GO_STATE_READY || IsTransport());
        if (m_model)
        {
            m_model->UpdatePosition();
            GetMap()->InsertGameObjectModel(*m_model);
        }

        EnableCollision(toggledState);
        WorldObject::AddToWorld();
    }
}

void GameObject::RemoveFromWorld()
{
    ///- Remove the gameobject from the accessor
    if(IsInWorld())
    {
		if (m_zoneScript)
			m_zoneScript->OnGameObjectRemove(this);

		//RemoveFromOwner();
        if (m_model)
            if (GetMap()->ContainsGameObjectModel(*m_model))
                GetMap()->RemoveGameObjectModel(*m_model);

        if (Transport* transport = GetTransport())
            transport->RemovePassenger(this, true);

		GetMap()->GetObjectsStore().Remove<GameObject>(GetGUID());

        if (m_spawnId)
            Trinity::Containers::MultimapErasePair(GetMap()->GetGameObjectBySpawnIdStore(), m_spawnId, this);

        WorldObject::RemoveFromWorld();
    }
}

//spawnId will be generated on save later if needed
bool GameObject::Create(uint32 guidlow, uint32 name_id, Map *map, uint32 phaseMask, Position const& pos, G3D::Quat const& rotation, uint32 animprogress, GOState go_state, uint32 ArtKit)
{
    ASSERT(map);
	SetMap(map);

    Relocate(pos);
    m_stationaryPosition.Relocate(pos);

    if(!IsPositionValid())
    {
        TC_LOG_ERROR("entities.gameobject","ERROR: Gameobject (GUID: %u Entry: %u ) not created. Suggested coordinates isn't valid (X: %f Y: %f)", guidlow, name_id, pos.GetPositionX(), pos.GetPositionY());
        return false;
    }

	SetPhaseMask(phaseMask, false);
    UpdatePositionData();

	SetZoneScript();
	if (m_zoneScript)
	{
		name_id = m_zoneScript->GetGameObjectEntry(guidlow, name_id);
		if (!name_id)
			return false;
	}

    GameObjectTemplate const* goinfo = sObjectMgr->GetGameObjectTemplate(name_id);
    if (!goinfo)
    {
        TC_LOG_ERROR("sql.sql","Gameobject (GUID: %u Entry: %u) not created: it have not exist entry in `gameobject_template`. Map: %u  (X: %f Y: %f Z: %f)",guidlow, name_id, map->GetId(), pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ());
        return false;
    }

    if (goinfo->type == GAMEOBJECT_TYPE_TRANSPORT)
    {
        m_updateFlag = (m_updateFlag | UPDATEFLAG_TRANSPORT);
        m_updateFlagLK = (m_updateFlagLK | LK_UPDATEFLAG_TRANSPORT) & ~LK_UPDATEFLAG_POSITION;
    }

    Object::_Create(guidlow, goinfo->entry, HighGuid::GameObject);

    m_goInfo = goinfo;

    if (goinfo->type >= MAX_GAMEOBJECT_TYPE)
    {
        TC_LOG_ERROR("sql.sql","Gameobject (GUID: %u Entry: %u) not created: it have not exist GO type '%u' in `gameobject_template`. It's will crash client if created.",guidlow,name_id,goinfo->type);
        return false;
    }

    SetFloatValue(GAMEOBJECT_POS_X, pos.GetPositionX());
    SetFloatValue(GAMEOBJECT_POS_Y, pos.GetPositionY());
    SetFloatValue(GAMEOBJECT_POS_Z, pos.GetPositionZ());
    SetFloatValue(GAMEOBJECT_FACING, pos.GetOrientation());                  //this is not facing angle

    SetFloatValue (GAMEOBJECT_PARENTROTATION, rotation.x);
    SetFloatValue (GAMEOBJECT_PARENTROTATION+1, rotation.y);
    SetFloatValue (GAMEOBJECT_PARENTROTATION+2, rotation.z);
    SetFloatValue (GAMEOBJECT_PARENTROTATION+3, rotation.w);

    SetObjectScale(goinfo->size);

    SetUInt32Value(GAMEOBJECT_FACTION, goinfo->faction);
    SetUInt32Value(GAMEOBJECT_FLAGS, goinfo->flags);

    SetEntry(goinfo->entry);
    SetName(goinfo->name);

    SetDisplayId(goinfo->displayId);

    m_model = CreateModel();
    
    SetGoType(GameobjectTypes(goinfo->type));
    m_prevGoState = go_state;
    SetGoState(GOState(go_state));

    SetGoArtKit(ArtKit);

	switch (goinfo->type)
	{
	case GAMEOBJECT_TYPE_TRAP:
		if (GetGOInfo()->trap.stealthed)
		{
			m_stealth.AddFlag(STEALTH_TRAP);
			m_stealth.AddValue(STEALTH_TRAP, 350); //TC has 70 here
		}

		if (GetGOInfo()->trap.invisible)
		{
			m_invisibility.AddFlag(INVISIBILITY_TRAP);
			m_invisibility.AddValue(INVISIBILITY_TRAP, 300);
		}
		break;
	default:
		SetGoAnimProgress(animprogress);
		break;
	}

    // Spell charges for GAMEOBJECT_TYPE_SPELLCASTER (22)
    if (goinfo->type == GAMEOBJECT_TYPE_SPELLCASTER)
        m_charges = goinfo->GetCharges();

    //Notify the map's instance data.
    //Only works if you create the object in it, not if it is moves to that map.
    //Normally non-players do not teleport to other maps.
    if(map->IsDungeon() && ((InstanceMap*)map)->GetInstanceScript())
    {
        ((InstanceMap*)map)->GetInstanceScript()->OnGameObjectCreate(this);
    }
    
    LastUsedScriptID = GetScriptId();

    return true;
}

void GameObject::Update(uint32 diff)
{
    if(!m_AI)
        if (!AIM_Initialize())
            TC_LOG_ERROR("misc","Could not initialize GameObjectAI");

    switch (m_lootState)
    {
        case GO_NOT_READY:
        {
            switch(GetGoType())
            {
                case GAMEOBJECT_TYPE_TRAP:
                {
                    // Arming Time for GAMEOBJECT_TYPE_TRAP (6)
                    Unit* owner = GetOwner();
                    if (owner && owner->IsInCombat())
                        m_cooldownTime = GameTime::GetGameTimeMS() + GetGOInfo()->GetCooldown() * SECOND * IN_MILLISECONDS;
                    else if (GetEntry() == 180647)
                        m_cooldownTime = GameTime::GetGameTimeMS() + GetGOInfo()->GetCooldown() * SECOND * IN_MILLISECONDS;
                    m_lootState = GO_READY;
                    break;
                }
                case GAMEOBJECT_TYPE_FISHINGNODE:
                {
                    // fishing code (bobber ready)
                    if( time(nullptr) > m_respawnTime - FISHING_BOBBER_READY_TIME )
                    {
                        // splash bobber (bobber ready now)
                        Unit* caster = GetOwner();
                        if(caster && caster->GetTypeId()==TYPEID_PLAYER)
                        {
                            Player* casterPlayer = (caster->ToPlayer());
                            SetGoState(GO_STATE_ACTIVE);
                            SetUInt32Value(GAMEOBJECT_FLAGS, GO_FLAG_NODESPAWN);

                            UpdateData udata;
                            WorldPacket packet;
                            BuildValuesUpdateBlockForPlayer(&udata,(caster->ToPlayer()));
                            udata.BuildPacket(&packet, casterPlayer->GetSession()->GetClientBuild());
                            casterPlayer->SendDirectMessage(&packet);

                            SendCustomAnim(GetGoAnimProgress());
                        }

                        m_lootState = GO_READY;                 // can be successfully open with some chance
                    }
                    return;
                }
                default:
                    m_lootState = GO_READY;                         // for other GOis same switched without delay to GO_READY
                    break;
            }
        }
        /* no break for switch (m_lootState)*/
        case GO_READY:
        {
            if (m_respawnTime > 0)                          // timer on
            {
                if (m_respawnTime <= time(nullptr))            // timer expired
                {
                    m_respawnTime = 0;
                    m_SkillupList.clear();
                    m_usetimes = 0;

                    switch (GetGoType())
                    {
                        case GAMEOBJECT_TYPE_FISHINGNODE:   //  can't fish now
                        {
                            Unit* caster = GetOwner();
                            if(caster && caster->GetTypeId()==TYPEID_PLAYER)
                            {
                                if(caster->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
                                {
                                    caster->GetCurrentSpell(CURRENT_CHANNELED_SPELL)->SendChannelUpdate(0);
                                    caster->GetCurrentSpell(CURRENT_CHANNELED_SPELL)->finish(false);
                                }

                                WorldPacket data(SMSG_FISH_NOT_HOOKED,0);
                                (caster->ToPlayer())->SendDirectMessage(&data);
                            }
                            // can be delete
                            m_lootState = GO_JUST_DEACTIVATED;
                            return;
                        }
                        case GAMEOBJECT_TYPE_DOOR:
                        case GAMEOBJECT_TYPE_BUTTON:
                            //we need to open doors if they are closed (add there another condition if this code breaks some usage, but it need to be here for battlegrounds)
                            if( !GetGoState() )
                                SwitchDoorOrButton(false);
                            //flags in AB are type_button and we need to add them here so no break!
                        default:
                            if(!m_spawnedByDefault)         // despawn timer
                            {
                                // can be despawned or destroyed
                                SetLootState(GO_JUST_DEACTIVATED);
                                return;
                            }
                            // respawn timer
                            if(!IsInWorld())
                                GetMap()->AddToMap(this);
                            break;
                    }


                    // Call AI Reset (required for example in SmartAI to clear one time events)
                    if (AI())
                        AI()->Reset();
                }
            }

            if(m_inactive)
                return;

            // traps can have time and can not have
            GameObjectTemplate const* goInfo = GetGOInfo();
            if(goInfo->type == GAMEOBJECT_TYPE_TRAP)
            {
                if (!this->IsInWorld())
                    return;

                // traps
                Unit* owner = GetOwner();
                Unit* trapTarget =  nullptr;                            // pointer to appropriate target if found any

                if(m_cooldownTime > GameTime::GetGameTimeMS())
                    return;

                bool IsBattlegroundTrap = false;
                //FIXME: this is activation radius (in different casting radius that must be selected from spell data)
                //TODO: move activated state code (cast itself) to GO_ACTIVATED, in this place only check activating and set state
                float radius = (float)(goInfo->trap.radius)/2; // TODO rename radius to diameter (goInfo->trap.radius) should be (goInfo->trap.diameter)

                if(!radius)
                {
                    if(goInfo->GetCooldown() != 3)            // cast in other case (at some triggering/linked go/etc explicit call)
                    {
                        // try to read radius from trap spell
                        if (const SpellInfo *spellEntry = sSpellMgr->GetSpellInfo(goInfo->trap.spellId))
                            radius = spellEntry->Effects[0].CalcRadius(owner ? owner->GetSpellModOwner(): nullptr);

                        if(!radius)
                            break;
                    }
                    else
                    {
                        if(m_respawnTime > 0)
                            break;

                        radius = goInfo->GetCooldown();       // battlegrounds gameobjects has data2 == 0 && data5 == 3
                        IsBattlegroundTrap = true;
                    }
                }

                bool NeedDespawn = (goInfo->trap.charges != 0);

                // Note: this hack with search required until GO casting not implemented
                // search unfriendly creature
                if(owner && NeedDespawn)                    // hunter trap
                {
                    Trinity::AnyUnfriendlyAoEAttackableUnitInObjectRangeCheck u_check(this, owner, radius);
                    Trinity::UnitSearcher<Trinity::AnyUnfriendlyAoEAttackableUnitInObjectRangeCheck> checker(this, trapTarget, u_check);

                    this->VisitNearbyGridObject(radius, checker);

                    // or unfriendly player/pet
                    if(!trapTarget)
                    {
                        this->VisitNearbyWorldObject(radius, checker);
                    }
                }
				else if (GetOwnerGUID())
				{ // We got a owner but it was not found? Possible case: Hunter traps from disconnected players
					m_despawnTime = 1; //trigger despawn
				}
                else // environmental trap
                {   
                    // environmental damage spells already have around enemies targeting but this not help in case not existed GO casting support

                    // affect only players
                    Player* p_ok = nullptr;
                    Trinity::AnyPlayerInObjectRangeCheck p_check(this, radius);
                    Trinity::PlayerSearcher<Trinity::AnyPlayerInObjectRangeCheck> checker(this, p_ok, p_check);
                    this->VisitNearbyWorldObject(radius, checker);
                    trapTarget = p_ok;
                }

                if (trapTarget)
                {
                    CastSpell(trapTarget, goInfo->trap.spellId,GetOwnerGUID());
                    if(owner)
                        if(SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(goInfo->trap.spellId))
                            owner->ProcDamageAndSpell(trapTarget,PROC_FLAG_ON_TRAP_ACTIVATION,PROC_FLAG_NONE,0,0,BASE_ATTACK,spellInfo);
                    
                    m_cooldownTime = GameTime::GetGameTimeMS() + (m_goInfo->GetCooldown() ? m_goInfo->GetCooldown() * SECOND * IN_MILLISECONDS : 4 * SECOND * IN_MILLISECONDS);

                    if(NeedDespawn)
                        SetLootState(GO_JUST_DEACTIVATED);  // can be despawned or destroyed

                    if(IsBattlegroundTrap && trapTarget->GetTypeId() == TYPEID_PLAYER)
                    {
                        //Battleground gameobjects case
                        if((trapTarget->ToPlayer())->InBattleground())
                            if(Battleground *bg = (trapTarget->ToPlayer())->GetBattleground())
                                bg->HandleTriggerBuff(GetGUID());
                    }
                }
            }

            if ((m_charges && m_usetimes >= m_charges) || (m_despawnTime && m_despawnTime <= time(nullptr)))
                SetLootState(GO_JUST_DEACTIVATED);          // can be despawned or destroyed


            break;
        }
        case GO_ACTIVATED:
        {
            switch(GetGoType())
            {
                case GAMEOBJECT_TYPE_DOOR:
                case GAMEOBJECT_TYPE_BUTTON:
                case GAMEOBJECT_TYPE_GOOBER:
                    if(GetAutoCloseTime() && (m_cooldownTime < GameTime::GetGameTimeMS()))
                    {
                        SwitchDoorOrButton(false);
                        SetLootState(GO_JUST_DEACTIVATED);
                    }
                    break;
                case GAMEOBJECT_TYPE_CHEST:
                    if(m_groupLootTimer && lootingGroupLeaderGUID)
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
                    break;
                default:
                    break;
            }// m_despawnTime ?
            break;
        }
        case GO_JUST_DEACTIVATED:
        {
            //if Gameobject should cast spell, then this, but some GOs (type = 10) should be destroyed
            if (GetGoType() == GAMEOBJECT_TYPE_GOOBER)
            {
                uint32 spellId = GetGOInfo()->goober.spellId;

                if(spellId)
                {
                    auto it = m_unique_users.begin();
                    auto end = m_unique_users.end();
                    for (; it != end; it++)
                    {
                        Unit* owner = ObjectAccessor::GetUnit(*this, uint64(*it));
                        if (owner) owner->CastSpell(owner, spellId, false);
                    }

                    m_unique_users.clear();
                    m_usetimes = 0;
                }
                //any return here in case battleground traps
            }

            if(GetOwnerGUID())
            {
                if (Unit* owner = GetOwner())
                    owner->RemoveGameObject(this, false);

                m_respawnTime = 0;
                Delete();
                return;
            }

            //burning flags in some battlegrounds, if you find better condition, just add it
            if (GetGoAnimProgress() > 0)
            {
                SendObjectDeSpawnAnim(this->GetGUID());
                //reset flags
                SetUInt32Value(GAMEOBJECT_FLAGS, GetGOInfo()->flags);
            }

            loot.clear();
            m_despawnTime = 0;
            SetLootState(GO_READY);

            if(!m_respawnDelayTime)
                return;

            if(!m_spawnedByDefault)
            {
                m_respawnTime = 0;
                return;
            }

            m_respawnTime = time(nullptr) + m_respawnDelayTime;

            // if option not set then object will be saved at grid unload
            if(sWorld->getConfig(CONFIG_SAVE_RESPAWN_TIME_IMMEDIATELY))
                SaveRespawnTime();

			DestroyForNearbyPlayers();
            break;
        }
    }
    
    if (AI())
        AI()->UpdateAI(diff);

    sScriptMgr->OnGameObjectUpdate(this, diff);
}

void GameObject::Refresh()
{
    // not refresh despawned not casted GO (despawned casted GO destroyed in all cases anyway)
    if(m_respawnTime > 0 && m_spawnedByDefault)
        return;

    if(isSpawned())
       GetMap()->AddToMap(this);
}

void GameObject::AddUniqueUse(Player* player)
{
    if(m_unique_users.find(player->GetGUIDLow()) != m_unique_users.end())
        return;
    AddUse();
    m_unique_users.insert(player->GetGUIDLow());
}

void GameObject::Delete()
{
    SendObjectDeSpawnAnim(GetGUID());

    SetGoState(GO_STATE_READY);
    SetUInt32Value(GAMEOBJECT_FLAGS, GetGOInfo()->flags);

    AddObjectToRemoveList();
}

void GameObject::getFishLoot(Loot *fishloot)
{
    fishloot->clear();

    uint32 subzone = GetAreaId();

    // if subzone loot exist use it
    if(LootTemplates_Fishing.HaveLootFor(subzone))
        fishloot->FillLoot(subzone, LootTemplates_Fishing, nullptr);
    // else use zone loot
    else
        fishloot->FillLoot(GetZoneId(), LootTemplates_Fishing, nullptr);
}

void GameObject::SaveToDB()
{
    // this should only be used when the gameobject has already been loaded
    // preferably after adding to map, because mapid may not be valid otherwise
    GameObjectData const *data = sObjectMgr->GetGOData(m_spawnId);
    if(!data)
    {
        TC_LOG_ERROR("FIXME","GameObject::SaveToDB failed, cannot get gameobject data!");
        return;
    }

    SaveToDB(GetMapId(), data->spawnMask);
}

void GameObject::SaveToDB(uint32 mapid, uint8 spawnMask)
{
    const GameObjectTemplate *goI = GetGOInfo();

    if (!goI)
        return;

    if (!m_spawnId)
		m_spawnId = sObjectMgr->GenerateGameObjectSpawnId();

    // update in loaded data (changing data only in this place)
    GameObjectData& data = sObjectMgr->NewGOData(m_spawnId);

    // data->guid = guid don't must be update at save
    data.id = GetEntry();
    data.mapid = mapid;
    data.posX = GetFloatValue(GAMEOBJECT_POS_X);
    data.posY = GetFloatValue(GAMEOBJECT_POS_Y);
    data.posZ = GetFloatValue(GAMEOBJECT_POS_Z);
    data.orientation = GetFloatValue(GAMEOBJECT_FACING);
    data.rotation = G3D::Quat(
        GetFloatValue(GAMEOBJECT_PARENTROTATION + 0), 
        GetFloatValue(GAMEOBJECT_PARENTROTATION + 1), 
        GetFloatValue(GAMEOBJECT_PARENTROTATION + 2), 
        GetFloatValue(GAMEOBJECT_PARENTROTATION + 3));
    data.spawntimesecs = m_spawnedByDefault ? m_respawnDelayTime : -(int32)m_respawnDelayTime;
    data.animprogress = GetGoAnimProgress();
    data.go_state = GetGoState();
    data.spawnMask = spawnMask;
    data.ArtKit = GetUInt32Value (GAMEOBJECT_ARTKIT);

    // updated in DB
    std::ostringstream ss;
    ss << "INSERT INTO gameobject VALUES ( "
        << m_spawnId << ", "
        << GetUInt32Value (OBJECT_FIELD_ENTRY) << ", "
        << mapid << ", "
        << (uint32)spawnMask << ", "
        << GetFloatValue(GAMEOBJECT_POS_X) << ", "
        << GetFloatValue(GAMEOBJECT_POS_Y) << ", "
        << GetFloatValue(GAMEOBJECT_POS_Z) << ", "
        << GetFloatValue(GAMEOBJECT_FACING) << ", "
        << GetFloatValue(GAMEOBJECT_PARENTROTATION) << ", "
        << GetFloatValue(GAMEOBJECT_PARENTROTATION+1) << ", "
        << GetFloatValue(GAMEOBJECT_PARENTROTATION+2) << ", "
        << GetFloatValue(GAMEOBJECT_PARENTROTATION+3) << ", "
        << m_respawnDelayTime << ", "
        << GetGoAnimProgress() << ", "
        << GetGoState() << ")";

    SQLTransaction trans = WorldDatabase.BeginTransaction();
    trans->PAppend("DELETE FROM gameobject WHERE guid = '%u'", m_spawnId);
    trans->PAppend( ss.str( ).c_str( ) );
    WorldDatabase.CommitTransaction(trans);
}

bool GameObject::LoadFromDB(uint32 guid, Map *map)
{
    GameObjectData const* data = sObjectMgr->GetGOData(guid);

    if( !data )
    {
        TC_LOG_ERROR("sql.sql","ERROR: Gameobject (GUID: %u) not found in table `gameobject`, can't load. ",guid);
        return false;
    }

    uint32 entry = data->id;
    float x = data->posX;
    float y = data->posY;
    float z = data->posZ;
    float ang = data->orientation;

    uint32 animprogress = data->animprogress;
    uint32 go_state = data->go_state;
    uint32 ArtKit = data->ArtKit;

    m_spawnId = guid;

    if (!Create(map->GenerateLowGuid<HighGuid::GameObject>(),entry, map, PHASEMASK_NORMAL, Position(x, y, z, ang), data->rotation, animprogress, GOState(go_state), ArtKit) )
        return false;

    switch(GetGOInfo()->type)
    {
        case GAMEOBJECT_TYPE_DOOR:
        case GAMEOBJECT_TYPE_BUTTON:
            /* this code (in comment) isn't correct because in battlegrounds we need despawnable doors and buttons, pls remove
            SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_NODESPAWN);
            m_spawnedByDefault = true;
            m_respawnDelayTime = 0;
            m_respawnTime = 0;
            break;*/
        default:
            if(data->spawntimesecs >= 0)
            {
                m_spawnedByDefault = true;
                m_respawnDelayTime = data->spawntimesecs;
                m_respawnTime = sObjectMgr->GetGORespawnTime(m_spawnId, map->GetInstanceId());

                                                            // ready to respawn
                if(m_respawnTime && m_respawnTime <= time(nullptr))
                {
                    m_respawnTime = 0;
                    sObjectMgr->SaveGORespawnTime(m_spawnId,GetMapId(),GetInstanceId(),0);
                }
            }
            else
            {
                m_spawnedByDefault = false;
                m_respawnDelayTime = -data->spawntimesecs;
                m_respawnTime = 0;
            }
            break;
    }

    m_goData = data;

    return true;
}

void GameObject::DeleteFromDB()
{
    sObjectMgr->SaveGORespawnTime(m_spawnId,GetMapId(), GetInstanceId(),0);
    sObjectMgr->DeleteGOData(m_spawnId);
    WorldDatabase.PExecute("DELETE FROM gameobject WHERE guid = '%u'", m_spawnId);
    WorldDatabase.PExecute("DELETE FROM game_event_gameobject WHERE guid = '%u'", m_spawnId);
}

void GameObject::SetLootState(LootState state, Unit* unit)
{
    m_lootState = state;

    if(AI())
        AI()->OnLootStateChanged(state, unit);

    sScriptMgr->OnGameObjectLootStateChanged(this, state, unit);

    /*if (m_model)
    {
        // startOpen determines whether we are going to add or remove the LoS on activation
        bool startOpen = (GetGoType() == GAMEOBJECT_TYPE_DOOR || GetGoType() == GAMEOBJECT_TYPE_BUTTON ? GetGOInfo()->door.startOpen : false);
        
        //if (GetGoState() == GO_NOT_READY)
            //startOpen = !startOpen;
        
        if (GetGoType() == GAMEOBJECT_TYPE_DOOR) {
            if (state == GO_READY || state == GO_ACTIVATED) // Opening
                EnableCollision(false);
            else // Closing
                EnableCollision(true);
        }
        else {
            if (state == GO_ACTIVATED || state == GO_JUST_DEACTIVATED)
                EnableCollision(startOpen);
            else if (state == GO_READY)
                EnableCollision(!startOpen);
        }
    }*/
}

void GameObject::SetGoState(GOState state)
{
    SetUInt32Value(GAMEOBJECT_STATE, state);
    if(AI())
        AI()->OnStateChanged(state, nullptr);

    sScriptMgr->OnGameObjectStateChanged(this, state);

    if (m_model && !IsTransport())
    {
        if (!IsInWorld())
            return;

        // startOpen determines whether we are going to add or remove the LoS on activation
        bool startOpen = (GetGoType() == GAMEOBJECT_TYPE_DOOR || GetGoType() == GAMEOBJECT_TYPE_BUTTON ? GetGOInfo()->door.startOpen : false);

        /*if (GetGoState() == GO_NOT_READY)
            startOpen = !startOpen;*/
        
        if (GetGoType() == GAMEOBJECT_TYPE_DOOR) {
            if (!state) // Opening
                EnableCollision(false);
            else // Closing
                EnableCollision(true);
        }
        else {
            if (state == GO_STATE_ACTIVE || state == GO_STATE_ACTIVE_ALTERNATIVE)
                EnableCollision(!startOpen);
            else if (state == GO_STATE_READY)
                EnableCollision(startOpen);
        }
    }
}

void GameObject::SetDisplayId(uint32 displayid)
{
    SetUInt32Value(GAMEOBJECT_DISPLAYID, displayid);
    UpdateModel();
}

void GameObject::SetPhaseMask(uint32 newPhaseMask, bool update)
{
    WorldObject::SetPhaseMask(newPhaseMask, update);

    if (m_model && m_model->isEnabled())
        EnableCollision(true);
}

void GameObject::EnableCollision(bool enable)
{
    if (!m_model)
        return;

    m_model->enable(enable);
}

void GameObject::UpdateModel()
{
    if (!IsInWorld())
        return;
    if (m_model)
    {
        if (GetMap()->ContainsGameObjectModel(*m_model))
            GetMap()->RemoveGameObjectModel(*m_model);

        delete m_model;
    }

    m_model = CreateModel();
    if (m_model)
        GetMap()->InsertGameObjectModel(*m_model);
}

GameObject* GameObject::GetGameObject(WorldObject& object, uint64 guid)
{
    return ObjectAccessor::GetGameObject(object,guid);
}

GameObjectTemplate const *GameObject::GetGOInfo() const
{
    return m_goInfo;
}

/*********************************************************/
/***                    QUEST SYSTEM                   ***/
/*********************************************************/
bool GameObject::HasQuest(uint32 quest_id) const
{
    QuestRelations const& qr = sObjectMgr->mGOQuestRelations;
    for(auto itr = qr.lower_bound(GetEntry()); itr != qr.upper_bound(GetEntry()); ++itr)
    {
        if(itr->second==quest_id)
            return true;
    }
    return false;
}

bool GameObject::HasInvolvedQuest(uint32 quest_id) const
{
    QuestRelations const& qr = sObjectMgr->mGOQuestInvolvedRelations;
    for(auto itr = qr.lower_bound(GetEntry()); itr != qr.upper_bound(GetEntry()); ++itr)
    {
        if(itr->second==quest_id)
            return true;
    }
    return false;
}

bool GameObject::IsTransport() const
{
    // If something is marked as a transport, don't transmit an out of range packet for it.
    GameObjectTemplate const * gInfo = GetGOInfo();
    if(!gInfo) return false;
    return gInfo->type == GAMEOBJECT_TYPE_TRANSPORT || gInfo->type == GAMEOBJECT_TYPE_MO_TRANSPORT;
}

Unit* GameObject::GetOwner() const
{
    return ObjectAccessor::GetUnit(*this, GetOwnerGUID());
}

void GameObject::SaveRespawnTime()
{
    if(m_respawnTime > time(nullptr) && m_spawnedByDefault)
        sObjectMgr->SaveGORespawnTime(m_spawnId,GetMapId(), GetInstanceId(),m_respawnTime);
}

/*
bool GameObject::IsVisibleForInState(Player const* u, bool inVisibleList) const
{
    // Not in world
    if(!IsInWorld() || !u->IsInWorld())
        return false;

    // Transport always visible at this step implementation
    if(IsTransport() && IsInMap(u))
        return true;
        
    // Hack for Brutallus Intro
    if (GetEntry() == 188119)
        return true;
        
    if (GetEntry() == 180647)
        return true;

    // quick check visibility false cases for non-GM-mode or gm in video group
    if(!(u->IsGameMaster() ) && !u->isSpectator())
    {
        // despawned and then not visible for non-GM in GM-mode
        if(!isSpawned())
            return false;

        // special invisibility cases
        if(GetGOInfo()->type == GAMEOBJECT_TYPE_TRAP && GetGOInfo()->trap.stealthed)
        {
            Unit *owner = GetOwner();
            if(owner && u->IsHostileTo(owner) && !canDetectTrap(u, GetDistance(u)))
                return false;
        }

        // Smuggled Mana Cell required 10 invisibility type detection/state
        if(GetEntry()==187039 && ((u->m_detectInvisibilityMask | u->m_invisibilityMask) & (1<<10))==0)
            return false;
    }

    const WorldObject* target = u->GetFarsightTarget();
    if (!target || !u->HasFarsightVision()) // Vision needs to be on the farsight target
        target = u;

    // check distance
    return IsWithinDistInMap(target,World::GetMaxVisibleDistanceForObject() +
        (inVisibleList ? World::GetVisibleObjectGreyDistance() : 0.0f), false);
}
*/

void GameObject::Respawn()
{
    if(m_spawnedByDefault && m_respawnTime > 0)
    {
        m_respawnTime = time(nullptr);
        sObjectMgr->SaveGORespawnTime(m_spawnId,GetMapId(), GetInstanceId(),0);
    }
    this->loot.ClearRemovedItemsList();
}

bool GameObject::ActivateToQuest( Player *pTarget)const
{
    if(!sObjectMgr->IsGameObjectForQuests(GetEntry()))
        return false;

    switch(GetGoType())
    {
        // scan GO chest with loot including quest items
        case GAMEOBJECT_TYPE_CHEST:
        {
            if(LootTemplates_Gameobject.HaveQuestLootForPlayer(GetGOInfo()->GetLootId(), pTarget))
            {
                //TODO: fix this hack
                //look for battlegroundAV for some objects which are only activated after mine gots captured by own team
                if(GetEntry() == BG_AV_OBJECTID_MINE_N || GetEntry() == BG_AV_OBJECTID_MINE_S)
                    if(Battleground *bg = pTarget->GetBattleground())
                        if(bg->GetTypeID() == BATTLEGROUND_AV && !(((BattlegroundAV*)bg)->PlayerCanDoMineQuest(GetEntry(),pTarget->GetTeam())))
                            return false;
                return true;
            }

            //some gobjects are activated even with no object for the quest
            if(GetGOInfo()->chest.questId && pTarget->HasQuest(GetGOInfo()->chest.questId))
                return true;
            break;
        }
        case GAMEOBJECT_TYPE_GOOBER:
        {
            if(pTarget->GetQuestStatus(GetGOInfo()->goober.questId) == QUEST_STATUS_INCOMPLETE)
                return true;
            break;
        }
        default:
            break;
    }

    return false;
}

void GameObject::TriggeringLinkedGameObject( uint32 trapEntry, Unit* target)
{
    GameObjectTemplate const* trapInfo = sObjectMgr->GetGameObjectTemplate(trapEntry);
    if(!trapInfo || trapInfo->type!=GAMEOBJECT_TYPE_TRAP)
        return;

    SpellInfo const* trapSpell = sSpellMgr->GetSpellInfo(trapInfo->trap.spellId);
    if(!trapSpell)                                          // checked at load already
        return;

    float range = float(target->GetSpellMaxRangeForTarget(GetOwner(), trapSpell));

    // search nearest linked GO
    GameObject* trapGO = nullptr;
    {
        // using original GO distance
        Trinity::NearestGameObjectEntryInObjectRangeCheck go_check(*target,trapEntry,range);
        Trinity::GameObjectLastSearcher<Trinity::NearestGameObjectEntryInObjectRangeCheck> checker(target, trapGO,go_check);
        VisitNearbyGridObject(range, checker);
    }

    // found correct GO
    if(trapGO)
        CastSpell(target,trapSpell->Id);
}

GameObject* GameObject::LookupFishingHoleAround(float range)
{
    GameObject* ok = nullptr;

    Trinity::NearestGameObjectFishingHole u_check(*this, range);
    Trinity::GameObjectSearcher<Trinity::NearestGameObjectFishingHole> checker(ok, u_check);
    VisitNearbyGridObject(range, checker);

    return ok;
}

void GameObject::UseDoorOrButton(uint32 time_to_restore /* = 0 */, bool alternative /* = false */, Unit* user /*=NULL*/)
{
    if(m_lootState != GO_READY)
        return;

    if(!time_to_restore)
        time_to_restore = GetAutoCloseTime();

    SwitchDoorOrButton(true,alternative);
    SetLootState(GO_ACTIVATED,user);

    m_cooldownTime = time_to_restore ? (GameTime::GetGameTimeMS() + time_to_restore * SECOND * IN_MILLISECONDS) : 0;
}

void GameObject::ResetDoorOrButton()
{
    if (m_lootState == GO_READY || m_lootState == GO_JUST_DEACTIVATED)
        return;

    RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_IN_USE);
    SetGoState(m_prevGoState);

    SetLootState(GO_JUST_DEACTIVATED);
    SetGoState(GO_STATE_READY);
    m_cooldownTime = 0;
}


bool GameObject::IsNeverVisible() const
{
	if (WorldObject::IsNeverVisible())
		return true;

	if (GetGoType() == GAMEOBJECT_TYPE_SPELL_FOCUS && GetGOInfo()->spellFocus.serverOnly == 1)
		return true;

	return false;
}

bool GameObject::IsAlwaysVisibleFor(WorldObject const* seer) const
{
	if (WorldObject::IsAlwaysVisibleFor(seer))
		return true;

	if (IsTransport() || IsMotionTransport() 
#ifdef LICH_KING
		|| IsDestructibleBuilding()
#endif
		)
		return true;

	if (!seer)
		return false;

	// Always seen by owner and friendly units
	if (ObjectGuid guid = GetOwnerGUID())
	{
		if (seer->GetGUID() == guid)
			return true;

		Unit* owner = GetOwner();
		if (Unit const* unitSeer = seer->ToUnit())
			if (owner && owner->IsFriendlyTo(unitSeer))
				return true;
	}

	return false;
}

bool GameObject::IsInvisibleDueToDespawn() const
{
	if (WorldObject::IsInvisibleDueToDespawn())
		return true;

	// Despawned
	if (!isSpawned())
		return true;

	return false;
}

void GameObject::SetGoArtKit(uint32 kit)
{
    SetUInt32Value(GAMEOBJECT_ARTKIT, kit);
    GameObjectData *data = const_cast<GameObjectData*>(sObjectMgr->GetGOData(m_spawnId));
    if(data)
        data->ArtKit = kit;
}

void GameObject::SwitchDoorOrButton(bool activate, bool alternative /* = false */)
{
    if (activate)
        SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_IN_USE);
    else
        RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_IN_USE);

    if (GetGoState() == GO_STATE_READY)                      //if closed -> open
        SetGoState(alternative ? GO_STATE_ACTIVE_ALTERNATIVE : GO_STATE_ACTIVE);
    else                                                    //if open -> close
        SetGoState(GO_STATE_READY);
}

void GameObject::Use(Unit* user)
{
    // by default spell caster is user
    Unit* spellCaster = user;
    uint32 spellId = 0;

    if (Player* playerUser = user->ToPlayer())
    {
        if (sScriptMgr->OnGossipHello(playerUser, this))
            return;

        if (AI()->GossipHello(playerUser, false))
            return;
    }

    switch(GetGoType())
    {
        case GAMEOBJECT_TYPE_DOOR:                          //0
        case GAMEOBJECT_TYPE_BUTTON:                        //1
            //doors/buttons never really despawn, only reset to default state/flags
            UseDoorOrButton();

            // activate script
            GetMap()->ScriptsStart(sGameObjectScripts, GetSpawnId(), spellCaster, this);
            return;

        case GAMEOBJECT_TYPE_QUESTGIVER:                    //2
        {
            if(user->GetTypeId()!=TYPEID_PLAYER)
                return;

            Player* player = user->ToPlayer();

            player->PrepareGossipMenu(this, GetGOInfo()->questgiver.gossipID, true);
            player->SendPreparedGossip(this);;
            return;
        }
        //Sitting: Wooden bench, chairs enzz
        case GAMEOBJECT_TYPE_CHAIR:                         //7
        {
            GameObjectTemplate const* info = GetGOInfo();
            if(!info)
                return;

            if(user->GetTypeId()!=TYPEID_PLAYER)
                return;

            Player* player = user->ToPlayer();

            // a chair may have n slots. we have to calculate their positions and teleport the player to the nearest one

            // check if the db is sane
            if(info->chair.slots > 0)
            {
                float lowestDist = DEFAULT_VISIBILITY_DISTANCE;

                float x_lowest = GetPositionX();
                float y_lowest = GetPositionY();

                // the object orientation + 1/2 pi
                // every slot will be on that straight line
                float orthogonalOrientation = GetOrientation()+M_PI*0.5f;
                // find nearest slot
                for(uint32 i=0; i<info->chair.slots; i++)
                {
                    // the distance between this slot and the center of the go - imagine a 1D space
                    float relativeDistance = (info->size*i)-(info->size*(info->chair.slots-1)/2.0f);

                    float x_i = GetPositionX() + relativeDistance * cos(orthogonalOrientation);
                    float y_i = GetPositionY() + relativeDistance * sin(orthogonalOrientation);

                    // calculate the distance between the player and this slot
                    float thisDistance = player->GetDistance2d(x_i, y_i);

                    if(thisDistance <= lowestDist)
                    {
                        lowestDist = thisDistance;
                        x_lowest = x_i;
                        y_lowest = y_i;
                    }
                }
                player->TeleportTo(GetMapId(), x_lowest, y_lowest, GetPositionZ(), GetOrientation(),TELE_TO_NOT_LEAVE_TRANSPORT | TELE_TO_NOT_LEAVE_COMBAT | TELE_TO_NOT_UNSUMMON_PET);
            }
            else
            {
                // fallback, will always work
                player->TeleportTo(GetMapId(), GetPositionX(), GetPositionY(), GetPositionZ(), GetOrientation(),TELE_TO_NOT_LEAVE_TRANSPORT | TELE_TO_NOT_LEAVE_COMBAT | TELE_TO_NOT_UNSUMMON_PET);
            }
            player->SetStandState(PLAYER_STATE_SIT_LOW_CHAIR+info->chair.height);
            return;
        }
        //big gun, its a spell/aura
        case GAMEOBJECT_TYPE_GOOBER:                        //10
        {
            GameObjectTemplate const* info = GetGOInfo();

            if(user->GetTypeId()==TYPEID_PLAYER)
            {
                Player* player = user->ToPlayer();

                // show page
                if(info->goober.pageId)
                {
                    WorldPacket data(SMSG_GAMEOBJECT_PAGETEXT, 8);
                    data << GetGUID();
                    player->SendDirectMessage(&data);
                } else if (info->goober.gossipID)
                {
                    player->PrepareGossipMenu(this, info->goober.gossipID);
                    player->SendPreparedGossip(this);
                }

                if (info->goober.eventId)
                {
                    TC_LOG_DEBUG("maps.script", "Goober ScriptStart id %u for GO entry %u (GUID %u).", info->goober.eventId, GetEntry(), GetSpawnId());
                    GetMap()->ScriptsStart(sEventScripts, info->goober.eventId, player, this);
                   // EventInform(info->goober.eventId, user);
                }

                // possible quest objective for active quests
                player->CastedCreatureOrGO(info->entry, GetGUID(), 0);
            }

            if (info->GetAutoCloseTime())
            {
                SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_IN_USE);
                SetLootState(GO_ACTIVATED, user);
                if (!info->goober.customAnim)
                    SetGoState(GO_STATE_ACTIVE);
            }

            // cast this spell later if provided
            spellId = info->goober.spellId;

            break;
        }
        case GAMEOBJECT_TYPE_CAMERA:                        //13
        {
            GameObjectTemplate const* info = GetGOInfo();
            if(!info)
                return;

            if(user->GetTypeId()!=TYPEID_PLAYER)
                return;

            Player* player = user->ToPlayer();

            if(info->camera.cinematicId)
            {
                WorldPacket data(SMSG_TRIGGER_CINEMATIC, 4);
                data << info->camera.cinematicId;
                player->SendDirectMessage(&data);
            }

			if (GetEntry() == 187578)
				SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_IN_USE);

            return;
            
        }
        //fishing bobber
        case GAMEOBJECT_TYPE_FISHINGNODE:                   //17
        {
            if(user->GetTypeId()!=TYPEID_PLAYER)
                return;

            Player* player = user->ToPlayer();

            if(player->GetGUID() != GetOwnerGUID())
                return;

            switch(getLootState())
            {
                case GO_READY:                              // ready for loot
                {
                    // 1) skill must be >= base_zone_skill
                    // 2) if skill == base_zone_skill => 5% chance
                    // 3) chance is linear dependence from (base_zone_skill-skill)

                    uint32 subzone = GetAreaId();

                    int32 zone_skill = sObjectMgr->GetFishingBaseSkillLevel( subzone );
                    if(!zone_skill)
                        zone_skill = sObjectMgr->GetFishingBaseSkillLevel( GetZoneId() );

                    //provide error, no fishable zone or area should be 0
                    if(!zone_skill)
                        TC_LOG_ERROR("entities.gameobject","Fishable areaId %u are not properly defined in `skill_fishing_base_level`.",subzone);

                    int32 skill = player->GetSkillValue(SKILL_FISHING);
                    int32 chance = skill - zone_skill + 5;
                    int32 roll = GetMap()->irand(1,100);

                    TC_LOG_DEBUG("entities.gameobject","Fishing check (skill: %i zone min skill: %i chance %i roll: %i",skill,zone_skill,chance,roll);

                    if(skill >= zone_skill && chance >= roll)
                    {
                        // prevent removing GO at spell cancel
                        player->RemoveGameObject(this,false);
                        SetOwnerGUID(player->GetGUID());

                        //fish catched
                        player->UpdateFishingSkill();

                        GameObject* ok = LookupFishingHoleAround(DEFAULT_VISIBILITY_DISTANCE);
                        if (ok)
                        {
                            player->SendLoot(ok->GetGUID(),LOOT_FISHINGHOLE);
                            SetLootState(GO_JUST_DEACTIVATED);
                        }
                        else
                            player->SendLoot(GetGUID(),LOOT_FISHING);
                    }
                    else
                    {
                        // fish escaped, can be deleted now
                        SetLootState(GO_JUST_DEACTIVATED);

                        WorldPacket data(SMSG_FISH_ESCAPED, 0);
                        player->SendDirectMessage(&data);
                    }
                    break;
                }
                case GO_JUST_DEACTIVATED:                   // nothing to do, will be deleted at next update
                    break;
                default:
                {
                    SetLootState(GO_JUST_DEACTIVATED);

                    WorldPacket data(SMSG_FISH_NOT_HOOKED, 0);
                    player->SendDirectMessage(&data);
                    break;
                }
            }

            if(player->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
            {
                player->GetCurrentSpell(CURRENT_CHANNELED_SPELL)->SendChannelUpdate(0);
                player->GetCurrentSpell(CURRENT_CHANNELED_SPELL)->finish();
            }
            return;
        }

        case GAMEOBJECT_TYPE_SUMMONING_RITUAL:              //18
        {
            if(user->GetTypeId()!=TYPEID_PLAYER)
                return;

            Player* player = user->ToPlayer();

            Unit* caster = GetOwner();

            GameObjectTemplate const* info = GetGOInfo();

            if( !caster || caster->GetTypeId()!=TYPEID_PLAYER )
                return;

            // accept only use by player from same group for caster except caster itself
            if((caster->ToPlayer())==player || !(caster->ToPlayer())->IsInSameRaidWith(player))
                return;

            AddUniqueUse(player);

            // full amount unique participants including original summoner
            if(GetUniqueUseCount() < info->summoningRitual.reqParticipants)
                return;

            // in case summoning ritual caster is GO creator
            spellCaster = caster;

            if(!caster->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
                return;

            spellId = info->summoningRitual.spellId;
            
            /*if (spellId == 18541) { // Doom guard
                if (Group* group = caster->ToPlayer()->GetGroup()) {
                    if (Player* plrTarget = group->GetRandomMember())
                        caster->CastSpell(plrTarget, 20625,true);
                }
            }*/

            // finish spell
            caster->GetCurrentSpell(CURRENT_CHANNELED_SPELL)->SendChannelUpdate(0);
            caster->GetCurrentSpell(CURRENT_CHANNELED_SPELL)->finish();

            // can be deleted now
            SetLootState(GO_JUST_DEACTIVATED);

            // go to end function to spell casting
            break;
        }
        case GAMEOBJECT_TYPE_SPELLCASTER:                   //22
        {
            SetUInt32Value(GAMEOBJECT_FLAGS, GO_FLAG_LOCKED);

            GameObjectTemplate const* info = GetGOInfo();
            if(!info)
                return;

            if(info->spellcaster.partyOnly)
            {
                Unit* caster = GetOwner();
                if( !caster || caster->GetTypeId()!=TYPEID_PLAYER )
                    return;
                    
                if (info->entry == 181621) {
                    if (caster->HasAuraEffect(18693))
                        spellId = 34150;
                    else if (caster->HasAuraEffect(18692))
                        spellId = 34149;
                }

                if(user->GetTypeId()!=TYPEID_PLAYER || !(user->ToPlayer())->IsInSameRaidWith(caster->ToPlayer()))
                    return;
            }

            if (!spellId)       // May be already defined
                spellId = info->spellcaster.spellId;

            AddUse();
            break;
        }
        case GAMEOBJECT_TYPE_MEETINGSTONE:                  //23
        {
            GameObjectTemplate const* info = GetGOInfo();

            if(user->GetTypeId()!=TYPEID_PLAYER)
                return;

            Player* player = user->ToPlayer();

            Player* targetPlayer = ObjectAccessor::FindPlayer(player->GetTarget());

            // accept only use by player from same group for caster except caster itself
            if(!targetPlayer || targetPlayer == player || !targetPlayer->IsInSameGroupWith(player))
                return;

            //required lvl checks!
            uint8 level = player->GetLevel();
            if (level < info->meetingstone.minLevel || level > info->meetingstone.MaxLevel)
                return;
            level = targetPlayer->GetLevel();
            if (level < info->meetingstone.minLevel || level > info->meetingstone.MaxLevel)
                return;

            spellId = 23598;

            break;
        }

        case GAMEOBJECT_TYPE_FLAGSTAND:                     // 24
        {
            if(user->GetTypeId()!=TYPEID_PLAYER)
                return;

            Player* player = user->ToPlayer();

            if( player->isAllowUseBattlegroundObject() )
            {
                // in battleground check
                Battleground *bg = player->GetBattleground();
                if(!bg)
                    return;
                // BG flag click
                // AB:
                // 15001
                // 15002
                // 15003
                // 15004
                // 15005
                bg->EventPlayerClickedOnFlag(player, this);
                return;                                     //we don;t need to delete flag ... it is despawned!
            }
            break;
        }
        case GAMEOBJECT_TYPE_FLAGDROP:                      // 26
        {
            if(user->GetTypeId()!=TYPEID_PLAYER)
                return;

            Player* player = user->ToPlayer();

            if( player->isAllowUseBattlegroundObject() )
            {
                // in battleground check
                Battleground *bg = player->GetBattleground();
                if(!bg)
                    return;
                // BG flag dropped
                // WS:
                // 179785 - Silverwing Flag
                // 179786 - Warsong Flag
                // EotS:
                // 184142 - Netherstorm Flag
                GameObjectTemplate const* info = GetGOInfo();
                if(info)
                {
                    switch(info->entry)
                    {
                        case 179785:                        // Silverwing Flag
                            // check if it's correct bg
                            if(bg->GetTypeID() == BATTLEGROUND_WS)
                                bg->EventPlayerClickedOnFlag(player, this);
                            break;
                        case 179786:                        // Warsong Flag
                            if(bg->GetTypeID() == BATTLEGROUND_WS)
                                bg->EventPlayerClickedOnFlag(player, this);
                            break;
                        case 184142:                        // Netherstorm Flag
                            if(bg->GetTypeID() == BATTLEGROUND_EY)
                                bg->EventPlayerClickedOnFlag(player, this);
                            break;
                    }
                }
                //this cause to call return, all flags must be deleted here!!
                spellId = 0;
                Delete();
            }
            break;
        }
        case GAMEOBJECT_TYPE_SPELL_FOCUS:
            GetMap()->ScriptsStart(sGameObjectScripts, GetEntry(), spellCaster, this);
            break;
        default:
            TC_LOG_ERROR("network.opcode","GameObject::Use - Unknown Object Type %u", GetGoType());
            break;
    }

    if(!spellId)
        return;

    SpellInfo const *spellInfo = sSpellMgr->GetSpellInfo( spellId );
    if(!spellInfo)
    {
        if(user->GetTypeId()!=TYPEID_PLAYER || !sOutdoorPvPMgr->HandleCustomSpell(user->ToPlayer(),spellId,this))
            TC_LOG_ERROR("sql.sql","WORLD: unknown spell id %u at use action for gameobject (Entry: %u GoType: %u )", spellId,GetEntry(),GetGoType());

        return;
    }

    auto spell = new Spell(spellCaster, spellInfo, false);

    // spell target is user of GO
    SpellCastTargets targets;
    targets.SetUnitTarget( user );

    spell->prepare(&targets);
}

uint32 GameObject::CastSpell(Unit* target, uint32 spellId, uint64 originalCaster)
{
	SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
	if (!spellInfo)
		return SPELL_FAILED_UNKNOWN;

	bool self = false;
	for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
	{
		if (spellInfo->Effects[i].TargetA.GetTarget() == TARGET_UNIT_CASTER)
		{
			self = true;
			break;
		}
	}

	if (self)
	{
		if (target)
			return target->CastSpell(target, spellInfo, true);
		return SPELL_FAILED_UNKNOWN;
	}

    //summon world trigger
    Creature *trigger = SummonTrigger(GetPositionX(), GetPositionY(), GetPositionZ(), 0, 1);
    if(!trigger) 
        return SPELL_FAILED_UNKNOWN;

	// remove immunity flags, to allow spell to target anything
	trigger->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_NPC | UNIT_FLAG_IMMUNE_TO_PC);

    trigger->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);
    if(Unit *owner = GetOwner())
    {
		if (owner->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PVP_ATTACKABLE))
			trigger->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PVP_ATTACKABLE);
        trigger->SetFaction(owner->GetFaction());
		// needed for GO casts for proper target validation checks
        trigger->SetOwnerGUID(owner->GetGUID());
        return trigger->CastSpell(target, spellId, true, nullptr, nullptr, originalCaster ? originalCaster : owner->GetGUID());
    }
    else
    {
		trigger->SetFaction(spellInfo->IsPositive() ? FACTION_FRIENDLY : FACTION_MONSTER);
        return trigger->CastSpell(target, spellId, true, nullptr, nullptr, originalCaster);
    }
}

void GameObject::EventInform(uint32 eventId)
{
    if (!eventId)
        return;

    if (AI())
        AI()->EventInform(eventId);

    if (m_zoneScript)
        m_zoneScript->ProcessEvent(this, eventId);
}

uint32 GameObject::GetScriptId() const
{
    if (GameObjectData const* gameObjectData = GetGOData())
        if (uint32 scriptId = gameObjectData->ScriptId)
            return scriptId;

    return GetGOInfo()->ScriptId;
}

// overwrite WorldObject function for proper name localization
std::string const& GameObject::GetNameForLocaleIdx(LocaleConstant loc_idx) const
{
    if (loc_idx >= 0)
    {
        GameObjectLocale const *cl = sObjectMgr->GetGameObjectLocale(GetEntry());
        if (cl)
        {
            if (cl->Name.size() > loc_idx && !cl->Name[loc_idx].empty())
                return cl->Name[loc_idx];
        }
    }

    return GetName();
}

void GameObject::SendCustomAnim(uint32 anim)
{
    WorldPacket data(SMSG_GAMEOBJECT_CUSTOM_ANIM, 8+4);
    data << GetGUID();
    data << uint32(anim);
    SendMessageToSet(&data, true);
}

bool GameObject::IsInRange(float x, float y, float z, float radius) const
{
    GameObjectDisplayInfoEntry const* info = sGameObjectDisplayInfoStore.LookupEntry(GetUInt32Value(GAMEOBJECT_DISPLAYID));
    if (!info)
        return IsWithinDist3d(x, y, z, radius + 2.0f);

    float sinA = sin(GetOrientation());
    float cosA = cos(GetOrientation());
    float dx = x - GetPositionX();
    float dy = y - GetPositionY();
    float dz = z - GetPositionZ();
    float dist = sqrt(dx*dx + dy*dy);
    //! Check if the distance between the 2 objects is 0, can happen if both objects are on the same position.
    //! The code below this check wont crash if dist is 0 because 0/0 in float operations is valid, and returns infinite
    if (G3D::fuzzyEq(dist, 0.0f))
        return true;

    float scale = GetFloatValue(OBJECT_FIELD_SCALE_X);
    float sinB = dx / dist;
    float cosB = dy / dist;
    dx = dist * (cosA * cosB + sinA * sinB);
    dy = dist * (cosA * sinB - sinA * cosB);
    return dx < (info->maxX*scale) + radius && dx >(info->minX*scale) - radius
        && dy < (info->maxY*scale) + radius && dy >(info->minY*scale) - radius
        && dz < (info->maxZ*scale) + radius && dz >(info->minZ*scale) - radius;
}

void GameObject::AddUse()
{
     ++m_usetimes;
     
    if(GetGoType() == GAMEOBJECT_TYPE_CHEST)
        SetDespawnTimer(CHEST_DESPAWN_TIME);
}

void GameObject::SetRespawnTime(int32 respawn)
{
    m_respawnTime = respawn > 0 ? time(nullptr) + respawn : 0;
    m_respawnDelayTime = respawn > 0 ? respawn : 0;
}

void GameObject::SetDespawnTimer(uint32 timer)
{
    m_despawnTime = time(nullptr) + timer;
}

float GameObject::GetInteractionDistance() const
{
    switch (GetGoType())
    {
        /// @todo find out how the client calculates the maximal usage distance to spellless working
        // gameobjects like guildbanks and mailboxes - 10.0 is a just an abitrary choosen number
        case GAMEOBJECT_TYPE_GUILD_BANK:
        case GAMEOBJECT_TYPE_MAILBOX:
            return 10.0f;
        case GAMEOBJECT_TYPE_FISHINGHOLE:
        case GAMEOBJECT_TYPE_FISHINGNODE:
            return 20.0f + CONTACT_DISTANCE; // max spell range
        default:
            return INTERACTION_DISTANCE + 4.0f; //sunstrider: little leeway
    }
}

void GameObject::UpdateModelPosition()
{
    if (!m_model)
        return;

    if (GetMap()->ContainsGameObjectModel(*m_model))
    {
        GetMap()->RemoveGameObjectModel(*m_model);
        m_model->UpdatePosition();
        GetMap()->InsertGameObjectModel(*m_model);
    }
}

class GameObjectModelOwnerImpl : public GameObjectModelOwnerBase
{
public:
    explicit GameObjectModelOwnerImpl(GameObject const* owner) : _owner(owner) { }

    bool IsSpawned() const override { return _owner->isSpawned(); }
    uint32 GetDisplayId() const override { return _owner->GetDisplayId(); }
    uint32 GetPhaseMask() const override { return _owner->GetPhaseMask(); }
    G3D::Vector3 GetPosition() const override { return G3D::Vector3(_owner->GetPositionX(), _owner->GetPositionY(), _owner->GetPositionZ()); }
    float GetOrientation() const override { return _owner->GetOrientation(); }
    float GetScale() const override { return _owner->GetObjectScale(); }
    void DebugVisualizeCorner(G3D::Vector3 const& corner) const override { _owner->SummonCreature(1, corner.x, corner.y, corner.z, 0, TEMPSUMMON_MANUAL_DESPAWN); }

private:
    GameObject const* _owner;
};

GameObjectModel* GameObject::CreateModel()
{
    return GameObjectModel::Create(Trinity::make_unique<GameObjectModelOwnerImpl>(this), sWorld->GetDataPath());
}

void GameObject::SetTransportPathRotation(G3D::Quat const& rot)
{
    SetFloatValue(GAMEOBJECT_PARENTROTATION + 0, rot.x);
    SetFloatValue(GAMEOBJECT_PARENTROTATION + 1, rot.y);
    SetFloatValue(GAMEOBJECT_PARENTROTATION + 2, rot.z);
    SetFloatValue(GAMEOBJECT_PARENTROTATION + 3, rot.w);
}

void GameObject::AIM_Destroy()
{
    delete m_AI;
    m_AI = nullptr;
}

bool GameObject::AIM_Initialize()
{
    AIM_Destroy();

    m_AI = FactorySelector::SelectGameObjectAI(this);

    if (!m_AI)
        return false;

    m_AI->InitializeAI();
    return true;
}

void GameObject::GetRespawnPosition(float &x, float &y, float &z, float* ori /* = NULL*/) const
{
    if (m_spawnId)
    {
        if (GameObjectData const* data = sObjectMgr->GetGOData(GetSpawnId()))
        {
            x = data->posX;
            y = data->posY;
            z = data->posZ;
            if (ori)
                *ori = data->orientation;
            return;
        }
    }

    x = GetPositionX();
    y = GetPositionY();
    z = GetPositionZ();
    if (ori)
        *ori = GetOrientation();
}