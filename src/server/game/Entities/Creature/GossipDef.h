
#ifndef TRINITYCORE_GOSSIP_H
#define TRINITYCORE_GOSSIP_H

#include "Common.h"
#include "ObjectGuid.h"
#include "QuestDef.h"
#include "NPCHandler.h"

class WorldSession;

#define GOSSIP_MAX_MENU_ITEMS 64                            // client supported items unknown, but provided number must be enough
#define DEFAULT_GOSSIP_MESSAGE              0xffffff

enum Gossip_Option
{
    GOSSIP_OPTION_NONE              = 0,                    //UNIT_NPC_FLAG_NONE              = 0,
    GOSSIP_OPTION_GOSSIP            = 1,                    //UNIT_NPC_FLAG_GOSSIP            = 1,
    GOSSIP_OPTION_QUESTGIVER        = 2,                    //UNIT_NPC_FLAG_QUESTGIVER        = 2,
    GOSSIP_OPTION_VENDOR            = 3,                    //UNIT_NPC_FLAG_VENDOR            = 4,
    GOSSIP_OPTION_TAXIVENDOR        = 4,                    //UNIT_NPC_FLAG_TAXIVENDOR        = 8,
    GOSSIP_OPTION_TRAINER           = 5,                    //UNIT_NPC_FLAG_TRAINER           = 16,
    GOSSIP_OPTION_SPIRITHEALER      = 6,                    //UNIT_NPC_FLAG_SPIRITHEALER      = 32,
    GOSSIP_OPTION_SPIRITGUIDE       = 7,                    //UNIT_NPC_FLAG_SPIRITGUIDE       = 64,
    GOSSIP_OPTION_INNKEEPER         = 8,                    //UNIT_NPC_FLAG_INNKEEPER         = 128,
    GOSSIP_OPTION_BANKER            = 9,                    //UNIT_NPC_FLAG_BANKER            = 256,
    GOSSIP_OPTION_PETITIONER        = 10,                   //UNIT_NPC_FLAG_PETITIONER        = 512,
    GOSSIP_OPTION_TABARDDESIGNER    = 11,                   //UNIT_NPC_FLAG_TABARDDESIGNER    = 1024,
    GOSSIP_OPTION_BATTLEFIELD       = 12,                   //UNIT_NPC_FLAG_BATTLEFIELDPERSON = 2048,
    GOSSIP_OPTION_AUCTIONEER        = 13,                   //UNIT_NPC_FLAG_AUCTIONEER        = 4096,
    GOSSIP_OPTION_STABLEPET         = 14,                   //UNIT_NPC_FLAG_STABLE            = 8192,
    GOSSIP_OPTION_ARMORER           = 15,                   //UNIT_NPC_FLAG_ARMORER           = 16384,
    GOSSIP_OPTION_UNLEARNTALENTS    = 16,                   //UNIT_NPC_FLAG_TRAINER (bonus option for GOSSIP_OPTION_TRAINER)
    GOSSIP_OPTION_UNLEARNPETTALENTS = 17,                   //UNIT_NPC_FLAG_TRAINER (bonus option for GOSSIP_OPTION_TRAINER)
    GOSSIP_OPTION_OUTDOORPVP        = 18,                   //added by code (option for outdoor pvp creatures)
    GOSSIP_OPTION_HALLOWS_END       = 19,                    // For Innkeepers during Hallow's End
    GOSSIP_OPTION_MAX
};

enum GossipOptionIcon
{
    GOSSIP_ICON_CHAT                = 0,                    //white chat bubble
    GOSSIP_ICON_VENDOR              = 1,                    //brown bag
    GOSSIP_ICON_TAXI                = 2,                    //flight
    GOSSIP_ICON_TRAINER             = 3,                    //book
    GOSSIP_ICON_INTERACT_1          = 4,                    //interaction wheel
    GOSSIP_ICON_INTERACT_2          = 5,                    //interaction wheel
    GOSSIP_ICON_MONEY_BAG           = 6,                    //brown bag with yellow dot
    GOSSIP_ICON_TALK                = 7,                    //white chat bubble with black dots
    GOSSIP_ICON_TABARD              = 8,                    //tabard
    GOSSIP_ICON_BATTLE              = 9,                    //two swords
    GOSSIP_ICON_DOT                 = 10,                   //yellow dot
//next ones may not be BC
    GOSSIP_ICON_CHAT_11             = 11,                   // white chat bubble
    GOSSIP_ICON_CHAT_12             = 12,                   // white chat bubble
    GOSSIP_ICON_CHAT_13             = 13,                   // white chat bubble
    GOSSIP_ICON_UNK_14              = 14,                   // INVALID - DO NOT USE
    GOSSIP_ICON_UNK_15              = 15,                   // INVALID - DO NOT USE
    GOSSIP_ICON_CHAT_16             = 16,                   // white chat bubble
    GOSSIP_ICON_CHAT_17             = 17,                   // white chat bubble
    GOSSIP_ICON_CHAT_18             = 18,                   // white chat bubble
    GOSSIP_ICON_CHAT_19             = 19,                   // white chat bubble
    GOSSIP_ICON_CHAT_20             = 20,                   // white chat bubble
    GOSSIP_ICON_MAX
};

//POI defines
enum Poi_Icon
{
    ICON_POI_0                  =   0,                      // Grey ?
    ICON_POI_1                  =   1,                      // Red ?
    ICON_POI_2                  =   2,                      // Blue ?
    ICON_POI_BWTOMB             =   3,                      // Blue and White Tomb Stone
    ICON_POI_HOUSE              =   4,                      // House
    ICON_POI_TOWER              =   5,                      // Tower
    ICON_POI_REDFLAG            =   6,                      // Red Flag with Yellow !
    ICON_POI_TOMB               =   7,                      // Tomb Stone
    ICON_POI_BWTOWER            =   8,                      // Blue and White Tower
    ICON_POI_REDTOWER           =   9,                      // Red Tower
    ICON_POI_BLUETOWER          =   10,                     // Blue Tower
    ICON_POI_RWTOWER            =   11,                     // Red and White Tower
    ICON_POI_REDTOMB            =   12,                     // Red Tomb Stone
    ICON_POI_RWTOMB             =   13,                     // Red and White Tomb Stone
    ICON_POI_BLUETOMB           =   14,                     // Blue Tomb Stone
    ICON_POI_NOTHING            =   15,                     // NOTHING
    ICON_POI_16                 =   16,                     // Red ?
    ICON_POI_17                 =   17,                     // Grey ?
    ICON_POI_18                 =   18,                     // Blue ?
    ICON_POI_19                 =   19,                     // Red and White ?
    ICON_POI_20                 =   20,                     // Red ?
    ICON_POI_GREYLOGS           =   21,                     // Grey Wood Logs
    ICON_POI_BWLOGS             =   22,                     // Blue and White Wood Logs
    ICON_POI_BLUELOGS           =   23,                     // Blue Wood Logs
    ICON_POI_RWLOGS             =   24,                     // Red and White Wood Logs
    ICON_POI_REDLOGS            =   25,                     // Red Wood Logs
    ICON_POI_26                 =   26,                     // Grey ?
    ICON_POI_27                 =   27,                     // Blue and White ?
    ICON_POI_28                 =   28,                     // Blue ?
    ICON_POI_29                 =   29,                     // Red and White ?
    ICON_POI_30                 =   30,                     // Red ?
    ICON_POI_GREYHOUSE          =   31,                     // Grey House
    ICON_POI_BWHOUSE            =   32,                     // Blue and White House
    ICON_POI_BLUEHOUSE          =   33,                     // Blue House
    ICON_POI_RWHOUSE            =   34,                     // Red and White House
    ICON_POI_REDHOUSE           =   35,                     // Red House
    ICON_POI_GREYHORSE          =   36,                     // Grey Horse
    ICON_POI_BWHORSE            =   37,                     // Blue and White Horse
    ICON_POI_BLUEHORSE          =   38,                     // Blue Horse
    ICON_POI_RWHORSE            =   39,                     // Red and White Horse
    ICON_POI_REDHORSE           =   40                      // Red Horse
};

struct GossipMenuItem
{
    uint8       MenuItemIcon;
    bool        IsCoded;
    std::string Message;
    uint32      Sender;
    uint32      OptionType;
    std::string BoxMessage;
    uint32      BoxMoney;
};

// need an ordered container
typedef std::map<uint32, GossipMenuItem> GossipMenuItemContainer;

struct GossipMenuItemData
{
    uint32 GossipActionMenuId;  // MenuId of the gossip triggered by this action
    uint32 GossipActionPoi;
};

// need an ordered container
typedef std::map<uint32, GossipMenuItemData> GossipMenuItemDataContainer;

struct QuestMenuItem
{
    uint32      QuestId;
    uint8       QuestIcon;
};

typedef std::vector<QuestMenuItem> QuestMenuItemList;

class TC_GAME_API GossipMenu
{
    public:
        GossipMenu();
        ~GossipMenu();

        void AddMenuItem(int32 menuItemId, uint8 icon, std::string const& message, uint32 sender, uint32 action, std::string const& boxMessage, uint32 boxMoney, bool coded = false);
        void AddMenuItem(uint32 menuId, uint32 menuItemId, uint32 sender, uint32 action);
        void AddMenuItemTextID(uint8 icon, uint32 textID, uint32 sender, uint32 action);

        void SetMenuId(uint32 menu_id) { _menuId = menu_id; }
        uint32 GetMenuId() const { return _menuId; }
        void SetSenderGUID(ObjectGuid guid) { _senderGUID = guid; }
        ObjectGuid GetSenderGUID() const { return _senderGUID; }
        void SetLocale(LocaleConstant locale) { _locale = locale; }
        LocaleConstant GetLocale() const { return _locale; }

        void AddGossipMenuItemData(uint32 menuItemId, uint32 gossipActionMenuId, uint32 gossipActionPoi);

        uint32 GetMenuItemCount() const
        {
            return _menuItems.size();
        }

        bool Empty() const
        {
            return _menuItems.empty();
        }

        GossipMenuItem const* GetItem(uint32 id) const
        {
            auto itr = _menuItems.find(id);
            if (itr != _menuItems.end())
                return &itr->second;

            return nullptr;
        }

        GossipMenuItemData const* GetItemData(uint32 indexId) const
        {
            auto itr = _menuItemData.find(indexId);
            if (itr != _menuItemData.end())
                return &itr->second;

            return nullptr;
        }

        uint32 GetMenuItemSender(uint32 menuItemId) const;
        uint32 GetMenuItemAction(uint32 menuItemId) const;
        bool IsMenuItemCoded(uint32 menuItemId) const;

        void ClearMenu();

        GossipMenuItemContainer const& GetMenuItems() const
        {
            return _menuItems;
        }

    private:
        GossipMenuItemContainer _menuItems;
        GossipMenuItemDataContainer _menuItemData;
        uint32 _menuId;
        ObjectGuid _senderGUID;
        LocaleConstant _locale;
};

class TC_GAME_API QuestMenu
{
    public:
        QuestMenu();
        ~QuestMenu();

        void AddMenuItem(uint32 QuestId, uint8 Icon);
        void ClearMenu();

        uint8 GetMenuItemCount() const
        {
            return _questMenuItems.size();
        }

        bool Empty() const
        {
            return _questMenuItems.empty();
        }

        bool HasItem(uint32 questId) const;

        QuestMenuItem const& GetItem(uint16 index) const
        {
            return _questMenuItems[index];
        }

    private:
        QuestMenuItemList _questMenuItems;
};

class TC_GAME_API PlayerMenu
{
    public:
        explicit PlayerMenu(WorldSession* session);
        ~PlayerMenu();

        GossipMenu& GetGossipMenu() { return _gossipMenu; }
        QuestMenu& GetQuestMenu() { return _questMenu; }

        bool Empty() const { return _gossipMenu.Empty() && _questMenu.Empty(); }

        void ClearMenus();
        uint32 GetGossipOptionSender(uint32 selection) const { return _gossipMenu.GetMenuItemSender(selection); }
        uint32 GetGossipOptionAction(uint32 selection) const { return _gossipMenu.GetMenuItemAction(selection); }
        bool IsGossipOptionCoded(uint32 selection) const { return _gossipMenu.IsMenuItemCoded(selection); }

        //Sends a gossip menu with given text id. /!\ This is NOT a menu id
        void SendGossipMenu(uint32 titleTextId, ObjectGuid objectGUID);
        void SendCloseGossip();
        void SendPointOfInterest(float x, float y, uint32 icon, uint32 flags, uint32 importance, const char* locName) const;
        void SendPointOfInterest(uint32 poiId) const;

        /*********************************************************/
        /***                    QUEST SYSTEM                   ***/
        /*********************************************************/
        void SendQuestGiverStatus(uint8 questStatus, ObjectGuid npcGUID) const;

        void SendQuestGiverQuestList(QEmote const& eEmote, const std::string& Title, ObjectGuid npcGUID);

        void SendQuestQueryResponse(Quest const* quest) const;
        void SendQuestGiverQuestDetails(Quest const* quest, ObjectGuid npcGUID, bool activateAccept) const;

        void SendQuestGiverOfferReward(Quest const* quest, ObjectGuid npcGUID, bool enableNext) const;
        void SendQuestGiverRequestItems(Quest const* quest, ObjectGuid npcGUID, bool canComplete, bool closeOnCancel) const;

        static void AddQuestLevelToTitle(std::string &title, int32 level);

    private:
        GossipMenu _gossipMenu;
        QuestMenu  _questMenu;
        WorldSession* _session;
};
#endif

