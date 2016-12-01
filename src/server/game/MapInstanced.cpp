
#include "MapInstanced.h"
#include "ObjectMgr.h"
#include "MapManager.h"
#include "BattleGround.h"
#include "Management/VMapFactory.h"
#include "Management/MMapFactory.h"
#include "InstanceSaveMgr.h"
#include "World.h"
#include "DBCStores.h"

MapInstanced::MapInstanced(uint32 id) : Map(MAP_TYPE_MAP_INSTANCED, id, 0, DUNGEON_DIFFICULTY_NORMAL)
{
    // fill with zero
    memset(&GridMapReference, 0, MAX_NUMBER_OF_GRIDS*MAX_NUMBER_OF_GRIDS*sizeof(uint16));
}

void MapInstanced::InitVisibilityDistance()
{
    if (m_InstancedMaps.empty())
        return;
    //initialize visibility distances for all instance copies
    for (auto & m_InstancedMap : m_InstancedMaps)
    {
        m_InstancedMap.second->InitVisibilityDistance();
    }
}

void MapInstanced::Update(const uint32& t)
{
    // take care of loaded GridMaps (when unused, unload it!)
    Map::Update(t);

    // update the instanced maps
    auto i = m_InstancedMaps.begin();
    while (i != m_InstancedMaps.end())
    {
        if(i->second->CanUnload(t))
        {
            if (!DestroyInstance(i))                             // iterator incremented
            {
                //m_unloadTimer
            }
        }
        else
        {
            // update only here, because it may schedule some bad things before delete
            if (sMapMgr->GetMapUpdater()->activated())
            {
                sMapMgr->GetMapUpdater()->schedule_update(*i->second, t);
            } else
            {
                try
                {
                    i->second->Update(t);
                }
                catch (std::exception& /* e */)
                {
                    MapCrashed(i->second);
                }
            }

            ++i;
        }
    }

    //crash recovery
    for (auto crashedMap : crashedMaps)
    {
        crashedMap->HandleCrash();
        //remove it from map list. Do not clear memory (map memory may be corrupted)
        for (auto iter = m_InstancedMaps.begin(); iter != m_InstancedMaps.end(); iter++)
        {
            if (iter->second == crashedMap)
            {
                TC_LOG_FATAL("mapcrash", "Crashed map remove");
                m_InstancedMaps.erase(iter);
                break;
            }
        }
    }
    crashedMaps.clear();

}

void MapInstanced::DelayedUpdate(const uint32 diff)
{
    for (auto & m_InstancedMap : m_InstancedMaps)
        m_InstancedMap.second->DelayedUpdate(diff);

    Map::DelayedUpdate(diff); // this may be removed
}

void MapInstanced::MapCrashed(Map* map)
{
    TC_LOG_FATAL("mapcrash", "Prevented crash in map updater. Map: %u - InstanceId: %u", map->GetId(), map->GetInstanceId());
    std::cerr << "Prevented crash in map updater. Map: " << map->GetId() << " - InstanceId: " << map->GetInstanceId() << std::endl;

    //backtrace is generated in the signal handler in Main.cpp (unix only)

    crashedMaps.push_back(map);
}

void MapInstanced::MoveAllCreaturesInMoveList()
{
    for (auto & m_InstancedMap : m_InstancedMaps)
    {
        m_InstancedMap.second->MoveAllCreaturesInMoveList();
    }

    Map::MoveAllCreaturesInMoveList();
}

void MapInstanced::RemoveAllObjectsInRemoveList()
{
    for (auto & m_InstancedMap : m_InstancedMaps)
    {
        m_InstancedMap.second->RemoveAllObjectsInRemoveList();
    }

    Map::RemoveAllObjectsInRemoveList();
}

bool MapInstanced::RemoveBones(uint64 guid, float x, float y)
{
    bool remove_result = false;

    for (auto & m_InstancedMap : m_InstancedMaps)
    {
        remove_result = remove_result || m_InstancedMap.second->RemoveBones(guid, x, y);
    }

    return remove_result || Map::RemoveBones(guid,x,y);
}

void MapInstanced::UnloadAll()
{
    // Unload instanced maps
    for (auto & m_InstancedMap : m_InstancedMaps)
        m_InstancedMap.second->UnloadAll();

    // Delete the maps only after everything is unloaded to prevent crashes
    for (auto & m_InstancedMap : m_InstancedMaps)
        delete m_InstancedMap.second;

    m_InstancedMaps.clear();

    // Unload own grids (just dummy(placeholder) grids, neccesary to unload GridMaps!)
    Map::UnloadAll();
}

/*
- return the right instance for the object, based on its InstanceId.
- If InstanceId == 0, get it from save or generate a new one
- create the instance if it's not created already
- the player is not actually added to the instance (only in InstanceMap::Add)
*/
Map* MapInstanced::GetInstance(const WorldObject* obj)
{
    if (obj->GetTypeId() != TYPEID_PLAYER)
    {
        assert(obj->GetMapId() == GetId() && obj->GetInstanceId());
        return FindInstanceMap(obj->GetInstanceId());
    }

    Player *player = const_cast<Player*>(obj->ToPlayer());
    uint32 instanceId = player->GetInstanceId();

    if (instanceId)
    {
        if (Map *map = FindInstanceMap(instanceId))
            return map;
    }

    if (IsBattlegroundOrArena())
    {
        instanceId = player->GetBattlegroundId();

        if (instanceId)
        {
            if (Map *map = FindInstanceMap(instanceId))
                return map;
            else
            {
                if (Battleground* bg = player->GetBattleground())
                    return CreateBattleground(instanceId, bg);
            }
        } else {
            return nullptr;
        }
    }

    if (InstanceSave *pSave = player->GetInstanceSave(GetId()))
    {
        if (!instanceId || player->IsGameMaster())
        {
            instanceId = pSave->GetInstanceId(); // go from outside to instance
            if (Map *map = FindInstanceMap(instanceId))
                return map;
        }
        else if (instanceId != pSave->GetInstanceId()) // cannot go from one instance to another
            return nullptr;
        // else log in at a saved instance

        return CreateInstance(instanceId, pSave, pSave->GetDifficulty());
    }
    else if (!player->GetSession()->PlayerLoading())
    {
        if (!instanceId)
            instanceId = sMapMgr->GenerateInstanceId();

        return CreateInstance(instanceId, nullptr, player->GetDifficulty());
    }

    return nullptr;
}

Map* MapInstanced::FindInstanceMap(uint32 InstanceId)
{
    auto i = m_InstancedMaps.find(InstanceId);
    return(i == m_InstancedMaps.end() ? nullptr : i->second);
}

InstanceMap* MapInstanced::CreateInstance(uint32 InstanceId, InstanceSave *save, Difficulty difficulty)
{
    // load/create a map
    std::lock_guard<std::mutex> lock(_mapLock);

    // make sure we have a valid map id
    const MapEntry* entry = sMapStore.LookupEntry(GetId());
    if(!entry)
    {
        TC_LOG_ERROR("maps","CreateInstance: no entry for map %d", GetId());
        ABORT();
    }
    const InstanceTemplate * iTemplate = sObjectMgr->GetInstanceTemplate(GetId());
    if(!iTemplate)
    {
        TC_LOG_ERROR("maps","CreateInstance: no instance template for map %d", GetId());
        ABORT();
    }

    // some instances only have one difficulty
    GetDownscaledMapDifficultyData(GetId(), difficulty);

    TC_LOG_DEBUG("maps", "MapInstanced::CreateInstance: %s map instance %d for %d created with difficulty %s", save ? "" : "new ", InstanceId, GetId(), difficulty ? "heroic" : "normal");

    auto map = new InstanceMap(GetId(), InstanceId, difficulty);
    assert(map->IsDungeon());

    bool load_data = save != nullptr;
    map->CreateInstanceScript(load_data);

    m_InstancedMaps[InstanceId] = map;
    return map;
}

BattlegroundMap* MapInstanced::CreateBattleground(uint32 InstanceId, Battleground* bg)
{
    // load/create a map
    std::lock_guard<std::mutex> lock(_mapLock);

    TC_LOG_DEBUG("maps", "MapInstanced::CreateBattleground: map bg %d for %d created.", InstanceId, GetId());

    auto map = new BattlegroundMap(GetId(), InstanceId);
    assert(map->IsBattlegroundOrArena());
    map->SetBG(bg);

    m_InstancedMaps[InstanceId] = map;
    return map;
}

bool MapInstanced::DestroyInstance(uint32 InstanceId)
{
    auto itr = m_InstancedMaps.find(InstanceId);
    if(itr != m_InstancedMaps.end())
       return DestroyInstance(itr);

    return false;
}

// increments the iterator after erase
bool MapInstanced::DestroyInstance(InstancedMaps::iterator &itr)
{
    itr->second->RemoveAllPlayers();
    if (itr->second->HavePlayers())
    {
        ++itr;
        return false;
    }

    itr->second->UnloadAll();
    // should only unload VMaps if this is the last instance and grid unloading is enabled
    if(m_InstancedMaps.size() <= 1 && sWorld->getConfig(CONFIG_GRID_UNLOAD))
    {
        VMAP::VMapFactory::createOrGetVMapManager()->unloadMap(itr->second->GetId());
        MMAP::MMapFactory::createOrGetMMapManager()->unloadMap(itr->second->GetId());
        // in that case, unload grids of the base map, too
        // so in the next map creation, (EnsureGridCreated actually) VMaps will be reloaded
        Map::UnloadAll();
    }

    // Free up the instance id and allow it to be reused for bgs and arenas (other instances are handled in the InstanceSaveMgr)
/*TCMAP    if (itr->second->IsBattlegroundOrArena())
        sMapMgr->FreeInstanceId(itr->second->GetInstanceId()); */

    // erase map
    delete itr->second;
    m_InstancedMaps.erase(itr++);

    return true;
}

bool MapInstanced::CanEnter(Player *player)
{
    if(Map* map = GetInstance(player))
        return map->CanEnter(player);

    return false;
}

