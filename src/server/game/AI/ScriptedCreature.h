/* Copyright (C) 2008 Trinity <http://www.trinitycore.org/>
 *
 * Thanks to the original authors: ScriptDev2 <https://scriptdev2.svn.sourceforge.net/>
 *
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef SC_CREATURE_H
#define SC_CREATURE_H

#include "CreatureAI.h"
#include "Creature.h"
#include "CreatureAIImpl.h"
#include "InstanceScript.h"
#include "EventMap.h"

#define CAST_AI(a,b)    (dynamic_cast<a*>(b))
#define ENSURE_AI(a,b)  (EnsureAI<a>(b))

template<class T, class U>
T* EnsureAI(U* ai)
{
    T* cast_ai = dynamic_cast<T*>(ai);
    ASSERT(cast_ai);
    return cast_ai;
};

class TC_GAME_API SummonList : public std::list<uint64>
{
public:
    SummonList(Creature* creature) : me(creature) {}
    void Summon(Creature *summon) {push_back(summon->GetGUID());}
    void Despawn(Creature *summon);
    void DespawnEntry(uint32 entry);
    void DespawnAll(bool withoutWorldBoss = false);
    bool IsEmpty();

    Creature* GetCreatureWithEntry(uint32 entry) const;
private:
    Creature* me;
};

class TC_GAME_API BumpHelper : std::map<uint64,uint32>
{
public:
    BumpHelper(uint32 cooldown) : m_cooldown(cooldown) {}
    void Update(const uint32 diff);
    bool AddCooldown(Unit* p, uint32 customValue = 0);
private:
    const uint32 m_cooldown;
};

//Get a single creature of given entry
TC_GAME_API Creature* FindCreature(uint32 entry, float range, Unit* Finder);
//Get every creatures of given entry within given range
TC_GAME_API void FindCreatures(std::list<Creature*>& list, uint32 entry, float range, Unit* Finder);

//Get a single gameobject of given entry
TC_GAME_API GameObject* FindGameObject(uint32 entry, float range, Unit* Finder);

struct TC_GAME_API ScriptedAI : public CreatureAI
{
    ScriptedAI(Creature* creature);
    ~ScriptedAI() override {}

    //*************
    //CreatureAI Functions
    //*************

    //Called at each attack of me by any victim
    void AttackStart(Unit *) override;
    //Attack start with forcing melee / forcing staying on place
    void AttackStart(Unit *, bool melee);
    void AttackStartNoMove(Unit *pTarget);

    //Called at stoping attack by any attacker
    void EnterEvadeMode(EvadeReason why = EVADE_REASON_OTHER) override;

    // Called at any Damage from any attacker (before damage apply)
    void DamageTaken(Unit *done_by, uint32 &damage) override {}
    
    void HealReceived(Unit* /*done_by*/, uint32& /*addhealth*/) override {}

    //Called at World update tick
    void UpdateAI(const uint32) override;

    //Called at creature death
    void JustDied(Unit*) override{}

    //Called at creature killing another unit
    void KilledUnit(Unit*) override{}

    // Called when the creature summon successfully other creature
    void JustSummoned(Creature*) override {}

    // Called when a summoned creature is despawned
    void SummonedCreatureDespawn(Creature* /*unit*/) override {}

    // Called when hit by a spell
    void SpellHit(Unit* caster, const SpellInfo*) override {}

    // Called when spell hits a target
    void SpellHitTarget(Unit* target, const SpellInfo*) override {}

    // Called when creature is spawned or respawned (for reseting variables)
    void JustRespawned() override;

    //Called at waypoint reached or PointMovement end
    void MovementInform(uint32, uint32) override {}

    // Called when AI is temporarily replaced or put back when possess is applied or removed
    void OnPossess(Unit* charmer, bool apply) override {}
    
    // Called when creature finishes a spell cast
    void OnSpellFinish(Unit *caster, uint32 spellId, Unit *target, bool ok) override {}
    
    //*************
    //Pure virtual functions
    //*************

    //Called at creature reset either by death or evade
    void Reset() override {}

    //Called at creature aggro either by MoveInLOS or Attack Start
    void EnterCombat(Unit*) override {}

    //*************
    //AI Helper Functions
    //*************

    //Start movement toward victim
    void DoStartMovement(Unit* victim, float distance = 0, float angle = 0);

    //Start no movement on victim
    void DoStartNoMovement(Unit* victim);

    //Stop attack of current victim
    void DoStopAttack();

    //Cast spell by Id, return SpellCastResult
    uint32 DoCast(Unit* victim, uint32 spellId, bool triggered = false);
    uint32 DoCastAOE(uint32 spellId, bool triggered = false);

    //Cast spell by spell info
    uint32 DoCastSpell(Unit* who, SpellInfo const *spellInfo, bool triggered = false);

    //Plays a sound to all nearby players
    void DoPlaySoundToSet(Unit* unit, uint32 sound);

    //Drops all threat to 0%. Does not remove players from the threat list
    void DoResetThreat();

    void DoModifyThreatPercent(Unit *pUnit, int32 pct);

    void DoTeleportTo(float x, float y, float z, uint32 time = 0);

    void DoAction(const int32 param) override {}

    //Teleports a player without dropping threat (only teleports to same map)
    void DoTeleportPlayer(Unit* pUnit, float x, float y, float z, float o);
    void DoTeleportAll(float x, float y, float z, float o);

    //Returns friendly unit with the most amount of hp missing from max hp
    Unit* DoSelectLowestHpFriendly(float range, uint32 MinHPDiff = 1);

    //Returns a list of friendly CC'd units within range
    std::list<Creature*> DoFindFriendlyCC(float range);

    //Returns a list of all friendly units missing a specific buff within range
    std::list<Creature*> DoFindFriendlyMissingBuff(float range, uint32 spellid);

    //Spawns a creature relative to m_creature
    Creature* DoSpawnCreature(uint32 id, float x, float y, float z, float angle, uint32 type, uint32 despawntime);

    //Returns spells that meet the specified criteria from the creatures spell list
    SpellInfo const* SelectSpell(Unit* Target, uint32 School, uint32 Mechanic, SelectSpellTarget Targets, uint32 PowerCostMin, uint32 PowerCostMax, float RangeMin, float RangeMax, SelectEffect Effect);

    //Checks if you can cast the specified spell
    bool CanCast(Unit* Target, SpellInfo const *Spell, bool Triggered = false);
    
    void SetEquipmentSlots(bool bLoadDefault, int32 uiMainHand = EQUIP_NO_CHANGE, int32 uiOffHand = EQUIP_NO_CHANGE, int32 uiRanged = EQUIP_NO_CHANGE);

    bool EnterEvadeIfOutOfCombatArea();
    virtual bool CheckEvadeIfOutOfCombatArea() const { return false; }

private:
    uint32 _evadeCheckCooldown;

};

class TC_GAME_API BossAI : public ScriptedAI
{
public:
    BossAI(Creature* creature, uint32 bossId);
    ~BossAI() override {}

    InstanceScript* const instance;
    BossBoundaryMap const* GetBoundary() const { return _boundary; }

    void JustSummoned(Creature* summon) override;
    void SummonedCreatureDespawn(Creature* summon) override;

    void UpdateAI(const uint32 diff) override;

    // Hook used to execute events scheduled into EventMap without the need
    // to override UpdateAI
    // note: You must re-schedule the event within this method if the event
    // is supposed to run more than once
    virtual void ExecuteEvent(uint32 /*eventId*/) { }

    void Reset() override { _Reset(); }
    void EnterCombat(Unit* /*who*/) override { _EnterCombat(); }
    void JustDied(Unit* /*killer*/) override { _JustDied(); }
    void JustReachedHome() override { _JustReachedHome(); }

protected:
    void _Reset();
    void _EnterCombat();
    void _JustDied();
    void _JustReachedHome() { me->SetKeepActive(false); }

    bool CheckInRoom()
    {
        if (CheckBoundary(me))
            return true;

        EnterEvadeMode(EVADE_REASON_BOUNDARY);
        return false;
    }

    bool CheckBoundary(Unit* who);
    //teleport players out of boundaries on boss
    void TeleportCheaters();

    EventMap events;
    SummonList summons;

private:
    BossBoundaryMap const* const _boundary;
    uint32 const _bossId;
};

// SD2 grid searchers.
TC_GAME_API Creature* GetClosestCreatureWithEntry(WorldObject const* source, uint32 entry, float maxSearchRange, bool alive = true);
TC_GAME_API GameObject* GetClosestGameObjectWithEntry(WorldObject const* source, uint32 entry, float maxSearchRange);

#endif

