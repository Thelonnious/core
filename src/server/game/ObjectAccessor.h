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

#ifndef TRINITY_OBJECTACCESSOR_H
#define TRINITY_OBJECTACCESSOR_H

#include "Define.h"
#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>

#include "ByteBuffer.h"
#include "UpdateData.h"

#include "GridDefines.h"
#include "Object.h"
#include "Player.h"

#include <set>

class Creature;
class Corpse;
class Unit;
class GameObject;
class DynamicObject;
class WorldObject;
class Map;

/** Static hash map */
template <class T>
class TC_GAME_API HashMapHolder
{
    //Non instanceable only static
    HashMapHolder() { }

public:
    static_assert(std::is_same<Player, T>::value
        || std::is_same<Transport, T>::value
        || std::is_same<Creature, T>::value
        || std::is_same<GameObject, T>::value
        || std::is_same<Corpse, T>::value
        || std::is_same<Pet, T>::value
        || std::is_same<DynamicObject, T>::value,
        "Only Player, Transport, DynamicObject, Creature, GameObject, Pet and Corpse can be registered in global HashMapHolder");

    typedef std::unordered_map<uint64, T*> MapType;

    static void Insert(T* o);

    static void Remove(T* o);

    static T* Find(uint64 guid);

    static MapType& GetContainer();

    static boost::shared_mutex* GetLock();
};

class TC_GAME_API ObjectAccessor
{
    ObjectAccessor();
    ~ObjectAccessor();
    ObjectAccessor(const ObjectAccessor &);
    ObjectAccessor& operator=(const ObjectAccessor &) = delete;

    public:
        static ObjectAccessor* instance()
        {
            static ObjectAccessor instance;
            return &instance;
        }
        
        typedef std::unordered_map<uint64, Corpse* > Player2CorpsesMapType;
        typedef std::unordered_map<Player*, UpdateData>::value_type UpdateDataValueType;

        template<class T> static T* GetObjectInWorld(uint64 guid, T* /*fake*/)
        {
            return HashMapHolder<T>::Find(guid);
        }

        // Player may be not in world while in ObjectAccessor
        static Player* GetObjectInWorld(uint64 guid, Player* /*typeSpecifier*/);
        static WorldObject* GetObjectInWorld(uint64 guid, WorldObject* p);
        static Unit* GetObjectInWorld(uint64 guid, Unit* /*fake*/);

        // returns object if is in map
        template<class T> static T* GetObjectInMap(ObjectGuid guid, Map* map, T* /*typeSpecifier*/)
        {
            ASSERT(map);
            if (T * obj = GetObjectInWorld(guid, (T*)NULL))
                if (obj->GetMap() == map)
                    return obj;
            return NULL;
        }

        template<class T> static T* GetObjectInWorld(uint32 mapid, float x, float y, uint64 guid, T* /*fake*/)
        {
            T* obj = HashMapHolder<T>::Find(guid);
            if(!obj || obj->GetMapId() != mapid) return nullptr;

            CellCoord p = Trinity::ComputeCellCoord(x,y);
            if (!p.IsCoordValid())
            {
                TC_LOG_ERROR("misc","ObjectAccessor::GetObjectInWorld: invalid coordinates supplied X:%f Y:%f grid cell [%u:%u]", x, y, p.x_coord, p.y_coord);
                return nullptr;
            }

            CellCoord q = Trinity::ComputeCellCoord(obj->GetPositionX(),obj->GetPositionY());
            if (!q.IsCoordValid())
            {
                TC_LOG_ERROR("misc","ObjectAccessor::GetObjecInWorld: object " UI64FMTD " has invalid coordinates X:%f Y:%f grid cell [%u:%u]", obj->GetGUID(), obj->GetPositionX(), obj->GetPositionY(), q.x_coord, q.y_coord);
                return nullptr;
            }

            int32 dx = int32(p.x_coord) - int32(q.x_coord);
            int32 dy = int32(p.y_coord) - int32(q.y_coord);

            if (dx > -2 && dx < 2 && dy > -2 && dy < 2) return obj;
            else return nullptr;
        }

        // thread-safe
        static Object* GetObjectByTypeMask(Player const &, uint64, uint32 typemask);
        // thread-safe
        static Creature* GetCreature(WorldObject const &, uint64);
        // thread-safe
        static Creature* GetCreatureOrPetOrVehicle(WorldObject const &, uint64);
        // thread-safe
        static Unit* GetUnit(WorldObject const &, uint64);
        // thread-safe
        inline static Pet* GetPet(Unit const &, uint64 guid) { return GetPet(guid); }
        // thread-safe
        inline static Player* GetPlayer(Unit const &, uint64 guid) { return FindPlayer(guid); }
        // thread-safe
        static GameObject* GetGameObject(WorldObject const &, uint64);
        // thread-safe
        static DynamicObject* GetDynamicObject(Unit const &, uint64);
        // thread-safe
        static Corpse* GetCorpse(WorldObject const &u, uint64 guid);
        // thread-safe
        static Pet* GetPet(uint64 guid);
        
        /* Find a player in world by guid, thread-safe */
        static Player* FindPlayer(uint32);
        static Player* FindPlayer(uint64);
        /* Find a player in all connected players by guid, thread-safe.
           /!\ Player may not be in world. */
        static Player* FindConnectedPlayer(uint64);
        /* Find a unit in world by guid, thread-safe */
        static Unit* FindUnit(uint64);

        /* Find a player in all connected players by name, thread-safe. */
        Player* FindPlayerByName(std::string const& name);
        /* Find a player in all connected players by name, thread-safe.
        /!\ Player may not be in world. */
        static Player* FindConnectedPlayerByName(std::string const& name);

        /* when using this, you must use the hashmapholder's lock
         Example: boost::shared_lock<boost::shared_mutex> lock(*HashMapHolder<Player>::GetLock());
        */
        static HashMapHolder<Player>::MapType const& GetPlayers()
        {
            return HashMapHolder<Player>::GetContainer();
        }

        /*when using this, you must use the hashmapholder's lock
         Example: boost::shared_lock<boost::shared_mutex> lock(*HashMapHolder<Creature>::GetLock());
        */
        static HashMapHolder<Creature>::MapType const& GetCreatures()
        {
            return HashMapHolder<Creature>::GetContainer();
        }

        /* when using this, you must use the hashmapholder's lock
        Example: boost::shared_lock<boost::shared_mutex> lock(*HashMapHolder<GameObject>::GetLock());
        */
        static HashMapHolder<GameObject>::MapType const& GetGameObjects()
        {
            return HashMapHolder<GameObject>::GetContainer();
        }

        /** Add an object in hash map holder, thread-safe */
        template<class T> void AddObject(T *object)
        {
            HashMapHolder<T>::Insert(object);
        }

        /** Removes an object from hash map holder, thread-safe */
        template<class T> void RemoveObject(T *object)
        {
            HashMapHolder<T>::Remove(object);
        }

        /** Removes a player from hash map holder, thread-safe */
        void RemoveObject(Player *pl)
        {
            HashMapHolder<Player>::Remove(pl);

            std::lock_guard<std::mutex> lock(_objectLock);

            auto iter2 = std::find(i_objects.begin(), i_objects.end(), (Object *)pl);
            if( iter2 != i_objects.end() )
                i_objects.erase(iter2);
        }

        /** save all players in database, thread-safe */
        void SaveAllPlayers();

        /** Mark object as updated, thread-safe */
        void AddUpdateObject(Object *obj);
        /** Mark object as not updated, thread-safe */
        void RemoveUpdateObject(Object *obj);

        /** Build and send ObjectFields updates of every objects for every players */
        void Update(uint32 diff);

        Corpse* GetCorpseForPlayerGUID(uint64 guid);
        /** Remove a corpse from world, thread-safe */
        void RemoveCorpse(Corpse *corpse);
        /** Add a corpse to work, thread-safe 
        Don't delete the corpse pointer afterwards as is used in the object accessor. Corpse object will be deleted in ConvertCorpseForPlayer.
        */
        void AddCorpse(Corpse* corpse);
        void AddCorpsesToGrid(GridPair const& gridpair,GridType& grid,Map* map);
        Corpse* ConvertCorpseForPlayer(uint64 player_guid, bool insignia = false);

        static void UpdateObjectVisibility(WorldObject* obj);
        
        void UnloadAll();

    private:
        /** Set of updated objects. The object is deleted from this set when the updates have been sent. */
        std::set<Object *> i_objects;
        /** List of every corpse currently in world  */
        Player2CorpsesMapType i_player2corpse;

        /** Lock for i_objects */
        std::mutex _objectLock;
        /** Lock for i_player2corpse */
        boost::shared_mutex _corpseLock;
};

#define sObjectAccessor ObjectAccessor::instance()

#endif
