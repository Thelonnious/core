/*
* Copyright (C) 2008-2014 TrinityCore <http://www.trinitycore.org/>
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the
* Free Software Foundation; either version 2 of the License, or (at your
* option) any later version.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _SPELLINFO_H
#define _SPELLINFO_H

#include "SharedDefines.h"
#include "DBCStructure.h"
#include "Util.h"

enum AuraType;
class SpellInfo;
class Spell;

enum SpellCustomAttributes
{
    //SPELL_ATTR0_CU_PLAYERS_ONLY      0x00000001,
    SPELL_ATTR0_CU_CONE_BACK                     = 0x00000002,
    SPELL_ATTR0_CU_CONE_LINE                     = 0x00000004,
    SPELL_ATTR0_CU_SHARE_DAMAGE                  = 0x00000008,
    SPELL_ATTR0_CU_AURA_HOT                      = 0x00000010,
    SPELL_ATTR0_CU_AURA_DOT                      = 0x00000020,
    SPELL_ATTR0_CU_AURA_CC                       = 0x00000040,
    SPELL_ATTR0_CU_AURA_SPELL                    = 0x00000080,
    SPELL_ATTR0_CU_DIRECT_DAMAGE                 = 0x00000100,
    SPELL_ATTR0_CU_CHARGE                        = 0x00000200,
    SPELL_ATTR0_CU_LINK_CAST                     = 0x00000400,
    SPELL_ATTR0_CU_LINK_HIT                      = 0x00000800,
    SPELL_ATTR0_CU_LINK_AURA                     = 0x00001000,
    SPELL_ATTR0_CU_LINK_REMOVE                   = 0x00002000,
    SPELL_ATTR0_CU_MOVEMENT_IMPAIR               = 0x00004000,
    SPELL_ATTR0_CU_IGNORE_ARMOR                  = 0x00008000,
    SPELL_ATTR0_CU_SAME_STACK_DIFF_CASTERS       = 0x00010000,
    SPELL_ATTR0_CU_ONE_STACK_PER_CASTER_SPECIAL  = 0x00020000,
    SPELL_ATTR0_CU_THREAT_GOES_TO_CURRENT_CASTER = 0x00040000,     // Instead of original caster
    SPELL_ATTR0_CU_CANT_BREAK_CC                 = 0x00080000,     // Damage done by these spells won't break crowd controls
    SPELL_ATTR0_CU_PUT_ONLY_CASTER_IN_COMBAT     = 0x00100000,
    SPELL_ATTR0_CU_REMOVE_ON_INSTANCE_ENTER      = 0x00200000,     // Auras removed when target enters an instance
    SPELL_ATTR0_CU_AOE_CANT_TARGET_SELF          = 0x00400000,
    SPELL_ATTR0_CU_CONE_180                      = 0x00800000,
    SPELL_ATTR0_CU_CAN_CHANNEL_DEAD_TARGET       = 0x01000000,

    SPELL_ATTR0_CU_NEGATIVE_EFF0                 = 0x02000000,
    SPELL_ATTR0_CU_NEGATIVE_EFF1                 = 0x04000000,
    SPELL_ATTR0_CU_NEGATIVE_EFF2                 = 0x08000000,

    SPELL_ATTR0_CU_NEGATIVE                      = SPELL_ATTR0_CU_NEGATIVE_EFF0 | SPELL_ATTR0_CU_NEGATIVE_EFF1 | SPELL_ATTR0_CU_NEGATIVE_EFF2
};

enum SpellCastTargetFlags
{
    /*TARGET_FLAG_NONE             = 0x0000,
    TARGET_FLAG_SWIMMER          = 0x0002,
    TARGET_FLAG_ITEM             = 0x0010,
    TARGET_FLAG_SOURCE_AREA      = 0x0020,
    TARGET_FLAG_DEST_AREA        = 0x0040,
    TARGET_FLAG_UNKNOWN          = 0x0080,
    TARGET_FLAG_SELF             = 0x0100,
    TARGET_FLAG_PVP_CORPSE       = 0x0200,
    TARGET_FLAG_MASS_SPIRIT_HEAL = 0x0400,
    TARGET_FLAG_BEAST_CORPSE     = 0x0402,
    TARGET_FLAG_OBJECT           = 0x4000,
    TARGET_FLAG_RESURRECTABLE    = 0x8000*/

    TARGET_FLAG_SELF            = 0x00000000,
    TARGET_FLAG_UNIT            = 0x00000002,               // pguid
    TARGET_FLAG_ITEM            = 0x00000010,               // pguid
    TARGET_FLAG_SOURCE_LOCATION = 0x00000020,               // 3 float
    TARGET_FLAG_DEST_LOCATION   = 0x00000040,               // 3 float
    TARGET_FLAG_OBJECT_UNK      = 0x00000080,               // ?
    TARGET_FLAG_PVP_CORPSE      = 0x00000200,               // pguid
    TARGET_FLAG_OBJECT          = 0x00000800,               // pguid
    TARGET_FLAG_TRADE_ITEM      = 0x00001000,               // pguid
    TARGET_FLAG_STRING          = 0x00002000,               // string
    TARGET_FLAG_UNK1            = 0x00004000,               // ?
    TARGET_FLAG_CORPSE          = 0x00008000,               // pguid
    TARGET_FLAG_UNK2            = 0x00010000                // pguid
};

// Spell clasification
enum SpellSpecificType
{
    SPELL_NORMAL            = 0,
    SPELL_SEAL              = 1,
    SPELL_BLESSING          = 2,
    SPELL_AURA              = 3,
    SPELL_STING             = 4,
    SPELL_CURSE             = 5,
    SPELL_ASPECT            = 6,
    SPELL_TRACKER           = 7,
    SPELL_WARLOCK_ARMOR     = 8,
    SPELL_MAGE_ARMOR        = 9,
    SPELL_ELEMENTAL_SHIELD  = 10,
    SPELL_MAGE_POLYMORPH    = 11,
    SPELL_POSITIVE_SHOUT    = 12,
    SPELL_JUDGEMENT         = 13,
    SPELL_BATTLE_ELIXIR     = 14,
    SPELL_GUARDIAN_ELIXIR   = 15,
    SPELL_FLASK_ELIXIR      = 16,
    SPELL_WARLOCK_CORRUPTION= 17,
    SPELL_WELL_FED          = 18,
    SPELL_DRINK             = 19,
    SPELL_FOOD              = 20,
    SPELL_CHARM             = 21,
    SPELL_WARRIOR_ENRAGE    = 22,
    SPELL_ARMOR_REDUCE      = 23,
    SPELL_DRUID_MANGLE      = 24
};

class SpellImplicitTargetInfo
{
private:
    Targets _target;

public:
    SpellImplicitTargetInfo() : _target(Targets(0)) { }
    SpellImplicitTargetInfo(uint32 target);

    bool IsArea() const;
    /*
    SpellTargetSelectionCategories GetSelectionCategory() const;
    SpellTargetReferenceTypes GetReferenceType() const;
    SpellTargetObjectTypes GetObjectType() const;
    SpellTargetCheckTypes GetCheckType() const;
    SpellTargetDirectionTypes GetDirectionType() const;
    float CalcDirectionAngle() const;
    */
    Targets GetTarget() const;
    uint32 GetExplicitTargetMask(bool& srcSet, bool& dstSet) const;

    /*
    private:
    struct StaticData
    {
    SpellTargetObjectTypes ObjectType;    // type of object returned by target type
    SpellTargetReferenceTypes ReferenceType; // defines which object is used as a reference when selecting target
    SpellTargetSelectionCategories SelectionCategory;
    SpellTargetCheckTypes SelectionCheckType; // defines selection criteria
    SpellTargetDirectionTypes DirectionType; // direction for cone and dest targets
    };
    static StaticData _data[TOTAL_SPELL_TARGETS];*/
};

class SpellEffectInfo
{
    SpellInfo const* _spellInfo;
    uint8 _effIndex;
public:
    uint32    Effect;
    uint32    ApplyAuraName;
    uint32    Amplitude;
    int32     DieSides;
    int32     BaseDice; //not LK
    float     DicePerLevel; //not LK
    float     RealPointsPerLevel;
    int32     BasePoints;
    float     PointsPerComboPoint;
    float     ValueMultiplier;
    float     DamageMultiplier;
#ifdef LICH_KING
    float     BonusMultiplier;
#endif
    int32     MiscValue;
    int32     MiscValueB;
    Mechanics Mechanic;
    SpellImplicitTargetInfo TargetA;
    SpellImplicitTargetInfo TargetB;
    SpellRadiusEntry const* RadiusEntry;
    uint32    ChainTarget;
    uint32    ItemType;
    uint32    TriggerSpell;
#ifdef LICH_KING
    flag96    SpellClassMask;
#endif
    //TODO Spellinfo std::list<Condition*>* ImplicitTargetConditions;

    SpellEffectInfo() : _spellInfo(NULL), _effIndex(0), Effect(0), ApplyAuraName(0), Amplitude(0), DieSides(0),
        RealPointsPerLevel(0), BasePoints(0), PointsPerComboPoint(0), ValueMultiplier(0), DamageMultiplier(0),
        MiscValue(0), MiscValueB(0), Mechanic(MECHANIC_NONE), RadiusEntry(NULL), ChainTarget(0),
        ItemType(0), TriggerSpell(0), /*ImplicitTargetConditions(NULL) */
#ifdef LICH_KING
        BonusMultiplier(0)
#else
        BaseDice(0), DicePerLevel(0)
#endif    
    {}
    SpellEffectInfo(SpellEntry const* spellEntry, SpellInfo const* spellInfo, uint8 effIndex);

    bool IsEffect() const;
    bool IsEffect(SpellEffects effectName) const;
    bool IsAura() const;
    bool IsAura(AuraType aura) const;
    bool IsTargetingArea() const;
    bool IsAreaAuraEffect() const;
    bool IsFarUnitTargetEffect() const;
    bool IsFarDestTargetEffect() const;
    bool IsUnitOwnedAuraEffect() const;

    bool HasRadius() const;
    //always use GetSpellModOwner() for caster
    float CalcRadius(Unit* caster = NULL, Spell* = NULL) const;
private:
    /*
    struct StaticData
    {
    SpellEffectImplicitTargetTypes ImplicitTargetType; // defines what target can be added to effect target list if there's no valid target type provided for effect
    SpellTargetObjectTypes UsedTargetObjectType; // defines valid target object type for spell effect
    };
    static StaticData _data[TOTAL_SPELL_EFFECTS]; */
};

class SpellInfo
{
public:
    uint32 Id;
    SpellCategoryEntry const* Category;
    uint32 Dispel;
    uint32 Mechanic;
    uint32 Attributes;
    uint32 AttributesEx;
    uint32 AttributesEx2;
    uint32 AttributesEx3;
    uint32 AttributesEx4;
    uint32 AttributesEx5;
    uint32 AttributesEx6;
    uint32 AttributesEx7; //LK
    uint32 AttributesCu;
    uint32 Stances;
    uint32 StancesNot;
    uint32 Targets;
    uint32 TargetCreatureType;
    uint32 RequiresSpellFocus;
    uint32 FacingCasterFlags;
    uint32 CasterAuraState;
    uint32 TargetAuraState;
    uint32 CasterAuraStateNot;
    uint32 TargetAuraStateNot;
    // LK --
    uint32 CasterAuraSpell;
    uint32 TargetAuraSpell;
    uint32 ExcludeCasterAuraSpell;
    uint32 ExcludeTargetAuraSpell;
    // -- LK
    SpellCastTimesEntry const* CastTimeEntry;
    uint32 RecoveryTime;
    uint32 CategoryRecoveryTime;
    uint32 StartRecoveryCategory;
    uint32 StartRecoveryTime;
    uint32 InterruptFlags;
    uint32 AuraInterruptFlags;
    uint32 ChannelInterruptFlags;
    uint32 ProcFlags;
    uint32 ProcChance;
    uint32 ProcCharges;
    uint32 MaxLevel;
    uint32 BaseLevel;
    uint32 SpellLevel;
    SpellDurationEntry const* DurationEntry;
    uint32 PowerType;
    uint32 ManaCost;
    uint32 ManaCostPerlevel;
    uint32 ManaPerSecond;
    uint32 ManaPerSecondPerLevel;
    uint32 ManaCostPercentage;
    uint32 RuneCostID; //LK
    SpellRangeEntry const* RangeEntry;
    float  Speed;
    uint32 StackAmount;
    uint32 Totem[2];
    int32  Reagent[MAX_SPELL_REAGENTS];
    uint32 ReagentCount[MAX_SPELL_REAGENTS];
    int32  EquippedItemClass;
    int32  EquippedItemSubClassMask;
    int32  EquippedItemInventoryTypeMask;
    //spell effects later
    uint32 TotemCategory[2];
#ifdef LICH_KING
    uint32 SpellVisual[2];
#else
    uint32 SpellVisual;
#endif
    uint32 SpellIconID;
    uint32 ActiveIconID;
    char* SpellName[16];
    char* Rank[16];
    uint32 MaxTargetLevel;
    uint32 MaxAffectedTargets;
    uint32 SpellFamilyName;
#ifdef LICH_KING
    flag96 SpellFamilyFlags;
#else
    uint64 SpellFamilyFlags;
#endif
    uint32 DmgClass;
    uint32 PreventionType;
#ifdef LICH_KING
    int32  AreaGroupId;
#else
    uint32 AreaId;
#endif
    uint32 SchoolMask;
    SpellEffectInfo Effects[MAX_SPELL_EFFECTS];
    /* TODO SPELLINFO
    uint32 ExplicitTargetMask;
    SpellChainNode const* ChainEntry;
    */

    SpellInfo(SpellEntry const* spellEntry);
    ~SpellInfo();

    uint32 GetCategory() const;
    /** -1 for all indexes */
    bool HasEffect(SpellEffects effect, uint8 effIndex = -1) const;
    bool HasAura(AuraType aura) const;
    bool HasAreaAuraEffect() const;

    inline bool HasAttribute(SpellAttr0 attribute) const { return !!(Attributes & attribute); }
    inline bool HasAttribute(SpellAttr1 attribute) const { return !!(AttributesEx & attribute); }
    inline bool HasAttribute(SpellAttr2 attribute) const { return !!(AttributesEx2 & attribute); }
    inline bool HasAttribute(SpellAttr3 attribute) const { return !!(AttributesEx3 & attribute); }
    inline bool HasAttribute(SpellAttr4 attribute) const { return !!(AttributesEx4 & attribute); }
    inline bool HasAttribute(SpellAttr5 attribute) const { return !!(AttributesEx5 & attribute); }
    inline bool HasAttribute(SpellAttr6 attribute) const { return !!(AttributesEx6 & attribute); }
    inline bool HasAttribute(SpellCustomAttributes customAttribute) const { return !!(AttributesCu & customAttribute); }

    bool IsAffectingArea() const;
    bool IsTargetingArea() const;
    bool IsAreaAuraEffect() const;
    bool IsChanneled() const;
    bool NeedsComboPoints() const;
    bool IsBreakingStealth() const;
    bool IsDeathPersistent() const;
    bool HasVisual(uint32 visual) const;
    bool CanBeUsedInCombat() const;
    bool IsPassive() const;
    /** Some spells, such as dispells, can be positive or negative depending on target */
    bool IsPositive(bool hostileTarget = false) const;
    /** Some effects, such as dispells, can be positive or negative depending on target */
    bool IsPositiveEffect(uint8 effIndex, bool hostileTarget = false) const;
    /* Internal check, will try to deduce result from spell effects + lots of hardcoded id's
    Use "deep" to enable recursive search in triggered spells
    */
    bool _IsPositiveEffect(uint32 effIndex, bool deep = true) const;
    bool _IsPositiveSpell() const;

    SpellSchoolMask GetSchoolMask() const;
    uint32 GetAllEffectsMechanicMask() const;
    uint32 GetEffectMechanicMask(uint8 effIndex) const;
    uint32 GetSpellMechanicMaskByEffectMask(uint32 effectMask) const;
    Mechanics GetEffectMechanic(uint8 effIndex) const;
    bool HasAnyEffectMechanic() const;

    AuraStateType GetAuraState() const;
    SpellSpecificType GetSpellSpecific() const;
    SpellSpecificType GetSpellElixirSpecific() const;

    float GetMinRange(bool positive = false) const;
    //always use GetSpellModOwner() for caster
    float GetMaxRange(bool positive = false, Unit* caster = NULL, Spell* spell = NULL) const;

private:
    //apply SpellCustomAttributes. Some custom attributes are also added in SpellMgr::LoadSpellLinked()
    void LoadCustomAttributes();
    static bool _IsPositiveTarget(uint32 targetA, uint32 targetB);
};

#endif // _SPELLINFO_H