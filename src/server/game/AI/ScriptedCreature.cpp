/* Copyright (C) 2008 Trinity <http://www.trinitycore.org/>
 *
 * Thanks to the original authors: ScriptDev2 <https://scriptdev2.svn.sourceforge.net/>
 *
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#include "ScriptedCreature.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Item.h"
#include "Spell.h"
#include "ObjectMgr.h"
#include "ScriptMgr.h"
#include "Containers.h"
#include "InstanceScript.h"

 // Spell summary for ScriptedAI::SelectSpell
struct TSpellSummary
{
    uint8 Targets;                                          // set of enum SelectTarget
    uint8 Effects;                                          // set of enum SelectEffect
} extern* SpellSummary;

void SummonList::Despawn(Creature *summon)
{
    uint64 guid = summon->GetGUID();
    for(auto i = begin(); i != end(); ++i)
    {
        if(*i == guid)
        {
            erase(i);
            return;
        }
    }
}

void SummonList::DespawnEntry(uint32 entry)
{
    for(auto itr = begin(); itr != end();)
    {
        if(Creature *summon = ObjectAccessor::GetCreature(*me, *itr))
        {
            if(summon->GetEntry() == entry)
            {
                summon->RemoveFromWorld();
                summon->SetDeathState(JUST_DIED);
                summon->RemoveCorpse();
                itr = erase(itr);
                continue;
            }
        }
        itr++;
    }
}

void SummonList::DespawnAll(bool withoutWorldBoss)
{
    for(uint64 & i : *this)
    {
        if(Creature *summon = ObjectAccessor::GetCreature(*me, i))
        {
            if (withoutWorldBoss && summon->IsWorldBoss())
                continue;

            summon->AddObjectToRemoveList();
        }
    }
    clear();
}

bool SummonList::IsEmpty()
{
    return empty();
}

Creature* SummonList::GetCreatureWithEntry(uint32 entry) const
{
    for (uint64 i : *this)
    {
        if (Creature* summon = ObjectAccessor::GetCreature(*me, i))
            if (summon->GetEntry() == entry)
                return summon;
    }

    return nullptr;
}

void BumpHelper::Update(const uint32 diff)
{
    for(auto itr = begin(); itr != end();)
    {
        if(itr->second < diff) //okay to erase
            itr = erase(itr);
        else //just decrease time left
        {
            itr->second -= diff;
            itr++;
        }
    }
}

//return true if not yet present in list
bool BumpHelper::AddCooldown(Unit* p, uint32 customValue)
{
    auto found = find(p->GetGUID());
    if(found != end())
        return false;

    insert(std::make_pair(p->GetGUID(),customValue?customValue:m_cooldown)); //3s before being knockable again
    return true;
}

ScriptedAI::ScriptedAI(Creature* creature) : CreatureAI(creature),
    _evadeCheckCooldown(2500)
{}

void ScriptedAI::AttackStart(Unit* who, bool melee)
{
    if (!who)
        return;

    if (me->Attack(who, melee))
    {
        me->AddThreat(who, 0.0f);

        if(melee)
            DoStartMovement(who);
        else
            DoStartNoMovement(who);
    }
}

void ScriptedAI::AttackStart(Unit* who)
{
    if (!who)
        return;
    
    bool melee = (GetCombatDistance() > ATTACK_DISTANCE) ? me->GetDistance(who) <= ATTACK_DISTANCE : true;
    if (me->Attack(who, melee))
    {
        me->AddThreat(who, 0.0f);

        if(IsCombatMovementAllowed())
        {
            DoStartMovement(who);
        } else {
            DoStartNoMovement(who);
        }
    }
}

void ScriptedAI::UpdateAI(const uint32 diff)
{
    //Check if we have a current target
    if (me->IsAlive() && UpdateVictim())
    {
        if (me->IsAttackReady() )
        {
            //If we are within range melee the target
            if (me->IsWithinMeleeRange(me->GetVictim()))
            {
                me->AttackerStateUpdate(me->GetVictim());
                me->ResetAttackTimer();
            }
        }
    }
}

void ScriptedAI::EnterEvadeMode(EvadeReason why)
{
    //me->InterruptNonMeleeSpells(true);
    me->RemoveAllAuras();
    me->DeleteThreatList();
    me->CombatStop();
    me->InitCreatureAddon();
    me->SetLootRecipient(nullptr);

    if(me->IsAlive())
    {
        if(Unit* owner = me->GetOwner())
        {
            if(owner->IsAlive())
                me->GetMotionMaster()->MoveFollow(owner,PET_FOLLOW_DIST,me->GetFollowAngle());
        }
        else
            me->GetMotionMaster()->MoveTargetedHome();
    }

    Reset();
}

void ScriptedAI::JustRespawned()
{
    Reset();
}

void ScriptedAI::DoStartMovement(Unit* victim, float distance, float angle)
{
    if (!victim)
        return;

    me->GetMotionMaster()->MoveChase(victim, distance, angle);
}

void ScriptedAI::DoStartNoMovement(Unit* victim)
{
    if (!victim)
        return;

    me->GetMotionMaster()->MoveIdle();
}

void ScriptedAI::DoStopAttack()
{
    if (me->GetVictim() != nullptr)
    {
        me->AttackStop();
    }
}

uint32 ScriptedAI::DoCast(Unit* victim, uint32 spellId, bool triggered)
{
    uint32 reason = me->CastSpell(victim, spellId, triggered);

    //restore combat movement on out of mana
    if(reason == SPELL_FAILED_NO_POWER && GetRestoreCombatMovementOnOOM() && !IsCombatMovementAllowed())
        SetCombatMovementAllowed(true);

    return reason;
}

uint32 ScriptedAI::DoCastAOE(uint32 spellId, bool triggered)
{
    return DoCast((Unit*)nullptr, spellId, triggered);
}

uint32 ScriptedAI::DoCastSpell(Unit* who,SpellInfo const *spellInfo, bool triggered)
{
    return me->CastSpell(who, spellInfo, triggered);
}

void ScriptedAI::DoPlaySoundToSet(Unit* unit, uint32 sound)
{
    if (!unit)
        return;

    if (!GetSoundEntriesStore()->LookupEntry(sound))
    {
        error_log("TSCR: Invalid soundId %u used in DoPlaySoundToSet (by unit TypeId %u, guid %u)", sound, unit->GetTypeId(), unit->GetGUID());
        return;
    }

    WorldPacket data(4);
    data.SetOpcode(SMSG_PLAY_SOUND);
    data << uint32(sound);
    unit->SendMessageToSet(&data,false);
}

Creature* ScriptedAI::DoSpawnCreature(uint32 id, float x, float y, float z, float angle, uint32 type, uint32 despawntime)
{
    return me->SummonCreature(id,me->GetPositionX() + x,me->GetPositionY() + y,me->GetPositionZ() + z, angle, (TempSummonType)type, despawntime);
}

SpellInfo const* ScriptedAI::SelectSpell(Unit* target, uint32 School, uint32 Mechanic, SelectSpellTarget Targets, uint32 PowerCostMin, uint32 PowerCostMax, float RangeMin, float RangeMax, SelectEffect Effects)
{
    //No target so we can't cast
    if (!target)
        return nullptr;

    //Silenced so we can't cast
    if (me->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SILENCED))
        return nullptr;;

    //Using the extended script system we first create a list of viable spells
    SpellInfo const* Spell[4];
    Spell[0] = nullptr;
    Spell[1] = nullptr;
    Spell[2] = nullptr;
    Spell[3] = nullptr;

    uint32 SpellCount = 0;

    SpellInfo const* TempSpell;

    //Check if each spell is viable(set it to null if not)
    for (uint32 m_spell : me->m_spells)
    {
        TempSpell = sSpellMgr->GetSpellInfo(m_spell);

        //This spell doesn't exist
        if (!TempSpell)
            continue;

        // Targets and Effects checked first as most used restrictions
        //Check the spell targets if specified
        if (Targets && !(SpellSummary[m_spell].Targets & (1 << (Targets - 1))))
            continue;

        //Check the type of spell if we are looking for a specific spell type
        if (Effects && !(SpellSummary[m_spell].Effects & (1 << (Effects - 1))))
            continue;

        //Check for school if specified
        if (School >= 0 && TempSpell->SchoolMask & School)
            continue;

        //Check for spell mechanic if specified
        if (Mechanic >= 0 && TempSpell->Mechanic != Mechanic)
            continue;

        //Make sure that the spell uses the requested amount of power
        if (PowerCostMin &&  TempSpell->ManaCost < PowerCostMin)
            continue;

        if (PowerCostMax && TempSpell->ManaCost > PowerCostMax)
            continue;

        //Continue if we don't have the mana to actually cast this spell
        if (TempSpell->ManaCost > me->GetPower((Powers)TempSpell->PowerType))
            continue;

        //Check if the spell meets our range requirements
        if (RangeMin && RangeMin < me->GetSpellMinRangeForTarget(target, TempSpell))
            continue;
        if (RangeMax && RangeMax > me->GetSpellMaxRangeForTarget(target, TempSpell))
            continue;

        //Check if our target is in range
        if (me->IsWithinDistInMap(target, me->GetSpellMinRangeForTarget(target, TempSpell)) || !me->IsWithinDistInMap(target, me->GetSpellMaxRangeForTarget(target, TempSpell)))
            continue;

        //All good so lets add it to the spell list
        Spell[SpellCount] = TempSpell;
        SpellCount++;
    }

    //We got our usable spells so now lets randomly pick one
    if (!SpellCount)
        return nullptr;

    return Spell[rand() % SpellCount];
}


bool ScriptedAI::CanCast(Unit* Target, SpellInfo const *Spell, bool Triggered)
{
    //No target so we can't cast
    if (!Target || !Spell)
        return false;

    //Silenced so we can't cast
    if (!Triggered && me->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SILENCED))
        return false;

    //Check for power
    if (!Triggered && me->GetPower((Powers)Spell->PowerType) < Spell->ManaCost)
        return false;

    //Unit is out of range of this spell
    if (me->GetDistance(Target) > Spell->GetMaxRange() || me->GetDistance(Target) < Spell->GetMinRange())
        return false;

    return true;
}

void ScriptedAI::SetEquipmentSlots(bool bLoadDefault, int32 uiMainHand, int32 uiOffHand, int32 uiRanged)
{
    if (bLoadDefault)
    {
        if (CreatureTemplate const* pInfo = sObjectMgr->GetCreatureTemplate(me->GetEntry()))
            me->LoadEquipment(pInfo->equipmentId,true);

        return;
    }
#ifdef LICH_KING
    if (uiMainHand >= 0)
        me->SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_ID + 0, uint32(uiMainHand));

    if (uiOffHand >= 0)
        me->SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_ID + 1, uint32(uiOffHand));

    if (uiRanged >= 0)
        me->SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_ID + 2, uint32(uiRanged));
#endif
}

bool ScriptedAI::EnterEvadeIfOutOfCombatArea()
{
    if (me->IsInEvadeMode() || !me->IsInCombat())
        return false;

    if (_evadeCheckCooldown == time(nullptr))
        return false;
    _evadeCheckCooldown = time(nullptr);

    if (!CheckEvadeIfOutOfCombatArea())
        return false;

    EnterEvadeMode(EVADE_REASON_BOUNDARY);
    return true;
}

void ScriptedAI::DoResetThreat()
{
    if (!me->CanHaveThreatList() || me->getThreatManager().isThreatListEmpty())
    {
        error_log("TSCR: DoResetThreat called for creature that either cannot have threat list or has empty threat list (me entry = %d)", me->GetEntry());

        return;
    }

    std::list<HostileReference*>& m_threatlist = me->getThreatManager().getThreatList();
    std::list<HostileReference*>::iterator itr;

    for(itr = m_threatlist.begin(); itr != m_threatlist.end(); ++itr)
    {
        Unit* pUnit = nullptr;
        pUnit = ObjectAccessor::GetUnit((*me), (*itr)->getUnitGuid());
        if(pUnit && me->GetThreat(pUnit))
            DoModifyThreatPercent(pUnit, -100);
    }
}

void ScriptedAI::DoModifyThreatPercent(Unit *pUnit, int32 pct)
{
    if(!pUnit) return;
    me->getThreatManager().modifyThreatPercent(pUnit, pct);
}

void ScriptedAI::DoTeleportTo(float x, float y, float z, uint32 time)
{
    me->Relocate(x, y, z);
    float speed = me->GetDistance(x, y, z) / ((float)time * 0.001f);
    me->MonsterMoveWithSpeed(x, y, z, speed);
}

void ScriptedAI::DoTeleportPlayer(Unit* pUnit, float x, float y, float z, float o)
{
    if(!pUnit || pUnit->GetTypeId() != TYPEID_PLAYER)
    {
        if(pUnit)
            error_log("TSCR: Creature %u (Entry: %u) Tried to teleport non-player unit (Type: %u GUID: %u) to x: %f y:%f z: %f o: %f. Aborted.", me->GetGUID(), me->GetEntry(), pUnit->GetTypeId(), pUnit->GetGUID(), x, y, z, o);
        return;
    }

    (pUnit->ToPlayer())->TeleportTo(pUnit->GetMapId(), x, y, z, o, TELE_TO_NOT_LEAVE_COMBAT);
}

void ScriptedAI::DoTeleportAll(float x, float y, float z, float o)
{
    Map *map = me->GetMap();
    if (!map->IsDungeon())
        return;

    Map::PlayerList const &PlayerList = map->GetPlayers();
    for(const auto & i : PlayerList)
        if (Player* i_pl = i.GetSource())
            if (i_pl->IsAlive())
                i_pl->TeleportTo(me->GetMapId(), x, y, z, o, TELE_TO_NOT_LEAVE_COMBAT);
}


// BossAI - for instanced bosses

BossAI::BossAI(Creature* creature, uint32 bossId) : ScriptedAI(creature),
instance(creature->GetInstanceScript()),
summons(creature),
_boundary(instance ? instance->GetBossBoundary(bossId) : nullptr),
_bossId(bossId)
{
}

void BossAI::_Reset()
{
    if (!me->IsAlive())
        return;

    //sunwell me->ResetLootMode();
    events.Reset();
    summons.DespawnAll();
    if (instance)
//NYI       instance->SetBossState(_bossId, NOT_STARTED);
        instance->SetData(_bossId, NOT_STARTED);
}

bool BossAI::CanAIAttack(Unit const* target) const
{
 //TC   return CheckBoundary(target);
    return true;
}

void BossAI::_JustDied()
{
    events.Reset();
    summons.DespawnAll();
    if (instance)
    {
        instance->SetBossState(_bossId, DONE);
        instance->SaveToDB();
    }
}

void BossAI::_EnterCombat()
{
    me->SetKeepActive(true);
    DoZoneInCombat();
    if (instance)
    {
        // bosses do not respawn, check only on enter combat
        if (!instance->CheckRequiredBosses(_bossId))
        {
            EnterEvadeMode();
            return;
        }
        instance->SetBossState(_bossId, IN_PROGRESS);
    }
}

void BossAI::TeleportCheaters()
{
    float x, y, z;
    me->GetPosition(x, y, z);

    ThreatContainer::StorageType threatList = me->getThreatManager().getThreatList();
    for (ThreatContainer::StorageType::const_iterator itr = threatList.begin(); itr != threatList.end(); ++itr)
        if (Unit* target = (*itr)->getTarget())
            if (target->GetTypeId() == TYPEID_PLAYER && !CheckBoundary(target))
                target->NearTeleportTo(x, y, z, 0);
}

bool BossAI::CheckBoundary(Unit* who)
{
    if (!GetBoundary() || !who)
        return true;

    for (const auto & itr : *GetBoundary())
    {
        switch (itr.first)
        {
        case BOUNDARY_N:
            if (who->GetPositionX() > itr.second)
                return false;
            break;
        case BOUNDARY_S:
            if (who->GetPositionX() < itr.second)
                return false;
            break;
        case BOUNDARY_E:
            if (who->GetPositionY() < itr.second)
                return false;
            break;
        case BOUNDARY_W:
            if (who->GetPositionY() > itr.second)
                return false;
            break;
        case BOUNDARY_NW:
            if (who->GetPositionX() + who->GetPositionY() > itr.second)
                return false;
            break;
        case BOUNDARY_SE:
            if (who->GetPositionX() + who->GetPositionY() < itr.second)
                return false;
            break;
        case BOUNDARY_NE:
            if (who->GetPositionX() - who->GetPositionY() > itr.second)
                return false;
            break;
        case BOUNDARY_SW:
            if (who->GetPositionX() - who->GetPositionY() < itr.second)
                return false;
            break;
        default:
            break;
        }
    }

    return true;
}

void BossAI::JustSummoned(Creature* summon)
{
    summons.Summon(summon);
    if (me->IsInCombat())
        DoZoneInCombat(summon);
}

void BossAI::SummonedCreatureDespawn(Creature* summon)
{
    summons.Despawn(summon);
}

void BossAI::UpdateAI(uint32 diff)
{
    if (!UpdateVictim())
        return;

    events.Update(diff);

    if (me->HasUnitState(UNIT_STATE_CASTING))
        return;

    while (uint32 eventId = events.ExecuteEvent())
        ExecuteEvent(eventId);

    DoMeleeAttackIfReady();
}

/* remove this block if you don't know why it's commented out

Creature* FindCreature(uint32 entry, float range, Unit* Finder)
{
    if(!Finder)
        return nullptr;
    Creature* target = nullptr;


    Trinity::AllCreaturesOfEntryInRange check(Finder, entry, range);
    Trinity::CreatureSearcher<Trinity::AllCreaturesOfEntryInRange> searcher(target, check);
    Finder->VisitNearbyGridObject(range, searcher);
    
    return target;
}

void FindCreatures(std::list<Creature*>& list, uint32 entry, float range, Unit* Finder)
{
    Trinity::AllCreaturesOfEntryInRange check(Finder, entry, range);
    Trinity::CreatureListSearcher<Trinity::AllCreaturesOfEntryInRange> searcher(Finder, list, check);
    Finder->VisitNearbyGridObject(range, searcher);
}

GameObject* FindGameObject(uint32 entry, float range, Unit* Finder)
{
    if(!Finder)
        return nullptr;
    GameObject* target = nullptr;

    Trinity::AllGameObjectsWithEntryInGrid go_check(entry);
    Trinity::GameObjectSearcher<Trinity::AllGameObjectsWithEntryInGrid> searcher(target, go_check);
    Finder->VisitNearbyGridObject(range, searcher);
    return target;
}
*/

Unit* ScriptedAI::DoSelectLowestHpFriendly(float range, uint32 MinHPDiff)
{
    Unit* pUnit = nullptr;
    Trinity::MostHPMissingInRange u_check(me, range, MinHPDiff);
    Trinity::UnitLastSearcher<Trinity::MostHPMissingInRange> searcher(me, pUnit, u_check);
    Cell::VisitAllObjects(me, searcher, range);
    return pUnit;
}

std::list<Creature*> ScriptedAI::DoFindFriendlyCC(float range)
{
    std::list<Creature*> pList;
    Trinity::FriendlyCCedInRange u_check(me, range);
    Trinity::CreatureListSearcher<Trinity::FriendlyCCedInRange> searcher(me, pList, u_check);
    Cell::VisitAllObjects(me, searcher, range);
    return pList;
}

std::list<Creature*> ScriptedAI::DoFindFriendlyMissingBuff(float range, uint32 spellid)
{
    std::list<Creature*> pList;
    Trinity::FriendlyMissingBuffInRange u_check(me, range, spellid);
    Trinity::CreatureListSearcher<Trinity::FriendlyMissingBuffInRange> searcher(me, pList, u_check);
    Cell::VisitAllObjects(me, searcher, range);
    return pList;
}


// SD2 grid searchers.
Creature* GetClosestCreatureWithEntry(WorldObject const* source, uint32 entry, float maxSearchRange, bool alive /*= true*/)
{
    return source->FindNearestCreature(entry, maxSearchRange, alive);
}

GameObject* GetClosestGameObjectWithEntry(WorldObject const* source, uint32 entry, float maxSearchRange)
{
    return source->FindNearestGameObject(entry, maxSearchRange);
}

void LoadOverridenSQLData()
{
    GameObjectTemplate *goInfo;

    // Sunwell Plateau : Kalecgos : Spectral Rift
    goInfo = const_cast<GameObjectTemplate*>(sObjectMgr->GetGameObjectTemplate(187055));
    if(goInfo && goInfo->type == GAMEOBJECT_TYPE_GOOBER)
        goInfo->goober.lockId = 57; // need LOCKTYPE_QUICK_OPEN
}
