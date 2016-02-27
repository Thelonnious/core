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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef TRINITY_MAPMANAGER_H
#define TRINITY_MAPMANAGER_H

#include "Define.h"
#include "Policies/Singleton.h"
#include "Map.h"
#include "MapUpdater.h"

class WorldLocation;
class GridState;

class MapManager
{
    typedef std::unordered_map<uint32, Map*> MapMapType;
    typedef std::pair<std::unordered_map<uint32, Map*>::iterator, bool>  MapMapPair;

    public:
        static MapManager* instance()
        {
            static MapManager instance;
            return &instance;
        }


        Map* CreateBaseMap(uint32 id);
        Map* FindBaseMap(uint32 id) const
        {
            MapMapType::const_iterator iter = i_maps.find(id);
            return (iter == i_maps.end() ? NULL : iter->second);
        }
        Map* FindBaseNonInstanceMap(uint32 mapId) const;
        Map* CreateMap(uint32 id, const WorldObject* obj);
        Map* FindMap(uint32 mapid, uint32 instanceId);

        // only const version for outer users
        Map const* GetBaseMap(uint32 id) const { return const_cast<MapManager*>(this)->CreateBaseMap(id); }
        
        uint32 GetAreaId(uint32 mapid, float x, float y, float z) const;
        uint32 GetZoneId(uint32 mapid, float x, float y, float z) const;
        void GetZoneAndAreaId(uint32& zoneid, uint32& areaid, uint32 mapid, float x, float y, float z);

        void Initialize(void);
        void Update(time_t);

        inline void SetGridCleanUpDelay(uint32 t)
        {
            if( t < MIN_GRID_DELAY )
                i_gridCleanUpDelay = MIN_GRID_DELAY;
            else
                i_gridCleanUpDelay = t;
        }

        inline void SetMapUpdateInterval(uint32 t)
        {
            if( t > MIN_MAP_UPDATE_DELAY )
                t = MIN_MAP_UPDATE_DELAY;

            i_timer.SetInterval(t);
            i_timer.Reset();
        }

        //void LoadGrid(int mapid, float x, float y, const WorldObject* obj, bool no_unload = false);
        void UnloadAll();

        static bool ExistMapAndVMap(uint32 mapid, float x, float y);
        static bool IsValidMAP(uint32 mapid, bool startUp = false);

        static bool IsValidMapCoord(uint32 mapid, float x,float y)
        {
            return IsValidMAP(mapid) && Trinity::IsValidMapCoord(x,y);
        }

        static bool IsValidMapCoord(uint32 mapid, float x,float y,float z)
        {
            return IsValidMAP(mapid) && Trinity::IsValidMapCoord(x,y,z);
        }

        static bool IsValidMapCoord(uint32 mapid, float x,float y,float z,float o)
        {
            return IsValidMAP(mapid) && Trinity::IsValidMapCoord(x,y,z,o);
        }

        static bool IsValidMapCoord(WorldLocation const& loc);

        bool CanPlayerEnter(uint32 mapid, Player* player);
        void RemoveBonesFromMap(uint32 mapid, uint64 guid, float x, float y);
        void InitializeVisibilityDistanceInfo();

        /* statistics */
        uint32 GetNumInstances();
        uint32 GetNumPlayersInInstances();
        uint32 GetNumPlayersInMap(uint32 mapId);

        // Instance ID management
        void InitInstanceIds();
        uint32 GenerateInstanceId();
        void RegisterInstanceId(uint32 instanceId);
        void FreeInstanceId(uint32 instanceId);

        uint32 GetNextInstanceId() const { return _nextInstanceId; };
        void SetNextInstanceId(uint32 nextInstanceId) { _nextInstanceId = nextInstanceId; };

        MapUpdater * GetMapUpdater() { return &m_updater; }

    private:
        GridState* i_GridStates[MAX_GRID_STATE];            // shadow entries to the global array in Map.cpp
        int i_GridStateErrorCount;
    private:
        typedef std::vector<bool> InstanceIds;

        MapManager();
        ~MapManager();

        MapManager(const MapManager &);
        MapManager& operator=(const MapManager &);
        

        uint32 i_gridCleanUpDelay;
        MapMapType i_maps;
        IntervalTimer i_timer;
        std::mutex _mapsLock;

        InstanceIds _instanceIds;
        uint32 _nextInstanceId;
        MapUpdater m_updater;
};
#define sMapMgr MapManager::instance()
#endif

