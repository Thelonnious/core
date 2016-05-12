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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef _SPELLMGR_H
#define _SPELLMGR_H

// For static or at-server-startup loaded spell data
// For more high level function for sSpellStore data

#include "SharedDefines.h"
#include "DBCStructure.h"
#include "SpellInfo.h"

#include <map>

class Player;
class Spell;

enum SpellCastResult : int
{
    SPELL_CAST_OK                               = -1,
    SPELL_FAILED_AFFECTING_COMBAT               = 0x00,
    SPELL_FAILED_ALREADY_AT_FULL_HEALTH         = 0x01,
    SPELL_FAILED_ALREADY_AT_FULL_MANA           = 0x02,
    SPELL_FAILED_ALREADY_AT_FULL_POWER          = 0x03,
    SPELL_FAILED_ALREADY_BEING_TAMED            = 0x04,
    SPELL_FAILED_ALREADY_HAVE_CHARM             = 0x05,
    SPELL_FAILED_ALREADY_HAVE_SUMMON            = 0x06,
    SPELL_FAILED_ALREADY_OPEN                   = 0x07,
    SPELL_FAILED_MORE_POWERFUL_SPELL_ACTIVE     = 0x08, //NYI, check SunWell core IsAuraStronger & IsStrongerAuraActive
    SPELL_FAILED_AUTOTRACK_INTERRUPTED          = 0x09,
    SPELL_FAILED_BAD_IMPLICIT_TARGETS           = 0x0A,
    SPELL_FAILED_BAD_TARGETS                    = 0x0B,
    SPELL_FAILED_CANT_BE_CHARMED                = 0x0C,
    SPELL_FAILED_CANT_BE_DISENCHANTED           = 0x0D,
    SPELL_FAILED_CANT_BE_DISENCHANTED_SKILL     = 0x0E,
    SPELL_FAILED_CANT_BE_PROSPECTED             = 0x0F,
    SPELL_FAILED_CANT_CAST_ON_TAPPED            = 0x10,
    SPELL_FAILED_CANT_DUEL_WHILE_INVISIBLE      = 0x11,
    SPELL_FAILED_CANT_DUEL_WHILE_STEALTHED      = 0x12,
    SPELL_FAILED_CANT_STEALTH                   = 0x13,
    SPELL_FAILED_CASTER_AURASTATE               = 0x14,
    SPELL_FAILED_CASTER_DEAD                    = 0x15,
    SPELL_FAILED_CHARMED                        = 0x16,
    SPELL_FAILED_CHEST_IN_USE                   = 0x17,
    SPELL_FAILED_CONFUSED                       = 0x18,
    SPELL_FAILED_DONT_REPORT                    = 0x19,
    SPELL_FAILED_EQUIPPED_ITEM                  = 0x1A,
    SPELL_FAILED_EQUIPPED_ITEM_CLASS            = 0x1B,
    SPELL_FAILED_EQUIPPED_ITEM_CLASS_MAINHAND   = 0x1C,
    SPELL_FAILED_EQUIPPED_ITEM_CLASS_OFFHAND    = 0x1D,
    SPELL_FAILED_ERROR                          = 0x1E,
    SPELL_FAILED_FIZZLE                         = 0x1F,
    SPELL_FAILED_FLEEING                        = 0x20,
    SPELL_FAILED_FOOD_LOWLEVEL                  = 0x21,
    SPELL_FAILED_HIGHLEVEL                      = 0x22,
    SPELL_FAILED_HUNGER_SATIATED                = 0x23,
    SPELL_FAILED_IMMUNE                         = 0x24,
    SPELL_FAILED_INTERRUPTED                    = 0x25,
    SPELL_FAILED_INTERRUPTED_COMBAT             = 0x26,
    SPELL_FAILED_ITEM_ALREADY_ENCHANTED         = 0x27,
    SPELL_FAILED_ITEM_GONE                      = 0x28,
    SPELL_FAILED_ITEM_NOT_FOUND                 = 0x29,
    SPELL_FAILED_ITEM_NOT_READY                 = 0x2A,
    SPELL_FAILED_LEVEL_REQUIREMENT              = 0x2B,
    SPELL_FAILED_LINE_OF_SIGHT                  = 0x2C,
    SPELL_FAILED_LOWLEVEL                       = 0x2D,
    SPELL_FAILED_LOW_CASTLEVEL                  = 0x2E,
    SPELL_FAILED_MAINHAND_EMPTY                 = 0x2F,
    SPELL_FAILED_MOVING                         = 0x30,
    SPELL_FAILED_NEED_AMMO                      = 0x31,
    SPELL_FAILED_NEED_AMMO_POUCH                = 0x32,
    SPELL_FAILED_NEED_EXOTIC_AMMO               = 0x33,
    SPELL_FAILED_NOPATH                         = 0x34,
    SPELL_FAILED_NOT_BEHIND                     = 0x35,
    SPELL_FAILED_NOT_FISHABLE                   = 0x36,
    SPELL_FAILED_NOT_FLYING                     = 0x37,
    SPELL_FAILED_NOT_HERE                       = 0x38,
    SPELL_FAILED_NOT_INFRONT                    = 0x39,
    SPELL_FAILED_NOT_IN_CONTROL                 = 0x3A,
    SPELL_FAILED_NOT_KNOWN                      = 0x3B,
    SPELL_FAILED_NOT_MOUNTED                    = 0x3C,
    SPELL_FAILED_NOT_ON_TAXI                    = 0x3D,
    SPELL_FAILED_NOT_ON_TRANSPORT               = 0x3E,
    SPELL_FAILED_NOT_READY                      = 0x3F,
    SPELL_FAILED_NOT_SHAPESHIFT                 = 0x40,
    SPELL_FAILED_NOT_STANDING                   = 0x41,
    SPELL_FAILED_NOT_TRADEABLE                  = 0x42,
    SPELL_FAILED_NOT_TRADING                    = 0x43,
    SPELL_FAILED_NOT_UNSHEATHED                 = 0x44,
    SPELL_FAILED_NOT_WHILE_GHOST                = 0x45,
    SPELL_FAILED_NO_AMMO                        = 0x46,
    SPELL_FAILED_NO_CHARGES_REMAIN              = 0x47,
    SPELL_FAILED_NO_CHAMPION                    = 0x48,
    SPELL_FAILED_NO_COMBO_POINTS                = 0x49,
    SPELL_FAILED_NO_DUELING                     = 0x4A,
    SPELL_FAILED_NO_ENDURANCE                   = 0x4B,
    SPELL_FAILED_NO_FISH                        = 0x4C,
    SPELL_FAILED_NO_ITEMS_WHILE_SHAPESHIFTED    = 0x4D,
    SPELL_FAILED_NO_MOUNTS_ALLOWED              = 0x4E,
    SPELL_FAILED_NO_PET                         = 0x4F,
    SPELL_FAILED_NO_POWER                       = 0x50,
    SPELL_FAILED_NOTHING_TO_DISPEL              = 0x51,
    SPELL_FAILED_NOTHING_TO_STEAL               = 0x52,
    SPELL_FAILED_ONLY_ABOVEWATER                = 0x53,
    SPELL_FAILED_ONLY_DAYTIME                   = 0x54,
    SPELL_FAILED_ONLY_INDOORS                   = 0x55,
    SPELL_FAILED_ONLY_MOUNTED                   = 0x56,
    SPELL_FAILED_ONLY_NIGHTTIME                 = 0x57,
    SPELL_FAILED_ONLY_OUTDOORS                  = 0x58,
    SPELL_FAILED_ONLY_SHAPESHIFT                = 0x59,
    SPELL_FAILED_ONLY_STEALTHED                 = 0x5A,
    SPELL_FAILED_ONLY_UNDERWATER                = 0x5B,
    SPELL_FAILED_OUT_OF_RANGE                   = 0x5C,
    SPELL_FAILED_PACIFIED                       = 0x5D,
    SPELL_FAILED_POSSESSED                      = 0x5E,
    SPELL_FAILED_REAGENTS                       = 0x5F,
    SPELL_FAILED_REQUIRES_AREA                  = 0x60,
    SPELL_FAILED_REQUIRES_SPELL_FOCUS           = 0x61,
    SPELL_FAILED_ROOTED                         = 0x62,
    SPELL_FAILED_SILENCED                       = 0x63,
    SPELL_FAILED_SPELL_IN_PROGRESS              = 0x64,
    SPELL_FAILED_SPELL_LEARNED                  = 0x65,
    SPELL_FAILED_SPELL_UNAVAILABLE              = 0x66,
    SPELL_FAILED_STUNNED                        = 0x67,
    SPELL_FAILED_TARGETS_DEAD                   = 0x68,
    SPELL_FAILED_TARGET_AFFECTING_COMBAT        = 0x69,
    SPELL_FAILED_TARGET_AURASTATE               = 0x6A,
    SPELL_FAILED_TARGET_DUELING                 = 0x6B,
    SPELL_FAILED_TARGET_ENEMY                   = 0x6C,
    SPELL_FAILED_TARGET_ENRAGED                 = 0x6D,
    SPELL_FAILED_TARGET_FRIENDLY                = 0x6E,
    SPELL_FAILED_TARGET_IN_COMBAT               = 0x6F,
    SPELL_FAILED_TARGET_IS_PLAYER               = 0x70,
    SPELL_FAILED_TARGET_IS_PLAYER_CONTROLLED    = 0x71,
    SPELL_FAILED_TARGET_NOT_DEAD                = 0x72,
    SPELL_FAILED_TARGET_NOT_IN_PARTY            = 0x73,
    SPELL_FAILED_TARGET_NOT_LOOTED              = 0x74,
    SPELL_FAILED_TARGET_NOT_PLAYER              = 0x75,
    SPELL_FAILED_TARGET_NO_POCKETS              = 0x76,
    SPELL_FAILED_TARGET_NO_WEAPONS              = 0x77,
    SPELL_FAILED_TARGET_UNSKINNABLE             = 0x78,
    SPELL_FAILED_THIRST_SATIATED                = 0x79,
    SPELL_FAILED_TOO_CLOSE                      = 0x7A,
    SPELL_FAILED_TOO_MANY_OF_ITEM               = 0x7B,
    SPELL_FAILED_TOTEM_CATEGORY                 = 0x7C,
    SPELL_FAILED_TOTEMS                         = 0x7D,
    SPELL_FAILED_TRAINING_POINTS                = 0x7E,
    SPELL_FAILED_TRY_AGAIN                      = 0x7F,
    SPELL_FAILED_UNIT_NOT_BEHIND                = 0x80,
    SPELL_FAILED_UNIT_NOT_INFRONT               = 0x81,
    SPELL_FAILED_WRONG_PET_FOOD                 = 0x82,
    SPELL_FAILED_NOT_WHILE_FATIGUED             = 0x83,
    SPELL_FAILED_TARGET_NOT_IN_INSTANCE         = 0x84,
    SPELL_FAILED_NOT_WHILE_TRADING              = 0x85,
    SPELL_FAILED_TARGET_NOT_IN_RAID             = 0x86,
    SPELL_FAILED_DISENCHANT_WHILE_LOOTING       = 0x87,
    SPELL_FAILED_PROSPECT_WHILE_LOOTING         = 0x88,
    SPELL_FAILED_PROSPECT_NEED_MORE             = 0x89,
    SPELL_FAILED_TARGET_FREEFORALL              = 0x8A,
    SPELL_FAILED_NO_EDIBLE_CORPSES              = 0x8B,
    SPELL_FAILED_ONLY_BATTLEGROUNDS             = 0x8C,
    SPELL_FAILED_TARGET_NOT_GHOST               = 0x8D,
    SPELL_FAILED_TOO_MANY_SKILLS                = 0x8E,
    SPELL_FAILED_TRANSFORM_UNUSABLE             = 0x8F,
    SPELL_FAILED_WRONG_WEATHER                  = 0x90,
    SPELL_FAILED_DAMAGE_IMMUNE                  = 0x91,
    SPELL_FAILED_PREVENTED_BY_MECHANIC          = 0x92,
    SPELL_FAILED_PLAY_TIME                      = 0x93,
    SPELL_FAILED_REPUTATION                     = 0x94,
    SPELL_FAILED_MIN_SKILL                      = 0x95,
    SPELL_FAILED_NOT_IN_ARENA                   = 0x96,
    SPELL_FAILED_NOT_ON_SHAPESHIFT              = 0x97,
    SPELL_FAILED_NOT_ON_STEALTHED               = 0x98,
    SPELL_FAILED_NOT_ON_DAMAGE_IMMUNE           = 0x99,
    SPELL_FAILED_NOT_ON_MOUNTED                 = 0x9A,
    SPELL_FAILED_TOO_SHALLOW                    = 0x9B,
    SPELL_FAILED_TARGET_NOT_IN_SANCTUARY        = 0x9C,
    SPELL_FAILED_TARGET_IS_TRIVIAL              = 0x9D,
    SPELL_FAILED_BM_OR_INVISGOD                 = 0x9E,
    SPELL_FAILED_EXPERT_RIDING_REQUIREMENT      = 0x9F,
    SPELL_FAILED_ARTISAN_RIDING_REQUIREMENT     = 0xA0,
    SPELL_FAILED_NOT_IDLE                       = 0xA1,
    SPELL_FAILED_NOT_INACTIVE                   = 0xA2,
    SPELL_FAILED_PARTIAL_PLAYTIME               = 0xA3,
    SPELL_FAILED_NO_PLAYTIME                    = 0xA4,
    SPELL_FAILED_NOT_IN_BATTLEGROUND            = 0xA5,
    SPELL_FAILED_ONLY_IN_ARENA                  = 0xA6,
    SPELL_FAILED_TARGET_LOCKED_TO_RAID_INSTANCE = 0xA7,
#ifdef LICH_KING
        SPELL_FAILED_ONLY_IN_ARENA = 168,
        SPELL_FAILED_TARGET_LOCKED_TO_RAID_INSTANCE = 169,
        SPELL_FAILED_ON_USE_ENCHANT = 170,
        SPELL_FAILED_NOT_ON_GROUND = 171,
        SPELL_FAILED_CUSTOM_ERROR = 172,
        SPELL_FAILED_CANT_DO_THAT_RIGHT_NOW = 173,
        SPELL_FAILED_TOO_MANY_SOCKETS = 174,
        SPELL_FAILED_INVALID_GLYPH = 175,
        SPELL_FAILED_UNIQUE_GLYPH = 176,
        SPELL_FAILED_GLYPH_SOCKET_LOCKED = 177,
        SPELL_FAILED_NO_VALID_TARGETS = 178,
        SPELL_FAILED_ITEM_AT_MAX_CHARGES = 179,
        SPELL_FAILED_NOT_IN_BARBERSHOP = 180,
        SPELL_FAILED_FISHING_TOO_LOW = 181,
        SPELL_FAILED_ITEM_ENCHANT_TRADE_WINDOW = 182,
        SPELL_FAILED_SUMMON_PENDING = 183,
        SPELL_FAILED_MAX_SOCKETS = 184,
        SPELL_FAILED_PET_CAN_RENAME = 185,
        SPELL_FAILED_TARGET_CANNOT_BE_RESURRECTED = 186,
        SPELL_FAILED_UNKNOWN = 187, // actually doesn't exist in client
#else
    SPELL_FAILED_UNKNOWN                        = 0xA8,
#endif
};

enum SpellFamilyNames
{
    SPELLFAMILY_GENERIC     = 0,
    SPELLFAMILY_UNK1        = 1,                            // events, holidays
    // 2 - unused
    SPELLFAMILY_MAGE        = 3,
    SPELLFAMILY_WARRIOR     = 4,
    SPELLFAMILY_WARLOCK     = 5,
    SPELLFAMILY_PRIEST      = 6,
    SPELLFAMILY_DRUID       = 7,
    SPELLFAMILY_ROGUE       = 8,
    SPELLFAMILY_HUNTER      = 9,
    SPELLFAMILY_PALADIN     = 10,
    SPELLFAMILY_SHAMAN      = 11,
    SPELLFAMILY_UNK2        = 12,
    SPELLFAMILY_POTION      = 13
};

enum SpellDisableTypes
{
    SPELL_DISABLE_PLAYER   = 0x1,
    SPELL_DISABLE_CREATURE = 0x2,
    SPELL_DISABLE_PET      = 0x4
};

enum SpellSelectTargetTypes
{
    TARGET_TYPE_DEFAULT,
    TARGET_TYPE_UNIT_CASTER,
    TARGET_TYPE_UNIT_TARGET,
    TARGET_TYPE_UNIT_NEARBY,
    TARGET_TYPE_AREA_SRC,
    TARGET_TYPE_AREA_DST,
    TARGET_TYPE_AREA_CONE,
    TARGET_TYPE_DEST_CASTER,
    TARGET_TYPE_DEST_TARGET,
    TARGET_TYPE_DEST_DEST,
    TARGET_TYPE_DEST_SPECIAL,
    TARGET_TYPE_CHANNEL,
    TARGET_TYPE_DEST_TARGET_ENEMY,
};

// only used in code
enum SpellCategories
{
    SPELLCATEGORY_HEALTH_MANA_POTIONS = 4,
    SPELLCATEGORY_DEVOUR_MAGIC        = 12,
    SPELLCATEGORY_JUDGEMENT           = 1210,               // Judgement (seal trigger)
    SPELLCATEGORY_FOOD                = 11,
    SPELLCATEGORY_DRINK               = 59
};

//some SpellFamilyFlags
enum SpellFamilyFlag : uint64
{
    SPELLFAMILYFLAG_ROGUE_VANISH = 0x000000800LL,
    SPELLFAMILYFLAG_ROGUE_STEALTH = 0x000400000LL,
    SPELLFAMILYFLAG_ROGUE_BACKSTAB = 0x000800004LL,
    SPELLFAMILYFLAG_ROGUE_SAP = 0x000000080LL,
    SPELLFAMILYFLAG_ROGUE_FEINT = 0x008000000LL,
    SPELLFAMILYFLAG_ROGUE_KIDNEYSHOT = 0x000200000LL,
    SPELLFAMILYFLAG_ROGUE_FINISHING_MOVE = 0x9003E0000LL,
    SPELLFAMILYFLAG_ROGUE_DEADLYPOISON = 0x000010000LL,

    SPELLFAMILYFLAG_WARRIOR_SUNDERARMOR = 0x000004000LL,
    SPELLFAMILYFLAG_WARRIOR_VICTORYRUSH = 0x10000000000LL,

    SPELLFAMILYFLAG_SHAMAN_FROST_SHOCK = 0x080000000LL,

    SPELLFAMILYFLAG_DRUID_MANGLE_BEAR = 0x4000000000LL,
    SPELLFAMILYFLAG_DRUID_REJUVENATION = 0x000000010LL,
    SPELLFAMILYFLAG_DRUID_REGROWTH = 0x000000040LL,
    SPELLFAMILYFLAG_DRUID_FAERIEFIRE = 0x000000400LL,
    SPELLFAMILYFLAG_DRUID_CYCLONE = 2000000000LL,

    SPELLFAMILYFLAG_WARLOCK_IMMOLATE = 0x000000004LL,
};

#define SPELL_LINKED_MAX_SPELLS  200000

enum SpellLinkedType
{
    SPELL_LINK_CAST     = 0,            // +: cast; -: remove
    SPELL_LINK_HIT      = 1 * 200000,
    SPELL_LINK_AURA     = 2 * 200000,   // +: aura; -: immune
    SPELL_LINK_REMOVE   = 0,
};

inline bool IsSpellHaveEffect(SpellInfo const *spellInfo, SpellEffects effect)
{
    for(int i= 0; i < MAX_SPELL_EFFECTS; ++i)
        if(spellInfo->Effects[i].Effect == uint32(effect))
            return true;
    return false;
}

//bool IsNoStackAuraDueToAura(uint32 spellId_1, uint32 effIndex_1, uint32 spellId_2, uint32 effIndex_2);

int32 CompareAuraRanks(uint32 spellId_1, uint32 effIndex_1, uint32 spellId_2, uint32 effIndex_2);
bool IsSingleFromSpellSpecificPerCaster(uint32 spellSpec1, uint32 spellSpec2);
bool IsSingleFromSpellSpecificPerTarget(uint32 spellSpec1, uint32 spellSpec2);
bool IsPassiveSpell(uint32 spellId);

bool IsSingleTargetSpells(SpellInfo const *spellInfo1, SpellInfo const *spellInfo2);

bool IsAuraAddedBySpell(uint32 auraType, uint32 spellId);

bool IsSpellAllowedInLocation(SpellInfo const *spellInfo,uint32 map_id,uint32 zone_id,uint32 area_id);

inline bool IsDispel(SpellInfo const *spellInfo)
{
    //spellsteal is also dispel
    if (spellInfo->Effects[0].Effect == SPELL_EFFECT_DISPEL ||
        spellInfo->Effects[1].Effect == SPELL_EFFECT_DISPEL ||
        spellInfo->Effects[2].Effect == SPELL_EFFECT_DISPEL)
        return true;
    return false;
}
inline bool IsDispelSpell(SpellInfo const *spellInfo)
{
    //spellsteal is also dispel
    if (spellInfo->Effects[0].Effect == SPELL_EFFECT_STEAL_BENEFICIAL_BUFF ||
        spellInfo->Effects[1].Effect == SPELL_EFFECT_STEAL_BENEFICIAL_BUFF ||
        spellInfo->Effects[2].Effect == SPELL_EFFECT_STEAL_BENEFICIAL_BUFF
        ||IsDispel(spellInfo))
        return true;
    return false;
}

SpellCastResult GetErrorAtShapeshiftedCast (SpellInfo const *spellInfo, uint32 form);

inline bool IsNeedCastSpellAtOutdoor(SpellInfo const* spellInfo)
{
    return (spellInfo->Attributes & SPELL_ATTR0_OUTDOORS_ONLY && spellInfo->Attributes & SPELL_ATTR0_PASSIVE);
}

inline uint32 GetSpellMechanicMask(SpellInfo const* spellInfo, int32 effect)
{
    uint32 mask = 0;
    if (spellInfo->Mechanic)
        mask |= 1<<spellInfo->Mechanic;
    if (spellInfo->Effects[effect].Mechanic)
        mask |= 1<<spellInfo->Effects[effect].Mechanic;
    return mask;
}

inline Mechanics GetEffectMechanic(SpellInfo const* spellInfo, int32 effect)
{
    if (spellInfo->Effects[effect].Mechanic)
        return Mechanics(spellInfo->Effects[effect].Mechanic);
    if (spellInfo->Mechanic)
        return Mechanics(spellInfo->Mechanic);
    return MECHANIC_NONE;
}

inline uint32 GetDispellMask(DispelType dispel)
{
    // If dispel all
    if (dispel == DISPEL_ALL)
        return DISPEL_ALL_MASK;
    else
        return (1 << dispel);
}

inline bool isRoguePoison(SpellInfo const* spell)
{
    if (!spell)
        return false;
    
    return spell->SpellFamilyName == SPELLFAMILY_ROGUE && (spell->HasVisual(19) || spell->HasVisual(5100));
}

// Diminishing Returns interaction with spells
DiminishingGroup GetDiminishingReturnsGroupForSpell(SpellInfo const* spellproto, bool triggered);
bool IsDiminishingReturnsGroupDurationLimited(DiminishingGroup group);
DiminishingReturnsType GetDiminishingReturnsGroupType(DiminishingGroup group);

// Spell affects related declarations (accessed using SpellMgr functions)
typedef std::map<uint32, uint64> SpellAffectMap;

// Spell proc event related declarations (accessed using SpellMgr functions)
enum ProcFlags
{
   PROC_FLAG_NONE                          = 0x00000000,

   PROC_FLAG_KILLED                        = 0x00000001,    // 00 Killed by agressor
   PROC_FLAG_KILL_AND_GET_XP               = 0x00000002,    // 01 Kill that yields experience or honor

   PROC_FLAG_SUCCESSFUL_MELEE_HIT          = 0x00000004,    // 02 Successful melee attack
   PROC_FLAG_TAKEN_MELEE_HIT               = 0x00000008,    // 03 Taken damage from melee strike hit

   PROC_FLAG_SUCCESSFUL_MELEE_SPELL_HIT    = 0x00000010,    // 04 Successful attack by Spell that use melee weapon
   PROC_FLAG_TAKEN_MELEE_SPELL_HIT         = 0x00000020,    // 05 Taken damage by Spell that use melee weapon

   PROC_FLAG_SUCCESSFUL_RANGED_HIT         = 0x00000040,    // 06 Successful Ranged attack (all ranged attack deal as spell so newer set :( )
   PROC_FLAG_TAKEN_RANGED_HIT              = 0x00000080,    // 07 Taken damage from ranged attack (all ranged attack deal as spell so newer set :( )

   PROC_FLAG_SUCCESSFUL_RANGED_SPELL_HIT   = 0x00000100,    // 08 Successful Ranged attack by Spell that use ranged weapon
   PROC_FLAG_TAKEN_RANGED_SPELL_HIT        = 0x00000200,    // 09 Taken damage by Spell that use ranged weapon

   PROC_FLAG_SUCCESSFUL_POSITIVE_AOE_HIT   = 0x00000400,    // 10 Successful AoE (not 100% shure unused)
   PROC_FLAG_TAKEN_POSITIVE_AOE            = 0x00000800,    // 11 Taken AoE      (not 100% shure unused)

   PROC_FLAG_SUCCESSFUL_AOE_SPELL_HIT      = 0x00001000,    // 12 Successful AoE damage spell hit (not 100% shure unused)
   PROC_FLAG_TAKEN_AOE_SPELL_HIT           = 0x00002000,    // 13 Taken AoE damage spell hit      (not 100% shure unused)

   PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL     = 0x00004000,    // 14 Successful cast positive spell (by default only on healing)
   PROC_FLAG_TAKEN_POSITIVE_SPELL          = 0x00008000,    // 15 Taken positive spell hit (by default only on healing)

   PROC_FLAG_SUCCESSFUL_NEGATIVE_SPELL_HIT = 0x00010000,    // 16 Successful negative spell cast (by default only on damage)
   PROC_FLAG_TAKEN_NEGATIVE_SPELL_HIT      = 0x00020000,    // 17 Taken negative spell (by default only on damage)

   PROC_FLAG_ON_DO_PERIODIC                = 0x00040000,    // 18 Successful do periodic (damage / healing, determined from 14-17 flags)
   PROC_FLAG_TAKEN_PERIODIC                = 0x00080000,    // 19 Taken spell periodic (damage / healing, determined from 14-17 flags)

   PROC_FLAG_TAKEN_ANY_DAMAGE              = 0x00100000,    // 20 Taken any damage
   PROC_FLAG_ON_TRAP_ACTIVATION            = 0x00200000,    // 21 On trap activation

   PROC_FLAG_TAKEN_OFFHAND_HIT             = 0x00400000,    // 22 Taken off-hand melee attacks(not used)
   PROC_FLAG_SUCCESSFUL_OFFHAND_HIT        = 0x00800000,    // 23 Successful off-hand melee attacks
   
   PROC_FLAG_HAD_DAMAGE_BUT_ABSORBED       = 0x01000000     // 24 Attack did clean damage, but all was absorbed (for example a mage with frost armor and mana shield)
};

#define MELEE_BASED_TRIGGER_MASK (PROC_FLAG_SUCCESSFUL_MELEE_HIT        | \
                                  PROC_FLAG_TAKEN_MELEE_HIT             | \
                                  PROC_FLAG_SUCCESSFUL_MELEE_SPELL_HIT  | \
                                  PROC_FLAG_TAKEN_MELEE_SPELL_HIT       | \
                                  PROC_FLAG_SUCCESSFUL_RANGED_HIT       | \
                                  PROC_FLAG_TAKEN_RANGED_HIT            | \
                                  PROC_FLAG_SUCCESSFUL_RANGED_SPELL_HIT | \
                                  PROC_FLAG_TAKEN_RANGED_SPELL_HIT)

enum ProcFlagsEx
{
   PROC_EX_NONE                = 0x0000000,                 // If none can tigger on Hit/Crit only (passive spells MUST defined by SpellFamily flag)
   PROC_EX_NORMAL_HIT          = 0x0000001,                 // If set only from normal hit (only damage spells)
   PROC_EX_CRITICAL_HIT        = 0x0000002,
   PROC_EX_MISS                = 0x0000004,
   PROC_EX_RESIST              = 0x0000008,
   PROC_EX_DODGE               = 0x0000010,
   PROC_EX_PARRY               = 0x0000020,
   PROC_EX_BLOCK               = 0x0000040,
   PROC_EX_EVADE               = 0x0000080,
   PROC_EX_IMMUNE              = 0x0000100,
   PROC_EX_DEFLECT             = 0x0000200,
   PROC_EX_ABSORB              = 0x0000400,
   PROC_EX_REFLECT             = 0x0000800,
   PROC_EX_INTERRUPT           = 0x0001000,                 // Melle hit result can be Interrupt (not used)
   PROC_EX_RESERVED1           = 0x0002000,
   PROC_EX_RESERVED2           = 0x0004000,
   PROC_EX_RESERVED3           = 0x0008000,
   PROC_EX_EX_TRIGGER_ALWAYS   = 0x0010000,                 // If set trigger always ( no matter another flags) used for drop charges
   PROC_EX_EX_ONE_TIME_TRIGGER = 0x0020000,                 // If set trigger always but only one time
   PROC_EX_INTERNAL_HOT        = 0x1000000,                 // Only for internal use
   PROC_EX_INTERNAL_DOT        = 0x2000000,                 // Only for internal use
   PROC_EX_INTERNAL_TRIGGERED  = 0x4000000
};

struct SpellProcEventEntry
{
    uint32      schoolMask;                                 // if nonzero - bit mask for matching proc condition based on spell candidate's school: Fire=2, Mask=1<<(2-1)=2
    uint32      spellFamilyName;                            // if nonzero - for matching proc condition based on candidate spell's SpellFamilyNamer value
    uint64      spellFamilyMask;                            // if nonzero - for matching proc condition based on candidate spell's SpellFamilyFlags (like auras 107 and 108 do)
    uint32      ProcFlags;                                  // bitmask for matching proc event
    uint32      procEx;                                     // proc Extend info (see ProcFlagsEx)
    float       ppmRate;                                    // for melee (ranged?) damage spells - proc rate per minute. if zero, falls back to flat chance from Spell.dbc
    float       customChance;                               // Owerride chance (in most cases for debug only)
    uint32      cooldown;                                   // hidden cooldown used for some spell proc events, applied to _triggered_spell_
};

typedef std::unordered_map<uint32, SpellProcEventEntry> SpellProcEventMap;

struct SpellEnchantProcEntry
{
    uint32      customChance;
    float       PPMChance;
    uint32      procEx;
};

typedef std::unordered_map<uint32, SpellEnchantProcEntry> SpellEnchantProcEventMap;

struct SpellBonusEntry
{
    float  direct_damage;
    float  dot_damage;
    float  ap_bonus;
    float  ap_dot_bonus;
};

typedef std::unordered_map<uint32, SpellBonusEntry>     SpellBonusMap;

#define ELIXIR_BATTLE_MASK    0x1
#define ELIXIR_GUARDIAN_MASK  0x2
#define ELIXIR_FLASK_MASK     (ELIXIR_BATTLE_MASK|ELIXIR_GUARDIAN_MASK)
#define ELIXIR_UNSTABLE_MASK  0x4
#define ELIXIR_SHATTRATH_MASK 0x8

typedef std::map<uint32, uint8> SpellElixirMap;

// Spell script target related declarations (accessed using SpellMgr functions)
enum SpellScriptTargetType
{
    SPELL_TARGET_TYPE_GAMEOBJECT = 0,
    SPELL_TARGET_TYPE_CREATURE   = 1,
    SPELL_TARGET_TYPE_DEAD       = 2
};

#define MAX_SPELL_TARGET_TYPE 3

struct SpellTargetEntry
{
    SpellTargetEntry(SpellScriptTargetType type_,uint32 targetEntry_) : type(type_), targetEntry(targetEntry_) {}
    SpellScriptTargetType type;
    uint32 targetEntry;
};

typedef std::multimap<uint32,SpellTargetEntry> SpellScriptTarget;

// coordinates for spells (accessed using SpellMgr functions)
struct SpellTargetPosition
{
    uint32 target_mapId;
    float  target_X;
    float  target_Y;
    float  target_Z;
    float  target_Orientation;
};

typedef std::unordered_map<uint32, SpellTargetPosition> SpellTargetPositionMap;

// Enum with EffectRadiusIndex and their actual radius
enum EffectRadiusIndex
{
    EFFECT_RADIUS_2_YARDS = 7,
    EFFECT_RADIUS_5_YARDS = 8,
    EFFECT_RADIUS_20_YARDS = 9,
    EFFECT_RADIUS_30_YARDS = 10,
    EFFECT_RADIUS_45_YARDS = 11,
    EFFECT_RADIUS_100_YARDS = 12,
    EFFECT_RADIUS_10_YARDS = 13,
    EFFECT_RADIUS_8_YARDS = 14,
    EFFECT_RADIUS_3_YARDS = 15,
    EFFECT_RADIUS_1_YARD = 16,
    EFFECT_RADIUS_13_YARDS = 17,
    EFFECT_RADIUS_15_YARDS = 18,
    EFFECT_RADIUS_18_YARDS = 19,
    EFFECT_RADIUS_25_YARDS = 20,
    EFFECT_RADIUS_35_YARDS = 21,
    EFFECT_RADIUS_200_YARDS = 22,
    EFFECT_RADIUS_40_YARDS = 23,
    EFFECT_RADIUS_65_YARDS = 24,
    EFFECT_RADIUS_70_YARDS = 25,
    EFFECT_RADIUS_4_YARDS = 26,
    EFFECT_RADIUS_50_YARDS = 27,
    EFFECT_RADIUS_50000_YARDS = 28,
    EFFECT_RADIUS_6_YARDS = 29,
    EFFECT_RADIUS_500_YARDS = 30,
    EFFECT_RADIUS_80_YARDS = 31,
    EFFECT_RADIUS_12_YARDS = 32,
    EFFECT_RADIUS_99_YARDS = 33,
    EFFECT_RADIUS_55_YARDS = 35,
    EFFECT_RADIUS_0_YARDS = 36,
    EFFECT_RADIUS_7_YARDS = 37,
    EFFECT_RADIUS_21_YARDS = 38,
    EFFECT_RADIUS_34_YARDS = 39,
    EFFECT_RADIUS_9_YARDS = 40,
    EFFECT_RADIUS_150_YARDS = 41,
    EFFECT_RADIUS_11_YARDS = 42,
    EFFECT_RADIUS_16_YARDS = 43,
    EFFECT_RADIUS_0_5_YARDS = 44,   // 0.5 yards
    EFFECT_RADIUS_10_YARDS_2 = 45,
    EFFECT_RADIUS_5_YARDS_2 = 46,
    EFFECT_RADIUS_15_YARDS_2 = 47,
    EFFECT_RADIUS_60_YARDS = 48,
    EFFECT_RADIUS_90_YARDS = 49,
    EFFECT_RADIUS_15_YARDS_3 = 50,
    EFFECT_RADIUS_60_YARDS_2 = 51,
    EFFECT_RADIUS_5_YARDS_3 = 52,
    EFFECT_RADIUS_60_YARDS_3 = 53,
    EFFECT_RADIUS_50000_YARDS_2 = 54,
    EFFECT_RADIUS_130_YARDS = 55,
    EFFECT_RADIUS_38_YARDS = 56,
    EFFECT_RADIUS_45_YARDS_2 = 57,
    EFFECT_RADIUS_32_YARDS = 59,
    EFFECT_RADIUS_44_YARDS = 60,
    EFFECT_RADIUS_14_YARDS = 61,
    EFFECT_RADIUS_47_YARDS = 62,
    EFFECT_RADIUS_23_YARDS = 63,
    EFFECT_RADIUS_3_5_YARDS = 64,   // 3.5 yards
    EFFECT_RADIUS_80_YARDS_2 = 65
};

// Spell pet auras
class PetAura
{
    public:
        PetAura()
        {
            auras.clear();
        }

        PetAura(uint16 petEntry, uint16 aura, bool _removeOnChangePet, int _damage) :
        removeOnChangePet(_removeOnChangePet), damage(_damage)
        {
            auras[petEntry] = aura;
        }

        uint16 GetAura(uint16 petEntry) const
        {
            std::map<uint16, uint16>::const_iterator itr = auras.find(petEntry);
            if(itr != auras.end())
                return itr->second;
            else
            {
                std::map<uint16, uint16>::const_iterator itr2 = auras.find(0);
                if(itr2 != auras.end())
                    return itr2->second;
                else
                    return 0;
            }
        }

        void AddAura(uint16 petEntry, uint16 aura)
        {
            auras[petEntry] = aura;
        }

        bool IsRemovedOnChangePet() const
        {
            return removeOnChangePet;
        }

        int32 GetDamage() const
        {
            return damage;
        }

    private:
        std::map<uint16, uint16> auras;
        bool removeOnChangePet;
        int32 damage;
};
typedef std::map<uint16, PetAura> SpellPetAuraMap;


struct SpellArea
{
    uint32 spellId;
    uint32 areaId;                                          // zone/subzone/or 0 is not limited to zone
    uint32 questStart;                                      // quest start (quest must be active or rewarded for spell apply)
    uint32 questEnd;                                        // quest end (quest must not be rewarded for spell apply)
    int32  auraSpell;                                       // spell aura must be applied for spell apply)if possitive) and it must not be applied in other case
    uint32 raceMask;                                        // can be applied only to races
    Gender gender;                                          // can be applied only to gender
    uint32 questStartStatus;                                // QuestStatus that quest_start must have in order to keep the spell
    uint32 questEndStatus;                                  // QuestStatus that the quest_end must have in order to keep the spell (if the quest_end's status is different than this, the spell will be dropped)
    bool autocast;                                          // if true then auto applied at area enter, in other case just allowed to cast

                                                            // helpers
    bool IsFitToRequirements(Player const* player, uint32 newZone, uint32 newArea) const;
};

typedef std::multimap<uint32, SpellArea> SpellAreaMap;
typedef std::multimap<uint32, SpellArea const*> SpellAreaForQuestMap;
typedef std::multimap<uint32, SpellArea const*> SpellAreaForAuraMap;
typedef std::multimap<uint32, SpellArea const*> SpellAreaForAreaMap;
typedef std::pair<SpellAreaMap::const_iterator, SpellAreaMap::const_iterator> SpellAreaMapBounds;
typedef std::pair<SpellAreaForQuestMap::const_iterator, SpellAreaForQuestMap::const_iterator> SpellAreaForQuestMapBounds;
typedef std::pair<SpellAreaForAuraMap::const_iterator, SpellAreaForAuraMap::const_iterator>  SpellAreaForAuraMapBounds;
typedef std::pair<SpellAreaForAreaMap::const_iterator, SpellAreaForAreaMap::const_iterator>  SpellAreaForAreaMapBounds;

// Spell rank chain  (accessed using SpellMgr functions)
struct SpellChainNode
{
    SpellInfo const* prev;
    SpellInfo const* next;
    SpellInfo const* first;
    SpellInfo const* last;
    uint8  rank;
};

typedef std::unordered_map<uint32, SpellChainNode> SpellChainMap;

//                 spell_id  req_spell
typedef std::unordered_map<uint32, uint32> SpellRequiredMap;

typedef std::multimap<uint32, uint32> SpellsRequiringSpellMap;

// Spell learning properties (accessed using SpellMgr functions)
struct SpellLearnSkillNode
{
    uint32 skill;
    uint16 step;                                            // 0 - 4
    uint32 value;                                           // 0  - max skill value for player level
    uint32 maxvalue;                                        // 0  - max skill value for player level
};

typedef std::map<uint32, SpellLearnSkillNode> SpellLearnSkillMap;

struct SpellLearnSpellNode
{
    uint32 spell;
    bool autoLearned;
};

typedef std::multimap<uint32, SpellLearnSpellNode> SpellLearnSpellMap;
typedef std::pair<SpellLearnSpellMap::const_iterator, SpellLearnSpellMap::const_iterator> SpellLearnSpellMapBounds;

typedef std::multimap<uint32, SkillLineAbilityEntry const*> SkillLineAbilityMap;
typedef std::pair<SkillLineAbilityMap::const_iterator, SkillLineAbilityMap::const_iterator> SkillLineAbilityMapBounds;

typedef std::vector<SpellInfo*> SpellInfoMap;

typedef std::map<int32, std::vector<int32> > SpellLinkedMap;

class SpellMgr
{
    // Constructors
    public:
        SpellMgr();
        ~SpellMgr();

        static SpellMgr* instance()
        {
            static SpellMgr instance;
            return &instance;
        }

        // Accessors (const or static functions)
    public:
        // Spell affects
        uint64 GetSpellAffectMask(uint16 spellId, uint8 effectId) const
        {
            SpellAffectMap::const_iterator itr = mSpellAffectMap.find((spellId<<8) + effectId);
            if( itr != mSpellAffectMap.end( ) )
                return itr->second;
            return 0;
        }

        bool IsAffectedBySpell(SpellInfo const *spellInfo, uint32 spellId, uint8 effectId, uint64 familyFlags) const;

        SpellElixirMap const& GetSpellElixirMap() const { return mSpellElixirs; }

        uint32 GetSpellElixirMask(uint32 spellid) const
        {
            SpellElixirMap::const_iterator itr = mSpellElixirs.find(spellid);
            if(itr==mSpellElixirs.end())
                return 0x0;

            return itr->second;
        }

        // Spell proc events
        SpellProcEventEntry const* GetSpellProcEvent(uint32 spellId) const
        {
            SpellProcEventMap::const_iterator itr = mSpellProcEventMap.find(spellId);
            if( itr != mSpellProcEventMap.end( ) )
                return &itr->second;
            return NULL;
        }

        static bool IsSpellProcEventCanTriggeredBy( SpellProcEventEntry const * spellProcEvent, uint32 EventProcFlag, SpellInfo const * procSpell, uint32 ProcFlags, uint32 procExtra, bool active);

        SpellEnchantProcEntry const* GetSpellEnchantProcEvent(uint32 enchId) const
        {
            SpellEnchantProcEventMap::const_iterator itr = mSpellEnchantProcEventMap.find(enchId);
            if( itr != mSpellEnchantProcEventMap.end( ) )
                return &itr->second;
            return NULL;
        }

        // Spell target coordinates. effIndex NYI
        SpellTargetPosition const* GetSpellTargetPosition(uint32 spell_id, SpellEffIndex /* effIndex */) const
        {
            SpellTargetPositionMap::const_iterator itr = mSpellTargetPositions.find( spell_id );
            if( itr != mSpellTargetPositions.end( ) )
                return &itr->second;
            return NULL;
        }

        // Spell ranks chains
        SpellChainNode const* GetSpellChainNode(uint32 spell_id) const
        {
            SpellChainMap::const_iterator itr = mSpellChains.find(spell_id);
            if(itr == mSpellChains.end())
                return NULL;

            return &itr->second;
        }

        uint32 GetSpellRequired(uint32 spell_id) const
        {
            SpellRequiredMap::const_iterator itr = mSpellReq.find(spell_id);
            if(itr == mSpellReq.end())
                return 0;

            return itr->second;
        }

        uint32 GetFirstSpellInChain(uint32 spell_id) const;
        uint32 GetLastSpellInChain(uint32 spell_id) const;
        uint32 GetNextSpellInChain(uint32 spell_id) const;
        uint32 GetPrevSpellInChain(uint32 spell_id) const;

        SpellsRequiringSpellMap const& GetSpellsRequiringSpell() const { return mSpellsReqSpell; }

        // Note: not use rank for compare to spell ranks: spell chains isn't linear order
        // Use IsHighRankOfSpell instead
        uint8 GetSpellRank(uint32 spell_id) const;

        uint8 IsHighRankOfSpell(uint32 spell1, uint32 spell2) const;

        bool IsRankSpellDueToSpell(SpellInfo const *spellInfo_1,uint32 spellId_2) const;
        static bool canStackSpellRanks(SpellInfo const *spellInfo);
        bool IsNoStackSpellDueToSpell(uint32 spellId_1, uint32 spellId_2, bool sameCaster) const;

        SpellInfo const* SelectAuraRankForPlayerLevel(SpellInfo const* spellInfo, uint32 playerLevel, bool hostileTarget) const;

        bool IsNearbyEntryEffect(SpellInfo const* spellInfo, uint8 effect) const;

        // Spell learning
        SpellLearnSkillNode const* GetSpellLearnSkill(uint32 spell_id) const;
        SpellLearnSpellMapBounds GetSpellLearnSpellMapBounds(uint32 spell_id) const;
        bool IsSpellLearnSpell(uint32 spell_id) const;
        bool IsSpellLearnToSpell(uint32 spell_id1, uint32 spell_id2) const;

        static bool IsProfessionSpell(uint32 spellId);
        static bool IsPrimaryProfessionSpell(uint32 spellId);
        bool IsPrimaryProfessionFirstRankSpell(uint32 spellId) const;

        // Spell script targets
        SpellScriptTarget::const_iterator GetBeginSpellScriptTarget(uint32 spell_id) const
        {
            return mSpellScriptTarget.lower_bound(spell_id);
        }

        SpellScriptTarget::const_iterator GetEndSpellScriptTarget(uint32 spell_id) const
        {
            return mSpellScriptTarget.upper_bound(spell_id);
        }

        // Spell correctess for client using
        static bool IsSpellValid(SpellInfo const * spellInfo, Player* pl = NULL, bool msg = true);

        SkillLineAbilityMapBounds GetSkillLineAbilityMapBounds(uint32 spell_id) const;

        // Spell bonus data table
        SpellBonusEntry const* GetSpellBonusData(uint32 spellId) const;

        // Spell threat table
        SpellThreatEntry const* GetSpellThreatEntry(uint32 spellID) const;

        typedef std::map<uint32, SpellThreatEntry> SpellThreatMap;

        PetAura const* GetPetAura(uint16 spell_id)
        {
            SpellPetAuraMap::const_iterator itr = mSpellPetAuraMap.find(spell_id);
            if(itr != mSpellPetAuraMap.end())
                return &itr->second;
            else
                return NULL;
        }
        
        const std::vector<int32> *GetSpellLinked(int32 spell_id) const
        {
            SpellLinkedMap::const_iterator itr = mSpellLinkedMap.find(spell_id);
            return itr != mSpellLinkedMap.end() ? &(itr->second) : NULL;
        }

        // Spell area
        SpellAreaMapBounds GetSpellAreaMapBounds(uint32 spell_id) const;
        SpellAreaForQuestMapBounds GetSpellAreaForQuestMapBounds(uint32 quest_id) const;
        SpellAreaForQuestMapBounds GetSpellAreaForQuestEndMapBounds(uint32 quest_id) const;
        SpellAreaForAuraMapBounds GetSpellAreaForAuraMapBounds(uint32 spell_id) const;
        SpellAreaForAreaMapBounds GetSpellAreaForAreaMapBounds(uint32 area_id) const;

        float GetSpellThreatModPercent(SpellInfo const* spellInfo) const;
        int GetSpellThreatModFlat(SpellInfo const* spellInfo) const;

        static bool IsBinaryMagicResistanceSpell(SpellInfo const* spell);


        static bool IsPrimaryProfessionSkill(uint32 skill);
        static bool IsProfessionSkill(uint32 skill);
        static bool IsProfessionOrRidingSkill(uint32 skill);

        // Modifiers
    public:
        static SpellMgr& Instance();

        // Loading data at server startup
        void LoadSpellChains();
        void UnloadSpellInfoChains();
        void LoadSpellRequired();
        void LoadSpellLearnSkills();
        void LoadSpellLearnSpells();
        void LoadSpellAffects();
        void LoadSpellElixirs();
        void LoadSpellProcEvents();
        void LoadSpellTargetPositions();
        void LoadSpellThreats();
        void LoadSpellBonusess();
        void LoadSkillLineAbilityMap();
        void LoadSpellPetAuras();
        /** Change SpellEntry entries with custom values. This MUST be done before loading spellInfo's, else it will have no effect. 
        No SpellCustomAttributes are being set in here. All this should be moved to db.*/
        void LoadSpellCustomAttr();
        void OverrideSpellItemEnchantment();
        /* This also changes SpellInfo's */
        void LoadSpellLinked();
        void LoadSpellEnchantProcData();

        //in reload case, does not delete spell and try to update already existing ones only. This allows to keep pointers valids. /!\ Note that pointers in SpellInfo object themselves may change.
        void LoadSpellInfoStore(bool reload = false);
        void UnloadSpellInfoStore();
        void LoadSpellAreas();

        // SpellInfo object management
        SpellInfo const* GetSpellInfo(uint32 spellId) const { return spellId < GetSpellInfoStoreSize() ? mSpellInfoMap[spellId] : NULL; }
        // Use this only with 100% valid spellIds
        SpellInfo const* EnsureSpellInfo(uint32 spellId) const
        {
            ASSERT(spellId < GetSpellInfoStoreSize());
            SpellInfo const* spellInfo = mSpellInfoMap[spellId];
            ASSERT(spellInfo);
            return spellInfo;
        }
        uint32 GetSpellInfoStoreSize() const { return mSpellInfoMap.size(); }

    private:
        SpellInfo* _GetSpellInfo(uint32 spellId) { return spellId < GetSpellInfoStoreSize() ? mSpellInfoMap[spellId] : NULL; }
        
        SpellBonusMap                mSpellBonusMap;
        SpellThreatMap               mSpellThreatMap;
        SpellScriptTarget            mSpellScriptTarget;
        SpellChainMap                mSpellChains;
        SpellsRequiringSpellMap      mSpellsReqSpell;
        SpellRequiredMap             mSpellReq;
        SpellLearnSkillMap           mSpellLearnSkills;
        SpellLearnSpellMap           mSpellLearnSpells;
        SpellTargetPositionMap       mSpellTargetPositions;
        SpellAffectMap               mSpellAffectMap;
        SpellElixirMap               mSpellElixirs;
        SpellProcEventMap            mSpellProcEventMap;
        SkillLineAbilityMap          mSkillLineAbilityMap;
        SpellPetAuraMap              mSpellPetAuraMap;
        SpellLinkedMap               mSpellLinkedMap;
        SpellEnchantProcEventMap     mSpellEnchantProcEventMap;
        SpellAreaMap               mSpellAreaMap;
        SpellAreaForQuestMap       mSpellAreaForQuestMap;
        SpellAreaForQuestMap       mSpellAreaForQuestEndMap;
        SpellAreaForAuraMap        mSpellAreaForAuraMap;
        SpellAreaForAreaMap        mSpellAreaForAreaMap;

        SpellInfoMap               mSpellInfoMap;
};

#define sSpellMgr SpellMgr::instance()
#endif

