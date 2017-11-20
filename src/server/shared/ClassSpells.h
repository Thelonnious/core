#ifndef H_CLASS_SPELLS
#define H_CLASS_SPELLS

namespace Talents
{
    enum Druid
    {
        //Balance
        //Feral Combat
        //Restoration
    };

    enum Hunter
    {
        //Beast Mastery
        //Marksmanship
        //Survival
    };

    enum Mage
    {
        //Arcane
        //Fire
        //Frost
    };

    enum Paladin
    {
        //Holy
        //Protection
        //Retribution
    };

    enum Priest
    {
        //Discipline
        WAND_SPEC_RNK_1 = 14524,
        WAND_SPEC_RNK_2 = 14525,
        WAND_SPEC_RNK_3 = 14526,
        WAND_SPEC_RNK_4 = 14527,
        WAND_SPEC_RNK_5 = 14528,

        //Holy

        //Shadow
    };

    enum Rogue
    {
        //Assassination
        //Combat
        //Subtlety
    };

    enum Shaman
    {
        //Elemental
        //Enhancement
        //Restoration
    };

    enum Warlock
    {
        //Affliction
        //Demonology
        //Destruction
    };

    enum Warrior
    {
        //Arms
        //Fury
        //Protection
    };
};

namespace ClassSpells
{
    enum Druid
    {
        //Balance
        //Feral Combat
        //Restoration
    };

    enum Hunter
    {
        //Beast Mastery
        //Marksmanship
        //Survival
    };

    enum Mage
    {
        //Arcane
        //Fire
        //Frost
    };

    enum Paladin
    {
        //Holy
        //Protection
        //Retribution
    };

    enum Priest
    {
        //Generic
        WAND = 5019,

        //Discipline
        //Holy
        //Shadow
    };

    enum Rogue
    {
        //Assassination
        //Combat
        //Subtlety
    };

    enum Shaman
    {
		// Generic
		MAIL = 8737,

		// Elemental
		CHAIN_LIGHTNING_RNK_1 = 421,
		CHAIN_LIGHTNING_RNK_2 = 930,
		CHAIN_LIGHTNING_RNK_3 = 2860,
		CHAIN_LIGHTNING_RNK_4 = 10605,
		CHAIN_LIGHTNING_RNK_5 = 25439,
		CHAIN_LIGHTNING_RNK_6 = 25442,
		EARTH_SHOCK_RNK_1 = 8042,
		EARTH_SHOCK_RNK_2 = 8044,
		EARTH_SHOCK_RNK_3 = 8045,
		EARTH_SHOCK_RNK_4 = 8046,
		EARTH_SHOCK_RNK_5 = 10412,
		EARTH_SHOCK_RNK_6 = 10413,
		EARTH_SHOCK_RNK_7 = 10414,
		EARTH_SHOCK_RNK_8 = 25454,
		EARTHBIND_TOTEM_RNK_1 = 2484,
		FIRE_ELEMENTAL_TOTEM_RNK_1 = 2894,
		FIRE_NOVA_TOTEM_RNK_1 = 1535,
		FIRE_NOVA_TOTEM_RNK_2 = 8498,
		FIRE_NOVA_TOTEM_RNK_3 = 8499,
		FIRE_NOVA_TOTEM_RNK_4 = 11314,
		FIRE_NOVA_TOTEM_RNK_5 = 11315,
		FIRE_NOVA_TOTEM_RNK_6 = 25546,
		FIRE_NOVA_TOTEM_RNK_7 = 25547,
		FLAME_SHOCK_RNK_1 = 8050,
		FLAME_SHOCK_RNK_2 = 8052,
		FLAME_SHOCK_RNK_3 = 8053,
		FLAME_SHOCK_RNK_4 = 10447,
		FLAME_SHOCK_RNK_5 = 10448,
		FLAME_SHOCK_RNK_6 = 29228,
		FLAME_SHOCK_RNK_7 = 25457,
		FROST_SHOCK_RNK_1 = 8056,
		FROST_SHOCK_RNK_2 = 8058,
		FROST_SHOCK_RNK_3 = 10472,
		FROST_SHOCK_RNK_4 = 10473,
		FROST_SHOCK_RNK_5 = 25464,
		LIGHTNING_BOLT_RNK_1 = 403,
		LIGHTNING_BOLT_RNK_2 = 529,
		LIGHTNING_BOLT_RNK_3 = 548,
		LIGHTNING_BOLT_RNK_4 = 915,
		LIGHTNING_BOLT_RNK_5 = 943,
		LIGHTNING_BOLT_RNK_6 = 6041,
		LIGHTNING_BOLT_RNK_7 = 10391,
		LIGHTNING_BOLT_RNK_8 = 10392,
		LIGHTNING_BOLT_RNK_9 = 15207,
		LIGHTNING_BOLT_RNK_10 = 15208,
		LIGHTNING_BOLT_RNK_11 = 25448,
		LIGHTNING_BOLT_RNK_12 = 25449,
		MAGMA_TOTEM_RNK_1 = 8190,
		MAGMA_TOTEM_RNK_2 = 10585,
		MAGMA_TOTEM_RNK_3 = 10586,
		MAGMA_TOTEM_RNK_4 = 10587,
		MAGMA_TOTEM_RNK_5 = 25552,
		PURGE_RNK_1 = 370,
		PURGE_RNK_2 = 8012,
		SEARING_TOTEM_RNK_1 = 3599,
		SEARING_TOTEM_RNK_2 = 6363,
		SEARING_TOTEM_RNK_3 = 6364,
		SEARING_TOTEM_RNK_4 = 6365,
		SEARING_TOTEM_RNK_5 = 10437,
		SEARING_TOTEM_RNK_6 = 10438,
		SEARING_TOTEM_RNK_7 = 25533,
		STONECLAW_TOTEM_RNK_1 = 5730,
		STONECLAW_TOTEM_RNK_2 = 6390,
		STONECLAW_TOTEM_RNK_3 = 6391,
		STONECLAW_TOTEM_RNK_4 = 6392,
		STONECLAW_TOTEM_RNK_5 = 10427,
		STONECLAW_TOTEM_RNK_6 = 10428,
		STONECLAW_TOTEM_RNK_7 = 25525,

		// Enhancement
		ASTRAL_RECALL_RNK_1 = 556,
		BLOODLUST_RNK_1 = 2825,
		EARTH_ELEMENTAL_TOTEM_RNK_1 = 2062,
		FAR_SIGHT_RNK_1 = 6196,
		FIRE_RESISTANCE_TOTEM_RNK_1 = 8184,
		FIRE_RESISTANCE_TOTEM_RNK_2 = 10537,
		FIRE_RESISTANCE_TOTEM_RNK_3 = 10538,
		FIRE_RESISTANCE_TOTEM_RNK_4 = 25563,
		FLAMETONGUE_TOTEM_RNK_1 = 8227,
		FLAMETONGUE_TOTEM_RNK_2 = 8249,
		FLAMETONGUE_TOTEM_RNK_3 = 10526,
		FLAMETONGUE_TOTEM_RNK_4 = 16387,
		FLAMETONGUE_TOTEM_RNK_5 = 25557,
		FLAMETONGUE_WEAPON_RNK_1 = 8024,
		FLAMETONGUE_WEAPON_RNK_2 = 8027,
		FLAMETONGUE_WEAPON_RNK_3 = 8030,
		FLAMETONGUE_WEAPON_RNK_4 = 16339,
		FLAMETONGUE_WEAPON_RNK_5 = 16341,
		FLAMETONGUE_WEAPON_RNK_6 = 16342,
		FLAMETONGUE_WEAPON_RNK_7 = 25489,
		FROST_RESISTANCE_TOTEM_RNK_1 = 8181,
		FROST_RESISTANCE_TOTEM_RNK_2 = 10478,
		FROST_RESISTANCE_TOTEM_RNK_3 = 10479,
		FROST_RESISTANCE_TOTEM_RNK_4 = 25560,
		FROSTBRAND_WEAPON_RNK_1 = 8033,
		FROSTBRAND_WEAPON_RNK_2 = 8038,
		FROSTBRAND_WEAPON_RNK_3 = 10456,
		FROSTBRAND_WEAPON_RNK_4 = 16355,
		FROSTBRAND_WEAPON_RNK_5 = 16356,
		FROSTBRAND_WEAPON_RNK_6 = 25500,
		GHOST_WOLF_RNK_1 = 2645,
		GRACE_OF_AIR_TOTEM_RNK_1 = 8835,
		GRACE_OF_AIR_TOTEM_RNK_2 = 10627,
		GRACE_OF_AIR_TOTEM_RNK_3 = 25359,
		GROUNDING_TOTEM_RNK_1 = 8177,
		LIGHTNING_SHIELD_RNK_1 = 324,
		LIGHTNING_SHIELD_RNK_2 = 325,
		LIGHTNING_SHIELD_RNK_3 = 905,
		LIGHTNING_SHIELD_RNK_4 = 945,
		LIGHTNING_SHIELD_RNK_5 = 8134,
		LIGHTNING_SHIELD_RNK_6 = 10431,
		LIGHTNING_SHIELD_RNK_7 = 10432,
		LIGHTNING_SHIELD_RNK_8 = 25469,
		LIGHTNING_SHIELD_RNK_9 = 25472,
		NATURE_RESISTANCE_TOTEM_RNK_1 = 10595,
		NATURE_RESISTANCE_TOTEM_RNK_2 = 10600,
		NATURE_RESISTANCE_TOTEM_RNK_3 = 10601,
		NATURE_RESISTANCE_TOTEM_RNK_4 = 25574,
		ROCKBITER_WEAPON_RNK_1 = 8017,
		ROCKBITER_WEAPON_RNK_2 = 8018,
		ROCKBITER_WEAPON_RNK_3 = 8019,
		ROCKBITER_WEAPON_RNK_4 = 10399,
		ROCKBITER_WEAPON_RNK_5 = 16314,
		ROCKBITER_WEAPON_RNK_6 = 16315,
		ROCKBITER_WEAPON_RNK_7 = 16316,
		ROCKBITER_WEAPON_RNK_8 = 25479,
		ROCKBITER_WEAPON_RNK_9 = 25485,
		SENTRY_TOTEM_RNK_1 = 6495,
		STONESKIN_TOTEM_RNK_1 = 8071,
		STONESKIN_TOTEM_RNK_2 = 8154,
		STONESKIN_TOTEM_RNK_3 = 8155,
		STONESKIN_TOTEM_RNK_4 = 10406,
		STONESKIN_TOTEM_RNK_5 = 10407,
		STONESKIN_TOTEM_RNK_6 = 10408,
		STONESKIN_TOTEM_RNK_7 = 25508,
		STRENGTH_OF_EARTH_TOTEM_RNK_1 = 8075,
		STRENGTH_OF_EARTH_TOTEM_RNK_2 = 8160,
		STRENGTH_OF_EARTH_TOTEM_RNK_3 = 8161,
		STRENGTH_OF_EARTH_TOTEM_RNK_4 = 10442,
		STRENGTH_OF_EARTH_TOTEM_RNK_5 = 25361,
		STRENGTH_OF_EARTH_TOTEM_RNK_6 = 25528,
		WATER_BREATHING_RNK_1 = 131,
		WATER_WALKING_RNK_1 = 546,
		WINDFURY_TOTEM_RNK_1 = 8512,
		WINDFURY_TOTEM_RNK_2 = 10613,
		WINDFURY_TOTEM_RNK_3 = 10614,
		WINDFURY_TOTEM_RNK_4 = 25585,
		WINDFURY_TOTEM_RNK_5 = 25587,
		WINDFURY_WEAPON_RNK_1 = 8232,
		WINDFURY_WEAPON_RNK_2 = 8235,
		WINDFURY_WEAPON_RNK_3 = 10486,
		WINDFURY_WEAPON_RNK_4 = 16362,
		WINDFURY_WEAPON_RNK_5 = 25505,
		WINDWALL_TOTEM_RNK_1 = 15107,
		WINDWALL_TOTEM_RNK_2 = 15111,
		WINDWALL_TOTEM_RNK_3 = 15112,
		WINDWALL_TOTEM_RNK_4 = 25577,
		WRATH_OF_AIR_TOTEM_RNK_1 = 3738,

		// Restoration
		ANCESTRAL_SPIRIT_RNK_1 = 2008,
		ANCESTRAL_SPIRIT_RNK_2 = 20609,
		ANCESTRAL_SPIRIT_RNK_3 = 20610,
		ANCESTRAL_SPIRIT_RNK_4 = 20776,
		ANCESTRAL_SPIRIT_RNK_5 = 20777,
		CHAIN_HEAL_RNK_1 = 974,
		CHAIN_HEAL_RNK_2 = 10622,
		CHAIN_HEAL_RNK_3 = 10623,
		CHAIN_HEAL_RNK_4 = 25422,
		CHAIN_HEAL_RNK_5 = 25423,
		CURE_DISEASE_RNK_1 = 2870,
		CURE_POISON_RNK_1 = 526,
		DISEASE_CLEANSING_TOTEM_RNK_1 = 8170,
		EARTH_SHIELD_RNK_1 = 32593,
		EARTH_SHIELD_RNK_2 = 32593,
		EARTH_SHIELD_RNK_3 = 32594,
		HEALING_STREAM_TOTEM_RNK_1 = 5394,
		HEALING_STREAM_TOTEM_RNK_2 = 6375,
		HEALING_STREAM_TOTEM_RNK_3 = 6377,
		HEALING_STREAM_TOTEM_RNK_4 = 10462,
		HEALING_STREAM_TOTEM_RNK_5 = 10463,
		HEALING_STREAM_TOTEM_RNK_6 = 25567,
		HEALING_WAVE_RNK_1 = 331,
		HEALING_WAVE_RNK_2 = 332,
		HEALING_WAVE_RNK_3 = 547,
		HEALING_WAVE_RNK_4 = 913,
		HEALING_WAVE_RNK_5 = 939,
		HEALING_WAVE_RNK_6 = 959,
		HEALING_WAVE_RNK_7 = 8005,
		HEALING_WAVE_RNK_8 = 10395,
		HEALING_WAVE_RNK_9 = 10396,
		HEALING_WAVE_RNK_10 = 25357,
		HEALING_WAVE_RNK_11 = 25391,
		HEALING_WAVE_RNK_12 = 25396,
		LESSER_HEALING_WAVE_RNK_1 = 8004,
		LESSER_HEALING_WAVE_RNK_2 = 8008,
		LESSER_HEALING_WAVE_RNK_3 = 8010,
		LESSER_HEALING_WAVE_RNK_4 = 10466,
		LESSER_HEALING_WAVE_RNK_5 = 10467,
		LESSER_HEALING_WAVE_RNK_6 = 10468,
		LESSER_HEALING_WAVE_RNK_7 = 25420,
		MANA_SPRING_TOTEM_RNK_1 = 5675,
		MANA_SPRING_TOTEM_RNK_2 = 10495,
		MANA_SPRING_TOTEM_RNK_3 = 10496,
		MANA_SPRING_TOTEM_RNK_4 = 10497,
		MANA_SPRING_TOTEM_RNK_5 = 25570,
		POISON_CLEANSING_TOTEM_RNK_1 = 8166,
		REINCARNATION_RNK_1 = 20608,
		STONESKIN_TOTEM_RNK_8 = 25509,
		TOTEMIC_CALL_RNK_1 = 36936,
		TRANQUIL_AIR_TOTEM_RNK_1 = 25908,
		TREMOR_TOTEM_RNK_1 = 8143,
		WATER_SHIELD_RNK_1 = 24398,
		WATER_SHIELD_RNK_2 = 33736,
    };

    enum Warlock
    {
        //Affliction
        //Demonology
        //Destruction
    };

    enum Warrior
    {
		// Generic
		DUAL_WIELD = 674,
		PARRY = 3127,
		PLATE_MAIL = 750,

		// Arms
		CHARGE_RNK_1 = 100,
		CHARGE_RNK_2 = 6178,
		CHARGE_RNK_3 = 11578,
		HAMSTRING_RNK_1 = 1715,
		HAMSTRING_RNK_2 = 7372,
		HAMSTRING_RNK_3 = 7373,
		HAMSTRING_RNK_4 = 25212,
		HEROIC_STRIKE_RNK_1 = 78,
		HEROIC_STRIKE_RNK_2 = 284,
		HEROIC_STRIKE_RNK_3 = 285,
		HEROIC_STRIKE_RNK_4 = 1608,
		HEROIC_STRIKE_RNK_5 = 11564,
		HEROIC_STRIKE_RNK_6 = 11565,
		HEROIC_STRIKE_RNK_7 = 11566,
		HEROIC_STRIKE_RNK_8 = 11567,
		HEROIC_STRIKE_RNK_9 = 25286,
		HEROIC_STRIKE_RNK_10 = 29707,
		MOCKING_BLOW_RNK_1 = 694,
		MOCKING_BLOW_RNK_2 = 7400,
		MOCKING_BLOW_RNK_3 = 7402,
		MOCKING_BLOW_RNK_4 = 20559,
		MOCKING_BLOW_RNK_5 = 20560,
		MOCKING_BLOW_RNK_6 = 25266,
		MORTAL_STRIKE_RNK_1 = 12294,
		MORTAL_STRIKE_RNK_2 = 21551,
		MORTAL_STRIKE_RNK_3 = 21552,
		MORTAL_STRIKE_RNK_4 = 21553,
		MORTAL_STRIKE_RNK_5 = 25248,
		MORTAL_STRIKE_RNK_6 = 30330,
		OVERPOWER_RNK_1 = 7384,
		OVERPOWER_RNK_2 = 7887,
		OVERPOWER_RNK_3 = 11584,
		OVERPOWER_RNK_4 = 11585,
		REND_RNK_1 = 772,
		REND_RNK_2 = 6546,
		REND_RNK_3 = 6547,
		REND_RNK_4 = 6548,
		REND_RNK_5 = 11572,
		REND_RNK_6 = 11573,
		REND_RNK_7 = 11574,
		REND_RNK_8 = 25208,
		RETALIATION_RNK_1 = 20230,
		SLAM_RNK_1 = 1464,
		SLAM_RNK_2 = 8820,
		SLAM_RNK_3 = 11604,
		SLAM_RNK_4 = 11605,
		SLAM_RNK_5 = 25241,
		SLAM_RNK_6 = 25242,
		THUNDER_CLAP_RNK_1 = 6343,
		THUNDER_CLAP_RNK_2 = 8198,
		THUNDER_CLAP_RNK_3 = 8204,
		THUNDER_CLAP_RNK_4 = 8205,
		THUNDER_CLAP_RNK_5 = 11580,
		THUNDER_CLAP_RNK_6 = 11581,
		THUNDER_CLAP_RNK_7 = 25264,

		// Fury
		BATTLE_SHOUT_RNK_1 = 6673,
		BATTLE_SHOUT_RNK_2 = 5242,
		BATTLE_SHOUT_RNK_3 = 6192,
		BATTLE_SHOUT_RNK_4 = 11549,
		BATTLE_SHOUT_RNK_5 = 11550,
		BATTLE_SHOUT_RNK_6 = 11551,
		BATTLE_SHOUT_RNK_7 = 25289,
		BATTLE_SHOUT_RNK_8 = 2048,
		BERSERKER_RAGE_RNK_1 = 18499,
		BERSERKER_STANCE_RNK_1 = 2458,
		BLOODTHIRST_RNK_1 = 23881,
		BLOODTHIRST_RNK_2 = 23892,
		BLOODTHIRST_RNK_3 = 23893,
		BLOODTHIRST_RNK_4 = 23894,
		BLOODTHIRST_RNK_5 = 25251,
		BLOODTHIRST_RNK_6 = 30335,
		CHALLENGING_SHOUT_RNK_1 = 1161,
		CLEAVE_RNK_1 = 845,
		CLEAVE_RNK_2 = 7369,
		CLEAVE_RNK_3 = 11608,
		CLEAVE_RNK_4 = 11609,
		CLEAVE_RNK_5 = 20569,
		CLEAVE_RNK_6 = 25231,
		COMMANDING_SHOUT_RNK_1 = 469,
		DEMORALIZING_SHOUT_RNK_1 = 1160,
		DEMORALIZING_SHOUT_RNK_2 = 6190,
		DEMORALIZING_SHOUT_RNK_3 = 11554,
		DEMORALIZING_SHOUT_RNK_4 = 11555,
		DEMORALIZING_SHOUT_RNK_5 = 11556,
		DEMORALIZING_SHOUT_RNK_6 = 25202,
		DEMORALIZING_SHOUT_RNK_7 = 25203,
		EXECUTE_RNK_1 = 5308,
		EXECUTE_RNK_2 = 20658,
		EXECUTE_RNK_3 = 20660,
		EXECUTE_RNK_4 = 20661,
		EXECUTE_RNK_5 = 20662,
		EXECUTE_RNK_6 = 25234,
		EXECUTE_RNK_7 = 25236,
		INTERCEPT_RNK_1 = 20252,
		INTERCEPT_RNK_2 = 20616,
		INTERCEPT_RNK_3 = 20617,
		INTERCEPT_RNK_4 = 25272,
		INTERCEPT_RNK_5 = 25275,
		INTIMIDATING_SHOUT_RNK_1 = 5246,
		PUMMEL_RNK_1 = 6552,
		PUMMEL_RNK_2 = 6554,
		RAMPAGE_RNK_1 = 29801,
		RAMPAGE_RNK_2 = 30030,
		RAMPAGE_RNK_3 = 30033,
		RECKLESSNESS_RNK_1 = 1719,
		VICTORY_RUSH_RNK_1 = 34428,
		WHIRLWIND_RNK_1 = 1680,

		// Protection
		BLOODRAGE_RNK_1 = 2687,
		DEFENSIVE_STANCE_RNK_1 = 71,
		DEVASTATE_RNK_1 = 20243,
		DEVASTATE_RNK_2 = 30016,
		DEVASTATE_RNK_3 = 30022,
		DISARM_RNK_1 = 676,
		INTERVENE_RNK_1 = 3411,
		REVENGE_RNK_1 = 6572,
		REVENGE_RNK_2 = 6574,
		REVENGE_RNK_3 = 7379,
		REVENGE_RNK_4 = 11600,
		REVENGE_RNK_5 = 11601,
		REVENGE_RNK_6 = 25288,
		REVENGE_RNK_7 = 25269,
		REVENGE_RNK_8 = 30357,
		SHIELD_BASH_RNK_1 = 72,
		SHIELD_BASH_RNK_2 = 1671,
		SHIELD_BASH_RNK_3 = 1672,
		SHIELD_BASH_RNK_4 = 29704,
		SHIELD_BLOCK_RNK_1 = 2565,
		SHIELD_SLAM_RNK_1 = 23922,
		SHIELD_SLAM_RNK_2 = 23923,
		SHIELD_SLAM_RNK_3 = 23924,
		SHIELD_SLAM_RNK_4 = 23925,
		SHIELD_SLAM_RNK_5 = 25258,
		SHIELD_SLAM_RNK_6 = 30356,
		SHIELD_WALL_RNK_1 = 871,
		SPELL_REFLECTION_RNK_1 = 23920,
		STANCE_MASTERY_RNK_1 = 12678,
		SUNDER_ARMOR_RNK_1 = 7386,
		SUNDER_ARMOR_RNK_2 = 7405,
		SUNDER_ARMOR_RNK_3 = 8380,
		SUNDER_ARMOR_RNK_4 = 11596,
		SUNDER_ARMOR_RNK_5 = 11597,
		SUNDER_ARMOR_RNK_6 = 25225,
		TAUNT_RNK_1 = 355,
    };
};

#endif // H_CLASS_SPELLS