
#ifndef _PLAYER_H
#define _PLAYER_H

#include "ItemPrototype.h"
#include "Unit.h"
#include "Item.h"

#include "QuestDef.h"
#include "Group.h"
#include "WorldSession.h"
#include "MapReference.h"
#include "Util.h"                                           // for Tokens typedef
#include "SpellMgr.h"
#include "PlayerTaxi.h"

#include<string>
#include<vector>

struct Mail;
class Channel;
class Creature;
class Pet;
class PlayerMenu;
class MotionTransport;
class UpdateMask;
class PlayerSocial;
class OutdoorPvP;
class SpectatorAddonMsg;
class ArenaTeam;
class Guild;
enum PetType : int;
enum PetSaveMode : int;
struct TrainerSpell;
class SpellCastTargets;
class PlayerAI;
class CinematicMgr;

#ifdef PLAYERBOT
// Playerbot mod
class PlayerbotAI;
class PlayerbotTestingAI;
class PlayerbotMgr;
#endif

typedef std::deque<Mail*> PlayerMails;

#define PLAYER_MAX_SKILLS           127
#define PLAYER_MAX_DAILY_QUESTS     25
#define PLAYER_EXPLORED_ZONES_SIZE  128

#define REPUTATION_CAP    42999
#define REPUTATION_BOTTOM -42999

// Note: SPELLMOD_* values is aura types in fact
enum SpellModType
{
    SPELLMOD_FLAT         = 107,                            // SPELL_AURA_ADD_FLAT_MODIFIER
    SPELLMOD_PCT          = 108                             // SPELL_AURA_ADD_PCT_MODIFIER
};

// 2^n values, Player::m_isunderwater is a bitmask. These are internal values, they are never send to any client
enum PlayerUnderwaterState
{
    UNDERWATER_NONE                     = 0x00,
    UNDERWATER_INWATER                  = 0x01,             // terrain type is water and player is afflicted by it
    UNDERWATER_INLAVA                   = 0x02,             // terrain type is lava and player is afflicted by it
    UNDERWATER_INSLIME                  = 0x04,             // terrain type is lava and player is afflicted by it
    UNDERWATER_INDARKWATER              = 0x08,             // terrain type is dark water and player is afflicted by it

    UNDERWATER_EXIST_TIMERS             = 0x10
};

enum BuyBankSlotResult
{
    ERR_BANKSLOT_FAILED_TOO_MANY    = 0,
    ERR_BANKSLOT_INSUFFICIENT_FUNDS = 1,
    ERR_BANKSLOT_NOTBANKER          = 2,
    ERR_BANKSLOT_OK                 = 3
};

enum PlayerSpellState
{
    PLAYERSPELL_UNCHANGED = 0,
    PLAYERSPELL_CHANGED   = 1,
    PLAYERSPELL_NEW       = 2,
    PLAYERSPELL_REMOVED   = 3,
    //TC PLAYERSPELL_TEMPORARY = 4
};

struct PlayerSpell
{
    uint16 slotId          : 16;
    PlayerSpellState state : 8;
    bool active            : 1;   // show in spellbook
    bool dependent         : 1;   // learned as result another spell learn, skill grow, quest reward, etc
    bool disabled          : 1;   // first rank has been learned in result talent learn but currently talent unlearned, save max learned ranks
};

#define SPELL_WITHOUT_SLOT_ID uint16(-1)

struct SpellModifier
{
    SpellModOp   op   : 8;
    SpellModType type : 8;
    int16 charges     : 16;
    int32 value;
    uint64 mask;
    uint32 spellId;
    uint32 effectId;
    Spell const* lastAffected;
};

typedef std::unordered_map<uint16, PlayerSpell*> PlayerSpellMap;
typedef std::list<SpellModifier*> SpellModList;

struct SpellCooldown
{
    time_t end;
    uint16 itemid;
};

typedef std::map<uint32, SpellCooldown> SpellCooldowns;

enum TrainerSpellState
{
    TRAINER_SPELL_GREEN = 0,
    TRAINER_SPELL_RED   = 1,
    TRAINER_SPELL_GRAY  = 2
};

enum ActionButtonUpdateState
{
    ACTIONBUTTON_UNCHANGED = 0,
    ACTIONBUTTON_CHANGED   = 1,
    ACTIONBUTTON_NEW       = 2,
    ACTIONBUTTON_DELETED   = 3
};

struct ActionButton
{
    ActionButton() : action(0), type(0), misc(0), uState( ACTIONBUTTON_NEW ) {}
    ActionButton(uint16 _action, uint8 _type, uint8 _misc) : action(_action), type(_type), misc(_misc), uState( ACTIONBUTTON_NEW ) {}

    uint16 action;
    uint8 type;
    uint8 misc;
    ActionButtonUpdateState uState;
};

enum ActionButtonType
{
    ACTION_BUTTON_SPELL = 0,
    ACTION_BUTTON_MACRO = 64,
    ACTION_BUTTON_CMACRO= 65,
    ACTION_BUTTON_ITEM  = 128
};

#define  MAX_ACTION_BUTTONS 132                             //checked in 2.3.0

typedef std::map<uint8,ActionButton> ActionButtonList;

enum RemovePetReason
{
    REMOVE_PET_REASON_OTHER,
    REMOVE_PET_REASON_SCRIPT,
    REMOVE_PET_REASON_PLAYER_DIED,
    REMOVE_PET_REASON_DISMISSED,
};

typedef std::pair<uint16, uint8> CreateSpellPair;

struct PlayerCreateInfoItem
{
    PlayerCreateInfoItem(uint32 id, uint32 amount) : item_id(id), item_amount(amount) {}

    uint32 item_id;
    uint32 item_amount;
};

typedef std::list<PlayerCreateInfoItem> PlayerCreateInfoItems;
struct PlayerCreateInfoSkill
{
    uint16 SkillId;
    uint16 Rank;
};
typedef std::vector<PlayerCreateInfoSkill> PlayerCreateInfoSkills;

struct PlayerClassLevelInfo
{
    PlayerClassLevelInfo() : basehealth(0), basemana(0) {}
    uint16 basehealth;
    uint16 basemana;
};

struct PlayerClassInfo
{
    PlayerClassInfo() : levelInfo(nullptr) { }

    PlayerClassLevelInfo* levelInfo;                        //[level-1] 0..MaxPlayerLevel-1
};

struct PlayerLevelInfo
{
    PlayerLevelInfo() { for(unsigned char & stat : stats) stat = 0; }

    uint8 stats[MAX_STATS];
};

struct PlayerInfo
{
                                                            // existence checked by displayId != 0             // existence checked by displayId != 0
    PlayerInfo() : displayId_m(0),displayId_f(0),levelInfo(nullptr)
    {
        positionZ = 0.0f;
        positionX = 0.0f;
        positionY = 0.0f;
        mapId = 0;
        areaId = 0;
    }

    uint32 mapId;
    uint32 areaId;
    float positionX;
    float positionY;
    float positionZ;
    uint16 displayId_m;
    uint16 displayId_f;
    PlayerCreateInfoItems item;
    std::list<CreateSpellPair> spell;
    PlayerCreateInfoSkills skills;
    std::list<uint16> action[4];

    PlayerLevelInfo* levelInfo;                             //[level-1] 0..MaxPlayerLevel-1
};

struct PvPInfo
{
    PvPInfo() : IsInHostileArea(false), IsHostile(false), IsInNoPvPArea(false), IsInFFAPvPArea(false), endTimer(0) {}

	bool IsHostile;
    bool IsInHostileArea;
    bool IsInNoPvPArea;                 ///> Marks if player is in a sanctuary or friendly capital city
    bool IsInFFAPvPArea;                ///> Marks if player is in an FFAPvP area (such as Gurubashi Arena)
    time_t endTimer;
};

struct DuelInfo
{
    DuelInfo() : initiator(nullptr), opponent(nullptr), startTimer(0), startTime(0), outOfBound(0), isCompleted(false) {}

    Player *initiator;
    Player *opponent;
    time_t startTimer;
    time_t startTime;
    time_t outOfBound;
    bool isCompleted;
};

enum Pack58
{
    PACK58_STEP1,
    PACK58_MELEE,
    PACK58_HEAL,
    PACK58_TANK,
    PACK58_MAGIC,
};

enum Pack58Type
{
    PACK58_TYPE_MELEE = 0,
    PACK58_TYPE_HEAL  = 1,
    PACK58_TYPE_TANK  = 2,
    PACK58_TYPE_MAGIC = 3,
};

struct Areas
{
    uint32 areaID;
    uint32 areaFlag;
    float x1;
    float x2;
    float y1;
    float y2;
};

enum FactionFlags
{
    FACTION_FLAG_VISIBLE            = 0x01,                 // makes visible in client (set or can be set at interaction with target of this faction)
    FACTION_FLAG_AT_WAR             = 0x02,                 // enable AtWar-button in client. player controlled (except opposition team always war state), Flag only set on initial creation
    FACTION_FLAG_HIDDEN             = 0x04,                 // hidden faction from reputation pane in client (player can gain reputation, but this update not sent to client)
    FACTION_FLAG_INVISIBLE_FORCED   = 0x08,                 // always overwrite FACTION_FLAG_VISIBLE and hide faction in rep.list, used for hide opposite team factions
    FACTION_FLAG_PEACE_FORCED       = 0x10,                 // always overwrite FACTION_FLAG_AT_WAR, used for prevent war with own team factions
    FACTION_FLAG_INACTIVE           = 0x20,                 // player controlled, state stored in characters.data ( CMSG_SET_FACTION_INACTIVE )
    FACTION_FLAG_RIVAL              = 0x40                  // flag for the two competing outland factions
};

enum LoadData
{
    LOAD_DATA_GUID,
    LOAD_DATA_ACCOUNT,
    LOAD_DATA_NAME,
    LOAD_DATA_RACE,
    LOAD_DATA_CLASS,
    LOAD_DATA_GENDER,
    LOAD_DATA_LEVEL,
    LOAD_DATA_XP,
    LOAD_DATA_MONEY,
    LOAD_DATA_PLAYERBYTES,
    LOAD_DATA_PLAYERBYTES2,
    LOAD_DATA_PLAYERFLAGS,
    LOAD_DATA_POSX,
    LOAD_DATA_POSY,
    LOAD_DATA_POSZ,
    LOAD_DATA_MAP,
    LOAD_DATA_INSTANCE_ID,
    LOAD_DATA_ORIENTATION,
    LOAD_DATA_TAXIMASK,
    LOAD_DATA_ONLINE,
    LOAD_DATA_CINEMATIC,
    LOAD_DATA_TOTALTIME,
    LOAD_DATA_LEVELTIME,
    LOAD_DATA_LOGOUT_TIME,
    LOAD_DATA_IS_LOGOUT_RESTING,
    LOAD_DATA_REST_BONUS,
    LOAD_DATA_RESETTALENTS_COST,
    LOAD_DATA_RESETTALENTS_TIME,
    LOAD_DATA_TRANSX,
    LOAD_DATA_TRANSY,
    LOAD_DATA_TRANSZ,
    LOAD_DATA_TRANSO,
    LOAD_DATA_TRANSGUID,
    LOAD_DATA_EXTRA_FLAGS,
    LOAD_DATA_STABLE_SLOTS,
    LOAD_DATA_AT_LOGIN,
    LOAD_DATA_ZONE,
    LOAD_DATA_DEATH_EXPIRE_TIME,
    LOAD_DATA_TAXI_PATH,
    LOAD_DATA_DUNGEON_DIFF,
    LOAD_DATA_ARENA_PEND_POINTS,
    LOAD_DATA_ARENAPOINTS,
    LOAD_DATA_TOTALHONORPOINTS,
    LOAD_DATA_TODAYHONORPOINTS,
    LOAD_DATA_YESTERDAYHONORPOINTS,
    LOAD_DATA_TOTALKILLS,
    LOAD_DATA_TODAYKILLS,
    LOAD_DATA_YESTERDAYKILLS,
    LOAD_DATA_CHOSEN_TITLE,
    LOAD_DATA_WATCHED_FACTION,
    LOAD_DATA_DRUNK,
    LOAD_DATA_HEALTH,
    LOAD_DATA_POWER1,
    LOAD_DATA_POWER2,
    LOAD_DATA_POWER3,
    LOAD_DATA_POWER4,
    LOAD_DATA_POWER5,
    LOAD_DATA_EXPLOREDZONES,
    LOAD_DATA_EQUIPMENTCACHE,
    LOAD_DATA_AMMOID,
    LOAD_DATA_KNOWNTITLES,
    LOAD_DATA_ACTIONBARS,
    LOAD_DATA_XP_BLOCKED,
    LOAD_DATA_CUSTOM_XP
};

typedef uint32 RepListID;
struct FactionState
{
    uint32 ID;
    RepListID ReputationListID;
    uint32 Flags;
    int32  Standing;
    bool Changed;
    bool Deleted;
};

typedef std::map<RepListID,FactionState> FactionStateList;

typedef std::map<uint32,ReputationRank> ForcedReactions;

typedef std::set<uint64> GuardianPetList;

struct EnchantDuration
{
    EnchantDuration() : item(nullptr), slot(MAX_ENCHANTMENT_SLOT), leftduration(0) {};
    EnchantDuration(Item * _item, EnchantmentSlot _slot, uint32 _leftduration) : item(_item), slot(_slot), leftduration(_leftduration) { assert(item); };

    Item * item;
    EnchantmentSlot slot;
    uint32 leftduration;
};

typedef std::list<EnchantDuration> EnchantDurationList;
typedef std::list<Item*> ItemDurationList;

struct LookingForGroupSlot
{
    LookingForGroupSlot() : entry(0), type(0) {}
    bool Empty() const { return !entry && !type; }
    void Clear() { entry = 0; type = 0; }
    void Set(uint32 _entry, uint32 _type ) { entry = _entry; type = _type; }
    bool Is(uint32 _entry, uint32 _type) const { return entry==_entry && type==_type; }
    bool canAutoJoin() const { return entry && (type == 1 || type == 5); }

    uint32 entry;
    uint32 type;
};

#define MAX_LOOKING_FOR_GROUP_SLOT 3

struct LookingForGroup
{
    LookingForGroup() {}
    bool HaveInSlot(LookingForGroupSlot const& slot) const { return HaveInSlot(slot.entry,slot.type); }
    bool HaveInSlot(uint32 _entry, uint32 _type) const
    {
        for(auto slot : slots)
            if(slot.Is(_entry,_type))
                return true;
        return false;
    }

    bool canAutoJoin() const
    {
        for(auto slot : slots)
            if(slot.canAutoJoin())
                return true;
        return false;
    }

    bool Empty() const
    {
        for(auto slot : slots)
            if(!slot.Empty())
                return false;
        return more.Empty();
    }

    LookingForGroupSlot slots[MAX_LOOKING_FOR_GROUP_SLOT];
    LookingForGroupSlot more;
    std::string comment;
};

enum PlayerMovementType
{
    MOVE_ROOT       = 1,
    MOVE_UNROOT     = 2,
    MOVE_WATER_WALK = 3,
    MOVE_LAND_WALK  = 4
};

enum DrunkenState
{
    DRUNKEN_SOBER   = 0,
    DRUNKEN_TIPSY   = 1,
    DRUNKEN_DRUNK   = 2,
    DRUNKEN_SMASHED = 3
};

enum PlayerStateType
{
    /*
        PLAYER_STATE_DANCE
        PLAYER_STATE_SLEEP
        PLAYER_STATE_SIT
        PLAYER_STATE_STAND
        PLAYER_STATE_READYUNARMED
        PLAYER_STATE_WORK
        PLAYER_STATE_POINT(DNR)
        PLAYER_STATE_NONE // not used or just no state, just standing there?
        PLAYER_STATE_STUN
        PLAYER_STATE_DEAD
        PLAYER_STATE_KNEEL
        PLAYER_STATE_USESTANDING
        PLAYER_STATE_STUN_NO_SHEATHE
        PLAYER_STATE_USESTANDING_NO_SHEATHE
        PLAYER_STATE_WORK_NO_SHEATHE
        PLAYER_STATE_SPELLPRECAST
        PLAYER_STATE_READYRIFLE
        PLAYER_STATE_WORK_NO_SHEATHE_MINING
        PLAYER_STATE_WORK_NO_SHEATHE_CHOPWOOD
        PLAYER_STATE_AT_EASE
        PLAYER_STATE_READY1H
        PLAYER_STATE_SPELLKNEELSTART
        PLAYER_STATE_SUBMERGED
    */

    PLAYER_STATE_NONE              = 0,
    PLAYER_STATE_SIT               = 1,
    PLAYER_STATE_SIT_CHAIR         = 2,
    PLAYER_STATE_SLEEP             = 3,
    PLAYER_STATE_SIT_LOW_CHAIR     = 4,
    PLAYER_STATE_SIT_MEDIUM_CHAIR  = 5,
    PLAYER_STATE_SIT_HIGH_CHAIR    = 6,
    PLAYER_STATE_DEAD              = 7,
    PLAYER_STATE_KNEEL             = 8,

    PLAYER_STATE_FORM_ALL          = 0x00FF0000,

    PLAYER_STATE_FLAG_UNTRACKABLE  = 0x04000000,
    PLAYER_STATE_FLAG_ALL          = 0xFF000000,
};

enum PlayerFlags
{
    PLAYER_FLAGS_GROUP_LEADER       = 0x00000001,
    PLAYER_FLAGS_AFK                = 0x00000002,
    PLAYER_FLAGS_DND                = 0x00000004,
    PLAYER_FLAGS_GM                 = 0x00000008,
    PLAYER_FLAGS_GHOST              = 0x00000010,
    PLAYER_FLAGS_RESTING            = 0x00000020,
    PLAYER_FLAGS_FFA_PVP            = 0x00000080,
    PLAYER_FLAGS_CONTESTED_PVP      = 0x00000100,               // Player has been involved in a PvP combat and will be attacked by contested guards
    PLAYER_FLAGS_IN_PVP             = 0x00000200,
    PLAYER_FLAGS_HIDE_HELM          = 0x00000400,
    PLAYER_FLAGS_HIDE_CLOAK         = 0x00000800,
    PLAYER_FLAGS_PLAYED_LONG_TIME   = 0x00001000,               // played long time
    PLAYER_FLAGS_PLAYED_TOO_LONG    = 0x00002000,               // played too long time
    PLAYER_FLAGS_IS_OUT_OF_BOUNDS   = 0x00004000,
    PLAYER_FLAGS_DEVELOPER          = 0x00008000,               // <Dev> prefix for something?
    PLAYER_FLAGS_SANCTUARY          = 0x00010000,               // player entered sanctuary
    PLAYER_FLAGS_TAXI_BENCHMARK     = 0x00020000,               // taxi benchmark mode (on/off) (2.0.1)
    PLAYER_FLAGS_PVP_COUNTER        = 0x00040000,               // 2.0.8...
    PLAYER_FLAGS_COMMENTATOR        = 0x00080000,
    PLAYER_FLAGS_UNK5               = 0x00100000,
    PLAYER_FLAGS_UNK6               = 0x00200000,
    PLAYER_FLAGS_COMMENTATOR_UBER   = 0x00400000
};

enum PlayerBytesOffsets : uint8
{
    PLAYER_BYTES_OFFSET_SKIN_ID         = 0,
    PLAYER_BYTES_OFFSET_FACE_ID         = 1,
    PLAYER_BYTES_OFFSET_HAIR_STYLE_ID   = 2,
    PLAYER_BYTES_OFFSET_HAIR_COLOR_ID   = 3
};

enum PlayerBytes2Offsets : uint8
{
    PLAYER_BYTES_2_OFFSET_FACIAL_STYLE      = 0,
	//BC OK ?   PLAYER_BYTES_2_OFFSET_PARTY_TYPE        = 1,
	//BC OK ?   PLAYER_BYTES_2_OFFSET_BANK_BAG_SLOTS    = 2,
	//BC OK ?   PLAYER_BYTES_2_OFFSET_REST_STATE        = 3
};
enum PlayerBytes3Offsets : uint8
{
    PLAYER_BYTES_3_OFFSET_GENDER = 0,
    //BC OK ? PLAYER_BYTES_3_OFFSET_INEBRIATION = 1,
    //BC OK ? PLAYER_BYTES_3_OFFSET_PVP_TITLE = 2,
    //BC OK ? PLAYER_BYTES_3_OFFSET_ARENA_FACTION = 3
};

enum PlayerFieldBytes2Offsets
{
#ifdef LICH_KING
    PLAYER_FIELD_BYTES_2_OFFSET_OVERRIDE_SPELLS_ID                  = 0,    // uint16!
    PLAYER_FIELD_BYTES_2_OFFSET_IGNORE_POWER_REGEN_PREDICTION_MASK  = 2,
    PLAYER_FIELD_BYTES_2_OFFSET_AURA_VISION                         = 3
#else
	//BC OK ? PLAYER_FIELD_BYTES_2_OFFSET_OVERRIDE_SPELLS_ID = 0,    // uint16!
	PLAYER_FIELD_BYTES_2_OFFSET_AURA_VISION = 1,
	//BC OK ? PLAYER_FIELD_BYTES_2_OFFSET_IGNORE_POWER_REGEN_PREDICTION_MASK = 2,
#endif
};

// used in PLAYER_FIELD_BYTES values
enum PlayerFieldByteFlags
{
	PLAYER_FIELD_BYTE_TRACK_STEALTHED	= 0x00000002,
	PLAYER_FIELD_BYTE_RELEASE_TIMER		= 0x00000008,       // Display time till auto release spirit
	PLAYER_FIELD_BYTE_NO_RELEASE_WINDOW = 0x00000010        // Display no "release spirit" window at all
};

// used in PLAYER_FIELD_BYTES2 values (with offsets in PlayerFieldBytes2Offsets)
enum PlayerFieldByte2Flags
{
    PLAYER_FIELD_BYTE2_NONE                 = 0x00,
    PLAYER_FIELD_BYTE2_STEALTH              = 0x20,
    PLAYER_FIELD_BYTE2_INVISIBILITY_GLOW    = 0x40
};


#define PLAYER_TITLE_MASK_ALLIANCE_PVP  \
    (PLAYER_TITLE_PRIVATE | PLAYER_TITLE_CORPORAL |  \
      PLAYER_TITLE_SERGEANT_A | PLAYER_TITLE_MASTER_SERGEANT | \
      PLAYER_TITLE_SERGEANT_MAJOR | PLAYER_TITLE_KNIGHT | \
      PLAYER_TITLE_KNIGHT_LIEUTENANT | PLAYER_TITLE_KNIGHT_CAPTAIN | \
      PLAYER_TITLE_KNIGHT_CHAMPION | PLAYER_TITLE_LIEUTENANT_COMMANDER | \
      PLAYER_TITLE_COMMANDER | PLAYER_TITLE_MARSHAL | \
      PLAYER_TITLE_FIELD_MARSHAL | PLAYER_TITLE_GRAND_MARSHAL)

#define PLAYER_TITLE_MASK_HORDE_PVP  \
    (PLAYER_TITLE_SCOUT | PLAYER_TITLE_GRUNT |  \
      PLAYER_TITLE_SERGEANT_H | PLAYER_TITLE_SENIOR_SERGEANT | \
      PLAYER_TITLE_FIRST_SERGEANT | PLAYER_TITLE_STONE_GUARD | \
      PLAYER_TITLE_BLOOD_GUARD | PLAYER_TITLE_LEGIONNAIRE | \
      PLAYER_TITLE_CENTURION | PLAYER_TITLE_CHAMPION | \
      PLAYER_TITLE_LIEUTENANT_GENERAL | PLAYER_TITLE_GENERAL | \
      PLAYER_TITLE_WARLORD | PLAYER_TITLE_HIGH_WARLORD)

#define PLAYER_TITLE_MASK_ALL_PVP  \
    (PLAYER_TITLE_MASK_ALLIANCE_PVP | PLAYER_TITLE_MASK_HORDE_PVP)

// used for PLAYER__FIELD_KNOWN_TITLES field (uint64), (1<<bit_index) without (-1)
// can't use enum for uint64 values
#define PLAYER_TITLE_DISABLED              0x0000000000000000LL
#define PLAYER_TITLE_NONE                  0x0000000000000001LL
#define PLAYER_TITLE_PRIVATE               0x0000000000000002LL // 1
#define PLAYER_TITLE_CORPORAL              0x0000000000000004LL // 2
#define PLAYER_TITLE_SERGEANT_A            0x0000000000000008LL // 3
#define PLAYER_TITLE_MASTER_SERGEANT       0x0000000000000010LL // 4
#define PLAYER_TITLE_SERGEANT_MAJOR        0x0000000000000020LL // 5
#define PLAYER_TITLE_KNIGHT                0x0000000000000040LL // 6
#define PLAYER_TITLE_KNIGHT_LIEUTENANT     0x0000000000000080LL // 7
#define PLAYER_TITLE_KNIGHT_CAPTAIN        0x0000000000000100LL // 8
#define PLAYER_TITLE_KNIGHT_CHAMPION       0x0000000000000200LL // 9
#define PLAYER_TITLE_LIEUTENANT_COMMANDER  0x0000000000000400LL // 10
#define PLAYER_TITLE_COMMANDER             0x0000000000000800LL // 11
#define PLAYER_TITLE_MARSHAL               0x0000000000001000LL // 12
#define PLAYER_TITLE_FIELD_MARSHAL         0x0000000000002000LL // 13
#define PLAYER_TITLE_GRAND_MARSHAL         0x0000000000004000LL // 14
#define PLAYER_TITLE_SCOUT                 0x0000000000008000LL // 15
#define PLAYER_TITLE_GRUNT                 0x0000000000010000LL // 16
#define PLAYER_TITLE_SERGEANT_H            0x0000000000020000LL // 17
#define PLAYER_TITLE_SENIOR_SERGEANT       0x0000000000040000LL // 18
#define PLAYER_TITLE_FIRST_SERGEANT        0x0000000000080000LL // 19
#define PLAYER_TITLE_STONE_GUARD           0x0000000000100000LL // 20
#define PLAYER_TITLE_BLOOD_GUARD           0x0000000000200000LL // 21
#define PLAYER_TITLE_LEGIONNAIRE           0x0000000000400000LL // 22
#define PLAYER_TITLE_CENTURION             0x0000000000800000LL // 23
#define PLAYER_TITLE_CHAMPION              0x0000000001000000LL // 24
#define PLAYER_TITLE_LIEUTENANT_GENERAL    0x0000000002000000LL // 25
#define PLAYER_TITLE_GENERAL               0x0000000004000000LL // 26
#define PLAYER_TITLE_WARLORD               0x0000000008000000LL // 27
#define PLAYER_TITLE_HIGH_WARLORD          0x0000000010000000LL // 28
#define PLAYER_TITLE_GLADIATOR             0x0000000020000000LL // 29
#define PLAYER_TITLE_DUELIST               0x0000000040000000LL // 30
#define PLAYER_TITLE_RIVAL                 0x0000000080000000LL // 31
#define PLAYER_TITLE_CHALLENGER            0x0000000100000000LL // 32
#define PLAYER_TITLE_SCARAB_LORD           0x0000000200000000LL // 33
#define PLAYER_TITLE_CONQUEROR             0x0000000400000000LL // 34
#define PLAYER_TITLE_JUSTICAR              0x0000000800000000LL // 35
#define PLAYER_TITLE_CHAMPION_OF_THE_NAARU 0x0000001000000000LL // 36
#define PLAYER_TITLE_MERCILESS_GLADIATOR   0x0000002000000000LL // 37
#define PLAYER_TITLE_OF_THE_SHATTERED_SUN  0x0000004000000000LL // 38
#define PLAYER_TITLE_HAND_OF_ADAL          0x0000008000000000LL // 39
#define PLAYER_TITLE_VENGEFUL_GLADIATOR    0x0000010000000000LL // 40

#define KNOWN_TITLES_SIZE   3
#define MAX_TITLE_INDEX     (KNOWN_TITLES_SIZE*64)          // 3 uint64 fields

#ifndef LICH_KING
enum LootType
{
    LOOT_NONE                   = 0,

    LOOT_CORPSE                 = 1,
    LOOT_SKINNING               = 2,
    LOOT_FISHING                = 3,
    LOOT_PICKPOCKETING          = 4,                        // unsupported by client, sending LOOT_SKINNING instead
    LOOT_DISENCHANTING          = 5,                        // unsupported by client, sending LOOT_SKINNING instead
    LOOT_PROSPECTING            = 6,                        // unsupported by client, sending LOOT_SKINNING instead
    LOOT_INSIGNIA               = 7,                        // unsupported by client, sending LOOT_SKINNING instead
    LOOT_FISHINGHOLE            = 8                         // unsupported by client, sending LOOT_FISHING instead
};
#else
enum LootType
{
    LOOT_NONE                   = 0,

    LOOT_CORPSE                 = 1,
    LOOT_PICKPOCKETING          = 2,
    LOOT_FISHING                = 3,
    LOOT_DISENCHANTING          = 4,
                                                            // ignored always by client
    LOOT_SKINNING               = 6,
    LOOT_PROSPECTING            = 7,
    LOOT_MILLING                = 8,

    LOOT_FISHINGHOLE            = 20,                       // unsupported by client, sending LOOT_FISHING instead
    LOOT_INSIGNIA               = 21,                       // unsupported by client, sending LOOT_CORPSE instead
    LOOT_FISHING_JUNK           = 22                        // unsupported by client, sending LOOT_FISHING instead
};
#endif

enum LootError
{
    LOOT_ERROR_DIDNT_KILL               = 0,    // You don't have permission to loot that corpse.
    LOOT_ERROR_TOO_FAR                  = 4,    // You are too far away to loot that corpse.
    LOOT_ERROR_BAD_FACING               = 5,    // You must be facing the corpse to loot it.
    LOOT_ERROR_LOCKED                   = 6,    // Someone is already looting that corpse.
    LOOT_ERROR_NOTSTANDING              = 8,    // You need to be standing up to loot something!
    LOOT_ERROR_STUNNED                  = 9,    // You can't loot anything while stunned!
    LOOT_ERROR_PLAYER_NOT_FOUND         = 10,   // Player not found
    LOOT_ERROR_PLAY_TIME_EXCEEDED       = 11,   // Maximum play time exceeded
    LOOT_ERROR_MASTER_INV_FULL          = 12,   // That player's inventory is full
    LOOT_ERROR_MASTER_UNIQUE_ITEM       = 13,   // Player has too many of that item already
    LOOT_ERROR_MASTER_OTHER             = 14,   // Can't assign item to that player
    LOOT_ERROR_ALREADY_PICKPOCKETED     = 15,   // Your target has already had its pockets picked
    LOOT_ERROR_NOT_WHILE_SHAPESHIFTED   = 16    // You can't do that while shapeshifted.
};

enum MirrorTimerType
{
    FATIGUE_TIMER      = 0,
    BREATH_TIMER       = 1,
    FIRE_TIMER         = 2
};
#define MAX_TIMERS 3
#define DISABLED_MIRROR_TIMER   -1

// 2^n values
enum PlayerExtraFlags
{
    // gm abilities
    PLAYER_EXTRA_GM_ON              = 0x0001,
    PLAYER_EXTRA_ACCEPT_WHISPERS    = 0x0004,
    PLAYER_EXTRA_TAXICHEAT          = 0x0008,
    PLAYER_EXTRA_GM_INVISIBLE       = 0x0010,
    PLAYER_EXTRA_GM_CHAT            = 0x0020,               // Show GM badge in chat messages

    // other states
    PLAYER_EXTRA_DUEL_AREA          = 0x0040,              // currently in duel area
    PLAYER_EXTRA_PVP_DEATH          = 0x0100               // store PvP death status until corpse creating.
};

// 2^n values
enum AtLoginFlags
{
    AT_LOGIN_NONE          = 0x00,
    AT_LOGIN_RENAME        = 0x01,
    AT_LOGIN_RESET_SPELLS  = 0x02,
    AT_LOGIN_RESET_TALENTS = 0x04,
    AT_LOGIN_SET_DESERTER  = 0x08,
    AT_LOGIN_RESET_FLYS    = 0x10,
    AT_LOGIN_ALL_REP       = 0x20,
    AT_LOGIN_FIRST         = 0x40,
	//0x80
	AT_LOGIN_RESURRECT     = 0x100,
};

typedef std::map<uint32, QuestStatusData> QuestStatusMap;

enum QuestSlotOffsets
{
    QUEST_ID_OFFSET     = 0,
    QUEST_STATE_OFFSET  = 1,
    QUEST_COUNTS_OFFSET = 2,
    QUEST_TIME_OFFSET   = 3
};

#define MAX_QUEST_OFFSET 4

enum QuestSlotStateMask
{
    QUEST_STATE_NONE     = 0x0000,
    QUEST_STATE_COMPLETE = 0x0001,
    QUEST_STATE_FAIL     = 0x0002,
};

enum SkillUpdateState
{
    SKILL_UNCHANGED     = 0,
    SKILL_CHANGED       = 1,
    SKILL_NEW           = 2,
    SKILL_DELETED       = 3
};

struct SkillStatusData
{
    SkillStatusData(uint8 _pos, SkillUpdateState _uState) : pos(_pos), uState(_uState)
    {
    }
    
    uint8 pos;
    SkillUpdateState uState;
};

typedef std::unordered_map<uint32, SkillStatusData> SkillStatusMap;

class Quest;
class Spell;
class Item;
class WorldSession;

enum PlayerSlots
{
    // first slot for item stored (in any way in player m_items data)
    PLAYER_SLOT_START           = 0,
    // last+1 slot for item stored (in any way in player m_items data)
    PLAYER_SLOT_END             = 118,
    PLAYER_SLOTS_COUNT          = (PLAYER_SLOT_END - PLAYER_SLOT_START)
};

enum EquipmentSlots
{
    EQUIPMENT_SLOT_START        = 0,
    EQUIPMENT_SLOT_HEAD         = 0,
    EQUIPMENT_SLOT_NECK         = 1,
    EQUIPMENT_SLOT_SHOULDERS    = 2,
    EQUIPMENT_SLOT_BODY         = 3,
    EQUIPMENT_SLOT_CHEST        = 4,
    EQUIPMENT_SLOT_WAIST        = 5,
    EQUIPMENT_SLOT_LEGS         = 6,
    EQUIPMENT_SLOT_FEET         = 7,
    EQUIPMENT_SLOT_WRISTS       = 8,
    EQUIPMENT_SLOT_HANDS        = 9,
    EQUIPMENT_SLOT_FINGER1      = 10,
    EQUIPMENT_SLOT_FINGER2      = 11,
    EQUIPMENT_SLOT_TRINKET1     = 12,
    EQUIPMENT_SLOT_TRINKET2     = 13,
    EQUIPMENT_SLOT_BACK         = 14,
    EQUIPMENT_SLOT_MAINHAND     = 15,
    EQUIPMENT_SLOT_OFFHAND      = 16,
    EQUIPMENT_SLOT_RANGED       = 17,
    EQUIPMENT_SLOT_TABARD       = 18,
    EQUIPMENT_SLOT_END          = 19
};

enum InventorySlots
{
    INVENTORY_SLOT_BAG_0        = 255,
    INVENTORY_SLOT_BAG_START    = 19,
    INVENTORY_SLOT_BAG_1        = 19,
    INVENTORY_SLOT_BAG_2        = 20,
    INVENTORY_SLOT_BAG_3        = 21,
    INVENTORY_SLOT_BAG_4        = 22,
    INVENTORY_SLOT_BAG_END      = 23,

    INVENTORY_SLOT_ITEM_START   = 23,
    INVENTORY_SLOT_ITEM_1       = 23,
    INVENTORY_SLOT_ITEM_2       = 24,
    INVENTORY_SLOT_ITEM_3       = 25,
    INVENTORY_SLOT_ITEM_4       = 26,
    INVENTORY_SLOT_ITEM_5       = 27,
    INVENTORY_SLOT_ITEM_6       = 28,
    INVENTORY_SLOT_ITEM_7       = 29,
    INVENTORY_SLOT_ITEM_8       = 30,
    INVENTORY_SLOT_ITEM_9       = 31,
    INVENTORY_SLOT_ITEM_10      = 32,
    INVENTORY_SLOT_ITEM_11      = 33,
    INVENTORY_SLOT_ITEM_12      = 34,
    INVENTORY_SLOT_ITEM_13      = 35,
    INVENTORY_SLOT_ITEM_14      = 36,
    INVENTORY_SLOT_ITEM_15      = 37,
    INVENTORY_SLOT_ITEM_16      = 38,
    INVENTORY_SLOT_ITEM_END     = 39
};

enum BankSlots
{
    BANK_SLOT_ITEM_START        = 39,
    BANK_SLOT_ITEM_1            = 39,
    BANK_SLOT_ITEM_2            = 40,
    BANK_SLOT_ITEM_3            = 41,
    BANK_SLOT_ITEM_4            = 42,
    BANK_SLOT_ITEM_5            = 43,
    BANK_SLOT_ITEM_6            = 44,
    BANK_SLOT_ITEM_7            = 45,
    BANK_SLOT_ITEM_8            = 46,
    BANK_SLOT_ITEM_9            = 47,
    BANK_SLOT_ITEM_10           = 48,
    BANK_SLOT_ITEM_11           = 49,
    BANK_SLOT_ITEM_12           = 50,
    BANK_SLOT_ITEM_13           = 51,
    BANK_SLOT_ITEM_14           = 52,
    BANK_SLOT_ITEM_15           = 53,
    BANK_SLOT_ITEM_16           = 54,
    BANK_SLOT_ITEM_17           = 55,
    BANK_SLOT_ITEM_18           = 56,
    BANK_SLOT_ITEM_19           = 57,
    BANK_SLOT_ITEM_20           = 58,
    BANK_SLOT_ITEM_21           = 59,
    BANK_SLOT_ITEM_22           = 60,
    BANK_SLOT_ITEM_23           = 61,
    BANK_SLOT_ITEM_24           = 62,
    BANK_SLOT_ITEM_25           = 63,
    BANK_SLOT_ITEM_26           = 64,
    BANK_SLOT_ITEM_27           = 65,
    BANK_SLOT_ITEM_28           = 66,
    BANK_SLOT_ITEM_END          = 67,

    BANK_SLOT_BAG_START         = 67,
    BANK_SLOT_BAG_1             = 67,
    BANK_SLOT_BAG_2             = 68,
    BANK_SLOT_BAG_3             = 69,
    BANK_SLOT_BAG_4             = 70,
    BANK_SLOT_BAG_5             = 71,
    BANK_SLOT_BAG_6             = 72,
    BANK_SLOT_BAG_7             = 73,
    BANK_SLOT_BAG_END           = 74
};

enum BuyBackSlots
{
    // stored in m_buybackitems
    BUYBACK_SLOT_START          = 74,
    BUYBACK_SLOT_1              = 74,
    BUYBACK_SLOT_2              = 75,
    BUYBACK_SLOT_3              = 76,
    BUYBACK_SLOT_4              = 77,
    BUYBACK_SLOT_5              = 78,
    BUYBACK_SLOT_6              = 79,
    BUYBACK_SLOT_7              = 80,
    BUYBACK_SLOT_8              = 81,
    BUYBACK_SLOT_9              = 82,
    BUYBACK_SLOT_10             = 83,
    BUYBACK_SLOT_11             = 84,
    BUYBACK_SLOT_12             = 85,
    BUYBACK_SLOT_END            = 86
};

enum KeyRingSlots
{
    KEYRING_SLOT_START          = 86,
    KEYRING_SLOT_END            = 118
};

struct ItemPosCount
{
    ItemPosCount(uint16 _pos, uint8 _count) : pos(_pos), count(_count) {}
    bool isContainedIn(std::vector<ItemPosCount> const& vec) const;
    uint16 pos;
    uint8 count;
};
typedef std::vector<ItemPosCount> ItemPosCountVec;

enum TradeSlots
{
    TRADE_SLOT_COUNT            = 7,
    TRADE_SLOT_TRADED_COUNT     = 6,
    TRADE_SLOT_NONTRADED        = 6
};

enum TransferAbortReason
{
    TRANSFER_ABORT_NONE                     = 0x00,
#ifdef LICH_KING
    TRANSFER_ABORT_ERROR                    = 0x01,
    TRANSFER_ABORT_MAX_PLAYERS              = 0x02,         // Transfer Aborted: instance is full
    TRANSFER_ABORT_NOT_FOUND                = 0x03,         // Transfer Aborted: instance not found
    TRANSFER_ABORT_TOO_MANY_INSTANCES       = 0x04,         // You have entered too many instances recently.
    TRANSFER_ABORT_ZONE_IN_COMBAT           = 0x06,         // Unable to zone in while an encounter is in progress.
    TRANSFER_ABORT_INSUF_EXPAN_LVL          = 0x07,         // You must have <TBC, WotLK> expansion installed to access this area.
    TRANSFER_ABORT_DIFFICULTY               = 0x08,         // <Normal, Heroic, Epic> difficulty mode is not available for %s.
    TRANSFER_ABORT_UNIQUE_MESSAGE           = 0x09,         // Until you've escaped TLK's grasp, you cannot leave this place!
    TRANSFER_ABORT_TOO_MANY_REALM_INSTANCES = 0x0A,         // Additional instances cannot be launched, please try again later.
    TRANSFER_ABORT_NEED_GROUP               = 0x0B,         // 3.1
    TRANSFER_ABORT_NOT_FOUND1               = 0x0C,         // 3.1
    TRANSFER_ABORT_NOT_FOUND2               = 0x0D,         // 3.1
    TRANSFER_ABORT_NOT_FOUND3               = 0x0E,         // 3.2
    TRANSFER_ABORT_REALM_ONLY               = 0x0F,         // All players on party must be from the same realm.
    TRANSFER_ABORT_MAP_NOT_ALLOWED          = 0x10,         // Map can't be entered at this time.
#else
    TRANSFER_ABORT_MAX_PLAYERS              = 0x0001,       // Transfer Aborted: instance is full
    TRANSFER_ABORT_NOT_FOUND                = 0x0002,       // Transfer Aborted: instance not found
    TRANSFER_ABORT_TOO_MANY_INSTANCES       = 0x0003,       // You have entered too many instances recently.
    TRANSFER_ABORT_ZONE_IN_COMBAT           = 0x0005,       // Unable to zone in while an encounter is in progress.
    TRANSFER_ABORT_INSUF_EXPAN_LVL          = 0x0106,       // You must have TBC expansion installed to access this area.
    TRANSFER_ABORT_DIFFICULTY1              = 0x0007,       // Normal difficulty mode is not available for %s.
    TRANSFER_ABORT_DIFFICULTY2              = 0x0107,       // Heroic difficulty mode is not available for %s.
    TRANSFER_ABORT_DIFFICULTY3              = 0x0207,       // Epic difficulty mode is not available for %s.
#endif
};

enum InstanceResetWarningType
{
    RAID_INSTANCE_WARNING_HOURS     = 1,                    // WARNING! %s is scheduled to reset in %d hour(s).
    RAID_INSTANCE_WARNING_MIN       = 2,                    // WARNING! %s is scheduled to reset in %d minute(s)!
    RAID_INSTANCE_WARNING_MIN_SOON  = 3,                    // WARNING! %s is scheduled to reset in %d minute(s). Please exit the zone or you will be returned to your bind location!
    RAID_INSTANCE_WELCOME           = 4                     // Welcome to %s. This raid instance is scheduled to reset in %s.
};

class InstanceSave;

enum RestType
{
    REST_TYPE_NO        = 0,
    REST_TYPE_IN_TAVERN = 1,
    REST_TYPE_IN_CITY   = 2
};

enum TeleportToOptions
{
    TELE_TO_GM_MODE             = 0x01,
    TELE_TO_NOT_LEAVE_TRANSPORT = 0x02,
    TELE_TO_NOT_LEAVE_COMBAT    = 0x04,
    TELE_TO_NOT_UNSUMMON_PET    = 0x08,
    TELE_TO_SPELL               = 0x10,
    TELE_TO_TRANSPORT_TELEPORT  = 0x20,
    TELE_TO_TEST_MODE           = 0x40,
};

/// Type of environmental damages
enum EnviromentalDamage
{
    DAMAGE_EXHAUSTED = 0,
    DAMAGE_DROWNING  = 1,
    DAMAGE_FALL      = 2,
    DAMAGE_LAVA      = 3,
    DAMAGE_SLIME     = 4,
    DAMAGE_FIRE      = 5,
    DAMAGE_FALL_TO_VOID = 6                                 // custom case for fall without durability loss
};

enum PlayerChatTag
{
    CHAT_TAG_NONE       = 0x00,
    CHAT_TAG_AFK        = 0x01,
    CHAT_TAG_DND        = 0x02,
    CHAT_TAG_GM         = 0x04,
    CHAT_TAG_COM        = 0x08, // Commentator
    CHAT_TAG_DEV        = 0x10
};

// used at player loading query list preparing, and later result selection
enum PlayerLoginQueryIndex
{
    PLAYER_LOGIN_QUERY_LOADFROM                 = 0,
    PLAYER_LOGIN_QUERY_LOADGROUP                = 1,
    PLAYER_LOGIN_QUERY_LOADBOUNDINSTANCES       = 2,
    PLAYER_LOGIN_QUERY_LOADAURAS                = 3,
    PLAYER_LOGIN_QUERY_LOADSPELLS               = 4,
    PLAYER_LOGIN_QUERY_LOADQUESTSTATUS          = 5,
    PLAYER_LOGIN_QUERY_LOADDAILYQUESTSTATUS     = 6,
    PLAYER_LOGIN_QUERY_LOADREPUTATION           = 7,
    PLAYER_LOGIN_QUERY_LOADINVENTORY            = 8,
    PLAYER_LOGIN_QUERY_LOADACTIONS              = 9,
    PLAYER_LOGIN_QUERY_LOADMAILCOUNT            = 10,
    PLAYER_LOGIN_QUERY_LOADMAILDATE             = 11,
    PLAYER_LOGIN_QUERY_LOADSOCIALLIST           = 12,
    PLAYER_LOGIN_QUERY_LOADHOMEBIND             = 13,
    PLAYER_LOGIN_QUERY_LOADSPELLCOOLDOWNS       = 14,
    PLAYER_LOGIN_QUERY_LOADDECLINEDNAMES        = 15,
    PLAYER_LOGIN_QUERY_LOADGUILD                = 16,
    PLAYER_LOGIN_QUERY_LOADARENAINFO            = 17,
    PLAYER_LOGIN_QUERY_LOADSKILLS               = 18,
    PLAYER_LOGIN_QUERY_LOAD_BG_DATA             = 19,

    MAX_PLAYER_LOGIN_QUERY
};

enum PlayerDelayedOperations
{
    DELAYED_SAVE_PLAYER         = 0x01,
    DELAYED_RESURRECT_PLAYER    = 0x02,
    DELAYED_SPELL_CAST_DESERTER = 0x04,
    DELAYED_BG_MOUNT_RESTORE    = 0x08,                     ///< Flag to restore mount state after teleport from BG
    DELAYED_BG_TAXI_RESTORE     = 0x10,                     ///< Flag to restore taxi state after teleport from BG
    DELAYED_BG_GROUP_RESTORE    = 0x20,                     ///< Flag to restore group state after teleport from BG
    DELAYED_END
};

// Player summoning auto-decline time (in secs)
#define MAX_PLAYER_SUMMON_DELAY                   (2*MINUTE)
#define MAX_MONEY_AMOUNT                       (0x7FFFFFFF-1)

struct InstancePlayerBind
{
    InstanceSave *save;
    bool perm;
    /* permanent PlayerInstanceBinds are created in Raid/Heroic instances for players
       that aren't already permanently bound when they are inside when a boss is killed
       or when they enter an instance that the group leader is permanently bound to. */
    
    /*LK
    // extend state listing:
    //EXPIRED  - doesn't affect anything unless manually re-extended by player
    //NORMAL   - standard state
    //EXTENDED - won't be promoted to EXPIRED at next reset period, will instead be promoted to NORMAL
    //BindExtensionState extendState;
    */
    InstancePlayerBind() : save(nullptr), perm(false) {}
};

enum CharDeleteMethod
{
    CHAR_DELETE_REMOVE = 0,                      // Completely remove from the database
    CHAR_DELETE_UNLINK = 1                       // The character gets unlinked from the account,
                                                 // the name gets freed up and appears as deleted ingame
};

enum PlayerCommandStates
{
    CHEAT_NONE      = 0x00,
    CHEAT_GOD       = 0x01,
    CHEAT_CASTTIME  = 0x02,
    CHEAT_COOLDOWN  = 0x04,
    CHEAT_POWER     = 0x08,
    CHEAT_WATERWALK = 0x10
};

struct SpamReport
{
    time_t time;
    uint64 reporterGUID;
    std::string message;
};

typedef std::map<uint32, SpamReport> SpamReports;

struct Gladiator {
    Gladiator(uint32 guid, uint8 r) : playerguid(guid), rank(r) {}
    uint32 playerguid;
    uint8 rank;
};

class LoginQueryHolder : public SQLQueryHolder
{
private:
    uint32 m_accountId;
    uint64 m_guid;
public:
    LoginQueryHolder(uint32 accountId, uint64 guid)
        : m_accountId(accountId), m_guid(guid) { }
    uint64 GetGuid() const { return m_guid; }
    uint32 GetAccountId() const { return m_accountId; }
    bool Initialize();
};

#define MAX_GLADIATORS_RANK 3

/// Holder for Battleground data
struct BGData
{
    BGData() : bgInstanceID(0), bgTypeID(BATTLEGROUND_TYPE_NONE), bgAfkReportedCount(0), bgAfkReportedTimer(0),
        bgTeam(0), mountSpell(0) {
        ClearTaxiPath();
    }

    uint32 bgInstanceID;                    ///< This variable is set to bg->m_InstanceID,
                                            ///  when player is teleported to BG - (it is battleground's GUID)
    BattlegroundTypeId bgTypeID;

    std::set<uint32>   bgAfkReporter;
    uint8              bgAfkReportedCount;
    time_t             bgAfkReportedTimer;

    uint32 bgTeam;                          ///< What side the player will be added to

    uint32 mountSpell;
    uint32 taxiPath[2];

    WorldLocation joinPos;                  ///< From where player entered BG

    void ClearTaxiPath() { taxiPath[0] = taxiPath[1] = 0; }
    bool HasTaxiPath() const { return taxiPath[0] && taxiPath[1]; }
};

class TC_GAME_API Player : public Unit, public GridObject<Player>
{
    friend class WorldSession;
    friend void Item::AddToUpdateQueueOf(Player *player);
    friend void Item::RemoveFromUpdateQueueOf(Player *player);
    public:
        explicit Player (WorldSession *session);
        ~Player ( ) override;

        PlayerAI* AI() const { return reinterpret_cast<PlayerAI*>(i_AI); }

        void CleanupsBeforeDelete(bool finalCleanup = true) override;

        void AddToWorld() override;
        void RemoveFromWorld() override;

		void StopCastingCharm();
        void StopCastingBindSight();

        void SetTeleportingToTest(uint32 instanceId);
        uint32 GetTeleportingToTest() const { return m_teleportToTestInstanceId; }
        bool TeleportTo(uint32 mapid, float x, float y, float z, float orientation, uint32 options = 0);

        bool TeleportTo(WorldLocation const &loc, uint32 options = 0)
        {
            return TeleportTo(loc.m_mapId, loc.m_positionX, loc.m_positionY, loc.m_positionZ, loc.m_orientation, options);
        }

        void SetSummonPoint(uint32 mapid, float x, float y, float z)
        {
            m_summon_expire = time(nullptr) + MAX_PLAYER_SUMMON_DELAY;
            m_summon_mapid = mapid;
            m_summon_x = x;
            m_summon_y = y;
            m_summon_z = z;
            m_invite_summon = true;
        }
        void SummonIfPossible(bool agree);
        bool IsBeingInvitedForSummon() { return m_invite_summon; }
        void UpdateSummonExpireTime() { m_summon_expire = time(nullptr) + MAX_PLAYER_SUMMON_DELAY; }
        time_t GetSummonExpireTimer() const { return m_summon_expire; }

        bool Create(uint32 guidlow, CharacterCreateInfo* createInfo);
        bool Create( uint32 guidlow, const std::string& name, uint8 race, uint8 class_, uint8 gender, uint8 skin, uint8 face, uint8 hairStyle, uint8 hairColor, uint8 facialHair, uint8 outfitId);
        virtual void SetMapAtCreation(PlayerInfo const* info);

        void Update( uint32 time ) override;

        static bool BuildEnumData( PreparedQueryResult  result,  WorldPacket * p_data, WorldSession const* session );

        bool IsImmunedToSpellEffect(SpellInfo const* spellInfo, uint32 index, Unit* caster) const override;

        void SetInWater(bool apply);

        bool IsInWater() const override { return m_isInWater; }
        bool IsUnderWater() const override;

        void SendInitialPacketsBeforeAddToMap();
        void SendInitialPacketsAfterAddToMap();
        void SendSupercededSpell(uint32 oldSpell, uint32 newSpell) const;
        void SendTransferAborted(uint32 mapid, uint16 reason);
        void SendInstanceResetWarning(uint32 mapid, uint32 time);

        bool CanInteractWithQuestGiver(Object* questGiver);
        Creature* GetNPCIfCanInteractWith(uint64 guid, uint32 npcflagmask = UNIT_NPC_FLAG_NONE);
        GameObject* GetGameObjectIfCanInteractWith(uint64 guid) const;
        GameObject* GetGameObjectIfCanInteractWith(uint64 guid, GameobjectTypes type) const;

        void ToggleAFK();
        void ToggleDND();
        bool IsAFK() const { return HasFlag(PLAYER_FLAGS,PLAYER_FLAGS_AFK); };
        bool IsDND() const { return HasFlag(PLAYER_FLAGS,PLAYER_FLAGS_DND); };
        uint8 GetChatTag() const;
        std::string afkMsg;
        std::string dndMsg;

        PlayerSocial *GetSocial() { return m_social; }

        PlayerTaxi m_taxi;
        void InitTaxiNodesForLevel();
        bool ActivateTaxiPathTo(std::vector<uint32> const& nodes, Creature* npc = nullptr, uint32 spellid = 0);
        bool ActivateTaxiPathTo(uint32 taxi_path_id, uint32 spellid = 0);
        void CleanupAfterTaxiFlight();
        void ContinueTaxiFlight();
                                                            // mount_id can be used in scripting calls
        bool IsAcceptWhispers() const { return m_ExtraFlags & PLAYER_EXTRA_ACCEPT_WHISPERS; }
        void SetAcceptWhispers(bool on) { if(on) m_ExtraFlags |= PLAYER_EXTRA_ACCEPT_WHISPERS; else m_ExtraFlags &= ~PLAYER_EXTRA_ACCEPT_WHISPERS; }
        bool IsGameMaster() const { return m_ExtraFlags & PLAYER_EXTRA_GM_ON; }
        void SetGameMaster(bool on);
        bool IsGMChat() const { return GetSession()->GetSecurity() >= SEC_GAMEMASTER1 && (m_ExtraFlags & PLAYER_EXTRA_GM_CHAT); }
        void SetGMChat(bool on) { if(on) m_ExtraFlags |= PLAYER_EXTRA_GM_CHAT; else m_ExtraFlags &= ~PLAYER_EXTRA_GM_CHAT; }
        bool IsTaxiCheater() const { return m_ExtraFlags & PLAYER_EXTRA_TAXICHEAT; }
        void SetTaxiCheater(bool on) { if(on) m_ExtraFlags |= PLAYER_EXTRA_TAXICHEAT; else m_ExtraFlags &= ~PLAYER_EXTRA_TAXICHEAT; }
        bool IsGMVisible() const { return !(m_ExtraFlags & PLAYER_EXTRA_GM_INVISIBLE); }
        void SetGMVisible(bool on);
        void SetPvPDeath(bool on) { if(on) m_ExtraFlags |= PLAYER_EXTRA_PVP_DEATH; else m_ExtraFlags &= ~PLAYER_EXTRA_PVP_DEATH; }
        bool IsInDuelArea() const;
        void SetDuelArea(bool on) { if(on) m_ExtraFlags |= PLAYER_EXTRA_DUEL_AREA; else m_ExtraFlags &= ~PLAYER_EXTRA_DUEL_AREA; }

        void GiveXP(uint32 xp, Unit* victim);
        void GiveLevel(uint32 level);
        void InitStatsForLevel(bool reapplyMods = false);

        // .cheat command related
        bool GetCommandStatus(uint32 command) const { return _activeCheats & command; }
        void SetCommandStatusOn(uint32 command)     { _activeCheats |= command; }
        void SetCommandStatusOff(uint32 command)    { _activeCheats &= ~command; }

        // Played Time Stuff
        time_t m_logintime;
        time_t m_Last_tick;
        uint32 m_Played_time[2];
        uint32 GetTotalPlayedTime() { return m_Played_time[0]; };
        uint32 GetLevelPlayedTime() { return m_Played_time[1]; };
        uint32 GetTotalAccountPlayedTime();

        void SetDeathState(DeathState s) override;                   // overwrite Unit::setDeathState

        void InnEnter (int time,uint32 mapid, float x,float y,float z)
        {
            inn_pos_mapid = mapid;
            inn_pos_x = x;
            inn_pos_y = y;
            inn_pos_z = z;
            time_inn_enter = time;
        };

        float GetRestBonus() const { return m_rest_bonus; };
        void SetRestBonus(float rest_bonus_new);

        RestType GetRestType() const { return rest_type; };
        void SetRestType(RestType n_r_type) { rest_type = n_r_type; };

        uint32 GetInnPosMapId() const { return inn_pos_mapid; };
        float GetInnPosX() const { return inn_pos_x; };
        float GetInnPosY() const { return inn_pos_y; };
        float GetInnPosZ() const { return inn_pos_z; };

        int GetTimeInnEnter() const { return time_inn_enter; };
        void UpdateInnerTime (int time) { time_inn_enter = time; };

        Pet* GetPet() const;
        Pet* SummonPet(uint32 entry, float x, float y, float z, float ang, PetType petType, uint32 despwtime);
        void RemovePet(Pet* pet, PetSaveMode mode, bool returnreagent = false, RemovePetReason reason = REMOVE_PET_REASON_OTHER);

        /// Handles said message in regular chat based on declared language and in config pre-defined Range.
        void Say(std::string const& text, Language language, WorldObject const* = nullptr) override;
        /// Handles yelled message in regular chat based on declared language and in config pre-defined Range.
        void Yell(std::string const& text, Language language, WorldObject const* = nullptr) override;
        /// Outputs an universal text which is supposed to be an action.
        void TextEmote(std::string const& text, WorldObject const* = nullptr, bool = false) override;
        /// Handles whispers from Addons and players based on sender, receiver's guid and language.
        void Whisper(std::string const& text, Language language, Player* receiver, bool = false) override;

        /*********************************************************/
        /***                    STORAGE SYSTEM                 ***/
        /*********************************************************/

        void SetVirtualItemSlot( uint8 i, Item* item);
        void SetSheath(SheathState sheathed ) override;
        uint8 FindEquipSlot( ItemTemplate const* proto, uint32 slot, bool swap ) const;
        uint32 GetItemCount( uint32 item, bool inBankAlso = false, Item* skipItem = nullptr ) const;
        Item* GetItemByGuid( uint64 guid ) const;
        Item* GetItemByPos( uint16 pos ) const;
        Item* GetItemByPos( uint8 bag, uint8 slot ) const;
        //Does additional check for disarmed weapons
        Item* GetUseableItemByPos(uint8 bag, uint8 slot) const;
        Item* GetWeaponForAttack(WeaponAttackType attackType, bool useable = false) const;
        bool HasMainWeapon() const override;
        Item* GetShield(bool useable = false) const;
        static WeaponAttackType GetAttackBySlot( uint8 slot );        // MAX_ATTACK if not weapon slot
        std::vector<Item *> &GetItemUpdateQueue() { return m_itemUpdateQueue; }
        static bool IsInventoryPos( uint16 pos ) { return IsInventoryPos(pos >> 8,pos & 255); }
        static bool IsInventoryPos( uint8 bag, uint8 slot );
        static bool IsEquipmentPos( uint16 pos ) { return IsEquipmentPos(pos >> 8,pos & 255); }
        static bool IsEquipmentPos( uint8 bag, uint8 slot );
        static bool IsBagPos( uint16 pos );
        static bool IsBankPos( uint16 pos ) { return IsBankPos(pos >> 8,pos & 255); }
        static bool IsBankPos( uint8 bag, uint8 slot );
        bool IsValidPos( uint16 pos ) { return IsBankPos(pos >> 8,pos & 255); }
        bool IsValidPos( uint8 bag, uint8 slot ) const;
        bool HasBankBagSlot( uint8 slot ) const;
        bool HasItemCount( uint32 item, uint32 count, bool inBankAlso = false ) const;
        uint32 GetEmptyBagSlotsCount() const;
        bool HasItemFitToSpellRequirements(SpellInfo const* spellInfo, Item const* ignoreItem = nullptr) const;
        Item* GetItemOrItemWithGemEquipped( uint32 item ) const;
        InventoryResult CanTakeMoreSimilarItems(Item* pItem) const { return _CanTakeMoreSimilarItems(pItem->GetEntry(),pItem->GetCount(),pItem); }
        InventoryResult CanTakeMoreSimilarItems(uint32 entry, uint32 count) const { return _CanTakeMoreSimilarItems(entry,count,nullptr); }
        InventoryResult CanStoreNewItem( uint8 bag, uint8 slot, ItemPosCountVec& dest, uint32 item, uint32 count, uint32* no_space_count = nullptr, ItemTemplate const* proto = nullptr ) const
        {
            return _CanStoreItem(bag, slot, dest, item, count, nullptr, false, no_space_count, proto );
        }
        InventoryResult CanStoreItem( uint8 bag, uint8 slot, ItemPosCountVec& dest, Item *pItem, bool swap = false ) const
        {
            if(!pItem)
                return EQUIP_ERR_ITEM_NOT_FOUND;
            uint32 count = pItem->GetCount();
            return _CanStoreItem( bag, slot, dest, pItem->GetEntry(), count, pItem, swap, nullptr, pItem ? pItem->GetTemplate() : nullptr );

        }
        InventoryResult CanStoreItems( std::vector<Item*> const& items, uint32 count) const;
        InventoryResult CanEquipNewItem( uint8 slot, uint16 &dest, uint32 item, bool swap, ItemTemplate const *proto = nullptr ) const;
        InventoryResult CanEquipItem( uint8 slot, uint16 &dest, Item *pItem, bool swap, bool not_loading = true ) const;
        InventoryResult CanUnequipItems( uint32 item, uint32 count ) const;
        InventoryResult CanUnequipItem( uint16 src, bool swap ) const;
        InventoryResult CanBankItem( uint8 bag, uint8 slot, ItemPosCountVec& dest, Item *pItem, bool swap, bool not_loading = true ) const;
        InventoryResult CanUseItem( Item *pItem, bool not_loading = true ) const;
        bool HasItemTotemCategory( uint32 TotemCategory ) const;
        bool CanUseItem( ItemTemplate const *pItem );
        InventoryResult CanUseAmmo( uint32 item ) const;
        Item* StoreNewItem( ItemPosCountVec const& pos, uint32 item, bool update,int32 randomPropertyId = 0, ItemTemplate const *proto = nullptr );
        Item* StoreItem( ItemPosCountVec const& pos, Item *pItem, bool update );
        Item* EquipNewItem( uint16 pos, uint32 item, bool update, ItemTemplate const *proto = nullptr );
        Item* EquipItem( uint16 pos, Item *pItem, bool update );
        void AutoUnequipOffhandIfNeed();
        bool StoreNewItemInBestSlots(uint32 item_id, uint32 item_count, ItemTemplate const *proto = nullptr);
        uint32 GetEquipedItemsLevelSum();
        //Add item count to player, return pointer to item in variable item
        Item* AddItem(uint32 itemId, uint32 count);

        InventoryResult _CanTakeMoreSimilarItems(uint32 entry, uint32 count, Item* pItem, uint32* no_space_count = nullptr) const;
        InventoryResult _CanStoreItem( uint8 bag, uint8 slot, ItemPosCountVec& dest, uint32 entry, uint32 count, Item *pItem = nullptr, bool swap = false, uint32* no_space_count = nullptr, ItemTemplate const* proto = nullptr ) const;

        void ApplyEquipCooldown( Item * pItem );
        void SetAmmo( uint32 item );
        void RemoveAmmo();
        float GetAmmoDPS() const { return m_ammoDPS; }
        bool CheckAmmoCompatibility(const ItemTemplate *ammo_proto) const;
        void QuickEquipItem( uint16 pos, Item *pItem);
        void VisualizeItem( uint8 slot, Item *pItem);
        void SetVisibleItemSlot(uint8 slot, Item *pItem);
        Item* BankItem( ItemPosCountVec const& dest, Item *pItem, bool update )
        {
            return StoreItem( dest, pItem, update);
        }
        Item* BankItem( uint16 pos, Item *pItem, bool update );
        void RemoveItem( uint8 bag, uint8 slot, bool update );
        void MoveItemFromInventory(uint8 bag, uint8 slot, bool update);
                                                            // in trade, auction, guild bank, mail....
        void MoveItemToInventory(ItemPosCountVec const& dest, Item* pItem, bool update, bool in_characterInventoryDB = false);
                                                            // in trade, guild bank, mail....
        void RemoveItemDependentAurasAndCasts(Item* pItem);
        void DestroyItem( uint8 bag, uint8 slot, bool update );
        void DestroyItemCount( uint32 item, uint32 count, bool update, bool unequip_check = false, bool inBankAlso = false);
        void DestroyItemCount( Item* item, uint32& count, bool update );
        void SwapItems(uint32 item1, uint32 item2);
        void DestroyConjuredItems( bool update );
        void DestroyZoneLimitedItem( bool update, uint32 new_zone );
        void SplitItem( uint16 src, uint16 dst, uint32 count );
        void SwapItem( uint16 src, uint16 dst );
        void AddItemToBuyBackSlot( Item *pItem );
        Item* GetItemFromBuyBackSlot( uint32 slot );
        void RemoveItemFromBuyBackSlot( uint32 slot, bool del );
        uint32 GetMaxKeyringSize() const { return KEYRING_SLOT_END-KEYRING_SLOT_START; }
        void SendEquipError( uint8 msg, Item* pItem, Item *pItem2 );
        void SendBuyError( uint8 msg, Creature* pCreature, uint32 item, uint32 param );
        void SendSellError( uint8 msg, Creature* pCreature, uint64 guid, uint32 param );
        void AddWeaponProficiency(uint32 newflag) { m_WeaponProficiency |= newflag; }
        void AddArmorProficiency(uint32 newflag) { m_ArmorProficiency |= newflag; }
        uint32 GetWeaponProficiency() const { return m_WeaponProficiency; }
        uint32 GetArmorProficiency() const { return m_ArmorProficiency; }
        bool IsInFeralForm() const { return m_form == FORM_CAT || m_form == FORM_BEAR || m_form == FORM_DIREBEAR; }
        bool IsUseEquipedWeapon( bool mainhand ) const
        {
            // disarm applied only to mainhand weapon
            return !IsInFeralForm() && (!mainhand || !HasFlag(UNIT_FIELD_FLAGS,UNIT_FLAG_DISARMED) );
        }
        void SendNewItem( Item *item, uint32 count, bool received, bool created, bool broadcast = false, bool sendChatMessage = true);
        bool BuyItemFromVendor(uint64 vendorguid, uint32 item, uint8 count, uint64 bagguid, uint8 slot);

        float GetReputationPriceDiscount( Creature const* pCreature ) const;
        float GetReputationPriceDiscount(FactionTemplateEntry const* factionTemplate) const;
        Player* GetTrader() const { return pTrader; }
        void ClearTrade();
        void TradeCancel(bool sendback);
        uint16 GetItemPosByTradeSlot(uint32 slot) const { return tradeItems[slot]; }

        CinematicMgr* GetCinematicMgr() const { return _cinematicMgr; }

        void UpdateEnchantTime(uint32 time);
        void UpdateItemDuration(uint32 time, bool realtimeonly=false);
        void AddEnchantmentDurations(Item *item);
        void RemoveEnchantmentDurations(Item *item);
        void RemoveAllEnchantments(EnchantmentSlot slot, bool arena);
        void RemoveAllCurrentPetAuras();
        void AddEnchantmentDuration(Item *item,EnchantmentSlot slot,uint32 duration);
        void ApplyEnchantment(Item *item,EnchantmentSlot slot,bool apply, bool apply_dur = true, bool ignore_condition = false);
        void ApplyEnchantment(Item *item,bool apply);
        void SendEnchantmentDurations();
        void AddItemDurations(Item *item);
        void RemoveItemDurations(Item *item);
        void SendItemDurations();
		void LoadCorpse(PreparedQueryResult result);
        void LoadPet();

        uint32 m_stableSlots;

        /*********************************************************/
        /***                    QUEST SYSTEM                   ***/
        /*********************************************************/

        void PrepareQuestMenu( uint64 guid );
        void SendPreparedQuest( uint64 guid );
        bool IsActiveQuest( uint32 quest_id ) const;
        Quest const *GetNextQuest( uint64 guid, Quest const *pQuest );
        bool CanSeeStartQuest( Quest const *pQuest );
        bool CanTakeQuest( Quest const *pQuest, bool msg );
        bool CanAddQuest( Quest const *pQuest, bool msg );
        bool CanCompleteQuest( uint32 quest_id );
        bool CanCompleteRepeatableQuest(Quest const *pQuest);
        bool CanRewardQuest( Quest const *pQuest, bool msg );
        bool CanRewardQuest( Quest const *pQuest, uint32 reward, bool msg );
        void AddQuestAndCheckCompletion(Quest const* quest, Object* questGiver);
        void AddQuest( Quest const *pQuest, Object *questGiver );
        void CompleteQuest( uint32 quest_id );
        void IncompleteQuest( uint32 quest_id );
        void RewardQuest( Quest const *pQuest, uint32 reward, Object* questGiver, bool announce = true );
        void FailQuest( uint32 quest_id );
        void FailTimedQuest( uint32 quest_id );
        bool SatisfyQuestSkillOrClass( Quest const* qInfo, bool msg );
        bool SatisfyQuestLevel( Quest const* qInfo, bool msg );
        bool SatisfyQuestLog( bool msg );
        bool SatisfyQuestPreviousQuest( Quest const* qInfo, bool msg );
        bool SatisfyQuestRace( Quest const* qInfo, bool msg );
        bool SatisfyQuestReputation( Quest const* qInfo, bool msg );
        bool SatisfyQuestStatus( Quest const* qInfo, bool msg );
        bool SatisfyQuestConditions(Quest const* qInfo, bool msg);
        bool SatisfyQuestTimed( Quest const* qInfo, bool msg );
        bool SatisfyQuestExclusiveGroup( Quest const* qInfo, bool msg );
        bool SatisfyQuestNextChain( Quest const* qInfo, bool msg );
        bool SatisfyQuestPrevChain( Quest const* qInfo, bool msg );
        bool SatisfyQuestDay( Quest const* qInfo, bool msg );
        bool GiveQuestSourceItem( Quest const *pQuest );
        bool TakeQuestSourceItem( uint32 quest_id, bool msg );
        bool GetQuestRewardStatus( uint32 quest_id ) const;
        QuestStatus GetQuestStatus( uint32 quest_id ) const;
        void SetQuestStatus( uint32 quest_id, QuestStatus status );
        void AutoCompleteQuest( Quest const* qInfo );
        QuestGiverStatus GetQuestDialogStatus(Object* questGiver);

        void SetDailyQuestStatus( uint32 quest_id );
        void ResetDailyQuestStatus();
        bool IsDailyQuestDone(uint32 quest_id) const;

        uint16 FindQuestSlot( uint32 quest_id ) const;
        uint32 GetQuestSlotQuestId(uint16 slot) const { return GetUInt32Value(PLAYER_QUEST_LOG_1_1 + slot*MAX_QUEST_OFFSET + QUEST_ID_OFFSET); }
        uint32 GetQuestSlotState(uint16 slot)   const { return GetUInt32Value(PLAYER_QUEST_LOG_1_1 + slot*MAX_QUEST_OFFSET + QUEST_STATE_OFFSET); }
        uint32 GetQuestSlotCounters(uint16 slot)const { return GetUInt32Value(PLAYER_QUEST_LOG_1_1 + slot*MAX_QUEST_OFFSET + QUEST_COUNTS_OFFSET); }
        uint8 GetQuestSlotCounter(uint16 slot,uint8 counter) const { return GetByteValue(PLAYER_QUEST_LOG_1_1 + slot*MAX_QUEST_OFFSET + QUEST_COUNTS_OFFSET,counter); }
        uint32 GetQuestSlotTime(uint16 slot)    const { return GetUInt32Value(PLAYER_QUEST_LOG_1_1 + slot*MAX_QUEST_OFFSET + QUEST_TIME_OFFSET); }
        void SetQuestSlot(uint16 slot,uint32 quest_id, uint32 timer = 0)
        {
            SetUInt32Value(PLAYER_QUEST_LOG_1_1 + slot*MAX_QUEST_OFFSET + QUEST_ID_OFFSET,quest_id);
            SetUInt32Value(PLAYER_QUEST_LOG_1_1 + slot*MAX_QUEST_OFFSET + QUEST_STATE_OFFSET,0);
            SetUInt32Value(PLAYER_QUEST_LOG_1_1 + slot*MAX_QUEST_OFFSET + QUEST_COUNTS_OFFSET,0);
            SetUInt32Value(PLAYER_QUEST_LOG_1_1 + slot*MAX_QUEST_OFFSET + QUEST_TIME_OFFSET,timer);
        }
        void SetQuestSlotCounter(uint16 slot,uint8 counter,uint8 count) { SetByteValue(PLAYER_QUEST_LOG_1_1 + slot*MAX_QUEST_OFFSET + QUEST_COUNTS_OFFSET,counter,count); }
        void SetQuestSlotState(uint16 slot,uint32 state) { SetFlag(PLAYER_QUEST_LOG_1_1 + slot*MAX_QUEST_OFFSET + QUEST_STATE_OFFSET,state); }
        void RemoveQuestSlotState(uint16 slot,uint32 state) { RemoveFlag(PLAYER_QUEST_LOG_1_1 + slot*MAX_QUEST_OFFSET + QUEST_STATE_OFFSET,state); }
        void SetQuestSlotTimer(uint16 slot,uint32 timer) { SetUInt32Value(PLAYER_QUEST_LOG_1_1 + slot*MAX_QUEST_OFFSET + QUEST_TIME_OFFSET,timer); }
        void SwapQuestSlot(uint16 slot1,uint16 slot2)
        {
            for (int i = 0; i < MAX_QUEST_OFFSET ; ++i )
            {
                uint32 temp1 = GetUInt32Value(PLAYER_QUEST_LOG_1_1 + MAX_QUEST_OFFSET *slot1 + i);
                uint32 temp2 = GetUInt32Value(PLAYER_QUEST_LOG_1_1 + MAX_QUEST_OFFSET *slot2 + i);

                SetUInt32Value(PLAYER_QUEST_LOG_1_1 + MAX_QUEST_OFFSET *slot1 + i, temp2);
                SetUInt32Value(PLAYER_QUEST_LOG_1_1 + MAX_QUEST_OFFSET *slot2 + i, temp1);
            }
        }
        uint32 GetReqKillOrCastCurrentCount(uint32 quest_id, int32 entry);
        void AdjustQuestRequiredItemCount( Quest const* pQuest );
        void AreaExploredOrEventHappens( uint32 questId );
        void GroupEventHappens( uint32 questId, WorldObject const* pEventObject );
        void ItemAddedQuestCheck( uint32 entry, uint32 count );
        void ItemRemovedQuestCheck( uint32 entry, uint32 count );
        void KilledMonsterCredit(uint32 entry, uint64 guid, uint32 questId = 0);
        void ActivatedGO(uint32 entry, uint64 guid);
        void CastedCreatureOrGO( uint32 entry, uint64 guid, uint32 spell_id );
        void TalkedToCreature( uint32 entry, uint64 guid );
        void MoneyChanged( uint32 value );
        bool HasQuestForItem( uint32 itemid ) const;
        bool HasQuestForGO(int32 GOId);
        void UpdateForQuestWorldObjects();
        bool CanShareQuest(uint32 quest_id) const;

        void SendQuestComplete( uint32 quest_id );
        void SendQuestReward( Quest const *pQuest, uint32 XP, Object* questGiver );
        void SendQuestFailed( uint32 quest_id );
        void SendQuestTimerFailed( uint32 quest_id );
        void SendCanTakeQuestResponse( uint32 msg );
        void SendQuestConfirmAccept(Quest const* pQuest, Player* pReceiver);
        void SendPushToPartyResponse( Player *pPlayer, uint32 msg );
        void SendQuestUpdateAddItem( Quest const* pQuest, uint32 item_idx, uint32 count );
        void SendQuestUpdateAddCreatureOrGo( Quest const* pQuest, uint64 guid, uint32 creatureOrGO_idx, uint32 old_count, uint32 add_count );

        uint64 GetDivider() { return m_divider; };
        void SetDivider( uint64 guid ) { m_divider = guid; };

        uint32 GetInGameTime() { return m_ingametime; };

        void SetInGameTime( uint32 time ) { m_ingametime = time; };

        void AddTimedQuest( uint32 quest_id ) { m_timedquests.insert(quest_id); }

        /*********************************************************/
        /***                   LOAD SYSTEM                     ***/
        /*********************************************************/

        bool LoadFromDB(uint32 guid, SQLQueryHolder *holder);
        static bool   LoadValuesArrayFromDB(Tokens& data,uint64 guid);
        static uint32 GetUInt32ValueFromArray(Tokens const& data, uint16 index);
        static float  GetFloatValueFromArray(Tokens const& data, uint16 index);
        static uint32 GetUInt32ValueFromDB(uint16 index, uint64 guid);
        static float  GetFloatValueFromDB(uint16 index, uint64 guid);
        static uint32 GetZoneIdFromDB(uint64 guid);
        static uint32 GetLevelFromStorage(uint64 guid);
        static bool   LoadPositionFromDB(uint32& mapid, float& x,float& y,float& z,float& o, bool& in_flight, uint64 guid);

        static bool IsValidGender(uint8 Gender) { return Gender <= GENDER_FEMALE; }

        /*********************************************************/
        /***                   SAVE SYSTEM                     ***/
        /*********************************************************/

        virtual void SaveToDB(bool create = false);
        virtual void SaveInventoryAndGoldToDB(SQLTransaction trans);                    // fast save function for item/money cheating preventing
        virtual void SaveGoldToDB(SQLTransaction trans);
        virtual void SaveDataFieldToDB();
        static bool SaveValuesArrayInDB(Tokens const& data,uint64 guid);
        static void SetUInt32ValueInArray(Tokens& data,uint16 index, uint32 value);
        static void SetFloatValueInArray(Tokens& data,uint16 index, float value);
        static void SetUInt32ValueInDB(uint16 index, uint32 value, uint64 guid);
        static void SetFloatValueInDB(uint16 index, float value, uint64 guid);
        static void SavePositionInDB(uint32 mapid, float x,float y,float z,float o,uint32 zone,uint64 guid);

        bool m_mailsLoaded;
        bool m_mailsUpdated;

        void SetBindPoint(uint64 guid);
        void SendTalentWipeConfirm(uint64 guid);
        void RewardRage( uint32 damage, uint32 weaponSpeedHitFactor, bool attacker );
        void SendPetSkillWipeConfirm();
        //void CalcRage( uint32 damage,bool attacker );
        void RegenerateAll();
        void Regenerate(Powers power);
        void RegenerateHealth();
        void ResetAllPowers();
        void setRegenTimer(uint32 time) {m_regenTimer = time;}
        void setWeaponChangeTimer(uint32 time) {m_weaponChangeTimer = time;}

        void UpdateWeaponDependentCritAuras(WeaponAttackType attackType);
        void UpdateAllWeaponDependentCritAuras();

        void UpdateWeaponDependentAuras(WeaponAttackType attackType);
        void ApplyItemDependentAuras(Item* item, bool apply);

        bool CheckAttackFitToAuraRequirement(WeaponAttackType attackType, AuraEffect const* aurEff) const override;

        uint32 GetMoney() const;
        bool ModifyMoney(int32 amount, bool sendError = true);
        void SetMoney( uint32 value );
        bool HasEnoughMoney(int32 amount) const;
        
        QuestStatusMap& getQuestStatusMap() { return m_QuestStatus; }

        bool IsQuestRewarded(uint32 quest_id) const;

        Unit* GetSelectedUnit() const;
        Player* GetSelectedPlayer() const;

        void SetTarget(uint64 /*guid*/) override { } /// Used for serverside target changes, does not apply to players
        void SetSelection(uint64 guid) { SetUInt64Value(UNIT_FIELD_TARGET, guid); }

        uint8 GetComboPoints() { return m_comboPoints; }
        uint8 GetComboPoints(Unit const* who = nullptr) const { return (who && m_comboTarget != who->GetGUID()) ? 0 : m_comboPoints; }
        uint64 GetComboTarget() { return m_comboTarget; }

        void AddComboPoints(Unit* target, int8 count, bool forceCurrent = false);
        void ClearComboPoints(uint32 spellId = 0);
        void SendComboPoints();

        void SendMailResult(uint32 mailId, uint32 mailAction, uint32 mailError, uint32 equipError = 0, uint32 item_guid = 0, uint32 item_count = 0);
        void SendNewMail();
        void UpdateNextMailTimeAndUnreads();
        void AddNewMailDeliverTime(time_t deliver_time);
        bool IsMailsLoaded() const { return m_mailsLoaded; }

        //void SetMail(Mail *m);
        void RemoveMail(uint32 id);

        void AddMail(Mail* mail) { m_mail.push_front(mail);}// for call from WorldSession::SendMailTo
        uint32 GetMailSize() { return m_mail.size();};
        Mail* GetMail(uint32 id);

        PlayerMails::iterator GetmailBegin() { return m_mail.begin();};
        PlayerMails::iterator GetmailEnd() { return m_mail.end();};

        /*********************************************************/
        /*** MAILED ITEMS SYSTEM ***/
        /*********************************************************/

        uint8 unReadMails;
        time_t m_nextMailDelivereTime;

        typedef std::unordered_map<uint32, Item*> ItemMap;

        ItemMap mMitems;                                    //template defined in objectmgr.cpp

        Item* GetMItem(uint32 id)
        {
            ItemMap::const_iterator itr = mMitems.find(id);
            if (itr != mMitems.end())
                return itr->second;

            return nullptr;
        }

        void AddMItem(Item* it)
        {
            ASSERT( it );
            //assert deleted, because items can be added before loading
            mMitems[it->GetGUIDLow()] = it;
        }

        bool RemoveMItem(uint32 id)
        {
            auto i = mMitems.find(id);
            if (i == mMitems.end())
                return false;

            mMitems.erase(i);
            return true;
        }

        void PetSpellInitialize();
        void CharmSpellInitialize();
        void PossessSpellInitialize();
        void SendRemoveControlBar() const;
        bool HasSpell(uint32 spell) const override;
        bool HasSpellButDisabled(uint32 spell) const;
        TrainerSpellState GetTrainerSpellState(TrainerSpell const* trainer_spell) const;
        bool IsSpellFitByClassAndRace( uint32 spell_id ) const;
        bool HandlePassiveSpellLearn(SpellInfo const* spellInfo);

        void SendProficiency(uint8 pr1, uint32 pr2);
        void SendInitialSpells();
        bool AddSpell(uint32 spell_id, bool active, bool learning = true, bool dependent = false, bool disabled = false, bool loading = false, uint32 fromSkill = 0 );
        void LearnSpell(uint32 spell_id, bool dependent, uint32 fromSkill = 0);
        void RemoveSpell(uint32 spell_id, bool disabled = false);
        void resetSpells();
        void LearnDefaultSkills();
        void LearnDefaultSkill(uint32 skillId, uint16 rank);
        //Old mangos/windrunner logic. Replaced by LearnDefaultSkills except for some rare spells (16 at time of writing). We'll keep this function for now
        void LearnDefaultSpells(bool loading = false); 
        void learnQuestRewardedSpells();
        void learnQuestRewardedSpells(Quest const* quest);
        void LearnAllClassSpells();
        void LearnAllClassProficiencies();

        void DoPack58(uint8 step);

        uint32 GetFreeTalentPoints() const { return GetUInt32Value(PLAYER_CHARACTER_POINTS1); }
        void SetFreeTalentPoints(uint32 points) { SetUInt32Value(PLAYER_CHARACTER_POINTS1,points); }
        bool ResetTalents(bool no_cost = false);
        uint32 ResetTalentsCost() const;
        void InitTalentForLevel();

        uint32 GetFreePrimaryProffesionPoints() const { return GetUInt32Value(PLAYER_CHARACTER_POINTS2); }
        void SetFreePrimaryProffesions(uint16 profs) { SetUInt32Value(PLAYER_CHARACTER_POINTS2,profs); }
        void InitPrimaryProffesions();

        PlayerSpellMap const& GetSpellMap() const { return m_spells; }
        PlayerSpellMap      & GetSpellMap()       { return m_spells; }

        //delete mod pointer on unapply
        void AddSpellMod(SpellModifier*& mod, bool apply);
        int32 GetTotalFlatMods(uint32 spellId, SpellModOp op);
        int32 GetTotalPctMods(uint32 spellId, SpellModOp op);
        bool IsAffectedBySpellmod(SpellInfo const *spellInfo, SpellModifier *mod, Spell const* spell = nullptr);
        template <class T> void ApplySpellMod(uint32 spellId, SpellModOp op, T &basevalue, Spell const* spell = nullptr);
        void RemoveSpellMods(Spell const* spell);
        void RestoreSpellMods(Spell const* spell);

        bool HasSpellCooldown(uint32 spell_id) const
        {
            auto itr = m_spellCooldowns.find(spell_id);
            return itr != m_spellCooldowns.end() && itr->second.end > time(nullptr);
        }
        uint32 GetSpellCooldownDelay(uint32 spell_id) const
        {
            auto itr = m_spellCooldowns.find(spell_id);
            time_t t = time(nullptr);
            return itr != m_spellCooldowns.end() && itr->second.end > t ? itr->second.end - t : 0;
        }
        static uint32 const infinityCooldownDelay = MONTH;  // used for set "infinity cooldowns" for spells and check
        static uint32 const infinityCooldownDelayCheck = MONTH/2;

        void AddSpellCooldown(uint32 spell_id, uint32 itemid, time_t end_time);
        void AddSpellAndCategoryCooldowns(SpellInfo const* spellInfo, uint32 itemId, Spell* spell = nullptr, bool infinityCooldown = false);
        void SendCooldownEvent(SpellInfo const *spellInfo, uint32 itemId = 0, Spell* spell = nullptr, bool setCooldown = true);
        void ProhibitSpellSchool(SpellSchoolMask idSchoolMask, uint32 unTimeMs) override;
        void RemoveSpellCooldown(uint32 spell_id, bool update = false);
        void RemoveArenaSpellCooldowns();
        void RemoveAllSpellCooldown();
        void SendClearCooldown(uint32 spell_id, Unit* target);

        void _LoadSpellCooldowns(QueryResult result);
        void _SaveSpellCooldowns(SQLTransaction trans);

        // global cooldown
        void AddGlobalCooldown(SpellInfo const *spellInfo, Spell const *spell, bool allowTinyCd = false);
        bool HasGlobalCooldown(SpellInfo const *spellInfo) const;
        void RemoveGlobalCooldown(SpellInfo const *spellInfo);

        void setResurrectRequestData(uint64 guid, uint32 mapId, float X, float Y, float Z, uint32 health, uint32 mana)
        {
            m_resurrectGUID = guid;
            m_resurrectMap = mapId;
            m_resurrectX = X;
            m_resurrectY = Y;
            m_resurrectZ = Z;
            m_resurrectHealth = health;
            m_resurrectMana = mana;
        };
        void clearResurrectRequestData() { setResurrectRequestData(0,0,0.0f,0.0f,0.0f,0,0); }
        bool isRessurectRequestedBy(uint64 guid) const { return m_resurrectGUID != 0 && m_resurrectGUID == guid; }
        bool isRessurectRequested() const { return m_resurrectGUID != 0; }
        void RessurectUsingRequestData();

        int getCinematic()
        {
            return m_cinematic;
        }
        void setCinematic(int cine)
        {
            m_cinematic = cine;
        }

        void addActionButton(uint8 button, uint16 action, uint8 type, uint8 misc);
        void removeActionButton(uint8 button);
        void SendInitialActionButtons();

        PvPInfo pvpInfo;
		void UpdatePvPState(bool onlyFFA = false);
        void UpdatePvP(bool state, bool ovrride=false);
        void UpdateZone(uint32 newZone, uint32 newArea);
        void UpdateArea(uint32 newArea);
		void SetNeedsZoneUpdate(bool needsUpdate) { m_needsZoneUpdate = needsUpdate; }
        bool IsInSanctuary() const override { return HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_SANCTUARY); }

        void UpdateZoneDependentAuras( uint32 zone_id );    // zones
        void UpdateAreaDependentAuras( uint32 area_id );    // subzones

        WorldLocation& GetTeleportDest() { return m_teleport_dest; }
        bool IsBeingTeleported() const { return mSemaphoreTeleport_Near || mSemaphoreTeleport_Far; }
        bool IsBeingTeleportedNear() const { return mSemaphoreTeleport_Near; }
        bool IsBeingTeleportedFar() const { return mSemaphoreTeleport_Far; }
        void SetSemaphoreTeleportNear(bool semphsetting) { mSemaphoreTeleport_Near = semphsetting; }
        void SetSemaphoreTeleportFar(bool semphsetting) { mSemaphoreTeleport_Far = semphsetting; }
        void ProcessDelayedOperations();

        bool IsCanDelayTeleport() const { return m_bCanDelayTeleport; }
        void SetCanDelayTeleport(bool setting) { m_bCanDelayTeleport = setting; }
        bool IsHasDelayedTeleport() const { return m_bHasDelayedTeleport; }
        void SetDelayedTeleportFlag(bool setting) { m_bHasDelayedTeleport = setting; }
        void ScheduleDelayedOperation(uint32 operation) { if (operation < DELAYED_END) m_DelayedOperations |= operation; }

        void UpdateAfkReport(time_t currTime);
        void UpdatePvPFlag(time_t currTime);
        void UpdateContestedPvP(uint32 currTime);
        void SetContestedPvPTimer(uint32 newTime) {m_contestedPvPTimer = newTime;}
        void ResetContestedPvP();

        /** todo: -maybe move UpdateDuelFlag+DuelComplete to independent DuelHandler.. **/
        DuelInfo *duel;
        void UpdateDuelFlag(time_t currTime);
        void CheckDuelDistance(time_t currTime);
        void DuelComplete(DuelCompleteType type);

        bool IsGroupVisibleFor(Player const* p) const;
        bool IsInSameGroupWith(Player const* p) const;
        bool IsInSameRaidWith(Player const* p) const { return p==this || (GetGroup() != nullptr && GetGroup() == p->GetGroup()); }
        void UninviteFromGroup();
        static void RemoveFromGroup(Group* group, uint64 guid);
        void RemoveFromGroup() { RemoveFromGroup(GetGroup(),GetGUID()); }
        void SendUpdateToOutOfRangeGroupMembers();

        void SetInGuild(uint32 guildId);
        void SetRank(uint32 rankId);
        void SetGuildIdInvited(uint32 guildId);
        uint32 GetGuildId() const;
        Guild* GetGuild() const;
        uint32 GetRank() const;
        int GetGuildIdInvited() const { return _guildIdInvited; }
        static void RemovePetitionsAndSigns(uint64 guid, uint32 type, SQLTransaction trans);
        static uint32 GetGuildIdFromCharacterInfo(uint32 guid);

        // Arena Team
        void SetInArenaTeam(uint32 ArenaTeamId, uint8 slot);
        static uint32 GetArenaTeamIdFromCharacterInfo(uint64 guid, uint8 slot);
        uint32 GetArenaTeamId(uint8 slot) const { return GetUInt32Value(PLAYER_FIELD_ARENA_TEAM_INFO_1_1 + (slot * 6)); }
        void SetArenaTeamIdInvited(uint32 ArenaTeamId) { _arenaTeamIdInvited = ArenaTeamId; }
        uint32 GetArenaTeamIdInvited() const { return _arenaTeamIdInvited; }
        uint8 GetGladiatorRank() const;
        void UpdateGladiatorTitle(uint8 rank);
        void UpdateArenaTitles();
        void UpdateArenaTitleForRank(uint8 rank, bool add);

        void SetDifficulty(Difficulty dungeon_difficulty, bool sendToPlayer = true, bool asGroup = false);
        //arg has no effect for now
        Difficulty GetDifficulty(bool isRaid = false) const { return m_dungeonDifficulty; }

        bool UpdateSkill(uint32 skill_id, uint32 step);
        bool UpdateSkillPro(uint16 SkillId, int32 Chance, uint32 step);

        bool UpdateCraftSkill(uint32 spellid);
        bool UpdateGatherSkill(uint32 SkillId, uint32 SkillValue, uint32 RedLevel, uint32 Multiplicator = 1);
        bool UpdateFishingSkill();

        uint32 GetBaseDefenseSkillValue() const { return GetBaseSkillValue(SKILL_DEFENSE); }
        uint32 GetBaseWeaponSkillValue(WeaponAttackType attType) const;

        uint32 GetSpellByProto(ItemTemplate *proto) const;

        float GetHealthBonusFromStamina() const;
        float GetManaBonusFromIntellect() const;

        bool UpdateStats(Stats stat) override;
        bool UpdateAllStats() override;
        void UpdateResistances(uint32 school) override;
        void UpdateArmor() override;
        void UpdateMaxHealth() override;
        void UpdateMaxPower(Powers power) override;
        void UpdateAttackPowerAndDamage(bool ranged = false) override;
        void UpdateShieldBlockValue();
        void UpdateSpellDamageAndHealingBonus();

        void CalculateMinMaxDamage(WeaponAttackType attType, bool normalized, bool addTotalPct, float& minDamage, float& maxDamage, Unit* target = nullptr) override;
        bool HasWand() const;

        void UpdateDefenseBonusesMod();
        void ApplyRatingMod(CombatRating cr, int32 value, bool apply);
        void UpdateHasteRating(CombatRating cr, int32 value, bool apply);
        float GetMeleeCritFromAgility();
        float GetDodgeFromAgility();
        float GetSpellCritFromIntellect();
        float OCTRegenHPPerSpirit();
        float OCTRegenMPPerSpirit();
        float GetRatingCoefficient(CombatRating cr) const;
        float GetRatingBonusValue(CombatRating cr) const;
        uint32 GetMeleeCritDamageReduction(uint32 damage) const;
        uint32 GetRangedCritDamageReduction(uint32 damage) const;
        uint32 GetSpellCritDamageReduction(uint32 damage) const;
        uint32 GetDotDamageReduction(uint32 damage) const;

        float GetExpertiseDodgeOrParryReduction(WeaponAttackType attType) const;
        void UpdateBlockPercentage();
        void UpdateCritPercentage(WeaponAttackType attType);
        void UpdateAllCritPercentages();
        void UpdateParryPercentage();
        void UpdateDodgePercentage();
        void UpdateAllSpellCritChances();
        void UpdateSpellCritChance(uint32 school);
        void UpdateExpertise(WeaponAttackType attType);
        void UpdateManaRegen();

        const uint64& GetLootGUID() const { return m_lootGuid; }
        void SetLootGUID(const uint64 &guid) { m_lootGuid = guid; }

        void RemovedInsignia(Player* looterPlr);

        WorldSession* GetSession() const { return m_session; }
        void SetSession(WorldSession *s) { m_session = s; }

        void BuildCreateUpdateBlockForPlayer(UpdateData *data, Player *target) const override;
        void DestroyForPlayer(Player *target, bool onDeath = false) const override;
        void SendDelayResponse(const uint32);
        void SendLogXPGain(uint32 GivenXP,Unit* victim,uint32 RestXP);

        //Low Level Packets
        void PlaySound(uint32 Sound, bool OnlySelf);
        //notifiers
        void SendAttackSwingCantAttack();
        void SendAttackSwingCancelAttack();
        void SendAttackSwingDeadTarget();
        void SendAttackSwingNotStanding();
        void SendAttackSwingNotInRange();
        void SendAttackSwingBadFacingAttack();
        void SendAutoRepeatCancel();
        void SendExplorationExperience(uint32 Area, uint32 Experience);

        void SendDungeonDifficulty(bool IsInGroup);
        void ResetInstances(uint8 method, bool isRaid = false);
        void SendResetInstanceSuccess(uint32 MapId);
        void SendResetInstanceFailed(uint32 reason, uint32 MapId);
        void SendResetFailedNotify(uint32 mapid);

        bool UpdatePosition(float x, float y, float z, float orientation, bool teleport = false) override;
		bool UpdatePosition(const Position &pos, bool teleport = false) override { return UpdatePosition(pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), pos.GetOrientation(), teleport); }
        void ProcessTerrainStatusUpdate(ZLiquidStatus status, Optional<LiquidData> const& liquidData) override;

        void SendMessageToSet(WorldPacket *data, bool self) override;
        void SendMessageToSetInRange(WorldPacket *data, float dist, bool self, bool includeMargin = false, Player const* skipped_rcvr = nullptr) override;
		void SendMessageToSetInRange(WorldPacket *data, float dist, bool self, bool includeMargin, bool own_team_only, Player const* skipped_rcvr = nullptr);
        void SendMessageToSet(WorldPacket* data, Player* skipped_rcvr) override;

        void SendTeleportAckPacket();

        /**
        * Deletes a character from the database
        *
        * By default character in just unlinked and not really deleted.
        *
        * @see Player::DeleteOldCharacters
        *
        * @param playerguid       the low-GUID from the player which should be deleted
        * @param accountId        the account id from the player
        * @param updateRealmChars when this flag is set, the amount of characters on that realm will be updated in the realmlist
        * @param deleteFinally    if this flag is set, the character will be permanently removed from the database
        */
        static void DeleteFromDB(uint64 playerguid, uint32 accountId, bool updateRealmChars = true, bool deleteFinally = false);
        static void LeaveAllArenaTeams(uint64 guid);
        static void DeleteOldCharacters();

        Corpse *GetCorpse() const;
        void SpawnCorpseBones(bool triggerSave = true);
		Corpse* CreateCorpse();
        void KillPlayer();
		static void OfflineResurrect(ObjectGuid const& guid, SQLTransaction& trans);
		bool HasCorpse() const { return _corpseLocation.GetMapId() != MAPID_INVALID; }
		WorldLocation GetCorpseLocation() const { return _corpseLocation; }
        uint32 GetResurrectionSpellId();
        void ResurrectPlayer(float restore_percent, bool applySickness = false);
        void BuildPlayerRepop();
        void RepopAtGraveyard();

        void DurabilityLossAll(double percent, bool inventory);
        void DurabilityLoss(Item* item, double percent);
        void DurabilityPointsLossAll(int32 points, bool inventory);
        void DurabilityPointsLoss(Item* item, int32 points);
        void DurabilityPointLossForEquipSlot(EquipmentSlots slot);
        uint32 DurabilityRepairAll(bool cost, float discountMod, bool guildBank);
        uint32 DurabilityRepair(uint16 pos, bool cost, float discountMod, bool guildBank);

        void UpdateMirrorTimers();
        void StopMirrorTimers()
        {
            StopMirrorTimer(FATIGUE_TIMER);
            StopMirrorTimer(BREATH_TIMER);
            StopMirrorTimer(FIRE_TIMER);
        }

        void SetMovement(PlayerMovementType pType);

        void JoinedChannel(Channel *c);
        void LeftChannel(Channel *c);
        void CleanupChannels();
        void UpdateLocalChannels( uint32 newZone );
        void LeaveLFGChannel();

        void UpdateDefense();
        void UpdateWeaponSkill (WeaponAttackType attType);
        void UpdateCombatSkills(Unit *pVictim, WeaponAttackType attType, MeleeHitOutcome outcome, bool defence);

        //step = skill tier (I guess)
        void SetSkill(uint32 id, uint16 step, uint16 currVal, uint16 maxVal);
        uint16 GetMaxSkillValue(uint32 skill) const;        // max + perm. bonus
        uint16 GetPureMaxSkillValue(uint32 skill) const;    // max
        uint16 GetSkillValue(uint32 skill) const;           // skill value + perm. bonus + temp bonus
        uint16 GetBaseSkillValue(uint32 skill) const;       // skill value + perm. bonus
        uint16 GetPureSkillValue(uint32 skill) const;       // skill value
        int16 GetSkillPermBonusValue(uint32 skill) const;
        int16 GetSkillTempBonusValue(uint32 skill) const;
        bool HasSkill(uint32 skill) const;
        void LearnSkillRewardedSpells( uint32 skillId, uint32 skillValue);

        void CheckAreaExploreAndOutdoor(void);

        static uint32 TeamForRace(uint8 race);
        uint32 GetTeam() const { return m_team; }
		TeamId GetTeamId() const { return m_team == ALLIANCE ? TEAM_ALLIANCE : TEAM_HORDE; }
        static uint32 getFactionForRace(uint8 race);
        void SetFactionForRace(uint8 race);
        
        static bool IsMainFactionForRace(uint32 race, uint32 factionId);
        static uint32 GetMainFactionForRace(uint32 race);
        static uint32 GetNewFactionForRaceChange(uint32 oldRace, uint32 newRace, uint32 factionId);

        bool IsAtGroupRewardDistance(WorldObject const* pRewardSource) const;
        bool RewardPlayerAndGroupAtKill(Unit* pVictim);
        void RewardPlayerAndGroupAtEvent(uint32 creature_id, WorldObject* pRewardSource);

        FactionStateList m_factions;
        ForcedReactions m_forcedReactions;
        FactionStateList const& GetFactionStateList() { return m_factions; }
        uint32 GetDefaultReputationFlags(const FactionEntry *factionEntry) const;
        int32 GetBaseReputation(const FactionEntry *factionEntry) const;
        int32 GetReputation(uint32 faction_id) const;
        int32 GetReputation(const FactionEntry *factionEntry) const;
        ReputationRank GetReputationRank(uint32 faction) const;
        ReputationRank GetReputationRank(const FactionEntry *factionEntry) const;
        ReputationRank GetBaseReputationRank(const FactionEntry *factionEntry) const;
        ReputationRank ReputationToRank(int32 standing) const;
        const static int32 ReputationRank_Length[MAX_REPUTATION_RANK];
        bool ModifyFactionReputation(uint32 FactionTemplateId, int32 DeltaReputation);
        bool ModifyFactionReputation(FactionEntry const* factionEntry, int32 standing);
        bool ModifyOneFactionReputation(FactionEntry const* factionEntry, int32 standing);
        bool SetFactionReputation(uint32 FactionTemplateId, int32 standing);
        bool SetFactionReputation(FactionEntry const* factionEntry, int32 standing);
        bool SetOneFactionReputation(FactionEntry const* factionEntry, int32 standing);
        void SwapFactionReputation(uint32 factionId1, uint32 factionId2);
        void DropFactionReputation(uint32 factionId);
        int32 CalculateReputationGain(uint32 creatureOrQuestLevel, int32 rep, bool for_quest);
        void RewardReputation(Unit *pVictim, float rate);
        void RewardReputation(Quest const *pQuest);
        void SetInitialFactions();
        void UpdateReputation();
        void SendFactionState(FactionState const* faction);
        void SendInitialReputations();
        FactionState const* GetFactionState( FactionEntry const* factionEntry) const;
        void SetFactionAtWar(FactionState* faction, bool atWar);
        void SetFactionInactive(FactionState* faction, bool inactive);
        void SetFactionVisible(FactionState* faction);
        void SetFactionVisibleForFactionTemplateId(uint32 FactionTemplateId);
        void SetFactionVisibleForFactionId(uint32 FactionId);
        void UpdateSkillsForLevel();
        void UpdateSkillsToMaxSkillsForLevel();             // for .levelup
        void ModifySkillBonus(uint32 skillid,int32 val, bool talent);

        /*********************************************************/
        /***                  PVP SYSTEM                       ***/
        /*********************************************************/
        void UpdateArenaFields();
        void UpdateHonorFields();
        bool RewardHonor(Unit *pVictim, uint32 groupsize, float honor = -1, bool pvptoken = false);
        uint32 GetHonorPoints() { return GetUInt32Value(PLAYER_FIELD_HONOR_CURRENCY); }
        uint32 GetArenaPoints() { return GetUInt32Value(PLAYER_FIELD_ARENA_CURRENCY); }
        void ModifyHonorPoints( int32 value );
        void ModifyArenaPoints( int32 value );
        uint32 GetMaxPersonalArenaRatingRequirement();
        void UpdateKnownPvPTitles();

        //End of PvP System

        void SetDrunkValue(uint16 newDrunkValue, uint32 itemid=0);
        uint16 GetDrunkValue() const { return m_drunk; }
        static DrunkenState GetDrunkenstateByValue(uint16 value);

        uint32 GetDeathTimer() const { return m_deathTimer; }
        uint32 GetDeathTime() const { return m_deathTime; }
        uint32 GetCorpseReclaimDelay(bool pvp) const;
        void UpdateCorpseReclaimDelay();
		int32 CalculateCorpseReclaimDelay(bool load = false) const;
		void SendCorpseReclaimDelay(uint32 delay);

        uint32 GetShieldBlockValue() const override;                 // overwrite Unit version (virtual)
        bool CanParry() const { return m_canParry; }
        void SetCanParry(bool value);
        bool CanBlock() const { return m_canBlock; }
        void SetCanBlock(bool value);

        void SetRegularAttackTime();

        void HandleBaseModFlatValue(BaseModGroup modGroup, float amount, bool apply);
        void ApplyBaseModPctValue(BaseModGroup modGroup, float pct);

        void SetBaseModFlatValue(BaseModGroup modGroup, float val);
        void SetBaseModPctValue(BaseModGroup modGroup, float val);

        void UpdateDamageDoneMods(WeaponAttackType attackType) override;
        void UpdateBaseModGroup(BaseModGroup modGroup);

        float GetBaseModValue(BaseModGroup modGroup, BaseModType modType) const;
        float GetTotalBaseModValue(BaseModGroup modGroup) const;

        void _ApplyAllStatBonuses();
        void _RemoveAllStatBonuses();

        void CastItemUseSpell(Item* item, SpellCastTargets const& targets, uint8 cast_count, uint32 glyphIndex = 0);
        void _ApplyItemMods(Item *item,uint8 slot,bool apply, bool updateItemAuras = true);
        void _RemoveAllItemMods();
        void _ApplyAllItemMods();
        void _ApplyItemBonuses(ItemTemplate const *proto,uint8 slot,bool apply);
        void _ApplyWeaponDamage(uint8 slot, ItemTemplate const* proto, /* TC ScalingStatValuesEntry const* ssv, */ bool apply);
        void _ApplyAmmoBonuses();
        bool EnchantmentFitsRequirements(uint32 enchantmentcondition, int8 slot);
        void ToggleMetaGemsActive(uint8 exceptslot, bool apply);
        void CorrectMetaGemEnchants(uint8 slot, bool apply);
        void InitDataForForm(bool reapplyMods = false);

        void ApplyItemEquipSpell(Item *item, bool apply, bool form_change = false);
        void ApplyEquipSpell(SpellInfo const* spellInfo, Item* item, bool apply, bool form_change = false);
        void UpdateEquipSpellsAtFormChange();
        void CastItemCombatSpell(Unit *target, WeaponAttackType attType, uint32 procVictim, uint32 procEx, SpellInfo const *spellInfo = nullptr);
        void CastItemCombatSpell(Unit *target, WeaponAttackType attType, uint32 procVictim, uint32 procEx, Item *item, ItemTemplate const * proto, SpellInfo const *spell = nullptr);

        void SendInitWorldStates(bool force = false, uint32 forceZoneId = 0);
        void SendUpdateWorldState(uint32 Field, uint32 Value);
        void SendDirectMessage(WorldPacket *data) const;

        void SendAuraDurationsForTarget(Unit* target);

        PlayerMenu* PlayerTalkClass;
        std::vector<ItemSetEffect *> ItemSetEff;

        void SendLoot(uint64 guid, LootType loot_type);
        void SendLootError(uint64 guid, LootError error);
        void SendLootRelease( uint64 guid );
        void SendNotifyLootItemRemoved(uint8 lootSlot);
        void SendNotifyLootMoneyRemoved();

        /*********************************************************/
        /***               BATTLEGROUND SYSTEM                 ***/
        /*********************************************************/

        bool InBattleground()       const { return m_bgData.bgInstanceID != 0; }
        uint32 GetBattlegroundId()  const { return m_bgData.bgInstanceID; }
        BattlegroundTypeId GetBattlegroundTypeId() const { return m_bgData.bgTypeID; }
        Battleground* GetBattleground() const;
        bool InArena() const;
          
        bool InBattlegroundQueue() const;

        BattlegroundQueueTypeId GetBattlegroundQueueTypeId(uint32 index) const;
        uint32 GetBattlegroundQueueIndex(BattlegroundQueueTypeId bgQueueTypeId) const;
        bool IsInvitedForBattlegroundQueueType(BattlegroundQueueTypeId bgQueueTypeId) const;
        bool InBattlegroundQueueForBattlegroundQueueType(BattlegroundQueueTypeId bgQueueTypeId) const;

        void SetBattlegroundId(uint32 val, BattlegroundTypeId bgTypeId);
        uint32 AddBattlegroundQueueId(BattlegroundQueueTypeId val);
        bool HasFreeBattlegroundQueueId() const;
        void RemoveBattlegroundQueueId(BattlegroundQueueTypeId val);
        void SetInviteForBattlegroundQueueType(BattlegroundQueueTypeId bgQueueTypeId, uint32 instanceId);
        bool IsInvitedForBattlegroundInstance(uint32 instanceId) const;
        WorldLocation const& GetBattlegroundEntryPoint() const { return m_bgData.joinPos; }
        void SetBattlegroundEntryPoint();

		bool TeleportToBGEntryPoint();

        void SetBGTeam(uint32 team) { m_bgTeam = team; }
        uint32 GetBGTeam() const { return m_bgTeam ? m_bgTeam : GetTeam(); }

        void LeaveBattleground(bool teleportToEntryPoint = true);
        bool CanJoinToBattleground(Battleground const* bg) const;
        bool CanReportAfkDueToLimit();
        void ReportedAfkBy(Player* reporter);
        void ClearAfkReports() { m_bgData.bgAfkReporter.clear(); }

        bool GetBGAccessByLevel(BattlegroundTypeId bgTypeId) const;
        bool isAllowUseBattlegroundObject();
        bool isAllowedToTakeBattlegroundBase();
        bool isTotalImmunity();

        /*********************************************************/
        /***               OUTDOOR PVP SYSTEM                  ***/
        /*********************************************************/

        OutdoorPvP * GetOutdoorPvP() const;
        // returns true if the player is in active state for outdoor pvp objective capturing, false otherwise
        bool IsOutdoorPvPActive();

        /*********************************************************/
        /***                    REST SYSTEM                    ***/
        /*********************************************************/

        bool isRested() const { return GetRestTime() >= 10000; }
        uint32 GetXPRestBonus(uint32 xp);
        uint32 GetRestTime() const { return m_restTime;};
        void SetRestTime(uint32 v) { m_restTime = v;};

        /*********************************************************/
        /***              ENVIROMENTAL SYSTEM                  ***/
        /*********************************************************/

        void EnvironmentalDamage(EnviromentalDamage type, uint32 damage);

        /*********************************************************/
        /***               FLOOD FILTER SYSTEM                 ***/
        /*********************************************************/

        void UpdateSpeakTime();
        bool CanSpeak() const;
        void ChangeSpeakTime(int utime);

        /*********************************************************/
        /***                 VARIOUS SYSTEMS                   ***/
        /*********************************************************/
        
        void UpdateFallInformationIfNeed(MovementInfo const& minfo, uint16 opcode);
		// only changed for direct client control (possess, vehicle etc.), not stuff you control using pet commands
		Unit* m_unitMovedByMe;
        WorldObject* m_seer;
        void SetFallInformation(uint32 time, float z);
        void HandleFall(MovementInfo const& movementInfo);

        bool SetDisableGravity(bool disable, bool packetOnly /* = false */) override;
        bool SetFlying(bool apply, bool packetOnly = false) override;
        bool SetWaterWalking(bool apply, bool packetOnly = false) override;
        bool SetFeatherFall(bool apply, bool packetOnly = false) override;
        bool SetHover(bool enable, bool packetOnly = false) override;

        bool CanFly() const override { return m_movementInfo.HasMovementFlag(MOVEMENTFLAG_CAN_FLY); }
        bool CanWalk() const override { return true; }
        bool CanSwim() const override { return true; }

        void HandleDrowning(uint32 time_diff);
        void HandleFallDamage(MovementInfo& movementInfo);
        void HandleFallUnderMap();

        //relocate without teleporting
        void RelocateToArenaZone(bool secondary = false);
        void TeleportToArenaZone(bool secondary = false);
        void RelocateToBetaZone();
        void TeleportToBetaZone();
        bool ShouldGoToSecondaryArenaZone();
        void GetArenaZoneCoord(bool secondary, uint32& map, float& x, float& y, float& z, float& o);
        void GetBetaZoneCoord(uint32& map, float& x, float& y, float& z, float& o);
        
        void SetClientControl(Unit* target, uint8 allowMove);

        void SetMover(Unit* target);

		void SetSeer(WorldObject* target) { m_seer = target; }
		void SetViewpoint(WorldObject* target, bool apply);
		WorldObject* GetViewpoint() const;

        uint32 GetSaveTimer() const { return m_nextSave; }
        void   SetSaveTimer(uint32 timer) { m_nextSave = timer; }

        #ifdef PLAYERBOT
        // A Player can either have a playerbotMgr (to manage its bots), or have playerbotAI (if it is a bot), or
        // neither. Code that enables bots must create the playerbotMgr and set it using SetPlayerbotMgr.
        // EquipmentSets& GetEquipmentSets() { return m_EquipmentSets; } //revmoed, not existing on BC
        void SetPlayerbotAI(PlayerbotAI* ai) { m_playerbotAI = ai; }
        PlayerbotAI* GetPlayerbotAI() { return m_playerbotAI; }
        void SetPlayerbotMgr(PlayerbotMgr* mgr) { m_playerbotMgr=mgr; }
        PlayerbotMgr* GetPlayerbotMgr() { return m_playerbotMgr; }
        void SetBotDeathTimer() { m_deathTimer = 0; }
        //PlayerTalentMap& GetTalentMap(uint8 spec) { return *m_talents[spec]; }
        #endif
        #ifdef TESTS
        PlayerbotTestingAI* GetTestingPlayerbotAI();
        #endif

        // Recall position
        uint32 m_recallMap;
        float  m_recallX;
        float  m_recallY;
        float  m_recallZ;
        float  m_recallO;
        void   SaveRecallPosition();
        
        uint32 m_ConditionErrorMsgId;

        // Homebind coordinates
        uint32 m_homebindMapId;
        uint16 m_homebindAreaId;
        float m_homebindX;
        float m_homebindY;
        float m_homebindZ;
        void SetHomebind(WorldLocation const& loc, uint32 area_id);

        // currently visible objects at player client
		GuidUnorderedSet m_clientGUIDs;

        bool HaveAtClient(WorldObject const* u) const { return u==this || m_clientGUIDs.find(u->GetGUID())!=m_clientGUIDs.end(); }

		bool IsNeverVisible() const override;
        bool IsVisibleGloballyFor(Player* pl) const;

        void SendInitialVisiblePackets(Unit* target);
		void UpdateObjectVisibility(bool forced = true) override;
		void UpdateVisibilityForPlayer();
		void UpdateVisibilityOf(WorldObject* target);
		void UpdateTriggerVisibility();

        template<class T>
            void UpdateVisibilityOf(T* target, UpdateData& data, std::set<Unit*>& visibleNow);

        uint8 m_forced_speed_changes[MAX_MOVE_TYPE];

        bool HasAtLoginFlag(AtLoginFlags f) const { return m_atLoginFlags & f; }
        void SetAtLoginFlag(AtLoginFlags f) { m_atLoginFlags |= f; }
        void RemoveAtLoginFlag(AtLoginFlags f) { m_atLoginFlags = m_atLoginFlags & ~f; }

        LookingForGroup m_lookingForGroup;

        // Temporarily removed pet cache
        uint32 GetTemporaryUnsummonedPetNumber() const { return m_temporaryUnsummonedPetNumber; }
        void SetTemporaryUnsummonedPetNumber(uint32 petnumber) { m_temporaryUnsummonedPetNumber = petnumber; }
        void UnsummonPetTemporaryIfAny();
        void ResummonPetTemporaryUnSummonedIfAny();
        bool IsPetNeedBeTemporaryUnsummoned() const;
        uint32 GetOldPetSpell() const { return m_oldpetspell; }
        void SetOldPetSpell(uint32 petspell) { m_oldpetspell = petspell; }
        
        // Experience Blocking
        bool IsXpBlocked() const { return m_isXpBlocked; }
        void SetXpBlocked(bool blocked) { m_isXpBlocked = blocked; }

        void SendCinematicStart(uint32 CinematicSequenceId) const;
        void SendMovieStart(uint32 MovieId) const;

        /*********************************************************/
        /***                 INSTANCE SYSTEM                   ***/
        /*********************************************************/

        typedef std::unordered_map< uint32 /*mapId*/, InstancePlayerBind > BoundInstancesMap;

        void UpdateHomebindTime(uint32 time);

        uint32 m_HomebindTimer;
        bool m_InstanceValid;
        // permanent binds and solo binds by difficulty
        BoundInstancesMap m_boundInstances[MAX_DIFFICULTY];
        InstancePlayerBind* GetBoundInstance(uint32 mapid, Difficulty difficulty);
        BoundInstancesMap& GetBoundInstances(Difficulty difficulty) { return m_boundInstances[difficulty]; }
        //raid is unused on BC
        InstanceSave * GetInstanceSave(uint32 mapid, bool raid = false);
        void UnbindInstance(uint32 mapid, Difficulty difficulty, bool unload = false);
        void UnbindInstance(BoundInstancesMap::iterator &itr, Difficulty difficulty, bool unload = false);
        InstancePlayerBind* BindToInstance(InstanceSave *save, bool permanent, bool load = false);
        void SendRaidInfo();
        void SendSavedInstances();
        bool Satisfy(AccessRequirement const*, uint32 target_map, bool report = false);

        /*********************************************************/
        /***                   GROUP SYSTEM                    ***/
        /*********************************************************/

        Group * GetGroupInvite() { return m_groupInvite; }
        void SetGroupInvite(Group *group) { m_groupInvite = group; }
        Group* GetGroup() { return m_group.getTarget(); }
        const Group* GetGroup() const { return (const Group*)m_group.getTarget(); }
        GroupReference& GetGroupRef() { return m_group; }
        void SetGroup(Group *group, int8 subgroup = -1);
        uint8 GetSubGroup() const { return m_group.getSubGroup(); }
        uint32 GetGroupUpdateFlag() const { return m_groupUpdateMask; }
        void SetGroupUpdateFlag(uint32 flag) { m_groupUpdateMask |= flag; }
        uint64 GetAuraUpdateMask() const { return m_auraUpdateMask; }
        void SetAuraUpdateMask(uint8 slot) { m_auraUpdateMask |= (uint64(1) << slot); }
        void UnsetAuraUpdateMask(uint8 slot) { m_auraUpdateMask &= ~(uint64(1) << slot); }
        Player* GetNextRandomRaidMember(float radius) const;
        PartyResult CanUninviteFromGroup() const;
        // Teleporter NPC: Check level requirements (in Config)
        bool HasLevelInRangeForTeleport() const;
        
        // Battleground Group System
        void SetBattlegroundRaid(Group* group, int8 subgroup = -1);
        void RemoveFromBattlegroundRaid();
        Group* GetOriginalGroup() { return m_originalGroup.getTarget(); }
        GroupReference& GetOriginalGroupRef() { return m_originalGroup; }
        uint8 GetOriginalSubGroup() const { return m_originalGroup.getSubGroup(); }
        void SetOriginalGroup(Group* group, int8 subgroup = -1);
        
        void SetPassOnGroupLoot(bool bPassOnGroupLoot) { m_bPassOnGroupLoot = bPassOnGroupLoot; }
        bool GetPassOnGroupLoot() const { return m_bPassOnGroupLoot; }

        MapReference &GetMapRef() { return m_mapRef; }

		// Set map to player and add reference
		void SetMap(Map* map) override;
		void ResetMap() override;

        bool IsAllowedToLoot(Creature const* creature) const;

        DeclinedName const* GetDeclinedNames() const { return m_declinedname; }
        bool HasTitle(uint32 bitIndex) const;
        bool HasTitle(CharTitlesEntry const* title) const { return HasTitle(title->bit_index); }
        void SetTitle(CharTitlesEntry const* title, bool notify = false, bool setCurrentTitle = false);
        void RemoveTitle(CharTitlesEntry const* title, bool notify = true);
        
        uint8 GetRace() const { return m_race; }
        uint8 GetGender() const { return m_gender; }
        void SetGender(uint8 gender) { m_gender = gender; }
        void SetRace(uint8 newrace) { m_race = newrace; } // Race/Faction change
        
        void SetSpiritRedeptionKiller(uint64 killerGUID) { m_spiritRedemptionKillerGUID = killerGUID; }
        uint64 GetSpiritRedemptionKiller() { return m_spiritRedemptionKillerGUID; }

        bool hasCustomXpRate() const { return m_customXp != 0.0f; }
        float getCustomXpRate() const { return m_customXp; }

        bool HaveSpectators() const;
        void SendSpectatorAddonMsgToBG(SpectatorAddonMsg msg);
        bool isSpectateCanceled() { return spectateCanceled; }
        void CancelSpectate()     { spectateCanceled = true; }
        Player* getSpectateFrom()   { return spectateFrom; }
        bool isSpectator() const  { return spectatorFlag; }
        void SetSpectate(bool on);
        void SendDataForSpectator();

        void setCommentator(bool on);

        void addSpamReport(uint64 reporterGUID, std::string message);
        time_t lastLagReport;
        
        std::vector<Item*> m_itemUpdateQueue;
        bool m_itemUpdateQueueBlocked;
        
        /*********************************************************/
        /***                    GOSSIP SYSTEM                  ***/
        /*********************************************************/

        void PrepareGossipMenu(WorldObject* source, uint32 menuId = 0, bool showQuests = false);
        void SendPreparedGossip(WorldObject* source);
        void OnGossipSelect(WorldObject* source, uint32 gossipListId, uint32 menuId);

        uint32 GetGossipTextId(uint32 menuId, WorldObject* source) const;
        uint32 GetGossipTextId(WorldObject* source) const;
        static uint32 GetDefaultGossipMenuForSource(WorldObject* source);

        void SetHasMovedInUpdate(bool moved) { m_hasMovedInUpdate = moved; }
        bool GetHasMovedInUpdate() const { return m_hasMovedInUpdate; }

    protected:

        /*********************************************************/
        /***               BATTLEGROUND SYSTEM                 ***/
        /*********************************************************/

        /*
        this is an array of BG queues (BgTypeIDs) in which is player
        */
        struct BgBattlegroundQueueID_Rec
        {
            BattlegroundQueueTypeId bgQueueTypeId;
            uint32 invitedToInstance;
        };
        BgBattlegroundQueueID_Rec m_bgBattlegroundQueueID[PLAYER_MAX_BATTLEGROUND_QUEUES];
        BGData                    m_bgData;

        uint8 m_bgAfkReportedCount;
        time_t m_bgAfkReportedTimer;
        uint32 m_contestedPvPTimer;

        uint32 m_bgTeam;    // what side the player will be added to

        /*********************************************************/
        /***                    QUEST SYSTEM                   ***/
        /*********************************************************/

        std::set<uint32> m_timedquests;

        uint64 m_divider;
        uint32 m_ingametime;

        /*********************************************************/
        /***                   LOAD SYSTEM                     ***/
        /*********************************************************/

        void _LoadActions(QueryResult result);
        void _LoadAuras(QueryResult result, uint32 timediff);
        void _LoadBoundInstances(QueryResult result);
        void _LoadInventory(QueryResult result, uint32 timediff);
        void _LoadMailInit(QueryResult resultUnread, QueryResult resultDelivery);
        void _LoadMail();
        void _LoadMailedItems(Mail *mail);
        void _LoadQuestStatus(QueryResult result);
        void _LoadDailyQuestStatus(QueryResult result);
        void _LoadGroup(QueryResult result);
        void _LoadSkills(QueryResult result);
        void _LoadReputation(QueryResult result);
        void _LoadSpells(QueryResult result);
        bool _LoadHomeBind(QueryResult result);
        void _LoadDeclinedNames(QueryResult result);
        void _LoadArenaTeamInfo(QueryResult result);
        void _LoadBGData(PreparedQueryResult result);

        /*********************************************************/
        /***                   SAVE SYSTEM                     ***/
        /*********************************************************/

        void _SaveActions(SQLTransaction trans);
        void _SaveAuras(SQLTransaction trans);
        void _SaveInventory(SQLTransaction trans);
        void _SaveMail(SQLTransaction trans);
        void _SaveQuestStatus(SQLTransaction trans);
        void _SaveDailyQuestStatus(SQLTransaction trans);
        void _SaveReputation(SQLTransaction trans);
        void _SaveSpells(SQLTransaction trans);
        void _SaveSkills(SQLTransaction trans);
        void _SaveBGData(SQLTransaction& trans);

        /*********************************************************/
        /***              ENVIRONMENTAL SYSTEM                 ***/
        /*********************************************************/
        void HandleSobering();
        void SendMirrorTimer(MirrorTimerType Type, uint32 MaxValue, uint32 CurrentValue, int32 Regen);
        void StopMirrorTimer(MirrorTimerType Type);
        int32 getMaxTimer(MirrorTimerType timer);
        bool m_isInWater;

         // Current teleport data
        WorldLocation m_teleport_dest;
        uint32 m_teleport_options;
        bool mSemaphoreTeleport_Near;
        bool mSemaphoreTeleport_Far;

        uint32 m_DelayedOperations;
        bool m_bCanDelayTeleport;
        bool m_bHasDelayedTeleport;

        /*********************************************************/
        /***                  HONOR SYSTEM                     ***/
        /*********************************************************/
        time_t m_lastHonorUpdateTime;

        bool _removeSpell(uint16 spell_id);
        uint64 m_lootGuid;

        uint8 m_race;
        uint8 m_class;
        uint8 m_gender;
        uint32 m_team;
        uint32 m_nextSave;
        time_t m_speakTime;
        uint32 m_speakCount;
        Difficulty m_dungeonDifficulty;

        uint32 m_atLoginFlags;

        Item* m_items[PLAYER_SLOTS_COUNT];
        uint32 m_currentBuybackSlot;

        uint32 m_ExtraFlags;

        uint64 m_comboTarget;
        int8 m_comboPoints;

        QuestStatusMap m_QuestStatus;
        
        SkillStatusMap mSkillStatus;

        uint32 _guildIdInvited;
        uint32 _arenaTeamIdInvited;

        PlayerMails m_mail;
        PlayerSpellMap m_spells;
        //TC PlayerTalentMap* m_talents[MAX_TALENT_SPECS];
        SpellCooldowns m_spellCooldowns;
        std::map<uint32, uint32> m_globalCooldowns; // whole start recovery category stored in one

        uint32 m_timeSyncCounter;
        uint32 m_timeSyncTimer;
        uint32 m_timeSyncClient;
        uint32 m_timeSyncServer;

        ActionButtonList m_actionButtons;

        float m_auraBaseFlatMod[BASEMOD_END];
        float m_auraBasePctMod[BASEMOD_END];
        SpellModList m_spellMods[MAX_SPELLMOD];
        int32 m_SpellModRemoveCount;
        EnchantDurationList m_enchantDuration;
        ItemDurationList m_itemDuration;

        void ResetTimeSync();
        void SendTimeSync();

        uint64 m_resurrectGUID;
        uint32 m_resurrectMap;
        float m_resurrectX, m_resurrectY, m_resurrectZ;
        uint32 m_resurrectHealth, m_resurrectMana;

        WorldSession *m_session;

        typedef std::list<Channel*> JoinedChannelsList;
        JoinedChannelsList m_channels;

        int m_cinematic;

        Player *pTrader;
        bool acceptTrade;
        uint16 tradeItems[TRADE_SLOT_COUNT];
        uint32 tradeGold;

        bool   m_DailyQuestChanged;
        time_t m_lastDailyQuestTime;

        uint32 m_regenTimer;
        uint32 m_hostileReferenceCheckTimer;
        uint32 m_drunkTimer;
        uint16 m_drunk;
        uint32 m_weaponChangeTimer;

        uint32 m_zoneUpdateId;
        uint32 m_zoneUpdateTimer;
        uint32 m_areaUpdateId;

        uint32 m_deathTimer; //time left before forced releasing
        time_t m_deathExpireTime; //added delay expiration time
        time_t m_deathTime; //time of death

        uint32 m_restTime;

        uint32 m_teleportToTestInstanceId; //bypass most teleport checks...

        uint32 m_WeaponProficiency;
        uint32 m_ArmorProficiency;
        bool m_canParry;
        bool m_canBlock;
        uint8 m_swingErrorMsg;
        float m_ammoDPS;
        ////////////////////Rest System/////////////////////
        int time_inn_enter;
        uint32 inn_pos_mapid;
        float  inn_pos_x;
        float  inn_pos_y;
        float  inn_pos_z;
        float m_rest_bonus;
        RestType rest_type;
        ////////////////////Rest System/////////////////////

        //movement anticheat
        uint32 m_anti_lastmovetime;     //last movement time
        uint64 m_anti_transportGUID;    //current transport GUID
        float  m_anti_MovedLen;         //Length of traveled way
        uint32 m_anti_NextLenCheck;
        uint32 m_anti_beginfalltime;    //alternative falling begin time
        uint32 m_anti_lastalarmtime;    //last time when alarm generated
        uint32 m_anti_alarmcount;       //alarm counter
        uint32 m_anti_TeleTime;

        uint32 m_resetTalentsCost;
        time_t m_resetTalentsTime;
        uint32 m_usedTalentCount;

        // Social
        PlayerSocial *m_social;

        // Groups
        GroupReference m_group;
        GroupReference m_originalGroup;
        Group *m_groupInvite;
        uint32 m_groupUpdateMask;
        uint64 m_auraUpdateMask;

        // Temporarily removed pet cache
        uint32 m_temporaryUnsummonedPetNumber;
        uint32 m_oldpetspell;

        uint32 _activeCheats; //mask from PlayerCommandStates

		bool CanAlwaysSee(WorldObject const* obj) const override;
		bool IsAlwaysDetectableFor(WorldObject const* seer) const override;

		bool m_needsZoneUpdate;

#ifdef PLAYERBOT
        PlayerbotAI* m_playerbotAI;
        PlayerbotMgr* m_playerbotMgr;
#endif

        //GuardianPetList m_guardianPets;

        // Player summoning
        time_t m_summon_expire;
        uint32 m_summon_mapid;
        float  m_summon_x;
        float  m_summon_y;
        float  m_summon_z;
        bool   m_invite_summon;

        DeclinedName *m_declinedname;
        
        // Experience Blocking
        bool m_isXpBlocked;
        
        // Spirit of Redemption
        uint64 m_spiritRedemptionKillerGUID;
        
        time_t _attackersCheckTime;
        
        bool m_bPassOnGroupLoot;
        
        // spectator system
        bool spectatorFlag;
        bool spectateCanceled;
        Player *spectateFrom;
       
        // true if player has moved in this update. In the previous system, in a player moved and stopped in the same update you had no way to know it. (this is used to fix spell not always properly interrupted)
        bool m_hasMovedInUpdate;

    private:
        // internal common parts for CanStore/StoreItem functions
        InventoryResult _CanStoreItem_InSpecificSlot( uint8 bag, uint8 slot, ItemPosCountVec& dest, ItemTemplate const *pProto, uint32& count, bool swap, Item *pSrcItem ) const;
        InventoryResult _CanStoreItem_InBag( uint8 bag, ItemPosCountVec& dest, ItemTemplate const *pProto, uint32& count, bool merge, bool non_specialized, Item *pSrcItem, uint8 skip_bag, uint8 skip_slot ) const;
        InventoryResult _CanStoreItem_InInventorySlots( uint8 slot_begin, uint8 slot_end, ItemPosCountVec& dest, ItemTemplate const *pProto, uint32& count, bool merge, Item *pSrcItem, uint8 skip_bag, uint8 skip_slot ) const;
        Item* _StoreItem( uint16 pos, Item *pItem, uint32 count, bool clone, bool update );

        CinematicMgr* _cinematicMgr;

        int32 m_MirrorTimer[MAX_TIMERS];
        uint8 m_MirrorTimerFlags;
        uint8 m_MirrorTimerFlagsLast;

        MapReference m_mapRef;

        UnitAI *i_AI;
        
        uint32 m_lastFallTime;
        float  m_lastFallZ;

        uint32 m_lastOpenLockKey;
        
        float m_customXp;
        
        SpamReports _spamReports;
        time_t _lastSpamAlert; // When was the last time we reported this ugly spammer to the staff?

		WorldLocation _corpseLocation;

    public:
        bool m_kickatnextupdate;
        uint32 m_swdBackfireDmg;
};

void AddItemsSetItem(Player*player,Item *item);
void RemoveItemsSetItem(Player*player,ItemTemplate const *proto);

template <class T>
void Player::ApplySpellMod(uint32 spellId, SpellModOp op, T &basevalue, Spell const* spell)
{
    SpellInfo const *spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return;

    float totalmul = 1.0f;
    int32 totalflat = 0;

    for (auto mod : m_spellMods[op])
    {
        // Charges can be set only for mods with auras
        /* TC
        if (!mod->ownerAura)
        ASSERT(mod->charges == 0);
        */

        if(!IsAffectedBySpellmod(spellInfo,mod,spell))
            continue;

        if (mod->type == SPELLMOD_FLAT)
            totalflat += mod->value;
        else if (mod->type == SPELLMOD_PCT)
        {
            // skip percent mods for null basevalue (most important for spell mods with charges )
            if(basevalue == T(0)) 
            {
                //HACKZ //Frost Warding + Molten Shields
                if (mod->spellId == 11189 || mod->spellId == 28332 || mod->spellId == 11094 || mod->spellId == 13043)
                    basevalue = 100;
                else
                    continue;
            }
            // special case (skip >10sec spell casts for instant cast setting)
            if( mod->op==SPELLMOD_CASTING_TIME  && basevalue >= T(10000) && mod->value <= -100)
                continue;

            totalmul += CalculatePct(1.0f, mod->value);
        }

        //TC DropModCharge(mod, spell);
        if (mod->charges > 0 )
        {
          if( !(spellInfo->SpellFamilyName == 8 && (spellInfo->SpellFamilyFlags & 0x200000000LL)))
            --mod->charges;
            if (mod->charges == 0)
            {
                mod->charges = -1;
                mod->lastAffected = spell;
                if(!mod->lastAffected)
                    mod->lastAffected = FindCurrentSpellBySpellId(spellId);
                ++m_SpellModRemoveCount;
            }
        }
    }

    basevalue = T(float(basevalue + totalflat) * totalmul);
}
#endif
