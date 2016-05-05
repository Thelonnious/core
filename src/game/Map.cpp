/*
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
 *
 * Copyright (C) 2008-2009 Trinity <http://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "MapManager.h"
#include "Player.h"
#include "GridNotifiers.h"
#include "WorldSession.h"
#include "Log.h"
#include "CellImpl.h"
#include "InstanceScript.h"
#include "Map.h"
#include "GridNotifiersImpl.h"
#include "Transport.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "World.h"
#include "ScriptCalls.h"
#include "Group.h"
#include "MapRefManager.h"
#include "ScriptMgr.h"
#include "Util.h"
#include "Chat.h"
#include "Language.h"
#include "Weather.h"
#include "MapInstanced.h"
#include "InstanceSaveMgr.h"
#include "Management/VMapFactory.h"
#include "Management/MMapManager.h"
#include "Management/MMapFactory.h"
#include "Management/IVMapManager.h"
#include "DynamicTree.h"
#include "BattleGround.h"
#include "GridMap.h"
#include "ObjectGridLoader.h"

#define DEFAULT_GRID_EXPIRY     300
#define MAX_GRID_LOAD_TIME      50
#define MAX_CREATURE_ATTACK_RADIUS  (45.0f * sWorld->GetRate(RATE_CREATURE_AGGRO))

extern u_map_magic MapMagic;
extern u_map_magic MapVersionMagic;
extern u_map_magic MapAreaMagic;
extern u_map_magic MapHeightMagic;
extern u_map_magic MapLiquidMagic; 

ZoneDynamicInfo::ZoneDynamicInfo() : 
    MusicId(0), 
    WeatherId(WEATHER_STATE_FINE), 
    WeatherGrade(0.0f),
    OverrideLightId(0), 
    LightFadeInTime(0) 
{ }

Map::~Map()
{
    sScriptMgr->OnDestroyMap(this);

    UnloadAll();

    MMAP::MMapFactory::createOrGetMMapManager()->unloadMapInstance(GetId(), i_InstanceId);
}

void Map::LoadVMap(int x,int y)
{
    if (!VMAP::VMapFactory::createOrGetVMapManager()->isMapLoadingEnabled())
        return;
                                                            // x and y are swapped !!
    int vmapLoadResult = VMAP::VMapFactory::createOrGetVMapManager()->loadMap((sWorld->GetDataPath()+ "vmaps").c_str(),  GetId(), x,y);
    switch(vmapLoadResult)
    {
        case VMAP::VMAP_LOAD_RESULT_OK:
            TC_LOG_DEBUG("maps","VMAP loaded name:%s, id:%d, x:%d, y:%d (vmap rep.: x:%d, y:%d)", GetMapName(), GetId(), x,y, x,y);
            break;
        case VMAP::VMAP_LOAD_RESULT_ERROR:
            TC_LOG_DEBUG("maps","Could not load VMAP name:%s, id:%d, x:%d, y:%d (vmap rep.: x:%d, y:%d)", GetMapName(), GetId(), x,y, x,y);
            break;
        case VMAP::VMAP_LOAD_RESULT_IGNORED:
            TC_LOG_DEBUG("maps","Ignored VMAP name:%s, id:%d, x:%d, y:%d (vmap rep.: x:%d, y:%d)", GetMapName(), GetId(), x,y, x,y);
            break;
    }
}

void Map::LoadMap(uint32 mapid, uint32 instanceid, int x,int y)
{
    if( instanceid != 0)
    {
        if(GridMaps[x][y])
            return;

        Map* baseMap = const_cast<Map*>(sMapMgr->GetBaseMap(mapid));

        // load gridmap for base map
        if (!baseMap->GridMaps[x][y])
            baseMap->EnsureGridCreated(GridPair(63-x,63-y));

//+++        if (!baseMap->GridMaps[x][y])  don't check for GridMaps[gx][gy], we need the management for vmaps
//            return;

        ((MapInstanced*)(baseMap))->AddGridMapReference(GridPair(x,y));
        GridMaps[x][y] = baseMap->GridMaps[x][y];
        return;
    }

    //map already load, delete it before reloading (Is it necessary? Do we really need the ability the reload maps during runtime?)
    if(GridMaps[x][y])
    {
        sScriptMgr->OnUnloadGridMap(this, GridMaps[x][y], x, y);

        TC_LOG_DEBUG("maps","Unloading already loaded map %u before reloading.",mapid);
        delete (GridMaps[x][y]);
        GridMaps[x][y]=NULL;
    }

    // map file name
    char *tmp=NULL;
    // Pihhan: dataPath length + "maps/" + 3+2+2+ ".map" length may be > 32 !
    int len = sWorld->GetDataPath().length()+strlen("maps/%03u%02u%02u.map")+1;
    tmp = new char[len];
    snprintf(tmp, len, (char *)(sWorld->GetDataPath()+"maps/%03u%02u%02u.map").c_str(),mapid,x,y);
    TC_LOG_DEBUG("maps","Loading map %s",tmp);
    // loading data
    GridMaps[x][y] = new GridMap();
    if (!GridMaps[x][y]->loadData(tmp))
    {
        TC_LOG_ERROR("maps","ERROR loading map file: \n %s\n", tmp);
    }
    delete [] tmp;

    sScriptMgr->OnLoadGridMap(this, GridMaps[x][y], x, y);
}

void Map::LoadMapAndVMap(uint32 mapid, uint32 instanceid, int x,int y)
{
    LoadMap(mapid,instanceid,x,y);
    if(VMAP::VMapFactory::createOrGetVMapManager()->isMapLoadingEnabled() && instanceid == 0) {
        LoadVMap(x, y);                                     // Only load the data for the base map
        // load navmesh
        MMAP::MMapFactory::createOrGetMMapManager()->loadMap((sWorld->GetDataPath() + "mmaps").c_str(),mapid, x, y);
    }
}

Map::Map(uint32 id, uint32 InstanceId, uint8 SpawnMode)
   : i_mapEntry (sMapStore.LookupEntry(id)), i_spawnMode(SpawnMode),
   i_id(id), i_InstanceId(InstanceId), m_unloadTimer(0), 
   m_VisibleDistance(DEFAULT_VISIBILITY_DISTANCE),
   m_activeForcedNonPlayersIter(m_activeForcedNonPlayers.end()), 
   _transportsUpdateIter(_transports.end()),
   _defaultLight(GetDefaultMapLight(id)),
   i_lock(true)
{
    for(unsigned int idx=0; idx < MAX_NUMBER_OF_GRIDS; ++idx)
    {
        for(unsigned int j=0; j < MAX_NUMBER_OF_GRIDS; ++j)
        {
            //z code
            GridMaps[idx][j] =NULL;
            setNGrid(NULL, idx, j);
        }
    }

    Map::InitVisibilityDistance();

    sScriptMgr->OnCreateMap(this);
}

void Map::InitVisibilityDistance()
{
    //init visibility for continents
    m_VisibleDistance = sWorld->GetMaxVisibleDistanceOnContinents();
}

// Template specialization of utility methods
template<class T>
void Map::AddToGrid(T* obj, NGridType *grid, Cell const& cell)
{
    (*grid)(cell.CellX(), cell.CellY()).template AddGridObject<T>(obj);
    obj->SetCurrentCell(cell);
}

template<>
void Map::AddToGrid(Player* obj, NGridType *grid, Cell const& cell)
{
    (*grid)(cell.CellX(), cell.CellY()).AddWorldObject(obj);
    obj->SetCurrentCell(cell);
}

template<>
void Map::AddToGrid(Corpse *obj, NGridType *grid, Cell const& cell)
{
    // add to world object registry in grid
    if(obj->GetType()!=CORPSE_BONES)
    {
        (*grid)(cell.CellX(), cell.CellY()).AddWorldObject(obj);
    }
    // add to grid object store
    else
    {
        (*grid)(cell.CellX(), cell.CellY()).AddGridObject(obj);
    }
    obj->SetCurrentCell(cell);
}

template<>
void Map::AddToGrid(Creature* obj, NGridType *grid, Cell const& cell)
{
    // add to world object registry in grid
    if(obj->IsPet() || obj->IsTempWorldObject)
    {
        (*grid)(cell.CellX(), cell.CellY()).AddWorldObject<Creature>(obj);
    }
    // add to grid object store
    else
    {
        (*grid)(cell.CellX(), cell.CellY()).AddGridObject<Creature>(obj);
    }
    obj->SetCurrentCell(cell);
}

template<>
void Map::AddToGrid(DynamicObject* obj, NGridType *grid, Cell const& cell)
{
    if(obj->isActiveObject()) // only farsight
        (*grid)(cell.CellX(), cell.CellY()).AddWorldObject<DynamicObject>(obj);
    else
        (*grid)(cell.CellX(), cell.CellY()).AddGridObject<DynamicObject>(obj);
    obj->SetCurrentCell(cell);
}

template<class T>
void Map::RemoveFromGrid(T* obj, NGridType *grid, Cell const& cell)
{
#ifdef TRINITY_DEBUG
    if (dynamic_cast<MotionTransport*>(obj))
        ASSERT("Map::RemoveFromGrid called with a transport object " && false); //transports should never be removed from map
#endif
    (*grid)(cell.CellX(), cell.CellY()).template RemoveGridObject<T>(obj);
}

template<>
void Map::RemoveFromGrid(Player* obj, NGridType *grid, Cell const& cell)
{
    (*grid)(cell.CellX(), cell.CellY()).RemoveWorldObject(obj);
}

template<>
void Map::RemoveFromGrid(Corpse *obj, NGridType *grid, Cell const& cell)
{
    // remove from world object registry in grid
    if(obj->GetType()!=CORPSE_BONES)
    {
        (*grid)(cell.CellX(), cell.CellY()).RemoveWorldObject(obj);
    }
    // remove from grid object store
    else
    {
        (*grid)(cell.CellX(), cell.CellY()).RemoveGridObject(obj);
    }
}

template<>
void Map::RemoveFromGrid(Creature* obj, NGridType *grid, Cell const& cell)
{
    // remove from world object registry in grid
    if(obj->IsPet() || obj->IsTempWorldObject)
    {
        (*grid)(cell.CellX(), cell.CellY()).RemoveWorldObject<Creature>(obj);
    }
    // remove from grid object store
    else
    {
        (*grid)(cell.CellX(), cell.CellY()).RemoveGridObject<Creature>(obj);
    }
}

template<>
void Map::RemoveFromGrid(DynamicObject* obj, NGridType *grid, Cell const& cell)
{
    if(obj->isActiveObject()) // only farsight
        (*grid)(cell.CellX(), cell.CellY()).RemoveWorldObject<DynamicObject>(obj);
    else
        (*grid)(cell.CellX(), cell.CellY()).RemoveGridObject<DynamicObject>(obj);
}

template<class T>
void Map::SwitchGridContainers(T* obj, bool on)
{
    CellCoord p = Trinity::ComputeCellCoord(obj->GetPositionX(), obj->GetPositionY());
    if (!p.IsCoordValid())
    {
        TC_LOG_ERROR("maps", "Map::SwitchGridContainers: Object " UI64FMTD " have invalid coordinates X:%f Y:%f grid cell [%u:%u]", obj->GetGUID(), obj->GetPositionX(), obj->GetPositionY(), p.x_coord, p.y_coord);
        return;
    }

    Cell cell(p);
    if( !loaded(GridPair(cell.data.Part.grid_x, cell.data.Part.grid_y)))
        return;

    TC_LOG_DEBUG("maps","Switch object " UI64FMTD " from grid[%u,%u] %u", obj->GetGUID(), cell.data.Part.grid_x, cell.data.Part.grid_y, on);
    NGridType *ngrid = getNGrid(cell.GridX(), cell.GridY());
    assert( ngrid != NULL);

    GridType &grid = (*ngrid)(cell.CellX(), cell.CellY());

    if(on)
    {
        grid.RemoveGridObject<T>(obj);
        grid.AddWorldObject<T>(obj);
    }
    else
    {
        grid.RemoveWorldObject<T>(obj);
        grid.AddGridObject<T>(obj);
    }
    obj->IsTempWorldObject = on;
}

template void Map::SwitchGridContainers(Creature *, bool);
template void Map::SwitchGridContainers(DynamicObject *, bool);

template<class T>
void Map::DeleteFromWorld(T* obj)
{
    // Note: In case resurrectable corpse and pet its removed from global lists in own destructor
    delete obj;
}

template<class T>
void Map::AddNotifier(T*)
{
}

template<>
void Map::AddNotifier(Player* obj)
{
    obj->m_Notified = false;
    obj->m_IsInNotifyList = false;
    AddUnitToNotify(obj);
}

template<>
void Map::AddNotifier(Creature* obj)
{
    obj->m_Notified = false;
    obj->m_IsInNotifyList = false;
    AddUnitToNotify(obj);
}

void Map::EnsureGridCreated(const GridPair &p)
{
    std::lock_guard<std::mutex> lock(_gridLock);
    EnsureGridCreated_i(p);
}

//Create NGrid so the object can be added to it
//But object data is not loaded here
void Map::EnsureGridCreated_i(const GridPair &p)
{
    if (!getNGrid(p.x_coord, p.y_coord))
    {
        if (!getNGrid(p.x_coord, p.y_coord))
        {
            setNGrid(new NGridType(p.x_coord*MAX_NUMBER_OF_GRIDS + p.y_coord, p.x_coord, p.y_coord),
                p.x_coord, p.y_coord);

            // build a linkage between this map and NGridType
            buildNGridLinkage(getNGrid(p.x_coord, p.y_coord));

            //z coord
            int gx = (MAX_NUMBER_OF_GRIDS - 1) - p.x_coord;
            int gy = (MAX_NUMBER_OF_GRIDS - 1) - p.y_coord;

            if (!GridMaps[gx][gy])
                Map::LoadMapAndVMap(i_id, i_InstanceId, gx, gy);
        }
    }
}

void
Map::EnsureGridLoaded(const Cell &cell, Player *player)
{
    EnsureGridCreated(GridPair(cell.GridX(), cell.GridY()));
    NGridType *grid = getNGrid(cell.GridX(), cell.GridY());

    assert(grid != NULL);
    if (!isGridObjectDataLoaded(cell.GridX(), cell.GridY()))
    {
        if (player)
        {
            player->SendDelayResponse(MAX_GRID_LOAD_TIME);
            //TC_LOG_DEBUG("maps","Player %s enter cell[%u,%u] triggers of loading grid[%u,%u] on map %u", player->GetName(), cell.CellX(), cell.CellY(), cell.GridX(), cell.GridY(), i_id);
        }
        else
        {
            TC_LOG_DEBUG("maps", "Active object nearby triggers of loading grid [%u,%u] on map %u", cell.GridX(), cell.GridY(), i_id);
        }

        setGridObjectDataLoaded(true, cell.GridX(), cell.GridY());
        ObjectGridLoader loader(*grid, this, cell);
        loader.LoadN();

        // Add resurrectable corpses to world object list in grid
        sObjectAccessor->AddCorpsesToGrid(GridPair(cell.GridX(), cell.GridY()), (*grid)(cell.CellX(), cell.CellY()), this);
        Balance();
    }

    if(player)
        AddToGrid(player,grid,cell);
}

void Map::LoadGrid(float x, float y)
{
    CellCoord pair = Trinity::ComputeCellCoord(x, y);
    Cell cell(pair);
    EnsureGridLoaded(cell);
}

bool Map::Add(Player *player)
{
    player->GetMapRef().link(this, player);

    player->SetInstanceId(GetInstanceId());

    // update player state for other player and visa-versa
    CellCoord p = Trinity::ComputeCellCoord(player->GetPositionX(), player->GetPositionY());
    Cell cell(p);
    EnsureGridLoaded(cell, player);
    player->AddToWorld();

    SendInitSelf(player);
    SendInitTransports(player);
    SendZoneDynamicInfo(player);

    player->m_clientGUIDs.clear();
    //AddNotifier(player);

    sScriptMgr->OnPlayerEnterMap(this, player);

    return true;
}

template<>
bool Map::Add(MotionTransport* obj, bool /* checkTransport */)
{
    assert(obj);

    CellCoord p = Trinity::ComputeCellCoord(obj->GetPositionX(), obj->GetPositionY());
    if (!p.IsCoordValid())
    {
        TC_LOG_ERROR("maps","Map::Add: Object " UI64FMTD " have invalid coordinates X:%f Y:%f grid cell [%u:%u]", obj->GetGUID(), obj->GetPositionX(), obj->GetPositionY(), p.x_coord, p.y_coord);
        return false;
    }

    Cell cell(p);
    if (obj->isActiveObject())
        EnsureGridLoaded(cell);

    obj->AddToWorld();

    if (obj->isActiveObject())
        AddToForceActive(obj);

    //DO NOT ADD TO GRID. Else transport will be removed with grid unload. Transports are being kept updated even in unloaded grid.
    _transports.insert(obj);

    // Broadcast creation to players
    if (!GetPlayers().isEmpty())
    {
        for (Map::PlayerList::const_iterator itr = GetPlayers().begin(); itr != GetPlayers().end(); ++itr)
        {
            if (itr->GetSource()->GetTransport() != obj)
            {
                UpdateData data;
                obj->BuildCreateUpdateBlockForPlayer(&data, itr->GetSource());
                WorldPacket packet;
                data.BuildPacket(&packet,true);
                itr->GetSource()->SendDirectMessage(&packet);
            }
        }
    }

    return true;
}

template<class T>
bool Map::Add(T *obj, bool checkTransport)
{
    CellCoord p = Trinity::ComputeCellCoord(obj->GetPositionX(), obj->GetPositionY());

    assert(obj);

    if (!p.IsCoordValid())
    {
        TC_LOG_ERROR("maps","Map::Add: Object " UI64FMTD " have invalid coordinates X:%f Y:%f grid cell [%u:%u]", obj->GetGUID(), obj->GetPositionX(), obj->GetPositionY(), p.x_coord, p.y_coord);
        return false;
    }

    Cell cell(p);
    if(obj->isActiveObject())
        EnsureGridLoaded(cell);
    else
        EnsureGridCreated(GridPair(cell.GridX(), cell.GridY()));

    NGridType *grid = getNGrid(cell.GridX(), cell.GridY());
    assert( grid != NULL);

    AddToGrid(obj,grid,cell);
    obj->AddToWorld();
    
    if (checkTransport)
        if (!(obj->GetTypeId() == TYPEID_GAMEOBJECT && obj->ToGameObject()->IsTransport())) // dont add transport to transport ;d
            if (Transport* transport = GetTransportForPos(obj->GetPhaseMask(), obj->GetPositionX(), obj->GetPositionY(), obj->GetPositionZ(), obj))
                transport->AddPassenger(obj, true);

    if(obj->isActiveObject())
        AddToForceActive(obj);

    TC_LOG_DEBUG("maps","Object %u enters grid[%u,%u]", GUID_LOPART(obj->GetGUID()), cell.GridX(), cell.GridY());

    UpdateObjectVisibility(obj,cell,p);

    //AddNotifier(obj);
    return true;
}

void Map::MessageBroadcast(Player *player, WorldPacket *msg, bool to_self, bool to_possessor)
{
    CellCoord p = Trinity::ComputeCellCoord(player->GetPositionX(), player->GetPositionY());

    if (!p.IsCoordValid())
    {
        TC_LOG_ERROR("maps","Map::MessageBroadcast: Player (GUID: %u) have invalid coordinates X:%f Y:%f grid cell [%u:%u]", player->GetGUIDLow(), player->GetPositionX(), player->GetPositionY(), p.x_coord, p.y_coord);
        return;
    }

    Cell cell(p);
    cell.data.Part.reserved = ALL_DISTRICT;

    if( !loaded(GridPair(cell.data.Part.grid_x, cell.data.Part.grid_y)))
        return;

    Trinity::MessageDeliverer post_man(*player, msg, to_possessor, to_self);
    TypeContainerVisitor<Trinity::MessageDeliverer, WorldTypeMapContainer > message(post_man);
    cell.Visit(p, message, *this, *player, GetVisibilityRange());
}

void Map::MessageBroadcast(WorldObject *obj, WorldPacket *msg, bool to_possessor)
{
    CellCoord p = Trinity::ComputeCellCoord(obj->GetPositionX(), obj->GetPositionY());
    if (!p.IsCoordValid())
    {
        TC_LOG_ERROR("maps","Map::MessageBroadcast: Object " UI64FMTD " have invalid coordinates X:%f Y:%f grid cell [%u:%u]", obj->GetGUID(), obj->GetPositionX(), obj->GetPositionY(), p.x_coord, p.y_coord);
        return;
    }

    Cell cell(p);
    cell.data.Part.reserved = ALL_DISTRICT;
    cell.SetNoCreate();

    if( !loaded(GridPair(cell.data.Part.grid_x, cell.data.Part.grid_y)))
        return;

    Trinity::ObjectMessageDeliverer post_man(*obj, msg, to_possessor);
    TypeContainerVisitor<Trinity::ObjectMessageDeliverer, WorldTypeMapContainer > message(post_man);
    cell.Visit(p, message, *this, *obj, GetVisibilityRange());
}

void Map::MessageDistBroadcast(Player *player, WorldPacket *msg, float dist, bool to_self, bool to_possessor, bool own_team_only)
{
    CellCoord p = Trinity::ComputeCellCoord(player->GetPositionX(), player->GetPositionY());

    if (!p.IsCoordValid())
    {
        TC_LOG_ERROR("maps","Map::MessageBroadcast: Player (GUID: %u) have invalid coordinates X:%f Y:%f grid cell [%u:%u]", player->GetGUIDLow(), player->GetPositionX(), player->GetPositionY(), p.x_coord, p.y_coord);
        return;
    }

    Cell cell(p);
    cell.data.Part.reserved = ALL_DISTRICT;

    if( !loaded(GridPair(cell.data.Part.grid_x, cell.data.Part.grid_y)))
        return;

    Trinity::MessageDistDeliverer post_man(*player, msg, to_possessor, dist, to_self, own_team_only);
    TypeContainerVisitor<Trinity::MessageDistDeliverer , WorldTypeMapContainer > message(post_man);
    cell.Visit(p, message, *this, *player, dist);
}

void Map::MessageDistBroadcast(WorldObject *obj, WorldPacket *msg, float dist, bool to_possessor)
{
    CellCoord p = Trinity::ComputeCellCoord(obj->GetPositionX(), obj->GetPositionY());

    if (!p.IsCoordValid())
    {
        TC_LOG_ERROR("maps","Map::MessageBroadcast: Object " UI64FMTD " have invalid coordinates X:%f Y:%f grid cell [%u:%u]", obj->GetGUID(), obj->GetPositionX(), obj->GetPositionY(), p.x_coord, p.y_coord);
        return;
    }

    Cell cell(p);
    cell.data.Part.reserved = ALL_DISTRICT;
    cell.SetNoCreate();

    if( !loaded(GridPair(cell.data.Part.grid_x, cell.data.Part.grid_y)))
        return;

    Trinity::ObjectMessageDistDeliverer post_man(*obj, msg, to_possessor, dist);
    TypeContainerVisitor<Trinity::ObjectMessageDistDeliverer, WorldTypeMapContainer > message(post_man);
    cell.Visit(p, message, *this, *obj, dist);
}

bool Map::loaded(const GridPair &p) const
{
    return ( getNGrid(p.x_coord, p.y_coord) && isGridObjectDataLoaded(p.x_coord, p.y_coord));
}

void Map::RelocationNotify()
{
    //Move backlog to notify list
    for(std::vector<uint64>::iterator iter = i_unitsToNotifyBacklog.begin(); iter != i_unitsToNotifyBacklog.end(); ++iter)
    {
        if(Unit *unit = ObjectAccessor::GetObjectInWorld(*iter, (Unit*)NULL))
        {
            i_unitsToNotify.push_back(unit);
        }
    }
    i_unitsToNotifyBacklog.clear();

    //Notify
    for(std::vector<Unit*>::iterator iter = i_unitsToNotify.begin(); iter != i_unitsToNotify.end(); ++iter)
    {
        Unit *unit = *iter;
        if(unit->m_Notified || !unit->IsInWorld() || unit->GetMapId() != GetId())
            continue;

        unit->m_Notified = true;
        unit->m_IsInNotifyList = false;
        
        float dist = std::fabs(unit->GetPositionX() - unit->oldX) + std::fabs(unit->GetPositionY() - unit->oldY);
        
        if(dist > 10.0f)
        {
            Trinity::VisibleChangesNotifier notifier(*unit);
            VisitWorld(unit->oldX, unit->oldY, GetVisibilityRange(), notifier);
            dist = 0;
        }
        
        if(unit->GetTypeId() == TYPEID_PLAYER)
        {
            Trinity::PlayerRelocationNotifier notifier(*(unit->ToPlayer()));
            VisitAll(unit->GetPositionX(), unit->GetPositionY(), unit->GetMap()->GetVisibilityRange() + dist, notifier);
            //also keep/update targets near our farsight target if we're updated
            if(WorldObject* farsightTarget = unit->ToPlayer()->GetFarsightTarget())
                VisitAll(farsightTarget->GetPositionX(), farsightTarget->GetPositionY(), farsightTarget->GetMap()->GetVisibilityRange() + dist, notifier);
            notifier.Notify();
        }
        else
        {
            Trinity::CreatureRelocationNotifier notifier(*(unit->ToCreature()));
            VisitAll(unit->GetPositionX(), unit->GetPositionY(), unit->GetMap()->GetVisibilityRange() + dist, notifier);
        }

    }
    for(std::vector<Unit*>::iterator iter = i_unitsToNotify.begin(); iter != i_unitsToNotify.end(); ++iter)
    {
        (*iter)->m_Notified = false;
    }
    i_unitsToNotify.clear();
}

void Map::AddUnitToNotify(Unit* u)
{
    if(u->m_IsInNotifyList)
        return;

    u->m_IsInNotifyList = true;
    u->oldX = u->GetPositionX();
    u->oldY = u->GetPositionY();
    u->oldZ = u->GetPositionZ();

    if(i_lock)
        i_unitsToNotifyBacklog.push_back(u->GetGUID());
    else
        i_unitsToNotify.push_back(u);
}

void Map::VisitNearbyCellsOf(WorldObject* obj, TypeContainerVisitor<Trinity::ObjectUpdater, GridTypeMapContainer> &gridVisitor, 
    TypeContainerVisitor<Trinity::ObjectUpdater, WorldTypeMapContainer> &worldVisitor)
{
    CellCoord standing_cell(Trinity::ComputeCellCoord(obj->GetPositionX(), obj->GetPositionY()));

    // Check for correctness of standing_cell, it also avoids problems with update_cell
    if (!standing_cell.IsCoordValid())
        return;

    // the overloaded operators handle range checking
    // so ther's no need for range checking inside the loop
    CellCoord begin_cell(standing_cell), end_cell(standing_cell);
    CellArea area = Cell::CalculateCellArea(*obj, GetVisibilityRange());
    area.ResizeBorders(begin_cell, end_cell);

    for(uint32 x = begin_cell.x_coord; x <= end_cell.x_coord; ++x)
    {
        for(uint32 y = begin_cell.y_coord; y <= end_cell.y_coord; ++y)
        {
            // marked cells are those that have been visited
            // don't visit the same cell twice
            uint32 cell_id = (y * TOTAL_NUMBER_OF_CELLS_PER_MAP) + x;
            if(!isCellMarked(cell_id))
            {
                markCell(cell_id);
                CellCoord pair(x,y);
                Cell cell(pair);
                cell.data.Part.reserved = CENTER_DISTRICT;
                //cell.SetNoCreate();
                cell.Visit(pair, gridVisitor,  *this);
                cell.Visit(pair, worldVisitor, *this);
            }
        }
    }
}

void Map::Update(const uint32 &t_diff)
{
    i_lock = false;

    _dynamicTree.update(t_diff);
    /// update worldsessions for existing players
    for(m_mapRefIter = m_mapRefManager.begin(); m_mapRefIter != m_mapRefManager.end(); ++m_mapRefIter)
    {
        Player* plr = m_mapRefIter->GetSource();
        if(plr && plr->IsInWorld())
        {
            //plr->Update(t_diff);
            WorldSession * pSession = plr->GetSession();
            MapSessionFilter updater(pSession);

            pSession->Update(t_diff, updater);
        }
    }

    resetMarkedCells();

    Trinity::ObjectUpdater updater(t_diff);
    // for creature
    TypeContainerVisitor<Trinity::ObjectUpdater, GridTypeMapContainer  > grid_object_update(updater);
    // for pets
    TypeContainerVisitor<Trinity::ObjectUpdater, WorldTypeMapContainer > world_object_update(updater);

    // the player iterator is stored in the map object
    // to make sure calls to Map::Remove don't invalidate it
    for(m_mapRefIter = m_mapRefManager.begin(); m_mapRefIter != m_mapRefManager.end(); ++m_mapRefIter)
    {
        Player* plr = m_mapRefIter->GetSource();

        if (!plr || !plr->IsInWorld())
            continue;

        // update players at tick
        plr->Update(t_diff);

        VisitNearbyCellsOf(plr, grid_object_update, world_object_update);
    }

    //must be done before creatures update
    for (auto itr : CreatureGroupHolder)
        itr.second->Update(t_diff);

    // non-player active objects
    if(!m_activeForcedNonPlayers.empty())
    {
        for(m_activeForcedNonPlayersIter = m_activeForcedNonPlayers.begin(); m_activeForcedNonPlayersIter != m_activeForcedNonPlayers.end();)
        {
            // skip not in world
            WorldObject* obj = *m_activeForcedNonPlayersIter;

            // step before processing, in this case if Map::Remove remove next object we correctly
            // step to next-next, and if we step to end() then newly added objects can wait next update.
            ++m_activeForcedNonPlayersIter;

            if (!obj || !obj->IsInWorld())
                continue;

            // Update bindsight players
            if(obj->isType(TYPEMASK_UNIT))
            {
                if(!((Unit*)obj)->GetSharedVisionList().empty()) {
                    for(SharedVisionList::const_iterator itr = ((Unit*)obj)->GetSharedVisionList().begin(); itr != ((Unit*)obj)->GetSharedVisionList().end(); ++itr)
                    {
                        if(!*itr)
                        {
                            TC_LOG_ERROR("maps","unit %u has invalid shared vision player, list size %u", obj->GetEntry(), uint32(((Unit*)obj)->GetSharedVisionList().size()));
                            continue;
                        }
                        if(Player* p = ObjectAccessor::FindPlayer(*itr))
                        {
                            Trinity::PlayerVisibilityNotifier notifier(*p);
                            VisitAll(obj->GetPositionX(), obj->GetPositionY(), World::GetMaxVisibleDistanceForObject(), notifier);
                            VisitAll(p->GetPositionX(), p->GetPositionY(), World::GetMaxVisibleDistanceForObject(), notifier);
                            notifier.Notify();
                        }
                    }
                }
            }
            else if(obj->GetTypeId() == TYPEID_DYNAMICOBJECT)
            {
                if(Unit *caster = ((DynamicObject*)obj)->GetCaster())
                    if(caster->GetTypeId() == TYPEID_PLAYER && caster->GetUInt64Value(PLAYER_FARSIGHT) == obj->GetGUID())
                    {
                        Trinity::PlayerVisibilityNotifier notifier(*(caster->ToPlayer()));
                        VisitAll(obj->GetPositionX(), obj->GetPositionY(), GetVisibilityRange(), notifier);
                        VisitAll(caster->GetPositionX(), caster->GetPositionY(), GetVisibilityRange(), notifier);
                        notifier.Notify();
                    }
            }

            VisitNearbyCellsOf(obj, grid_object_update, world_object_update);
        }
    }

    //update our transports
    for (_transportsUpdateIter = _transports.begin(); _transportsUpdateIter != _transports.end();)
    {
        WorldObject* obj = *_transportsUpdateIter;
        ++_transportsUpdateIter;

        if (!obj->IsInWorld())
            continue;

        obj->Update(t_diff);
    }

    i_lock = true;

    MoveAllCreaturesInMoveList();
    MoveAllGameObjectsInMoveList();

    RelocationNotify();
    RemoveAllObjectsInRemoveList();

    // Don't unload grids if it's battleground, since we may have manually added GOs,creatures, those doesn't load from DB at grid re-load !
    // This isn't really bother us, since as soon as we have instanced BG-s, the whole map unloads as the BG gets ended
    if (IsBattlegroundOrArena())
        return;

    sScriptMgr->OnMapUpdate(this, t_diff);
}

void Map::Remove(Player *player, bool remove)
{
    ASSERT(player);

    // this may be called during Map::Update
    // after decrement+unlink, ++m_mapRefIter will continue correctly
    // when the first element of the list is being removed
    // nocheck_prev will return the padding element of the RefManager
    // instead of NULL in the case of prev
    if(m_mapRefIter == player->GetMapRef())
        m_mapRefIter = m_mapRefIter->nocheck_prev();
    player->GetMapRef().unlink();
    CellCoord p = Trinity::ComputeCellCoord(player->GetPositionX(), player->GetPositionY());
    if(!p.IsCoordValid())
    {
        // invalid coordinates
        player->RemoveFromWorld();

        if( remove)
            DeleteFromWorld(player);

        return;
    }

    Cell cell(p);

    if( !getNGrid(cell.data.Part.grid_x, cell.data.Part.grid_y))
    {
        TC_LOG_ERROR("maps","Map::Remove() i_grids was NULL x:%d, y:%d",cell.data.Part.grid_x,cell.data.Part.grid_y);
        return;
    }

    //TC_LOG_DEBUG("maps","Remove player %s from grid[%u,%u]", player->GetName(), cell.GridX(), cell.GridY());
    NGridType *grid = getNGrid(cell.GridX(), cell.GridY());
    assert(grid != NULL);

    player->RemoveFromWorld();
    RemoveFromGrid(player,grid,cell);

    SendRemoveTransports(player);
    UpdateObjectVisibility(player,cell,p);

    if (remove)
    {
        DeleteFromWorld(player);
        sScriptMgr->OnPlayerLeaveMap(this, player);
    }
}

bool Map::RemoveBones(uint64 guid, float x, float y)
{
   // if (IsRemovalGrid(x, y))
    //{
        Corpse * corpse = sObjectAccessor->GetObjectInWorld(GetId(), x, y, guid, (Corpse*)NULL);
        if(corpse && corpse->GetTypeId() == TYPEID_CORPSE && corpse->GetType() == CORPSE_BONES)
            corpse->DeleteBonesFromWorld();
     //   else
       //     return false;
    //}
    return true;
}

template<class T>
void
Map::Remove(T *obj, bool remove)
{
    CellCoord p = Trinity::ComputeCellCoord(obj->GetPositionX(), obj->GetPositionY());
    if (!p.IsCoordValid())
    {
        TC_LOG_ERROR("maps","Map::Remove: Object " UI64FMTD " have invalid coordinates X:%f Y:%f grid cell [%u:%u]", obj->GetGUID(), obj->GetPositionX(), obj->GetPositionY(), p.x_coord, p.y_coord);
        return;
    }

    Cell cell(p);
    if( !loaded(GridPair(cell.data.Part.grid_x, cell.data.Part.grid_y)))
        return;

    TC_LOG_DEBUG("maps","Remove object " UI64FMTD " from grid[%u,%u]", obj->GetGUID(), cell.data.Part.grid_x, cell.data.Part.grid_y);
    NGridType *grid = getNGrid(cell.GridX(), cell.GridY());
    assert( grid != NULL);

    obj->RemoveFromWorld();
    if(obj->isActiveObject())
        RemoveFromForceActive(obj);
    RemoveFromGrid(obj,grid,cell);

    UpdateObjectVisibility(obj,cell,p);

    if( remove)
    {
        // if option set then object already saved at this moment
        if(!sWorld->getConfig(CONFIG_SAVE_RESPAWN_TIME_IMMEDIATELY))
            obj->SaveRespawnTime();
        DeleteFromWorld(obj);
    }
}

template<>
void Map::Remove(MotionTransport *obj, bool remove)
{
    obj->RemoveFromWorld();
    if (obj->isActiveObject())
        RemoveFromForceActive(obj);

    //remove for players in map
    Map::PlayerList const& players = GetPlayers();
    if (!players.isEmpty())
    {
        UpdateData data;
        obj->BuildOutOfRangeUpdateBlock(&data);
        WorldPacket packet;
        data.BuildPacket(&packet);
        for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr)
        if (itr->GetSource()->GetTransport() != obj)
            itr->GetSource()->SendDirectMessage(&packet);
    }

    if (_transportsUpdateIter != _transports.end())
    {
        TransportsContainer::iterator itr = _transports.find(obj);
        if (itr == _transports.end())
            return;
        if (itr == _transportsUpdateIter)
            ++_transportsUpdateIter;

        _transports.erase(itr);
    }
    else
        _transports.erase(obj);

    obj->SetMapId(0);

    if (remove)
    {
        // if option set then object already saved at this moment
        if (!sWorld->getBoolConfig(CONFIG_SAVE_RESPAWN_TIME_IMMEDIATELY))
            obj->SaveRespawnTime();
        DeleteFromWorld(obj);
    }
}

void Map::PlayerRelocation(Player *player, float x, float y, float z, float orientation)
{
    assert(player);

    CellCoord old_val = Trinity::ComputeCellCoord(player->GetPositionX(), player->GetPositionY());
    CellCoord new_val = Trinity::ComputeCellCoord(x, y);

    Cell old_cell(old_val);
    Cell new_cell(new_val);
    new_cell |= old_cell;
    bool same_cell = (new_cell == old_cell);

    player->Relocate(x, y, z, orientation);

    if( old_cell.DiffGrid(new_cell) || old_cell.DiffCell(new_cell))
    {
        //TC_LOG_DEBUG("maps","Player %s relocation grid[%u,%u]cell[%u,%u]->grid[%u,%u]cell[%u,%u]", player->GetName(), old_cell.GridX(), old_cell.GridY(), old_cell.CellX(), old_cell.CellY(), new_cell.GridX(), new_cell.GridY(), new_cell.CellX(), new_cell.CellY());

        // update player position for group at taxi flight
        if(player->GetGroup() && player->IsInFlight())
            player->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_POSITION);

        NGridType* oldGrid = getNGrid(old_cell.GridX(), old_cell.GridY());
        RemoveFromGrid(player, oldGrid,old_cell);
        if( !old_cell.DiffGrid(new_cell))
            AddToGrid(player, oldGrid,new_cell);
        else
            EnsureGridLoaded(new_cell, player);
    }

    AddUnitToNotify(player);
}

void Map::CreatureRelocation(Creature *creature, float x, float y, float z, float ang)
{
    assert(CheckGridIntegrity(creature,false));

    Cell old_cell = creature->GetCurrentCell();

    CellCoord new_val = Trinity::ComputeCellCoord(x, y);
    Cell new_cell(new_val);

    // delay creature move for grid/cell to grid/cell moves
    if( old_cell.DiffCell(new_cell) || old_cell.DiffGrid(new_cell))
    {
        TC_LOG_DEBUG("debug.creature","Creature (GUID: %u Entry: %u) added to moving list from grid[%u,%u]cell[%u,%u] to grid[%u,%u]cell[%u,%u].", creature->GetGUIDLow(), creature->GetEntry(), old_cell.GridX(), old_cell.GridY(), old_cell.CellX(), old_cell.CellY(), new_cell.GridX(), new_cell.GridY(), new_cell.CellX(), new_cell.CellY());
        AddCreatureToMoveList(creature,x,y,z,ang);
        // in diffcell/diffgrid case notifiers called at finishing move creature in Map::MoveAllCreaturesInMoveList
    }
    else
    {
        creature->Relocate(x, y, z, ang);
#ifdef LICH_KING
        if (creature->IsVehicle())
            creature->GetVehicleKit()->RelocatePassengers();
#endif
        AddUnitToNotify(creature);
    }
    assert(CheckGridIntegrity(creature,true));
}

void Map::GameObjectRelocation(GameObject* go, float x, float y, float z, float ang)
{
    assert(CheckGridIntegrity(go,false));

    Cell old_cell = go->GetCurrentCell();

    CellCoord new_val = Trinity::ComputeCellCoord(x, y);
    Cell new_cell(new_val);

    // delay creature move for grid/cell to grid/cell moves
    if( old_cell.DiffCell(new_cell) || old_cell.DiffGrid(new_cell))
    {
        TC_LOG_DEBUG("debug.creature","GameObject (GUID: %u Entry: %u) added to moving list from grid[%u,%u]cell[%u,%u] to grid[%u,%u]cell[%u,%u].", go->GetGUIDLow(), go->GetEntry(), old_cell.GridX(), old_cell.GridY(), old_cell.CellX(), old_cell.CellY(), new_cell.GridX(), new_cell.GridY(), new_cell.CellX(), new_cell.CellY());
        AddGameObjectToMoveList(go, x, y, z, ang);
        // in diffcell/diffgrid case notifiers called at finishing move creature in Map::MoveAllCreaturesInMoveList
    }
    else
    {
        go->Relocate(x, y, z, ang);
        go->UpdateModelPosition();
        UpdateObjectVisibility(go,new_cell,new_val);
    }
    assert(CheckGridIntegrity(go,true));
}

void Map::AddCreatureToMoveList(Creature *c, float x, float y, float z, float ang)
{
    if(!c)
        return;

    i_creaturesToMove[c] = ObjectMover(x,y,z,ang);
}

void Map::AddGameObjectToMoveList(GameObject* go, float x, float y, float z, float ang)
{
    if(!go)
        return;

    i_gameObjectsToMove[go] = ObjectMover(x,y,z,ang);
}

void Map::MoveAllCreaturesInMoveList()
{
    while(!i_creaturesToMove.empty())
    {
        // get data and remove element;
        CreatureMoveList::iterator iter = i_creaturesToMove.begin();
        Creature* c = iter->first;
        ObjectMover cm = iter->second;
        i_creaturesToMove.erase(iter);

        // calculate cells
        CellCoord new_val = Trinity::ComputeCellCoord(cm.x, cm.y);
        Cell new_cell(new_val);

        // do move or do move to respawn or remove creature if previous all fail
        if(CreatureCellRelocation(c,new_cell))
        {
            // update pos
            c->Relocate(cm.x, cm.y, cm.z, cm.ang);
            //CreatureRelocationNotify(c,new_cell,new_cell.cellPair());
            AddUnitToNotify(c);
        }
        else
        {
            // if creature can't be move in new cell/grid (not loaded) move it to repawn cell/grid
            // creature coordinates will be updated and notifiers send
            if(!CreatureRespawnRelocation(c,false))
            {
                // ... or unload (if respawn grid also not loaded)
                TC_LOG_DEBUG("debug.creature","Creature (GUID: %u Entry: %u) can't be move to unloaded respawn grid.",c->GetGUIDLow(),c->GetEntry());
                /// @todo pets will disappear if this is outside CreatureRespawnRelocation
                //need to check why pet is frequently relocated to an unloaded cell
                if (c->IsPet())
                    ((Pet*)c)->Remove(PET_SAVE_NOT_IN_SLOT, true);
                else 
                    AddObjectToRemoveList(c);
            }
        }
    }
}

void Map::MoveAllGameObjectsInMoveList()
{
    while(!i_creaturesToMove.empty())
    {
        // get data and remove element;
        GameObjectMoveList::iterator iter = i_gameObjectsToMove.begin();
        GameObject* go = iter->first;
        ObjectMover mover = iter->second;
        i_gameObjectsToMove.erase(iter);

        // calculate cells
        CellCoord new_val = Trinity::ComputeCellCoord(mover.x, mover.y);
        Cell new_cell(new_val);

        // do move or do move to respawn or remove creature if previous all fail
        if(GameObjectCellRelocation(go,new_cell))
        {
            // update pos
            go->Relocate(mover.x, mover.y, mover.z, mover.ang);
            go->UpdateModelPosition();
            UpdateObjectVisibility(go,new_cell,new_val);
        }
        else
        {
            // if creature can't be move in new cell/grid (not loaded) move it to repawn cell/grid
            // creature coordinates will be updated and notifiers send
            if(!GameObjectRespawnRelocation(go,false))
            {
                // ... or unload (if respawn grid also not loaded)
                TC_LOG_DEBUG("debug.creature","GameObject (GUID: %u Entry: %u) can't be move to unloaded respawn grid.",go->GetGUIDLow(),go->GetEntry());

                AddObjectToRemoveList(go);
            }
        }
    }
}

bool Map::CreatureCellRelocation(Creature *c, Cell new_cell)
{
    Cell const& old_cell = c->GetCurrentCell();
    if(!old_cell.DiffGrid(new_cell))                       // in same grid
    {
        // if in same cell then none do
        if(old_cell.DiffCell(new_cell))
        {
            TC_LOG_DEBUG("debug.grid","Creature (GUID: %u Entry: %u) moved in grid[%u,%u] from cell[%u,%u] to cell[%u,%u].", c->GetGUIDLow(), c->GetEntry(), old_cell.GridX(), old_cell.GridY(), old_cell.CellX(), old_cell.CellY(), new_cell.CellX(), new_cell.CellY());

            RemoveFromGrid(c,getNGrid(old_cell.GridX(), old_cell.GridY()),old_cell);
            AddToGrid(c,getNGrid(new_cell.GridX(), new_cell.GridY()),new_cell);
        }
        else
        {
            TC_LOG_DEBUG("debug.grid","Creature (GUID: %u Entry: %u) move in same grid[%u,%u]cell[%u,%u].", c->GetGUIDLow(), c->GetEntry(), old_cell.GridX(), old_cell.GridY(), old_cell.CellX(), old_cell.CellY());
        }

        return true;
    }

    // in diff. grids but active creature
    if(c->isActiveObject())
    {
        EnsureGridLoaded(new_cell);

        TC_LOG_DEBUG("debug.creature","Active creature (GUID: %u Entry: %u) moved from grid[%u,%u]cell[%u,%u] to grid[%u,%u]cell[%u,%u].", c->GetGUIDLow(), c->GetEntry(), old_cell.GridX(), old_cell.GridY(), old_cell.CellX(), old_cell.CellY(), new_cell.GridX(), new_cell.GridY(), new_cell.CellX(), new_cell.CellY());

        RemoveFromGrid(c,getNGrid(old_cell.GridX(), old_cell.GridY()),old_cell);
        AddToGrid(c,getNGrid(new_cell.GridX(), new_cell.GridY()),new_cell);

        return true;
    }

    // in diff. loaded grid normal creature
    if(loaded(GridPair(new_cell.GridX(), new_cell.GridY())))
    {
        TC_LOG_DEBUG("debug.creature","Creature (GUID: %u Entry: %u) moved from grid[%u,%u]cell[%u,%u] to grid[%u,%u]cell[%u,%u].", c->GetGUIDLow(), c->GetEntry(), old_cell.GridX(), old_cell.GridY(), old_cell.CellX(), old_cell.CellY(), new_cell.GridX(), new_cell.GridY(), new_cell.CellX(), new_cell.CellY());

        RemoveFromGrid(c,getNGrid(old_cell.GridX(), old_cell.GridY()),old_cell);
        EnsureGridCreated(GridPair(new_cell.GridX(), new_cell.GridY()));
        AddToGrid(c,getNGrid(new_cell.GridX(), new_cell.GridY()),new_cell);

        return true;
    }

    // fail to move: normal creature attempt move to unloaded grid
    TC_LOG_DEBUG("debug.creature","Creature (GUID: %u Entry: %u) attempt move from grid[%u,%u]cell[%u,%u] to unloaded grid[%u,%u]cell[%u,%u].", c->GetGUIDLow(), c->GetEntry(), old_cell.GridX(), old_cell.GridY(), old_cell.CellX(), old_cell.CellY(), new_cell.GridX(), new_cell.GridY(), new_cell.CellX(), new_cell.CellY());
    return false;
}

bool Map::GameObjectCellRelocation(GameObject* go, Cell new_cell)
{
    Cell const& old_cell = go->GetCurrentCell();
    if(!old_cell.DiffGrid(new_cell))                       // in same grid
    {
        // if in same cell then none do
        if(old_cell.DiffCell(new_cell))
        {
            TC_LOG_DEBUG("debug.creature","GameObject (GUID: %u Entry: %u) moved in grid[%u,%u] from cell[%u,%u] to cell[%u,%u].", go->GetGUIDLow(), go->GetEntry(), old_cell.GridX(), old_cell.GridY(), old_cell.CellX(), old_cell.CellY(), new_cell.CellX(), new_cell.CellY());

            RemoveFromGrid(go,getNGrid(old_cell.GridX(), old_cell.GridY()),old_cell);
            AddToGrid(go,getNGrid(new_cell.GridX(), new_cell.GridY()),new_cell);
        }
        else
        {
            TC_LOG_DEBUG("debug.creature","GameObject (GUID: %u Entry: %u) move in same grid[%u,%u]cell[%u,%u].", go->GetGUIDLow(), go->GetEntry(), old_cell.GridX(), old_cell.GridY(), old_cell.CellX(), old_cell.CellY());
        }

        return true;
    }

    // in diff. grids but active creature
    if(go->isActiveObject())
    {
        EnsureGridLoaded(new_cell);

        TC_LOG_DEBUG("debug.creature","Active gameobject (GUID: %u Entry: %u) moved from grid[%u,%u]cell[%u,%u] to grid[%u,%u]cell[%u,%u].", go->GetGUIDLow(), go->GetEntry(), old_cell.GridX(), old_cell.GridY(), old_cell.CellX(), old_cell.CellY(), new_cell.GridX(), new_cell.GridY(), new_cell.CellX(), new_cell.CellY());

        RemoveFromGrid(go,getNGrid(old_cell.GridX(), old_cell.GridY()),old_cell);
        AddToGrid(go,getNGrid(new_cell.GridX(), new_cell.GridY()),new_cell);

        return true;
    }

    // in diff. loaded grid normal creature
    if(loaded(GridPair(new_cell.GridX(), new_cell.GridY())))
    {
        TC_LOG_DEBUG("debug.creature","GameObject (GUID: %u Entry: %u) moved from grid[%u,%u]cell[%u,%u] to grid[%u,%u]cell[%u,%u].", go->GetGUIDLow(), go->GetEntry(), old_cell.GridX(), old_cell.GridY(), old_cell.CellX(), old_cell.CellY(), new_cell.GridX(), new_cell.GridY(), new_cell.CellX(), new_cell.CellY());

        RemoveFromGrid(go,getNGrid(old_cell.GridX(), old_cell.GridY()),old_cell);
        EnsureGridCreated(GridPair(new_cell.GridX(), new_cell.GridY()));
        AddToGrid(go,getNGrid(new_cell.GridX(), new_cell.GridY()),new_cell);

        return true;
    }

    // fail to move: normal creature attempt move to unloaded grid
    TC_LOG_DEBUG("debug.creature","GameObject (GUID: %u Entry: %u) attempt move from grid[%u,%u]cell[%u,%u] to unloaded grid[%u,%u]cell[%u,%u].", go->GetGUIDLow(), go->GetEntry(), old_cell.GridX(), old_cell.GridY(), old_cell.CellX(), old_cell.CellY(), new_cell.GridX(), new_cell.GridY(), new_cell.CellX(), new_cell.CellY());

    return false;
}

bool Map::CreatureRespawnRelocation(Creature *c, bool diffGridOnly)
{
    float resp_x, resp_y, resp_z, resp_o;
    c->GetRespawnPosition(resp_x, resp_y, resp_z, &resp_o);

    CellCoord resp_val = Trinity::ComputeCellCoord(resp_x, resp_y);
    Cell resp_cell(resp_val);

    const Cell current_cell = c->GetCurrentCell();
    //creature will be unloaded with grid
    if (diffGridOnly && !current_cell.DiffGrid(resp_cell))
        return true;

    c->CombatStop();
    c->GetMotionMaster()->Clear();

    TC_LOG_DEBUG("debug.creature","Creature (GUID: %u Entry: %u) will moved from grid[%u,%u]cell[%u,%u] to respawn grid[%u,%u]cell[%u,%u].", c->GetGUIDLow(), c->GetEntry(), current_cell.GridX(), current_cell.GridY(), current_cell.CellX(),current_cell.CellY(), resp_cell.GridX(), resp_cell.GridY(), resp_cell.CellX(), resp_cell.CellY());

    // teleport it to respawn point (like normal respawn if player see)
    if(CreatureCellRelocation(c,resp_cell))
    {
        c->Relocate(resp_x, resp_y, resp_z, resp_o);
        c->GetMotionMaster()->Initialize();                 // prevent possible problems with default move generators
        //CreatureRelocationNotify(c,resp_cell,resp_cell.cellPair());
        AddUnitToNotify(c);
        return true;
    }
    else
        return false;
}

bool Map::GameObjectRespawnRelocation(GameObject* go, bool diffGridOnly)
{
    float resp_x, resp_y, resp_z, resp_o;
    go->GetRespawnPosition(resp_x, resp_y, resp_z, &resp_o);

    CellCoord resp_val = Trinity::ComputeCellCoord(resp_x, resp_y);
    Cell resp_cell(resp_val);

    const Cell current_cell = go->GetCurrentCell();
    //gameobjects will be unloaded with grid
    if (diffGridOnly && !current_cell.DiffGrid(resp_cell))
        return true;

    TC_LOG_DEBUG("debug.creature","GameObject (GUID: %u Entry: %u) will moved from grid[%u,%u]cell[%u,%u] to respawn grid[%u,%u]cell[%u,%u].", go->GetGUIDLow(), go->GetEntry(), current_cell.GridX(), current_cell.GridY(), current_cell.CellX(),current_cell.CellY(), resp_cell.GridX(), resp_cell.GridY(), resp_cell.CellX(), resp_cell.CellY());

    // teleport it to respawn point (like normal respawn if player see)
    if(GameObjectCellRelocation(go,resp_cell))
    {
        go->Relocate(resp_x, resp_y, resp_z, resp_o);
        UpdateObjectVisibility(go,resp_cell,resp_val);
        return true;
    }

    return false;
}

bool Map::UnloadGrid(const uint32 &x, const uint32 &y, bool unloadAll)
{
    NGridType *grid = getNGrid(x, y);
    assert( grid != NULL);

    {
        if(!unloadAll)
        {  
            //pets, possessed creatures (must be active), transport passengers
            if (grid->GetWorldObjectCountInNGrid<Creature>())
                return false;

             if(ActiveObjectsNearGrid(x, y))
                return false;
        }

        ObjectGridUnloader unloader(*grid);

        if(!unloadAll)
        {
            // Finish creature moves, remove and delete all creatures with delayed remove before moving to respawn grids
            // Must know real mob position before move
            MoveAllCreaturesInMoveList();
            MoveAllGameObjectsInMoveList();

            // move creatures to respawn grids if this is diff.grid or to remove list
            unloader.MoveToRespawnN();

            // Finish creature moves, remove and delete all creatures with delayed remove before unload
            MoveAllCreaturesInMoveList();
            MoveAllGameObjectsInMoveList();
        }

        ObjectGridCleaner cleaner(*grid);
        cleaner.CleanN();

        RemoveAllObjectsInRemoveList();

        unloader.UnloadN();

        assert(i_objectsToRemove.empty());

        delete grid;
        setNGrid(NULL, x, y);
    }
    int gx=63-x;
    int gy=63-y;

    // delete grid map, but don't delete if it is from parent map (and thus only reference)
    //+++if (GridMaps[gx][gy]) don't check for GridMaps[gx][gy], we might have to unload vmaps
    {
        if (i_InstanceId == 0)
        {
            if (GridMaps[gx][gy])
            {
                GridMaps[gx][gy]->unloadData();
                delete GridMaps[gx][gy];
            }
            VMAP::VMapFactory::createOrGetVMapManager()->unloadMap(GetId(), gx, gy);
            MMAP::MMapFactory::createOrGetMMapManager()->unloadMap(GetId(), gx, gy);
        }
        else
            ((MapInstanced*)(sMapMgr->GetBaseMap(i_id)))->RemoveGridMapReference(GridPair(gx, gy));

        GridMaps[gx][gy] = NULL;
    }
    TC_LOG_DEBUG("maps","Unloading grid[%u,%u] for map %u finished", x,y, i_id);
    return true;
}

void Map::UnloadAll()
{
    // clear all delayed moves, useless anyway do this moves before map unload.
    i_creaturesToMove.clear();

    for (GridRefManager<NGridType>::iterator i = GridRefManager<NGridType>::begin(); i != GridRefManager<NGridType>::end();)
    {
        NGridType &grid(*i->GetSource());
        ++i;
        UnloadGrid(grid.getX(), grid.getY(), true);       // deletes the grid and removes it from the GridRefManager
    }

    // pussywizard: crashfix, some npc can be left on transport (not a default passenger)
    if (!AllTransportsEmpty())
        AllTransportsRemovePassengers();

    for (TransportsContainer::iterator itr = _transports.begin(); itr != _transports.end();)
    {
        MotionTransport* transport = *itr;
        ++itr;

        transport->RemoveFromWorld();
        delete transport;
    }
    _transports.clear();
}


bool Map::getObjectHitPos(PhaseMask phasemask, float x1, float y1, float z1, float x2, float y2, float z2, float& rx, float& ry, float& rz, float modifyDist)
{
    G3D::Vector3 startPos = G3D::Vector3(x1, y1, z1);
    G3D::Vector3 dstPos = G3D::Vector3(x2, y2, z2);
    
    G3D::Vector3 resultPos;
    bool result = _dynamicTree.getObjectHitPos(phasemask, startPos, dstPos, resultPos, modifyDist);
    
    rx = resultPos.x;
    ry = resultPos.y;
    rz = resultPos.z;
    return result;
}

float Map::GetWaterOrGroundLevel(float x, float y, float z, float* ground /*= NULL*/, bool /*swim = false*/) const
{
    if (const_cast<Map*>(this)->GetGrid(x, y))
    {
        // we need ground level (including grid height version) for proper return water level in point
        float ground_z = GetHeight(x, y, z, true, 50.0f);
        if (ground)
            *ground = ground_z;

        LiquidData liquid_status;

        ZLiquidStatus res = getLiquidStatus(x, y, ground_z, BASE_LIQUID_TYPE_MASK_ALL, &liquid_status);
        return res ? liquid_status.level : ground_z;
    }

    return INVALID_HEIGHT;
}

bool Map::GetLiquidLevelBelow(float x, float y, float z, float& liquidLevel, float maxSearchDist) const
{
    return false; //TODO
}

float Map::GetHeight(PhaseMask phasemask, float x, float y, float z, bool vmap/*=true*/, float maxSearchDist/*=DEFAULT_HEIGHT_SEARCH*/, bool walkableOnly /*= false*/) const
{
    return std::max<float>(GetHeight(x, y, z, vmap, maxSearchDist, walkableOnly), _dynamicTree.getHeight(x, y, z, maxSearchDist, phasemask)); //walkableOnly not implemented in dynamicTree
}

Transport* Map::GetTransportForPos(uint32 phase, float x, float y, float z, WorldObject* worldobject)
{
    G3D::Vector3 v(x, y, z + 2.0f);
    G3D::Ray r(v, G3D::Vector3(0, 0, -1));
    for (TransportsContainer::const_iterator itr = _transports.begin(); itr != _transports.end(); ++itr)
        if ((*itr)->IsInWorld() && (*itr)->GetExactDistSq(x, y, z) < 75.0f*75.0f && (*itr)->m_model)
        {
            float dist = 30.0f;
            bool hit = (*itr)->m_model->intersectRay(r, dist, true, phase);
            if (hit)
                return *itr;
        }

    if (worldobject)
        if (GameObject* staticTrans = worldobject->FindNearestGameObjectOfType(GAMEOBJECT_TYPE_TRANSPORT, 75.0f))
            if (staticTrans->m_model)
            {
                float dist = 10.0f;
                bool hit = staticTrans->m_model->intersectRay(r, dist, true, phase);
                if (hit)
                    if (GetHeight(/*phase,*/ x, y, z, true, 30.0f) < (v.z - dist + 1.0f))
                        return staticTrans->ToTransport();
            }

    return NULL;
}

float Map::GetHeight(float x, float y, float z, bool checkVMap, float maxSearchDist/*=DEFAULT_HEIGHT_SEARCH*/, bool walkableOnly /* = false */) const
{
    // find raw .map surface under Z coordinates
    float mapHeight = INVALID_HEIGHT;
    float mapHeightDist = 0.0f;
    if (GridMap* gmap = const_cast<Map*>(this)->GetGrid(x, y))
    {
        float gridHeight = gmap->getHeight(x, y, walkableOnly);
        // if valid map height found, check for max search distance
        if (gridHeight > INVALID_HEIGHT)
        {
            mapHeightDist = fabs(gridHeight - z);
            if(mapHeightDist < maxSearchDist)
                mapHeight = gridHeight;
        }
    }

    // if no vmap check to do, we're done
    if(!checkVMap)
        return mapHeight;

    // else get vmap height
    float vmapHeight = VMAP_INVALID_HEIGHT_VALUE;
    VMAP::IVMapManager* vmgr = VMAP::VMapFactory::createOrGetVMapManager();
    if (vmgr->isHeightCalcEnabled())
        vmapHeight = vmgr->getHeight(GetId(), x, y, z + 2.0f, maxSearchDist);   // look from a bit higher pos to find the floor
    
    // no valid vmap height found, we're done, return map height
    if(vmapHeight == VMAP_INVALID_HEIGHT_VALUE)
        return mapHeight; //may be INVALID_HEIGHT
    
    // at this point vmapHeight is valid, and mapHeight may still not be
    // if no valid mapHeight found, returns the found vmapHeight
    if(mapHeight == INVALID_HEIGHT)
        return vmapHeight;

    // we have both map height and vmap height at valid distance, lets choose the closer
    return mapHeightDist < std::fabs(vmapHeight - z) ? mapHeight : vmapHeight;
}

float Map::_GetHeight(float x, float y, float z, bool pUseVmaps, float maxSearchDist) const
{
    // find raw .map surface under Z coordinates
    float mapHeight;
    if (GridMap *gmap = const_cast<Map*>(this)->GetGrid(x, y))
    {
        float _mapheight = gmap->getHeight(x,y);

        // look from a bit higher pos to find the floor, ignore under surface case
        if (z + 2.0f > _mapheight)
            mapHeight = _mapheight;
        else
            mapHeight = VMAP_INVALID_HEIGHT_VALUE;
    }
    else
        mapHeight = VMAP_INVALID_HEIGHT_VALUE;

    float vmapHeight;
    if (pUseVmaps)
    {
        VMAP::IVMapManager* vmgr = VMAP::VMapFactory::createOrGetVMapManager();
        if (vmgr->isHeightCalcEnabled())
        {
            // look from a bit higher pos to find the floor
            vmapHeight = vmgr->getHeight(GetId(), x, y, z + 2.0f, maxSearchDist);
        }
        else
            vmapHeight = VMAP_INVALID_HEIGHT_VALUE;
    }
    else
        vmapHeight = VMAP_INVALID_HEIGHT_VALUE;

    // mapHeight set for any above raw ground Z or <= INVALID_HEIGHT
    // vmapheight set for any under Z value or <= INVALID_HEIGHT

    if (vmapHeight > INVALID_HEIGHT)
    {
        if (mapHeight > INVALID_HEIGHT)
        {
            // we have mapheight and vmapheight and must select more appropriate

            // we are already under the surface or vmap height above map heigt
            // or if the distance of the vmap height is less the land height distance
            if (z < mapHeight || vmapHeight > mapHeight || fabs(mapHeight-z) > fabs(vmapHeight-z))
                return vmapHeight;
            else
                return mapHeight;                           // better use .map surface height

        }
        else
            return vmapHeight;                              // we have only vmapHeight (if have)
    }
    else
    {
        if (!pUseVmaps)
            return mapHeight;                               // explicitly use map data (if have)
        else if (mapHeight > INVALID_HEIGHT && (z < mapHeight + 2 || z == MAX_HEIGHT))
            return mapHeight;                               // explicitly use map data if original z < mapHeight but map found (z+2 > mapHeight)
        else
            return VMAP_INVALID_HEIGHT_VALUE;               // we not have any height
    }
}

float Map::GetMinHeight(float x, float y) const
{
    if (GridMap const* grid = const_cast<Map*>(this)->GetGrid(x, y))
        return grid->getMinHeight(x, y);
    
    return -500.0f;
}

inline bool IsOutdoorWMO(uint32 mogpFlags, int32 adtId, int32 rootId, int32 groupId, WMOAreaTableEntry const* wmoEntry, AreaTableEntry const* atEntry, uint32 mapId)
{
    // If this flag is set we are outdoors and can mount up
    if (mogpFlags & 0x8000)
        return true;
    
    if (mogpFlags == 0x809 || mogpFlags == 0x849) // From observations, these zones are actually outdoor
        return true;
    
    // If flag 0x800 is set and we are in non-flyable areas we cannot mount up even if we are physically outdoors
    if (mapId != 530 && (mogpFlags & 0x800))
        return false;
    
    // If this flag is set we are physically outdoors, mounting up is allowed if previous check failed
    return mogpFlags & 0x8;
}

bool Map::IsOutdoors(float x, float y, float z) const
{
    uint32 mogpFlags;
    int32 adtId, rootId, groupId;

    // no wmo found? -> outside by default
    if(!GetAreaInfo(x, y, z, mogpFlags, adtId, rootId, groupId))
        return true;

    AreaTableEntry const* atEntry = 0;
    WMOAreaTableEntry const* wmoEntry= GetWMOAreaTableEntryByTripple(rootId, adtId, groupId);
    if(wmoEntry)
    {
        TC_LOG_DEBUG("maps","Got WMOAreaTableEntry! flag %u, areaid %u", wmoEntry->Flags, wmoEntry->areaId);
        atEntry = sAreaTableStore.LookupEntry(wmoEntry->areaId);
    }
    return IsOutdoorWMO(mogpFlags, adtId, rootId, groupId, wmoEntry, atEntry, i_mapEntry->MapID);
}

bool Map::GetAreaInfo(float x, float y, float z, uint32 &flags, int32 &adtId, int32 &rootId, int32 &groupId) const
{
    float vmap_z = z;
    VMAP::IVMapManager* vmgr = VMAP::VMapFactory::createOrGetVMapManager();
    if (vmgr->getAreaInfo(GetId(), x, y, vmap_z, flags, adtId, rootId, groupId))
    {
        // check if there's terrain between player height and object height
        if(GridMap *gmap = const_cast<Map*>(this)->GetGrid(x, y))
        {
            float _mapheight = gmap->getHeight(x,y);
            // z + 2.0f condition taken from GetHeight(), not sure if it's such a great choice...
            if(z + 2.0f > _mapheight &&  _mapheight > vmap_z)
                return false;
        }
        return true;
    }
    return false;
}

uint32 Map::GetAreaId(float x, float y, float z, bool *isOutdoors) const
{
    uint32 mogpFlags;
    int32 adtId, rootId, groupId;
    WMOAreaTableEntry const* wmoEntry = 0;
    AreaTableEntry const* atEntry = 0;
    bool haveAreaInfo = false;

    if (GetAreaInfo(x, y, z, mogpFlags, adtId, rootId, groupId))
    {
        haveAreaInfo = true;
        if ((wmoEntry = GetWMOAreaTableEntryByTripple(rootId, adtId, groupId)))
            atEntry = sAreaTableStore.LookupEntry(wmoEntry->areaId);
    }

    uint16 areaId;

    if (atEntry)
        areaId = atEntry->ID;
    else
    {
        if (GridMap *gmap = const_cast<Map*>(this)->GetGrid(x, y))
            areaId = gmap->getArea(x, y);
        // this used while not all *.map files generated (instances)

        if (!areaId)
            areaId = i_mapEntry->linked_zone;
    }

    if (isOutdoors) //pointer can be null
    {
        if (haveAreaInfo)
            *isOutdoors = IsOutdoorWMO(mogpFlags, adtId, rootId, groupId, wmoEntry, atEntry, i_mapEntry->MapID);
        else
            *isOutdoors = true;
    }
    return areaId;
}

uint8 Map::GetTerrainType(float x, float y) const
{
    if (GridMap *gmap = const_cast<Map*>(this)->GetGrid(x, y))
        return gmap->getTerrainType(x, y);
    else
        return 0;
}

ZLiquidStatus Map::getLiquidStatus(float x, float y, float z, BaseLiquidTypeMask reqBaseLiquidTypeMask, LiquidData* data) const
{
    ZLiquidStatus result = LIQUID_MAP_NO_WATER;
    VMAP::IVMapManager* vmgr = VMAP::VMapFactory::createOrGetVMapManager();
    float liquid_level = INVALID_HEIGHT;
    float ground_level = INVALID_HEIGHT;
    BaseLiquidType baseLiquidType = BASE_LIQUID_TYPE_NO_WATER;
    if (vmgr->GetLiquidLevel(GetId(), x, y, z, reqBaseLiquidTypeMask, liquid_level, ground_level, baseLiquidType))
    {
        TC_LOG_DEBUG("maps", "getLiquidStatus(): vmap liquid level: %f ground: %f base liquid type: %u", liquid_level, ground_level, uint32(baseLiquidType));
        // Check water level and ground level
        if (liquid_level > ground_level && z > ground_level - 2)
        {
            // All ok in water -> store data
            if (data)
            {
                data->level = liquid_level;
                data->depth_level = ground_level;
                data->baseLiquidType = baseLiquidType;
            }

            float delta = liquid_level - z;

            // Get position delta
            if (delta > 2.0f)                   // Under water
                return LIQUID_MAP_UNDER_WATER;
            if (delta > 0.0f)                   // In water
                return LIQUID_MAP_IN_WATER;
            if (delta > -0.1f)                   // Walk on water
                return LIQUID_MAP_WATER_WALK;
            result = LIQUID_MAP_ABOVE_WATER;
        }
    }

    if (GridMap* gmap = const_cast<Map*>(this)->GetGrid(x, y))
    {
        LiquidData map_data;
        ZLiquidStatus map_result = gmap->getLiquidStatus(x, y, z, reqBaseLiquidTypeMask, &map_data);
        // Not override LIQUID_MAP_ABOVE_WATER with LIQUID_MAP_NO_WATER:
        if (map_result != LIQUID_MAP_NO_WATER && (map_data.level > ground_level))
        {
            if (data)
            {
                *data = map_data;
            }
            return map_result;
        }
    }
    return result;
}

float Map::GetWaterLevel(float x, float y) const
{
    if (GridMap* gmap = const_cast<Map*>(this)->GetGrid(x, y))
        return gmap->getLiquidLevel(x, y);
    else
        return 0;
}

uint32 Map::GetAreaId(float x, float y, float z) const
{
    return GetAreaId(x, y, z, nullptr);
}

uint32 Map::GetZoneId(float x, float y, float z) const
{
    uint32 areaId = GetAreaId(x, y, z);
    if (AreaTableEntry const* area = sAreaTableStore.LookupEntry(areaId))
        if (area->zone)
            return area->zone;

    return areaId;
}

void Map::GetZoneAndAreaId(uint32& zoneid, uint32& areaid, float x, float y, float z) const
{
    areaid = zoneid = GetAreaId(x, y, z);
    if (AreaTableEntry const* area = sAreaTableStore.LookupEntry(areaid))
        if (area->zone)
            zoneid = area->zone;
}


bool Map::isInLineOfSight(float x1, float y1, float z1, float x2, float y2, float z2, PhaseMask phasemask) const
{
    return VMAP::VMapFactory::createOrGetVMapManager()->isInLineOfSight(GetId(), x1, y1, z1, x2, y2, z2)
            && _dynamicTree.isInLineOfSight(x1, y1, z1, x2, y2, z2, phasemask);
}

bool Map::IsInWater(float x, float y, float pZ, LiquidData *data) const
{
    // Check surface in x, y point for liquid
    if (const_cast<Map*>(this)->GetGrid(x, y))
    {
        LiquidData liquid_status;
        LiquidData *liquid_ptr = data ? data : &liquid_status;
        if (getLiquidStatus(x, y, pZ, BASE_LIQUID_TYPE_MASK_ALL, liquid_ptr) != LIQUID_MAP_NO_WATER)
            return true;
    }
    return false;
}

bool Map::IsUnderWater(float x, float y, float z) const
{
    if (const_cast<Map*>(this)->GetGrid(x, y))
    {
        if (getLiquidStatus(x, y, z, BaseLiquidTypeMask(BASE_LIQUID_TYPE_MASK_WATER|BASE_LIQUID_TYPE_MASK_OCEAN)) == LIQUID_MAP_UNDER_WATER)
            return true;
    }
    return false;
}

bool Map::CheckGridIntegrity(Creature* c, bool moved) const
{
    Cell const& cur_cell = c->GetCurrentCell();

    CellCoord xy_val = Trinity::ComputeCellCoord(c->GetPositionX(), c->GetPositionY());
    Cell xy_cell(xy_val);
    if(xy_cell != cur_cell)
    {
        TC_LOG_DEBUG("maps","Creature (GUIDLow: %u) X: %f Y: %f (%s) in grid[%u,%u]cell[%u,%u] instead grid[%u,%u]cell[%u,%u]",
            c->GetGUIDLow(),
            c->GetPositionX(),c->GetPositionY(),(moved ? "final" : "original"),
            cur_cell.GridX(), cur_cell.GridY(), cur_cell.CellX(), cur_cell.CellY(),
            xy_cell.GridX(),  xy_cell.GridY(),  xy_cell.CellX(),  xy_cell.CellY());
        return true;                                        // not crash at error, just output error in debug mode
    }

    return true;
}

bool Map::CheckGridIntegrity(GameObject* go, bool moved) const
{
    Cell const& cur_cell = go->GetCurrentCell();

    CellCoord xy_val = Trinity::ComputeCellCoord(go->GetPositionX(), go->GetPositionY());
    Cell xy_cell(xy_val);
    if(xy_cell != cur_cell)
    {
        TC_LOG_DEBUG("maps","GameObject (GUIDLow: %u) X: %f Y: %f (%s) in grid[%u,%u]cell[%u,%u] instead grid[%u,%u]cell[%u,%u]",
            go->GetGUIDLow(),
            go->GetPositionX(),go->GetPositionY(),(moved ? "final" : "original"),
            cur_cell.GridX(), cur_cell.GridY(), cur_cell.CellX(), cur_cell.CellY(),
            xy_cell.GridX(),  xy_cell.GridY(),  xy_cell.CellX(),  xy_cell.CellY());
        return true;                                        // not crash at error, just output error in debug mode
    }

    return true;
}

const char* Map::GetMapName() const
{
    return i_mapEntry ? i_mapEntry->name[sWorld->GetDefaultDbcLocale()] : "UNNAMEDMAP\x0";
}

void Map::UpdateObjectVisibility( WorldObject* obj, Cell cell, CellCoord cellpair)
{
    cell.data.Part.reserved = ALL_DISTRICT;
    cell.SetNoCreate();
    Trinity::VisibleChangesNotifier notifier(*obj);
    TypeContainerVisitor<Trinity::VisibleChangesNotifier, WorldTypeMapContainer > player_notifier(notifier);
    cell.Visit(cellpair, player_notifier, *this, *obj, GetVisibilityRange());
}

void Map::SendInitSelf( Player * player)
{
    TC_LOG_DEBUG("maps","Creating player data for himself %u", player->GetGUIDLow());

    UpdateData data;

    bool hasTransport = false;

    // attach to player data current transport data
    if(Transport* transport = player->GetTransport())
    {
        hasTransport = true;
        transport->BuildCreateUpdateBlockForPlayer(&data, player);
    }

    // build data for self presence in world at own client (one time for map)
    player->BuildCreateUpdateBlockForPlayer(&data, player);

    // build other passengers at transport also (they always visible and marked as visible and will not send at visibility update at add to map
    if(Transport* transport = player->GetTransport())
        for(Transport::PassengerSet::const_iterator itr = transport->GetPassengers().begin();itr!=transport->GetPassengers().end();++itr)
            if(player!=(*itr) && player->HaveAtClient(*itr))
            {
                hasTransport = true;
                (*itr)->BuildCreateUpdateBlockForPlayer(&data, player);
            }

    WorldPacket packet;
    data.BuildPacket(&packet, hasTransport);
    player->SendDirectMessage(&packet);
}

void Map::SendInitTransports( Player * player)
{
    // Hack to send out transports
    UpdateData transData;
    bool hasTransport = false;
    for (TransportsContainer::const_iterator i = _transports.begin(); i != _transports.end(); ++i)
        if (*i != player->GetTransport())
            {
                (*i)->BuildCreateUpdateBlockForPlayer(&transData, player);
                hasTransport = true;
            }

    WorldPacket packet;
    transData.BuildPacket(&packet,hasTransport);
    player->SendDirectMessage(&packet);
}

void Map::SendRemoveTransports(Player* player)
{
    // Hack to send out transports
    UpdateData transData;
    for (TransportsContainer::const_iterator i = _transports.begin(); i != _transports.end(); ++i)
        if (*i != player->GetTransport())
            (*i)->BuildOutOfRangeUpdateBlock(&transData);

    // pussywizard: remove static transports from client
    for (Player::ClientGUIDs::const_iterator it = player->m_clientGUIDs.begin(); it != player->m_clientGUIDs.end(); )
    {
        if (IS_TRANSPORT(*it))
        {
            transData.AddOutOfRangeGUID(*it);
            it = player->m_clientGUIDs.erase(it);
        }
        else
            ++it;
    }

    WorldPacket packet;
    transData.BuildPacket(&packet);
    player->SendDirectMessage(&packet);
}


inline void Map::setNGrid(NGridType *grid, uint32 x, uint32 y)
{
    if(x >= MAX_NUMBER_OF_GRIDS || y >= MAX_NUMBER_OF_GRIDS)
    {
        TC_LOG_ERROR("maps","map::setNGrid() Invalid grid coordinates found: %d, %d!",x,y);
        ABORT();
    }
    i_grids[x][y] = grid;
}

void Map::DelayedUpdate(const uint32 t_diff)
{
    for (_transportsUpdateIter = _transports.begin(); _transportsUpdateIter != _transports.end();)
    {
        MotionTransport* transport = *_transportsUpdateIter;
        ++_transportsUpdateIter;

        if (!transport->IsInWorld())
            continue;

        transport->DelayedUpdate(t_diff);
    }

    RemoveAllObjectsInRemoveList();
}

void Map::AddObjectToRemoveList(WorldObject *obj)
{
    assert(obj->GetMapId()==GetId() && obj->GetInstanceId()==GetInstanceId());

    obj->CleanupsBeforeDelete(false); 

    i_objectsToRemove.insert(obj);
    //TC_LOG_DEBUG("maps","Object (GUID: %u TypeId: %u) added to removing list.",obj->GetGUIDLow(),obj->GetTypeId());
}

void Map::AddObjectToSwitchList(WorldObject *obj, bool on)
{
    assert(obj->GetMapId()==GetId() && obj->GetInstanceId()==GetInstanceId());

    std::map<WorldObject*, bool>::iterator itr = i_objectsToSwitch.find(obj);
    if(itr == i_objectsToSwitch.end())
        i_objectsToSwitch.insert(itr, std::make_pair(obj, on));
    else if(itr->second != on)
        i_objectsToSwitch.erase(itr);
    else
        ABORT();
}

void Map::RemoveAllObjectsInRemoveList()
{
    while(!i_objectsToSwitch.empty())
    {
        std::map<WorldObject*, bool>::iterator itr = i_objectsToSwitch.begin();
        WorldObject *obj = itr->first;
        bool on = itr->second;
        i_objectsToSwitch.erase(itr);

        switch(obj->GetTypeId())
        {
        case TYPEID_UNIT:
            if(!(obj->ToCreature())->IsPet())
                SwitchGridContainers(obj->ToCreature(), on);
            break;
        }
    }

    //TC_LOG_DEBUG("maps","Object remover 1 check.");
    while(!i_objectsToRemove.empty())
    {
        std::set<WorldObject*>::iterator itr = i_objectsToRemove.begin();
        WorldObject* obj = *itr;

        switch(obj->GetTypeId())
        {
        case TYPEID_CORPSE:
        {
            Corpse* corpse = sObjectAccessor->GetCorpse(*obj, obj->GetGUID());
            if (!corpse)
                TC_LOG_ERROR("maps","Try delete corpse/bones %u that not in map", obj->GetGUIDLow());
            else
                Remove(corpse,true);
            break;
        }
        case TYPEID_DYNAMICOBJECT:
            Remove((DynamicObject*)obj,true);
            break;
        case TYPEID_GAMEOBJECT:
            if (MotionTransport* transport = obj->ToGameObject()->ToMotionTransport())
                Remove(transport, true);
            else
                Remove(obj->ToGameObject(), true);
            break;
        case TYPEID_UNIT:
            // in case triggered sequence some spell can continue casting after prev CleanupsBeforeDelete call
            // make sure that like sources auras/etc removed before destructor start
            (obj->ToCreature())->CleanupsBeforeDelete ();
            Remove(obj->ToCreature(),true);
            break;
        default:
            TC_LOG_ERROR("maps","Non-grid object (TypeId: %u) in grid object removing list, ignored.",obj->GetTypeId());
            break;
        }

        i_objectsToRemove.erase(itr);
    }
    //TC_LOG_DEBUG("maps","Object remover 2 check.");
}

uint32 Map::GetPlayersCountExceptGMs() const
{
    uint32 count = 0;
    for(MapRefManager::const_iterator itr = m_mapRefManager.begin(); itr != m_mapRefManager.end(); ++itr)
        if(!itr->GetSource()->IsGameMaster())
            ++count;
    return count;
}

void Map::SendToPlayers(WorldPacket* data) const
{
    for(MapRefManager::const_iterator itr = m_mapRefManager.begin(); itr != m_mapRefManager.end(); ++itr)
        itr->GetSource()->SendDirectMessage(data);
}

bool Map::ActiveObjectsNearGrid(uint32 x, uint32 y) const
{
    assert(x < MAX_NUMBER_OF_GRIDS);
    assert(y < MAX_NUMBER_OF_GRIDS);

    CellCoord cell_min(x*MAX_NUMBER_OF_CELLS, y*MAX_NUMBER_OF_CELLS);
    CellCoord cell_max(cell_min.x_coord + MAX_NUMBER_OF_CELLS, cell_min.y_coord+MAX_NUMBER_OF_CELLS);

    //we must find visible range in cells so we unload only non-visible cells...
    float viewDist = GetVisibilityRange();
    int cell_range = (int)ceilf(viewDist / SIZE_OF_GRID_CELL) + 1;

    cell_min << cell_range;
    cell_min -= cell_range;
    cell_max >> cell_range;
    cell_max += cell_range;

    for(MapRefManager::const_iterator iter = m_mapRefManager.begin(); iter != m_mapRefManager.end(); ++iter)
    {
        Player* plr = iter->GetSource();

        CellCoord p = Trinity::ComputeCellCoord(plr->GetPositionX(), plr->GetPositionY());
        if( (cell_min.x_coord <= p.x_coord && p.x_coord <= cell_max.x_coord) &&
            (cell_min.y_coord <= p.y_coord && p.y_coord <= cell_max.y_coord))
            return true;
    }

    for(ActiveForcedNonPlayers::const_iterator iter = m_activeForcedNonPlayers.begin(); iter != m_activeForcedNonPlayers.end(); ++iter)
    {
        WorldObject* obj = *iter;

        CellCoord p = Trinity::ComputeCellCoord(obj->GetPositionX(), obj->GetPositionY());
        if( (cell_min.x_coord <= p.x_coord && p.x_coord <= cell_max.x_coord) &&
            (cell_min.y_coord <= p.y_coord && p.y_coord <= cell_max.y_coord))
            return true;
    }

    return false;
}

void Map::AddToForceActive( Creature* c)
{
    AddToForceActiveHelper(c);

    // also not allow unloading spawn grid to prevent creating creature clone at load
    if(!c->IsPet() && c->GetDBTableGUIDLow())
    {
        float x,y,z;
        c->GetRespawnPosition(x,y,z);
        GridPair p = Trinity::ComputeGridPair(x, y);
        if(!getNGrid(p.x_coord, p.y_coord))
        {
            GridPair p2 = Trinity::ComputeGridPair(c->GetPositionX(), c->GetPositionY());
            TC_LOG_ERROR("maps","Active creature (GUID: %u Entry: %u) added to grid[%u,%u] but spawn grid[%u,%u] not loaded.",
                c->GetGUIDLow(), c->GetEntry(), p.x_coord, p.y_coord, p2.x_coord, p2.y_coord);
        }
    }
}

void Map::RemoveFromForceActive( Creature* c)
{
    RemoveFromForceActiveHelper(c);

    // also allow unloading spawn grid
    if(!c->IsPet() && c->GetDBTableGUIDLow())
    {
        float x,y,z;
        c->GetRespawnPosition(x,y,z);
        GridPair p = Trinity::ComputeGridPair(x, y);
        if(!getNGrid(p.x_coord, p.y_coord))
        {
            GridPair p2 = Trinity::ComputeGridPair(c->GetPositionX(), c->GetPositionY());
            TC_LOG_ERROR("maps","Active creature (GUID: %u Entry: %u) removed from grid[%u,%u] but spawn grid[%u,%u] not loaded.",
                c->GetGUIDLow(), c->GetEntry(), p.x_coord, p.y_coord, p2.x_coord, p2.y_coord);
        }
    }
}

Creature* Map::GetCreature(uint64 guid)
{
    Creature * ret = NULL;
    if(IS_CREATURE_GUID(guid))
        ret = ObjectAccessor::GetObjectInWorld(guid, (Creature*)NULL);

    if(!ret)
        return NULL;

    if(ret->GetMapId() != GetId())
        return NULL;

    if(ret->GetInstanceId() != GetInstanceId())
        return NULL;

    return ret;
}

Creature* Map::GetCreatureWithTableGUID(uint32 tableGUID) const
{
    boost::shared_lock<boost::shared_mutex> lock(*HashMapHolder<Creature>::GetLock());
    auto creatures = sObjectAccessor->GetCreatures();
    for (auto pair : creatures)
    {
        auto c = pair.second;

        if (c->GetMapId() != GetId())
            continue;

        if (c->GetInstanceId() != GetInstanceId())
            continue;

        if (c->GetDBTableGUIDLow() != tableGUID)
            continue;

        return c;
    }

    return nullptr;
}

GameObject* Map::GetGameObject(uint64 guid)
{
    GameObject * ret = ObjectAccessor::GetObjectInWorld(guid, (GameObject*)NULL);
    if(!ret)
        return NULL;
    if(ret->GetMapId() != GetId())
        return NULL;
    if(ret->GetInstanceId() != GetInstanceId())
        return NULL;
    return ret;
}

void Map::AddCreatureToPool(Creature *cre, uint32 poolId)
{
    CreaturePoolMember::iterator itr = m_cpmembers.find(poolId);
    if (itr == m_cpmembers.end()) {
        std::set<uint64> newSet;
        newSet.insert(cre->GetGUID());
        m_cpmembers[poolId] = newSet;
    } else {
        itr->second.insert(cre->GetGUID());
    }
}

void Map::RemoveCreatureFromPool(Creature *cre, uint32 poolId)
{
    CreaturePoolMember::iterator itr = m_cpmembers.find(poolId);
    if (itr != m_cpmembers.end()) {
        std::set<uint64> membersSet = itr->second;
        auto itr = membersSet.find(cre->GetGUID());
        if(itr != membersSet.end())
        {
            cre->SetCreaturePoolId(0);
            membersSet.erase(itr);
            return;
        }
        TC_LOG_ERROR("maps","Creature %u could not be removed from pool %u", cre->GetDBTableGUIDLow(), poolId);
    } else {
        TC_LOG_ERROR("maps","Pool %u not found for creature %u", poolId, cre->GetDBTableGUIDLow());
    }
}

std::list<Creature*> Map::GetAllCreaturesFromPool(uint32 poolId)
{
    std::list<Creature*> creatureList;

    CreaturePoolMember::iterator itr = m_cpmembers.find(poolId);
    if (itr != m_cpmembers.end())
    {
        for(auto guid : itr->second)
        {
            Creature* c = GetCreature(guid);
            if(c)
                creatureList.push_back(c);
            else
                TC_LOG_ERROR("maps", "GetAllCreaturesFromPool : couldn't get unit with guid " UI64FMTD, guid);
        }
    }

    return creatureList;
}

bool Map::AllTransportsEmpty() const
{
    for (TransportsContainer::const_iterator itr = _transports.begin(); itr != _transports.end(); ++itr)
        if (!(*itr)->GetPassengers().empty())
            return false;

    return true;
}

void Map::AllTransportsRemovePassengers()
{
    for (TransportsContainer::const_iterator itr = _transports.begin(); itr != _transports.end(); ++itr)
        while (!(*itr)->GetPassengers().empty())
            (*itr)->RemovePassenger(*((*itr)->GetPassengers().begin()), true);
}

template bool Map::Add(Corpse *, bool);
template bool Map::Add(Creature *, bool);
template bool Map::Add(GameObject *, bool);
template bool Map::Add(DynamicObject *, bool);

template void Map::Remove(Corpse *,bool);
template void Map::Remove(Creature *,bool);
template void Map::Remove(GameObject *, bool);
template void Map::Remove(DynamicObject *, bool);

/* ******* Dungeon Instance Maps ******* */

InstanceMap::InstanceMap(uint32 id, uint32 InstanceId, uint8 SpawnMode)
  : Map(id, InstanceId, SpawnMode), i_data(NULL),
    m_resetAfterUnload(false), m_unloadWhenEmpty(false)
{
    //lets initialize visibility distance for dungeons
    InstanceMap::InitVisibilityDistance();

    // the timer is started by default, and stopped when the first player joins
    // this make sure it gets unloaded if for some reason no player joins
    m_unloadTimer = std::max(sWorld->getConfig(CONFIG_INSTANCE_UNLOAD_DELAY), (uint32)MIN_UNLOAD_DELAY);
}

InstanceMap::~InstanceMap()
{
    if(i_data)
    {
        delete i_data;
        i_data = NULL;
    }
    
    // unload instance specific navigation data
    MMAP::MMapFactory::createOrGetMMapManager()->unloadMapInstance(GetId(), GetInstanceId());
}

void InstanceMap::InitVisibilityDistance()
{
    //init visibility distance for instances
    m_VisibleDistance = sWorld->GetMaxVisibleDistanceInInstances();
}

/*
    Do map specific checks to see if the player can enter
*/
bool InstanceMap::CanEnter(Player *player)
{
    if(player->GetMapRef().getTarget() == this)
    {
//        TC_LOG_ERROR("maps","InstanceMap::CanEnter - player %s(%u) already in map %d,%d,%d!", player->GetName(), player->GetGUIDLow(), GetId(), GetInstanceId(), GetSpawnMode());
//        ABORT();
        return false;
    }

    // cannot enter if the instance is full (player cap), GMs don't count
    InstanceTemplate const* iTemplate = sObjectMgr->GetInstanceTemplate(GetId());
    if (!player->IsGameMaster() && GetPlayersCountExceptGMs() >= iTemplate->maxPlayers)
    {
        //TC_LOG_DEBUG("maps","MAP: Instance '%u' of map '%s' cannot have more than '%u' players. Player '%s' rejected", GetInstanceId(), GetMapName(), iTemplate->maxPlayers, player->GetName());
        player->SendTransferAborted(GetId(), TRANSFER_ABORT_MAX_PLAYERS);
        return false;
    }

    // cannot enter while players in the instance are in combat
    Group *pGroup = player->GetGroup();
    if(!player->IsGameMaster() && pGroup && pGroup->InCombatToInstance(GetInstanceId()) && player->GetMapId() != GetId())
    {
        player->SendTransferAborted(GetId(), TRANSFER_ABORT_ZONE_IN_COMBAT);
        return false;
    }

    return Map::CanEnter(player);
}

/*
    Do map specific checks and add the player to the map if successful.
*/
bool InstanceMap::Add(Player *player)
{
    // TODO: Not sure about checking player level: already done in HandleAreaTriggerOpcode
    // GMs still can teleport player in instance.
    // Is it needed?

    {
        std::lock_guard<std::mutex> lock(_mapLock);
        if(!CanEnter(player))
            return false;

        // get or create an instance save for the map
        InstanceSave *mapSave = sInstanceSaveMgr->GetInstanceSave(GetInstanceId());
        if(!mapSave)
        {
            TC_LOG_DEBUG("maps","InstanceMap::Add: creating instance save for map %d difficulty %d with instance id %d", GetId(), GetDifficulty(), GetInstanceId());
            mapSave = sInstanceSaveMgr->AddInstanceSave(GetId(), GetInstanceId(), GetDifficulty(), 0, true);
        }

        // check for existing instance binds
        InstancePlayerBind *playerBind = player->GetBoundInstance(GetId(), GetDifficulty());
        if(playerBind && playerBind->perm)
        {
            // cannot enter other instances if bound permanently
            if(playerBind->save != mapSave)
            {
                //TC_LOG_ERROR("maps","InstanceMap::Add: player %s(%d) is permanently bound to instance %d,%d,%d,%d,%d,%d but he is being put in instance %d,%d,%d,%d,%d,%d", player->GetName(), player->GetGUIDLow(), playerBind->save->GetMapId(), playerBind->save->GetInstanceId(), playerBind->save->GetDifficulty(), playerBind->save->GetPlayerCount(), playerBind->save->GetGroupCount(), playerBind->save->CanReset(), mapSave->GetMapId(), mapSave->GetInstanceId(), mapSave->GetDifficulty(), mapSave->GetPlayerCount(), mapSave->GetGroupCount(), mapSave->CanReset());
                ABORT();
            }
        }
        else
        {
            Group *pGroup = player->GetGroup();
            if(pGroup)
            {
                // solo saves should be reset when entering a group
                InstanceGroupBind *groupBind = pGroup->GetBoundInstance(GetDifficulty(), GetId());
                if(playerBind)
                {
                  //  TC_LOG_ERROR("maps","InstanceMap::Add: player %s(%d) is being put in instance %d,%d,%d,%d,%d,%d but he is in group %d and is bound to instance %d,%d,%d,%d,%d,%d!", player->GetName(), player->GetGUIDLow(), mapSave->GetMapId(), mapSave->GetInstanceId(), mapSave->GetDifficulty(), mapSave->GetPlayerCount(), mapSave->GetGroupCount(), mapSave->CanReset(), GUID_LOPART(pGroup->GetLeaderGUID()), playerBind->save->GetMapId(), playerBind->save->GetInstanceId(), playerBind->save->GetDifficulty(), playerBind->save->GetPlayerCount(), playerBind->save->GetGroupCount(), playerBind->save->CanReset());
                    if(groupBind) TC_LOG_ERROR("maps","InstanceMap::Add: the group is bound to instance %d,%d,%d,%d,%d,%d", groupBind->save->GetMapId(), groupBind->save->GetInstanceId(), groupBind->save->GetDifficulty(), groupBind->save->GetPlayerCount(), groupBind->save->GetGroupCount(), groupBind->save->CanReset());
                    TC_LOG_ERROR("maps","InstanceMap::Add: do not let player %s enter instance otherwise crash will happen", player->GetName().c_str());
                    return false;
                    //player->UnbindInstance(GetId(), GetSpawnMode());
                    //ABORT();
                }
                // bind to the group or keep using the group save
                if(!groupBind)
                    pGroup->BindToInstance(mapSave, false);
                else
                {
                    // cannot jump to a different instance without resetting it
                    if(groupBind->save != mapSave)
                    {
//                        TC_LOG_ERROR("maps","InstanceMap::Add: player %s(%d) is being put in instance %d,%d,%d but he is in group %d which is bound to instance %d,%d,%d!", player->GetName(), player->GetGUIDLow(), mapSave->GetMapId(), mapSave->GetInstanceId(), mapSave->GetDifficulty(), GUID_LOPART(pGroup->GetLeaderGUID()), groupBind->save->GetMapId(), groupBind->save->GetInstanceId(), groupBind->save->GetDifficulty());
                        if(mapSave)
                            TC_LOG_ERROR("maps","MapSave players: %d, group count: %d", mapSave->GetPlayerCount(), mapSave->GetGroupCount());
                        else
                            TC_LOG_ERROR("maps","MapSave NULL");
                        if(groupBind->save)
                            TC_LOG_ERROR("maps","GroupBind save players: %d, group count: %d", groupBind->save->GetPlayerCount(), groupBind->save->GetGroupCount());
                        else
                            TC_LOG_ERROR("maps","GroupBind save NULL");
                        ABORT();
                    }
                    // if the group/leader is permanently bound to the instance
                    // players also become permanently bound when they enter
                    if(groupBind->perm)
                    {
                        WorldPacket data(SMSG_INSTANCE_SAVE_CREATED, 4);
                        data << uint32(0);
                        player->SendDirectMessage(&data);
                        player->BindToInstance(mapSave, true);
                    }
                }
            }
            else
            {
                // set up a solo bind or continue using it
                if(!playerBind)
                    player->BindToInstance(mapSave, false);
                else
                    // cannot jump to a different instance without resetting it
                    assert(playerBind->save == mapSave);
            }
        }

        if(i_data) i_data->OnPlayerEnter(player);
        // for normal instances cancel the reset schedule when the
        // first player enters (no players yet)
        SetResetSchedule(false);

        player->SendInitWorldStates();
        //TC_LOG_DEBUG("maps","MAP: Player '%s' entered the instance '%u' of map '%s'", player->GetName(), GetInstanceId(), GetMapName());
        // initialize unload state
        m_unloadTimer = 0;
        m_resetAfterUnload = false;
        m_unloadWhenEmpty = false;
    }
    
    // Remove auras that cannot be present in instance
    player->RemoveAurasWithCustomAttribute(SPELL_ATTR_CU_REMOVE_ON_INSTANCE_ENTER);

    // this will acquire the same mutex so it cannot be in the previous block
    Map::Add(player);
    return true;
}

void InstanceMap::Update(const uint32& t_diff)
{
    Map::Update(t_diff);

    if(i_data)
        i_data->Update(t_diff);
}

void InstanceMap::Remove(Player *player, bool remove)
{
    //TC_LOG_DEBUG("maps","MAP: Removing player '%s' from instance '%u' of map '%s' before relocating to other map", player->GetName(), GetInstanceId(), GetMapName());
    //if last player set unload timer
    if(!m_unloadTimer && m_mapRefManager.getSize() == 1)
        m_unloadTimer = m_unloadWhenEmpty ? MIN_UNLOAD_DELAY : std::max(sWorld->getConfig(CONFIG_INSTANCE_UNLOAD_DELAY), (uint32)MIN_UNLOAD_DELAY);
    Map::Remove(player, remove);
    // for normal instances schedule the reset after all players have left
    SetResetSchedule(true);
}

Player* Map::GetPlayer(uint64 guid)
{
    Player * obj = HashMapHolder<Player>::Find(guid);
    if(obj && obj->GetInstanceId() != GetInstanceId()) obj = NULL;
    return obj;
}

inline GridMap *Map::GetGrid(float x, float y)
{
    // half opt method
    int gx=(int)(32-x/SIZE_OF_GRIDS);                       //grid x
    int gy=(int)(32-y/SIZE_OF_GRIDS);                       //grid y

    // ensure GridMap is loaded
    EnsureGridCreated(GridPair(63-gx,63-gy));

    return GridMaps[gx][gy];
}

void InstanceMap::CreateInstanceScript(bool load)
{
    if(i_data != NULL)
        return;

    InstanceTemplate const* mInstance = sObjectMgr->GetInstanceTemplate(GetId());
    if (mInstance)
    {
        i_script_id = mInstance->script_id;
        i_data = sScriptMgr->CreateInstanceScript(this);
    }

    if(!i_data)
        return;

    i_data->Initialize();

    if(load)
    {
        // TODO: make a global storage for this
        QueryResult result = CharacterDatabase.PQuery("SELECT data FROM instance WHERE map = '%u' AND id = '%u'", GetId(), i_InstanceId);
        if (result)
        {
            Field* fields = result->Fetch();
            const char* data = fields[0].GetCString();
            if(data)
                i_data->Load(data);
        }
    }
}

/*
    Returns true if there are no players in the instance
*/
bool InstanceMap::Reset(uint8 method)
{
    // note: since the map may not be loaded when the instance needs to be reset
    // the instance must be deleted from the DB by InstanceSaveManager

    if(HavePlayers())
    {
        if(method == INSTANCE_RESET_ALL)
        {
            // notify the players to leave the instance so it can be reset
            for(MapRefManager::iterator itr = m_mapRefManager.begin(); itr != m_mapRefManager.end(); ++itr)
                itr->GetSource()->SendResetFailedNotify(GetId());
        }
        else
        {
            if(method == INSTANCE_RESET_GLOBAL)
            {
                // set the homebind timer for players inside (1 minute)
                for(MapRefManager::iterator itr = m_mapRefManager.begin(); itr != m_mapRefManager.end(); ++itr)
                    itr->GetSource()->m_InstanceValid = false;
            }

            // the unload timer is not started
            // instead the map will unload immediately after the players have left
            m_unloadWhenEmpty = true;
            m_resetAfterUnload = true;
        }
    }
    else
    {
        // unloaded at next update
        m_unloadTimer = MIN_UNLOAD_DELAY;
        m_resetAfterUnload = true;
    }

    return m_mapRefManager.isEmpty();
}

void InstanceMap::PermBindAllPlayers(Player *player)
{
    InstanceSave *save = sInstanceSaveMgr->GetInstanceSave(GetInstanceId());
    if(!save)
    {
        TC_LOG_ERROR("maps","Cannot bind players, no instance save available for map!\n");
        return;
    }

    Group *group = player->GetGroup();
    // group members outside the instance group don't get bound
    for(MapRefManager::iterator itr = m_mapRefManager.begin(); itr != m_mapRefManager.end(); ++itr)
    {
        Player* plr = itr->GetSource();
        // players inside an instance cannot be bound to other instances
        // some players may already be permanently bound, in this case nothing happens
        InstancePlayerBind *bind = plr->GetBoundInstance(save->GetMapId(), save->GetDifficulty());
        if(!bind || !bind->perm)
        {
            plr->BindToInstance(save, true);
            WorldPacket data(SMSG_INSTANCE_SAVE_CREATED, 4);
            data << uint32(0);
            plr->SendDirectMessage(&data);
        }

        // if the leader is not in the instance the group will not get a perm bind
        if(group && group->GetLeaderGUID() == plr->GetGUID())
            group->BindToInstance(save, true);
    }
}

void Map::RemoveAllPlayers()
{
    if (HavePlayers())
    {
        for (MapRefManager::iterator itr = m_mapRefManager.begin(); itr != m_mapRefManager.end(); ++itr)
        {
            Player* player = itr->GetSource();
            if (!player->IsBeingTeleportedFar())
            {
                TC_LOG_ERROR("maps", "Map::UnloadAll: player %s is still in map %u during unload, this should not happen!", player->GetName().c_str(), GetId());
                player->TeleportTo(player->m_homebindMapId, player->m_homebindX, player->m_homebindY, player->m_homebindZ, player->GetOrientation());
               
            }
        }
    }
}

void InstanceMap::UnloadAll()
{
    if(HavePlayers())
    {
        TC_LOG_ERROR("maps","InstanceMap::UnloadAll: there are still players in the instance at unload, should not happen!");
        std::list<Player*> players;
        //dont teleport players in this loop at this will invalidate our iterator
        for (MapRefManager::iterator itr = m_mapRefManager.begin(); itr != m_mapRefManager.end(); ++itr)
            players.push_back(itr->GetSource());

        for (auto plr : players)
        {
            AreaTrigger const* at = sObjectMgr->GetGoBackTrigger(GetId());
            if (at)
                plr->TeleportTo(at->target_mapId, at->target_X, at->target_Y, at->target_Z, plr->GetOrientation());
            else
                plr->TeleportTo(plr->m_homebindMapId, plr->m_homebindX, plr->m_homebindY, plr->m_homebindZ, plr->GetOrientation());
        }
    }

    if(m_resetAfterUnload == true)
        sObjectMgr->DeleteRespawnTimeForInstance(GetInstanceId());

    Map::UnloadAll();
}

void InstanceMap::HandleCrash()
{
    if (HavePlayers())
    {
        std::list<Player*> players;
        //dont teleport players in this loop at this will invalidate our iterator
        for (MapRefManager::iterator itr = m_mapRefManager.begin(); itr != m_mapRefManager.end(); ++itr)
            players.push_back(itr->GetSource());

        for (auto plr : players)
        {
            bool tpResult = false;
            AreaTrigger const* at = sObjectMgr->GetGoBackTrigger(GetId());
            if (at)
                tpResult = plr->TeleportTo(at->target_mapId, at->target_X, at->target_Y, at->target_Z, plr->GetOrientation());

            if(!tpResult)
                tpResult = plr->TeleportTo(plr->m_homebindMapId, plr->m_homebindX, plr->m_homebindY, plr->m_homebindZ, plr->GetOrientation());

            if (!tpResult) //just to be extra sure
                plr->m_kickatnextupdate = true;

            ChatHandler(plr).SendSysMessage(LANG_INSTANCE_CRASHED);
        }
    }

    if (m_resetAfterUnload == true)
        sObjectMgr->DeleteRespawnTimeForInstance(GetInstanceId());

}

void InstanceMap::SendResetWarnings(uint32 timeLeft) const
{
    for(MapRefManager::const_iterator itr = m_mapRefManager.begin(); itr != m_mapRefManager.end(); ++itr)
        itr->GetSource()->SendInstanceResetWarning(GetId(), timeLeft);
}

void InstanceMap::SetResetSchedule(bool on)
{
    // only for normal instances
    // the reset time is only scheduled when there are no payers inside
    // it is assumed that the reset time will rarely (if ever) change while the reset is scheduled
    if(!HavePlayers() && !IsRaid() && !IsHeroic())
    {
        InstanceSave *save = sInstanceSaveMgr->GetInstanceSave(GetInstanceId());
        if(!save) 
            TC_LOG_ERROR("maps", "InstanceMap::SetResetSchedule: cannot turn schedule %s, no save available for instance %u of %u", on ? "on" : "off", GetInstanceId(), GetId());
        else 
            sInstanceSaveMgr->ScheduleReset(on, save->GetResetTime(), InstanceSaveManager::InstResetEvent(0, GetId(), GetDifficulty(), GetInstanceId()));
    }
}

MapDifficulty const* Map::GetMapDifficulty() const
{
    return GetMapDifficultyData(GetId(), GetDifficulty());
}

Difficulty Map::GetDifficulty() const
{ 
    return Difficulty(GetSpawnMode()); 
}

bool Map::IsRegularDifficulty() const
{ 
    return GetDifficulty() == REGULAR_DIFFICULTY; 
}

#ifdef LICH_KING
bool Map::IsHeroic() const 
{ 
    return IsRaid() ? i_spawnMode >= RAID_DIFFICULTY_10MAN_HEROIC : i_spawnMode >= DUNGEON_DIFFICULTY_HEROIC; 
}
#else
bool Map::IsHeroic() const 
{ 
    return i_spawnMode == DUNGEON_DIFFICULTY_HEROIC; 
}
#endif

/* ******* Battleground Instance Maps ******* */

BattlegroundMap::BattlegroundMap(uint32 id, uint32 InstanceId)
  : Map(id, InstanceId, REGULAR_DIFFICULTY)
{
    //lets initialize visibility distance for BG/Arenas
    BattlegroundMap::InitVisibilityDistance();
}

BattlegroundMap::~BattlegroundMap()
{
}

void BattlegroundMap::InitVisibilityDistance()
{
    //init visibility distance for BG/Arenas
    m_VisibleDistance = sWorld->GetMaxVisibleDistanceInBGArenas();
}

bool BattlegroundMap::CanEnter(Player * player)
{
    if(player->GetMapRef().getTarget() == this)
    {
        TC_LOG_ERROR("maps","BGMap::CanEnter - player %u already in map!", player->GetGUIDLow());
        ABORT();
        return false;
    }

    if(player->GetBattlegroundId() != GetInstanceId())
        return false;

    // player number limit is checked in bgmgr, no need to do it here

    return Map::CanEnter(player);
}

bool BattlegroundMap::Add(Player * player)
{
    {
        std::lock_guard<std::mutex> lock(_mapLock);
        if(!CanEnter(player))
            return false;
        // reset instance validity, battleground maps do not homebind
        player->m_InstanceValid = true;
    }
    return Map::Add(player);
}

void BattlegroundMap::Remove(Player *player, bool remove)
{
    ASSERT(player);
    if (player->isSpectator() && !player->isSpectateCanceled())
    {
        if (GetBG())
            GetBG()->RemoveSpectator(player->GetGUID());

        if (player->isSpectator())
            player->SetSpectate(false);
    }

    //TC_LOG_DEBUG("maps","MAP: Removing player '%s' from bg '%u' of map '%s' before relocating to other map", player->GetName(), GetInstanceId(), GetMapName());
    Map::Remove(player, remove);
}

void BattlegroundMap::SetUnload()
{
    m_unloadTimer = MIN_UNLOAD_DELAY;
}

void BattlegroundMap::RemoveAllPlayers()
{
    if (HavePlayers())
        for (MapRefManager::iterator itr = m_mapRefManager.begin(); itr != m_mapRefManager.end(); ++itr)
            if (Player* player = itr->GetSource())
                if (!player->IsBeingTeleportedFar())
                    player->TeleportTo(player->GetBattlegroundEntryPoint());
}

void BattlegroundMap::HandleCrash()
{
    if (HavePlayers())
    {
        std::list<Player*> players;
        //dont teleport players in this loop at this will invalidate our iterator
        for (MapRefManager::iterator itr = m_mapRefManager.begin(); itr != m_mapRefManager.end(); ++itr)
            players.push_back(itr->GetSource());

        for (auto plr : players)
        {
            bool tpResult = plr->TeleportTo(plr->GetBattlegroundEntryPoint());
            ChatHandler(plr).SendSysMessage(LANG_INSTANCE_CRASHED);
            if (!tpResult)
                plr->m_kickatnextupdate = true;
        }
    }
}

Transport* Map::GetTransport(uint64 guid)
{
#ifdef LICH_KING
    //ON BC transports use Gameobject guids (not 100% sure but currently that's what we do)
    if (GUID_HIPART(guid) != HIGHGUID_MO_TRANSPORT && GUID_HIPART(guid) != HIGHGUID_TRANSPORT)
        return nullptr;
#endif

    GameObject* go = GetGameObject(guid);
    return go ? go->ToTransport() : NULL;
}

DynamicObject* Map::GetDynamicObject(uint64 guid)
{
    DynamicObject * obj = HashMapHolder<DynamicObject>::Find(guid);
    if(obj && obj->GetInstanceId() != GetInstanceId()) obj = NULL;
    return obj;
}
/*--------------------------TRINITY-------------------------*/

void Map::SendZoneDynamicInfo(Player* player)
{
    uint32 zoneId = GetZoneId(player->GetPositionX(), player->GetPositionY(), player->GetPositionZ());
    ZoneDynamicInfoMap::const_iterator itr = _zoneDynamicInfo.find(zoneId);
    if (itr == _zoneDynamicInfo.end())
        return;

    if (itr->second.MusicId)
        player->GetSession()->SendPlayMusic(itr->second.MusicId);

    if (WeatherState weather = itr->second.WeatherId)
        player->GetSession()->SendWeather(weather, itr->second.WeatherGrade, 0);

    if (uint32 overrideLight = itr->second.OverrideLightId)
        player->GetSession()->SendOverrideLight(_defaultLight, overrideLight, itr->second.LightFadeInTime);
}

void Map::SetZoneMusic(uint32 zoneId, uint32 musicId)
{
    if (_zoneDynamicInfo.find(zoneId) == _zoneDynamicInfo.end())
        _zoneDynamicInfo.insert(ZoneDynamicInfoMap::value_type(zoneId, ZoneDynamicInfo()));

    _zoneDynamicInfo[zoneId].MusicId = musicId;

    Map::PlayerList const& players = GetPlayers();
    if (!players.isEmpty())
    {
        for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr)
            if (Player* player = itr->GetSource())
                if (player->GetZoneId() == zoneId)
                    player->GetSession()->SendPlayMusic(musicId);
    }
}

void Map::SetZoneWeather(uint32 zoneId, WeatherState weatherId, float weatherGrade)
{
    if (_zoneDynamicInfo.find(zoneId) == _zoneDynamicInfo.end())
        _zoneDynamicInfo.insert(ZoneDynamicInfoMap::value_type(zoneId, ZoneDynamicInfo()));

    ZoneDynamicInfo& info = _zoneDynamicInfo[zoneId];
    info.WeatherId = weatherId;
    info.WeatherGrade = weatherGrade;
    Map::PlayerList const& players = GetPlayers();

    if (!players.isEmpty())
    {
        for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr)
            if (Player* player = itr->GetSource())
                if (player->GetZoneId() == zoneId)
                    player->GetSession()->SendWeather(weatherId, weatherGrade, 0);
    }
}

void Map::SetZoneOverrideLight(uint32 zoneId, uint32 lightId, uint32 fadeInTime)
{
    if (_zoneDynamicInfo.find(zoneId) == _zoneDynamicInfo.end())
        _zoneDynamicInfo.insert(ZoneDynamicInfoMap::value_type(zoneId, ZoneDynamicInfo()));

    ZoneDynamicInfo& info = _zoneDynamicInfo[zoneId];
    info.OverrideLightId = lightId;
    info.LightFadeInTime = fadeInTime;
    Map::PlayerList const& players = GetPlayers();

    if (!players.isEmpty())
    {
        WorldPacket data(SMSG_OVERRIDE_LIGHT, 4 + 4 + 4);
        data << uint32(_defaultLight);
        data << uint32(lightId);
        data << uint32(fadeInTime);

        for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr)
            if (Player* player = itr->GetSource())
                if (player->GetZoneId() == zoneId)
                    player->SendDirectMessage(&data);
    }
}

bool Map::IsGridLoaded(float x, float y) const
{
    GridPair gp = Trinity::ComputeGridPair(x,y);
    if((gp.x_coord >= MAX_NUMBER_OF_GRIDS) || (gp.y_coord >= MAX_NUMBER_OF_GRIDS))
        return false;

    return loaded(gp);
}

//LK OK
void WorldSession::SendPlayMusic(uint32 musicId)
{
    WorldPacket data(SMSG_PLAY_MUSIC, 4);
    data << uint32(musicId);
    SendPacket(&data);
}

//LK OK
void WorldSession::SendWeather(WeatherState weatherId, float weatherGrade, uint8 unk)
{
    WorldPacket data(SMSG_WEATHER, 4 + 4 + 4);
    data << uint32(weatherId);
    data << float(weatherGrade);
    data << uint8(unk);
    SendPacket(&data);
}

//LK OK
void WorldSession::SendOverrideLight(uint32 defaultLightId, uint32 overrideLightId, uint32 fadeTime)
{
    WorldPacket data(SMSG_OVERRIDE_LIGHT, 4 + 4 + 4);
    data << uint32(defaultLightId);
    data << uint32(overrideLightId);
    data << uint32(fadeTime);
    SendPacket(&data);
}

bool Map::Instanceable() const { return i_mapEntry && i_mapEntry->Instanceable(); }
bool Map::IsDungeon() const { return i_mapEntry && i_mapEntry->IsDungeon(); }
bool Map::IsNonRaidDungeon() const { return i_mapEntry && i_mapEntry->IsNonRaidDungeon(); }
bool Map::IsRaid() const { return i_mapEntry && i_mapEntry->IsRaid(); }
bool Map::IsWorldMap() const { return i_mapEntry && i_mapEntry->IsWorldMap(); }

bool Map::IsBattleground() const { return i_mapEntry && i_mapEntry->IsBattleground(); }
bool Map::IsBattleArena() const { return i_mapEntry && i_mapEntry->IsBattleArena(); }
bool Map::IsBattlegroundOrArena() const { return i_mapEntry && i_mapEntry->IsBattlegroundOrArena(); }