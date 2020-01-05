#include "Log.h"
#include "SpellMgr.h"
#include "ScriptMgr.h"
#include "SpellScript.h"
#include "Spell.h"

enum WarriorSpells
{
    SPELL_RELATIATION_PROC                          = 22858,
    SPELL_WARRIOR_BLOODTHIRST                       = 23885,
    SPELL_WARRIOR_BLOODTHIRST_DAMAGE                = 23881,
    SPELL_WARRIOR_CHARGE                            = 34846,
    SPELL_WARRIOR_DEEP_WOUNDS_RANK_1                = 12162,
    SPELL_WARRIOR_DEEP_WOUNDS_RANK_2                = 12850,
    SPELL_WARRIOR_DEEP_WOUNDS_RANK_3                = 12868,
    SPELL_WARRIOR_DEEP_WOUNDS_PERIODIC              = 12721,
    SPELL_WARRIOR_EXECUTE                           = 20647,
    SPELL_WARRIOR_LAST_STAND_TRIGGERED              = 12976,
    SPELL_WARRIOR_RETALIATION_DAMAGE                = 20240,
    SPELL_WARRIOR_SUDDEN_DEATH_R1                   = 29723,
    SPELL_WARRIOR_SWEEPING_STRIKES_EXTRA_ATTACK_1   = 12723,
    SPELL_WARRIOR_SWEEPING_STRIKES_EXTRA_ATTACK_2   = 26654,
    SPELL_WARRIOR_TAUNT                             = 355,
    SPELL_WARRIOR_SECOND_WIND_TRIGGER_1             = 29841,
    SPELL_WARRIOR_SECOND_WIND_TRIGGER_2             = 29842,
};

// 5246 - Intimidating Shout - remove main target from effect 1 and 2
class spell_warr_intimidating_shout : public SpellScriptLoader
{
public:
    spell_warr_intimidating_shout() : SpellScriptLoader("spell_warr_intimidating_shout") { }

    class spell_warr_intimidating_shout_SpellScript : public SpellScript
    {
        PrepareSpellScript(spell_warr_intimidating_shout_SpellScript);

        void FilterTargets(std::list<WorldObject*>& unitList)
        {
            unitList.remove(GetExplTargetWorldObject());
        }

        void Register() override
        {
            OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_warr_intimidating_shout_SpellScript::FilterTargets, EFFECT_1, TARGET_UNIT_SRC_AREA_ENEMY);
            OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_warr_intimidating_shout_SpellScript::FilterTargets, EFFECT_2, TARGET_UNIT_SRC_AREA_ENEMY);
        }
    };

    SpellScript* GetSpellScript() const override
    {
        return new spell_warr_intimidating_shout_SpellScript();
    }
};

// 20230 - Warrior relatiation - attacks in back do not enable proc
class spell_warr_retaliation : public SpellScriptLoader
{
public:
    spell_warr_retaliation() : SpellScriptLoader("spell_warr_retaliation") { }

    class spell_warr_retaliation_AuraScript : public AuraScript
    {
        PrepareAuraScript(spell_warr_retaliation_AuraScript);

        bool Validate(SpellInfo const* /*spellInfo*/) override
        {
            return ValidateSpellInfo({ SPELL_RELATIATION_PROC });
        }

        bool CheckProc(ProcEventInfo& eventInfo)
        {
            // check attack comes not from behind
            if (!GetCaster()->HasInArc(float(M_PI), eventInfo.GetActor()) || GetCaster()->HasUnitState(UNIT_STATE_STUNNED))
                return false;

            return true;
        }

        void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
        {
            PreventDefaultAction();
            GetCaster()->CastSpell(eventInfo.GetActor(), SPELL_RELATIATION_PROC, aurEff);
        }

        void Register() override
        {
            DoCheckProc += AuraCheckProcFn(spell_warr_retaliation_AuraScript::CheckProc);
            OnEffectProc += AuraEffectProcFn(spell_warr_retaliation_AuraScript::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
        }
    };

    AuraScript* GetAuraScript() const override
    {
        return new spell_warr_retaliation_AuraScript();
    }
};

// -29834 - Second wind
class spell_warr_second_wind : public SpellScriptLoader
{
public:
    spell_warr_second_wind() : SpellScriptLoader("spell_warr_second_wind") { }

    class spell_warr_second_wind_AuraScript : public AuraScript
    {
        PrepareAuraScript(spell_warr_second_wind_AuraScript);

        bool Validate(SpellInfo const* /*spellInfo*/) override
        {
            return ValidateSpellInfo({ SPELL_WARRIOR_SECOND_WIND_TRIGGER_1, SPELL_WARRIOR_SECOND_WIND_TRIGGER_2 });
        }

        bool CheckProc(ProcEventInfo& eventInfo)
        {
            Spell const* procSpell = eventInfo.GetProcSpell();
            // only for spells and hit/crit (trigger start always) and not start from self casted spells (5530 Mace Stun Effect for example)
            if (procSpell == nullptr || !(eventInfo.GetHitMask() & (PROC_HIT_NORMAL | PROC_HIT_CRITICAL)) || GetCaster() == eventInfo.GetActor())
                return false;

            return procSpell->GetSpellInfo()->GetAllEffectsMechanicMask() & ((1 << MECHANIC_ROOT) | (1 << MECHANIC_STUN));
        }

        void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
        {
            uint32 triggered_spell_id;
            switch (GetSpellInfo()->Id)
            {
            case 29834: triggered_spell_id = SPELL_WARRIOR_SECOND_WIND_TRIGGER_1; break;
            case 29838: triggered_spell_id = SPELL_WARRIOR_SECOND_WIND_TRIGGER_2; break;
            default:
                TC_LOG_ERROR("spell", "spell_warr_second_wind non handled spell id: %u", GetSpellInfo()->Id);
                return;
            }

            PreventDefaultAction();
            GetCaster()->CastSpell(GetCaster(), triggered_spell_id, aurEff);
        }

        void Register() override
        {
            DoCheckProc += AuraCheckProcFn(spell_warr_second_wind_AuraScript::CheckProc);
            OnEffectProc += AuraEffectProcFn(spell_warr_second_wind_AuraScript::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
        }
    };

    AuraScript* GetAuraScript() const override
    {
        return new spell_warr_second_wind_AuraScript();
    }
};

// 12328, 18765, 35429 - Sweeping Strikes
class spell_warr_sweeping_strikes : public SpellScriptLoader
{
public:
    spell_warr_sweeping_strikes() : SpellScriptLoader("spell_warr_sweeping_strikes") { }

    class spell_warr_sweeping_strikes_AuraScript : public AuraScript
    {
        PrepareAuraScript(spell_warr_sweeping_strikes_AuraScript);

        bool Validate(SpellInfo const* /*spellInfo*/) override
        {
            return ValidateSpellInfo({ SPELL_WARRIOR_SWEEPING_STRIKES_EXTRA_ATTACK_1, SPELL_WARRIOR_SWEEPING_STRIKES_EXTRA_ATTACK_2 });
        }

        bool CheckProc(ProcEventInfo& eventInfo)
        {
            _procTarget = eventInfo.GetActor()->SelectNearbyTarget(eventInfo.GetProcTarget());
            return _procTarget != nullptr;
        }

        void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
        {
            PreventDefaultAction();
            if (DamageInfo* damageInfo = eventInfo.GetDamageInfo())
            {
                SpellInfo const* spellInfo = damageInfo->GetSpellInfo();
                if (spellInfo && spellInfo->Id == SPELL_WARRIOR_EXECUTE && !_procTarget->HasAuraState(AURA_STATE_HEALTHLESS_20_PERCENT))
                {
                    // If triggered by Execute (while target is not under 20% hp) deals normalized weapon damage (sun: no idea how this is possible, but let's suppose it makes sense for now. Remove if needed)
                    GetTarget()->CastSpell(_procTarget, SPELL_WARRIOR_SWEEPING_STRIKES_EXTRA_ATTACK_2, aurEff);
                }
                else
                {
                    CastSpellExtraArgs args(aurEff);
                    args.AddSpellBP0(damageInfo->GetDamage());
                    GetTarget()->CastSpell(_procTarget, SPELL_WARRIOR_SWEEPING_STRIKES_EXTRA_ATTACK_1, args);
                }
            }
        }

        void Register() override
        {
            DoCheckProc += AuraCheckProcFn(spell_warr_sweeping_strikes_AuraScript::CheckProc);
            OnEffectProc += AuraEffectProcFn(spell_warr_sweeping_strikes_AuraScript::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
        }

        Unit* _procTarget = nullptr;
    };

    AuraScript* GetAuraScript() const override
    {
        return new spell_warr_sweeping_strikes_AuraScript();
    }
};

// -12162 - Deep Wounds
class spell_warr_deep_wounds : public SpellScript
{
    PrepareSpellScript(spell_warr_deep_wounds);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo(
            {
                SPELL_WARRIOR_DEEP_WOUNDS_RANK_1,
                SPELL_WARRIOR_DEEP_WOUNDS_RANK_2,
                SPELL_WARRIOR_DEEP_WOUNDS_RANK_3,
                SPELL_WARRIOR_DEEP_WOUNDS_PERIODIC
            });
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        int32 damage = GetEffectValue();
        Unit* caster = GetCaster();
        if (Unit* target = GetHitUnit())
        {
#ifdef LICH_KING
            ApplyPct(damage, 16 * GetSpellInfo()->GetRank());
#else
            ApplyPct(damage, 20 * GetSpellInfo()->GetRank()); //"dealing 20% of your melee weapon's average damage"
#endif

            SpellInfo const* spellInfo = sSpellMgr->AssertSpellInfo(SPELL_WARRIOR_DEEP_WOUNDS_PERIODIC);

            ASSERT(spellInfo->GetMaxTicks() > 0);
            damage /= spellInfo->GetMaxTicks();

            CastSpellExtraArgs args(TRIGGERED_FULL_MASK);
            args.AddSpellBP0(damage);
            caster->CastSpell(target, SPELL_WARRIOR_DEEP_WOUNDS_PERIODIC, args);
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_warr_deep_wounds::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// -12834 - Deep Wounds Aura
class spell_warr_deep_wounds_aura : public AuraScript
{
    PrepareAuraScript(spell_warr_deep_wounds_aura);

    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo({ spellInfo->Effects[EFFECT_0].TriggerSpell });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        DamageInfo* damageInfo = eventInfo.GetDamageInfo();
        if (!damageInfo)
            return false;

        return eventInfo.GetActor()->GetTypeId() == TYPEID_PLAYER;
    }

    void OnProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();

        Unit* actor = eventInfo.GetActor();
        float damage = 0.f;

        if (eventInfo.GetDamageInfo()->GetAttackType() == OFF_ATTACK)
            damage = (actor->GetFloatValue(UNIT_FIELD_MINOFFHANDDAMAGE) + actor->GetFloatValue(UNIT_FIELD_MAXOFFHANDDAMAGE)) / 2.f;
        else
            damage = (actor->GetFloatValue(UNIT_FIELD_MINDAMAGE) + actor->GetFloatValue(UNIT_FIELD_MAXDAMAGE)) / 2.f;

        CastSpellExtraArgs args(aurEff);
        args.AddSpellBP0(damage);
        actor->CastSpell(eventInfo.GetProcTarget(), GetSpellInfo()->Effects[EFFECT_0].TriggerSpell, args);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_warr_deep_wounds_aura::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_warr_deep_wounds_aura::OnProc, EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL);
    }
};

// -23881 - Bloodthirst
class spell_warr_bloodthirst : public SpellScript
{
    PrepareSpellScript(spell_warr_bloodthirst);

    void HandleDamage(SpellEffIndex /*effIndex*/)
    {
        uint32 APbonus = GetCaster()->GetTotalAttackPowerValue(BASE_ATTACK);
        if (Unit* victim = GetHitUnit())
            APbonus += victim->GetTotalAuraModifier(SPELL_AURA_MELEE_ATTACK_POWER_ATTACKER_BONUS);

        SetEffectValue(CalculatePct(APbonus, GetEffectValue()));
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
#ifdef LICH_KING
        uint32 spellId = SPELL_WARRIOR_BLOODTHIRST;
#else
        uint32 spellId = sSpellMgr->GetSpellWithRank(SPELL_WARRIOR_BLOODTHIRST, GetSpell()->GetSpellInfo()->GetRank());
#endif
        GetCaster()->CastSpell(GetCaster(), spellId, true);
    }

    void Register() override
    {
        OnEffectLaunchTarget += SpellEffectFn(spell_warr_bloodthirst::HandleDamage, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
#ifdef LICH_KING
        OnEffectHit += SpellEffectFn(spell_warr_bloodthirst::HandleDummy, EFFECT_1, SPELL_EFFECT_DUMMY);
#else
        OnEffectHit += SpellEffectFn(spell_warr_bloodthirst::HandleDummy, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
#endif
    }
};

void AddSC_warrior_spell_scripts()
{
    new spell_warr_intimidating_shout();
    new spell_warr_retaliation();
    new spell_warr_second_wind();
    new spell_warr_sweeping_strikes();
    RegisterSpellScript(spell_warr_deep_wounds);
    RegisterAuraScript(spell_warr_deep_wounds_aura);
    RegisterSpellScript(spell_warr_bloodthirst);
}
