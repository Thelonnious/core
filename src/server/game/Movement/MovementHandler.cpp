/*
 * Copyright (C) 2008-2014 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
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

#include "Common.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "Log.h"
#include "Corpse.h"
#include "Player.h"
#include "SpellAuras.h"
#include "MapManager.h"
#include "Transport.h"
#include "BattleGround.h"
#include "WaypointMovementGenerator.h"
#include "InstanceSaveMgr.h"
#include "ObjectMgr.h"
#include "World.h"
#include "IRCMgr.h"
#include "Pet.h"

#define MOVEMENT_PACKET_TIME_DELAY 0

void WorldSession::HandleMoveWorldportAckOpcode(WorldPacket & /*recvData*/)
{
   // TC_LOG_DEBUG("network", "WORLD: got MSG_MOVE_WORLDPORT_ACK.");
    HandleMoveWorldportAck();
}

void WorldSession::HandleMoveWorldportAck()
{
    // ignore unexpected far teleports
    if (!GetPlayer()->IsBeingTeleportedFar())
        return;

    GetPlayer()->SetSemaphoreTeleportFar(false);

    // get the teleport destination
    WorldLocation &loc = GetPlayer()->GetTeleportDest();

    // possible errors in the coordinate validity check
    if(!MapManager::IsValidMapCoord(loc.m_mapId,loc.m_positionX,loc.m_positionY,loc.m_positionZ,loc.m_orientation))
    {
        LogoutPlayer(false);
        return;
    }

    // get the destination map entry, not the current one, this will fix homebind and reset greeting
    MapEntry const* mEntry = sMapStore.LookupEntry(loc.m_mapId);
    InstanceTemplate const* mInstance = sObjectMgr->GetInstanceTemplate(loc.m_mapId);

    // reset instance validity, except if going to an instance inside an instance
    if(GetPlayer()->m_InstanceValid == false && !mInstance)
        GetPlayer()->m_InstanceValid = true;

	Map* oldMap = GetPlayer()->GetMap();
	Map* newMap = sMapMgr->CreateMap(loc.GetMapId(), GetPlayer());

	if (GetPlayer()->IsInWorld())
	{
		TC_LOG_ERROR("network", "%s %s is still in world when teleported from map %s (%u) to new map %s (%u)", ObjectGuid(GetPlayer()->GetGUID()).ToString().c_str(), GetPlayer()->GetName().c_str(), oldMap->GetMapName(), oldMap->GetId(), newMap ? newMap->GetMapName() : "Unknown", loc.GetMapId());
		oldMap->RemovePlayerFromMap(GetPlayer(), false);
	}

	// relocate the player to the teleport destination
	// the CannotEnter checks are done in TeleporTo but conditions may change
	// while the player is in transit, for example the map may get full
	if (!newMap || newMap->CannotEnter(GetPlayer()))
	{
		TC_LOG_ERROR("network", "Map %d (%s) could not be created for player %d (%s), porting player to homebind", loc.GetMapId(), newMap ? newMap->GetMapName() : "Unknown", ObjectGuid(GetPlayer()->GetGUID()).GetCounter(), GetPlayer()->GetName().c_str());
		GetPlayer()->TeleportTo(GetPlayer()->m_homebindMapId, GetPlayer()->m_homebindX, GetPlayer()->m_homebindY, GetPlayer()->m_homebindZ, GetPlayer()->GetOrientation());
		return;
	}


	float z = loc.GetPositionZ();
	if (GetPlayer()->HasUnitMovementFlag(MOVEMENTFLAG_HOVER))
#ifdef LICH_KING
		z += GetPlayer()->GetFloatValue(UNIT_FIELD_HOVERHEIGHT);
#else
		z += DEFAULT_HOVER_HEIGHT;
#endif
    GetPlayer()->Relocate(loc.m_positionX, loc.m_positionY, z, loc.m_orientation);
    GetPlayer()->SetFallInformation(0, GetPlayer()->GetPositionZ());

	GetPlayer()->ResetMap();
	GetPlayer()->SetMap(newMap);

    // check this before Map::Add(player), because that will create the instance save!
    bool reset_notify = (GetPlayer()->GetBoundInstance(GetPlayer()->GetMapId(), GetPlayer()->GetDifficulty()) == NULL);

    GetPlayer()->SendInitialPacketsBeforeAddToMap();
    // the CanEnter checks are done in TeleportTo but conditions may change
    // while the player is in transit, for example the map may get full
    if(!GetPlayer()->GetMap()->AddPlayerToMap(GetPlayer()))
    {
		TC_LOG_ERROR("network", "WORLD: failed to teleport player %s (%d) to map %d (%s) because of unknown reason!",
			GetPlayer()->GetName().c_str(), ObjectGuid(GetPlayer()->GetGUID()).GetCounter(), loc.GetMapId(), newMap ? newMap->GetMapName() : "Unknown");
		GetPlayer()->ResetMap();
		GetPlayer()->SetMap(oldMap);

        // teleport the player home
        GetPlayer()->SetDontMove(false);
        if(!GetPlayer()->TeleportTo(GetPlayer()->m_homebindMapId, GetPlayer()->m_homebindX, GetPlayer()->m_homebindY, GetPlayer()->m_homebindZ, GetPlayer()->GetOrientation()))
        {
            // the player must always be able to teleport home
            TC_LOG_ERROR("network","WORLD: failed to teleport player %s (%d) to homebind location %d,%f,%f,%f,%f!", GetPlayer()->GetName().c_str(), GetPlayer()->GetGUIDLow(), GetPlayer()->m_homebindMapId, GetPlayer()->m_homebindX, GetPlayer()->m_homebindY, GetPlayer()->m_homebindZ, GetPlayer()->GetOrientation());
            ABORT();
        }
        return;
    }

    //this will set player's team ... so IT MUST BE CALLED BEFORE SendInitialPacketsAfterAddToMap()
    // battleground state prepare (in case join to BG), at relogin/tele player not invited
    // only add to bg group and object, if the player was invited (else he entered through command)
    if(_player->InBattleground())
    {
        // cleanup seting if outdated
        if(!mEntry->IsBattlegroundOrArena())
        {
            // Do next only if found in battleground
            _player->SetBattlegroundId(0);                          // We're not in BG.
            // reset destination bg team
            _player->SetBGTeam(0);
        }
        // join to bg case
        else if(Battleground *bg = _player->GetBattleground())
        {
            if(_player->IsInvitedForBattlegroundInstance(_player->GetBattlegroundId()))
                bg->AddPlayer(_player);

            if (bg->isSpectator(_player->GetGUID()))
                bg->onAddSpectator(_player);
        }
    }

    GetPlayer()->SendInitialPacketsAfterAddToMap();

    // flight fast teleport case
    if(GetPlayer()->GetMotionMaster()->GetCurrentMovementGeneratorType() == FLIGHT_MOTION_TYPE)
    {
        if(!_player->InBattleground())
        {
            // short preparations to continue flight
            FlightPathMovementGenerator* flight = (FlightPathMovementGenerator*)(GetPlayer()->GetMotionMaster()->top());
            flight->Initialize(GetPlayer());
            return;
        }

        // battleground state prepare, stop flight
        GetPlayer()->GetMotionMaster()->MovementExpired();
        GetPlayer()->CleanupAfterTaxiFlight();
    }

    // resurrect character at enter into instance where his corpse exist after add to map
	if (mEntry->IsDungeon() && !GetPlayer()->IsAlive())
		if (GetPlayer()->GetCorpseLocation().GetMapId() == mEntry->MapID)
    {
        if( mEntry->IsDungeon() )
        {
            GetPlayer()->ResurrectPlayer(0.5f,false);
            GetPlayer()->SpawnCorpseBones();
            GetPlayer()->SaveToDB();
        }
    }

    if(mEntry->IsRaid() && mInstance)
    {
        if(reset_notify)
        {
#ifdef LICH_KING
            FIXME; //LK has this message for dungeon as well
#else
            uint32 timeleft = sInstanceSaveMgr->GetResetTimeFor(GetPlayer()->GetMapId(), RAID_DIFFICULTY_NORMAL) - time(NULL);
            GetPlayer()->SendInstanceResetWarning(GetPlayer()->GetMapId(), timeleft); // greeting at the entrance of the resort raid instance
#endif
        }
    }

    // mount allow check
    if(!mEntry->IsMountAllowed())
        _player->RemoveAurasByType(SPELL_AURA_MOUNTED);

    // honorless target
    if(GetPlayer()->pvpInfo.IsHostile)
        GetPlayer()->CastSpell(GetPlayer(), 2479, true);

    // resummon pet
    GetPlayer()->ResummonPetTemporaryUnSummonedIfAny();

    GetPlayer()->SetDontMove(false);

    //lets process all delayed operations on successful teleport
    GetPlayer()->ProcessDelayedOperations();
}

void WorldSession::HandleMoveTeleportAck(WorldPacket& recvData)
{
    //TC_LOG_DEBUG("network", "MSG_MOVE_TELEPORT_ACK");
    uint64 guid;
    if(GetClientBuild() == BUILD_335)
        recvData.readPackGUID(guid);
    else
        recvData >> guid;

    uint32 sequenceIndex, time;
    recvData >> sequenceIndex >> time;
    //TC_LOG_DEBUG("network", "Guid " UI64FMTD, guid);
    //TC_LOG_DEBUG("network", "Flags %u, time %u", flags, time/IN_MILLISECONDS);

    Player* plMover = _player->m_unitMovedByMe->ToPlayer();
    if (guid != plMover->GetGUID())
        return;

    if (!plMover || !plMover->IsBeingTeleportedNear())
        return;

    plMover->SetSemaphoreTeleportNear(false);

    uint32 old_zone = plMover->GetZoneId();

    WorldLocation const& dest = plMover->GetTeleportDest();
    Position oldPos(*plMover);

    plMover->UpdatePosition(dest, true);
    plMover->SetFallInformation(0, GetPlayer()->GetPositionZ());

	uint32 newzone, newarea;
	plMover->GetZoneAndAreaId(newzone, newarea);
	plMover->UpdateZone(newzone, newarea);

	// new zone
	if (old_zone != newzone)
	{
		// honorless target
		if (plMover->pvpInfo.IsHostile)
			plMover->CastSpell(plMover, 2479, true);

		// in friendly area
		else if (plMover->IsPvP() && !plMover->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_IN_PVP))
			plMover->UpdatePvP(false, false);
	}

    // teleport pets if they are not unsummoned
    if (Pet* pet = plMover->GetPet())
    {
        if (!pet->IsWithinDist3d(plMover, plMover->GetMap()->GetVisibilityRange() - 5.0f))
            pet->NearTeleportTo(plMover->GetPositionX(), plMover->GetPositionY(), plMover->GetPositionZ(), pet->GetOrientation());
    }

    // resummon pet
    GetPlayer()->ResummonPetTemporaryUnSummonedIfAny();

    //lets process all delayed operations on successful teleport
    GetPlayer()->ProcessDelayedOperations();

    plMover->GetMotionMaster()->ReinitializeMovement();
}

/*
MSG_MOVE_START_FORWARD MSG_MOVE_START_BACKWARD MSG_MOVE_STOP MSG_MOVE_START_STRAFE_LEFT MSG_MOVE_START_STRAFE_RIGHT
MSG_MOVE_STOP_STRAFE MSG_MOVE_JUMP MSG_MOVE_START_TURN_LEFT MSG_MOVE_START_TURN_RIGHT MSG_MOVE_STOP_TURN 
MSG_MOVE_START_PITCH_UP MSG_MOVE_START_PITCH_DOWN MSG_MOVE_STOP_PITCH MSG_MOVE_SET_RUN_MODE MSG_MOVE_SET_WALK_MODE
MSG_MOVE_FALL_LAND MSG_MOVE_START_SWIM MSG_MOVE_STOP_SWIM MSG_MOVE_SET_FACING MSG_MOVE_SET_PITCH MSG_MOVE_HEARTBEAT
CMSG_MOVE_FALL_RESET CMSG_MOVE_SET_FLY MSG_MOVE_START_ASCEND MSG_MOVE_STOP_ASCEND CMSG_MOVE_CHNG_TRANSPORT
MSG_MOVE_START_DESCEND 
*/
void WorldSession::HandleMovementOpcodes(WorldPacket& recvData)
{
    uint16 opcode = recvData.GetOpcode();

    Unit* mover = _player->m_unitMovedByMe;

    ASSERT(mover != NULL);                      // there must always be a mover

    Player* plrMover = mover->ToPlayer(); // The unit we're currently moving

    // ignore, waiting processing in WorldSession::HandleMoveWorldportAckOpcode and WorldSession::HandleMoveTeleportAck
    if (plrMover && plrMover->IsBeingTeleported())
    {
        recvData.rfinish();                     // prevent warnings spam
        return;
    } 

#ifdef LICH_KING
    uint64 guid;

    recvData.readPackGUID(guid);
#endif

    MovementInfo movementInfo;
#ifdef LICH_KING
    movementInfo.guid = guid;
#endif
    ReadMovementInfo(recvData, &movementInfo);

    recvData.rfinish();                         // prevent warnings spam

    if (!movementInfo.pos.IsPositionValid())
        return;

    /* handle special cases */
    if (movementInfo.HasMovementFlag(MOVEMENTFLAG_ONTRANSPORT))
    {
        // We were teleported, skip packets that were broadcast before teleport
        if (movementInfo.pos.GetExactDist2d(mover) > SIZE_OF_GRIDS)
        {
            recvData.rfinish();                 // prevent warnings spam
            return;
        }

#ifdef LICH_KING
        // T_POS ON VEHICLES!
        if (mover->GetVehicle())
            movementInfo.transport.pos = mover->m_movementInfo.transport.pos;
#endif

        // transports size limited
        // (also received at zeppelin leave by some reason with t_* as absolute in continent coordinates, can be safely skipped)
        if (fabs(movementInfo.transport.pos.GetPositionX()) > 75.0f || fabs(movementInfo.transport.pos.GetPositionY()) > 75.0f || fabs(movementInfo.transport.pos.GetPositionZ()) > 75.0f ) 
        {
            recvData.rfinish();                   // prevent warnings spam
            return;
        }

        if (!Trinity::IsValidMapCoord(movementInfo.pos.GetPositionX() + movementInfo.transport.pos.GetPositionX(), movementInfo.pos.GetPositionY() + movementInfo.transport.pos.GetPositionY(),
            movementInfo.pos.GetPositionZ() + movementInfo.transport.pos.GetPositionZ(), movementInfo.pos.GetOrientation() + movementInfo.transport.pos.GetOrientation()))
        {
            recvData.rfinish();                 // prevent warnings spam
            return;
        }

        // if we boarded a transport, add us to it
        if (plrMover)
        {
            if (!plrMover->GetTransport())
            {
                if (Transport* transport = plrMover->GetMap()->GetTransport(movementInfo.transport.guid))
                {
                    plrMover->m_transport = transport;
                    transport->AddPassenger(plrMover);
                }
            }
            else if (plrMover->GetTransport()->GetGUID() != movementInfo.transport.guid)
            {
                bool foundNewTransport = false;
                plrMover->m_transport->RemovePassenger(plrMover);
                if (Transport* transport = plrMover->GetMap()->GetTransport(movementInfo.transport.guid))
                {
                    foundNewTransport = true;
                    plrMover->m_transport = transport;
                    transport->AddPassenger(plrMover);
                    plrMover->m_anti_transportGUID = transport->GetGUID();
                }

                if (!foundNewTransport)
                {
                    plrMover->m_transport = NULL;
                    movementInfo.transport.Reset();
                }
            }
        }

        if (!mover->GetTransport()
#ifdef LICH_KING
            && !mover->GetVehicle()
#endif
            )
            movementInfo.flags &= ~MOVEMENTFLAG_ONTRANSPORT;
    }
    else if (plrMover && plrMover->GetTransport())                // if we were on a transport, leave
    {
        plrMover->m_transport->RemovePassenger(plrMover);
        plrMover->m_transport = NULL;
        plrMover->m_anti_transportGUID = 0;
        movementInfo.transport.Reset();
    }

    if (plrMover && ((movementInfo.flags & MOVEMENTFLAG_SWIMMING) != 0) != plrMover->IsInWater())
    {
        // now client not include swimming flag in case jumping under water
        plrMover->SetInWater(!plrMover->IsInWater() || plrMover->GetBaseMap()->IsUnderWater(movementInfo.pos.GetPositionX(), movementInfo.pos.GetPositionY(), movementInfo.pos.GetPositionZ()));
    }
    // Dont allow to turn on walking if charming other player
    if (mover->GetGUID() != _player->GetGUID())
        movementInfo.flags &= ~MOVEMENTFLAG_WALKING;

    // ---- anti-cheat features -->>>
    if(plrMover && _player->GetGUID() == plrMover->GetGUID()) //disabled for charmed
    {
        if (opcode == MSG_MOVE_FALL_LAND && plrMover && !plrMover->IsInFlight())
        {
            //alternate falltime calculation
            if (plrMover->m_anti_beginfalltime != 0) 
            {
                uint32 ServerFallTime = GetMSTime() - plrMover->m_anti_beginfalltime;
                if (movementInfo.fallTime < ServerFallTime && (time(NULL) - plrMover->m_anti_TeleTime) > 15) {
                    movementInfo.fallTime = ServerFallTime;
                }
                plrMover->m_anti_beginfalltime = 0;
            }

            if ((plrMover->m_anti_transportGUID == 0) && sWorld->GetMvAnticheatEnable() &&
                plrMover->GetSession()->GetSecurity() <= sWorld->GetMvAnticheatGmLevel() &&
                /*plrMover->GetSession()->GetGroupId() == 0 &&*/ //ignore gm in groups
                plrMover->GetMotionMaster()->GetCurrentMovementGeneratorType()!=FLIGHT_MOTION_TYPE &&
                (time(NULL) - plrMover->m_anti_TeleTime) > 15)
            {
                const uint32 CurTime=GetMSTime();
                if(GetMSTimeDiff(plrMover->m_anti_lastalarmtime,CurTime) > sWorld->GetMvAnticheatAlarmPeriod())
                {
                    plrMover->m_anti_alarmcount = 0;
                }
                /* I really don't care about movement-type yet (todo)
                UnitMoveType move_type;

                if (MovementFlags & MOVEMENTFLAG_PLAYER_FLYING) move_type = MOVE_FLY;
                else if (MovementFlags & MOVEMENTFLAG_SWIMMING) move_type = MOVE_SWIM;
                else if (MovementFlags & MOVEMENTFLAG_WALK_MODE) move_type = MOVE_WALK;
                else move_type = MOVE_RUN;*/

                float delta_x = plrMover->GetPositionX() - movementInfo.pos.GetPositionX();
                float delta_y = plrMover->GetPositionY() - movementInfo.pos.GetPositionY();
                //float delta_z = plrMover->GetPositionZ() - movementInfo.pos.GetPositionZ();
                float delta = sqrt(delta_x * delta_x + delta_y * delta_y); // Len of movement-vector via Pythagoras (a^2+b^2=Len)
                //float tg_z = 0.0f; //tangens
                //float delta_t = GetMSTimeDiff(GetPlayer()->m_anti_lastmovetime,CurTime); //value not used
                plrMover->m_anti_lastmovetime = CurTime;
                plrMover->m_anti_MovedLen += delta;

                /*if (delta_t > 15000.0f)
                    delta_t = 15000.0f;*/

                //antiOFF fall-damage, MOVEMENTFLAG_UNK4 seted by client if player try movement when falling and unset in this case the MOVEMENTFLAG_JUMPING_OR_FALLING flag.
                if ((plrMover->m_anti_beginfalltime == 0) &&
                        (movementInfo.GetMovementFlags() & (MOVEMENTFLAG_JUMPING_OR_FALLING | MOVEMENTFLAG_FALLING_FAR)) != 0) {
                    plrMover->m_anti_beginfalltime = CurTime;
                } else if (plrMover->m_anti_beginfalltime != 0 &&
                        ((movementInfo.GetMovementFlags() & (MOVEMENTFLAG_JUMPING_OR_FALLING | MOVEMENTFLAG_FALLING_FAR)) == 0) &&
                        (movementInfo.GetMovementFlags() & MOVEMENTFLAG_SWIMMING) != 0) {
                    plrMover->m_anti_beginfalltime = 0;
                }

                if (plrMover->m_anti_NextLenCheck <= CurTime) {
                    // Check every 500ms is a lot more advisable then 1000ms, because normal movment packet arrives every 500ms
                    uint32 FH__Tmp1 = plrMover->m_anti_NextLenCheck;
                    float delta_xyt = plrMover->m_anti_MovedLen / (CurTime - (plrMover->m_anti_NextLenCheck - 500));
                    plrMover->m_anti_NextLenCheck = CurTime + 500;
                    plrMover->m_anti_MovedLen = 0.0f;
                    if (delta_xyt > 0.04f && delta <= 80.0f) {
                        Anti__CheatOccurred(CurTime, "Speed hack", delta_xyt, GetOpcodeNameForLogging(static_cast<OpcodeClient>(opcode)).c_str(),
                                (float) (plrMover->GetMotionMaster()->GetCurrentMovementGeneratorType()),
                                (float) (CurTime - (FH__Tmp1 - 500)), &movementInfo);
                    }
                }
        
                if (delta > 80.0f) {
                    Anti__ReportCheat("Tele hack", delta, GetOpcodeNameForLogging(static_cast<OpcodeClient>(opcode)).c_str());
                }

                // Check for waterwalking
                if (((movementInfo.GetMovementFlags() & MOVEMENTFLAG_WATERWALKING) != 0) &&
                        ((movementInfo.GetMovementFlags() ^ MOVEMENTFLAG_WATERWALKING) != 0) && // Client sometimes set waterwalk where it shouldn't do that...
                        ((movementInfo.GetMovementFlags() & MOVEMENTFLAG_JUMPING_OR_FALLING) == 0) &&
                        plrMover->GetBaseMap()->IsUnderWater(movementInfo.pos.GetPositionX(), movementInfo.pos.GetPositionY(), movementInfo.pos.GetPositionZ() - 6.0f) &&
                        !(plrMover->HasAuraType(SPELL_AURA_WATER_WALK) || plrMover->HasAuraType(SPELL_AURA_GHOST))) {
                    Anti__CheatOccurred(CurTime, "Water walking", 0.0f, NULL, 0.0f, (uint32)(movementInfo.GetMovementFlags()));
                }

                float Anti__GroundZ = plrMover->GetMap()->GetHeight(plrMover->GetPositionX(),plrMover->GetPositionY(),MAX_HEIGHT);
                float Anti__FloorZ = plrMover->GetMap()->GetHeight(plrMover->GetPositionX(),plrMover->GetPositionY(),plrMover->GetPositionZ());
                float Anti__MapZ = (Anti__FloorZ < -199900.0f) ? Anti__GroundZ : Anti__FloorZ;

                if(!plrMover->CanFly() &&
                    ((movementInfo.GetMovementFlags() & (MOVEMENTFLAG_CAN_FLY | MOVEMENTFLAG_PLAYER_FLYING)) != 0) &&
                    !plrMover->GetBaseMap()->IsUnderWater(movementInfo.pos.GetPositionX(), movementInfo.pos.GetPositionY(), movementInfo.pos.GetPositionZ()-5.0f) &&
                    (Anti__MapZ==INVALID_HEIGHT || Anti__MapZ+5.0f < plrMover->GetPositionZ()) && Anti__MapZ >= -199900.0f)
                {
                    Anti__CheatOccurred(CurTime,"Fly hack",
                        ((uint8)(plrMover->HasAuraType(SPELL_AURA_FLY))) +
                        ((uint8)(plrMover->HasAuraType(SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED))*2),
                        NULL,plrMover->GetPositionZ()-(Anti__MapZ+5.0f));
                }
            }
        }
    }
    // <<---- anti-cheat features

    uint32 mstime = GetMSTime();
    /*----------------------*/
    if (m_clientTimeDelay == 0)
        m_clientTimeDelay = mstime > movementInfo.time ? std::min(mstime - movementInfo.time, (uint32)100) : 0;

    // Xinef: do not allow to move with UNIT_FLAG_REMOVE_CLIENT_CONTROL
    if (mover->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_REMOVE_CLIENT_CONTROL))
    {
        // Xinef: skip moving packets
        if (movementInfo.HasMovementFlag(MOVEMENTFLAG_MASK_MOVING))
            return;
        movementInfo.pos.Relocate(mover->GetPositionX(), mover->GetPositionY(), mover->GetPositionZ());

#ifdef LICH_KING
        if (mover->GetTypeId() == TYPEID_UNIT)
        {
            movementInfo.transport.guid = mover->m_movementInfo.transport.guid;
            movementInfo.transport.pos.Relocate(mover->m_movementInfo.transport.pos.GetPositionX(), mover->m_movementInfo.transport.pos.GetPositionY(), mover->m_movementInfo.transport.pos.GetPositionZ());
            movementInfo.transport.seat = mover->m_movementInfo.transport.seat;
        }
#endif
    }

    /* process position-change */
    WorldPacket data(opcode, recvData.size());
    movementInfo.time = movementInfo.time + m_clientTimeDelay + MOVEMENT_PACKET_TIME_DELAY;

    data.appendPackGUID(mover->GetGUID());
    WriteMovementInfo(&data, &movementInfo);
    mover->SendMessageToSet(&data, _player);
    mover->m_movementInfo = movementInfo;
    
#ifdef LICH_KING
    // this is almost never true (sunwell: only one packet when entering vehicle), normally use mover->IsVehicle()
    if (mover->GetVehicle())
    {
        mover->SetOrientation(movementInfo.pos.GetOrientation());
        mover->UpdatePosition(movementInfo.pos);
        return;
    }
#endif

    // sunwell: previously always mover->UpdatePosition(movementInfo.pos);
    if (movementInfo.flags & MOVEMENTFLAG_ONTRANSPORT && mover->GetTransport())
    {
        float x, y, z, o;
        movementInfo.transport.pos.GetPosition(x, y, z, o);
        mover->GetTransport()->CalculatePassengerPosition(x, y, z, &o);
        mover->UpdatePosition(x, y, z, o);
    }
    else
        mover->UpdatePosition(movementInfo.pos);

    if (!mover->IsStandState() && (movementInfo.flags & (MOVEMENTFLAG_MASK_MOVING | MOVEMENTFLAG_MASK_TURNING)))
        mover->SetStandState(UNIT_STAND_STATE_STAND);

    // fall damage generation (ignore in flight case that can be triggered also at lags in moment teleportation to another map).
    if (opcode == MSG_MOVE_FALL_LAND && plrMover && !plrMover->IsInFlight() && (!plrMover->GetTransport() || plrMover->GetTransport()->IsStaticTransport()))
        plrMover->HandleFall(movementInfo);

#ifdef LICH_KING
    //  interrupt parachutes upon falling or landing in water
    if (opcode == MSG_MOVE_FALL_LAND || opcode == MSG_MOVE_START_SWIM)
        mover->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_LANDING); // Parachutes
#endif

    if (plrMover)                                            // nothing is charmed, or player charmed
    {
        plrMover->UpdateFallInformationIfNeed(movementInfo, opcode);

        //used to handle spell interrupts on move (client does not always does it by itself)
        if (plrMover->isMoving())
            plrMover->SetHasMovedInUpdate(true);

        if (movementInfo.pos.GetPositionZ() < plrMover->GetMap()->GetMinHeight(movementInfo.pos.GetPositionX(), movementInfo.pos.GetPositionY()))
        {
            if (!(plrMover->GetBattleground() && plrMover->GetBattleground()->HandlePlayerUnderMap(_player)))
            {
                // NOTE: this is actually called many times while falling
                // even after the player has been teleported away
                if (plrMover->IsAlive())
                {
                    plrMover->SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_IS_OUT_OF_BOUNDS);
                    plrMover->EnvironmentalDamage(DAMAGE_FALL_TO_VOID, GetPlayer()->GetMaxHealth());
                    // player can be alive if GM/etc
                    // change the death state to CORPSE to prevent the death timer from
                    // starting in the next player update
                    if (!plrMover->IsAlive())
                        plrMover->KillPlayer();
                }
            }
        }
    }
}

/*
CMSG_FORCE_FLIGHT_SPEED_CHANGE_ACK
CMSG_FORCE_FLIGHT_BACK_SPEED_CHANGE_ACK
CMSG_FORCE_RUN_SPEED_CHANGE_ACK
CMSG_FORCE_RUN_BACK_SPEED_CHANGE_ACK
CMSG_FORCE_SWIM_SPEED_CHANGE_ACK
CMSG_FORCE_WALK_SPEED_CHANGE_ACK
CMSG_FORCE_SWIM_BACK_SPEED_CHANGE_ACK
CMSG_FORCE_TURN_RATE_CHANGE_ACK
*/
void WorldSession::HandleForceSpeedChangeAck(WorldPacket &recvData)
{
    uint32 opcode = recvData.GetOpcode();
   // TC_LOG_DEBUG("network", "WORLD: Recvd %s (%u, 0x%X) opcode", GetOpcodeNameForLogging(static_cast<OpcodeClient>(opcode)).c_str(), opcode, opcode);

    /* extract packet */
    uint64 guid;
    uint32 unk1;
    float  newspeed;

    recvData >> guid;

    // now can skip not our packet
    if (_player->GetGUID() != guid)
    {
        recvData.rfinish();                   // prevent warnings spam
        return;
    }

    // continue parse packet

    recvData >> unk1;                                      // counter or moveEvent

    MovementInfo movementInfo;
    ReadMovementInfo(recvData, &movementInfo);

    recvData >> newspeed;
    /*----------------*/

    // client ACK send one packet for mounted/run case and need skip all except last from its
    // in other cases anti-cheat check can be fail in false case
    UnitMoveType move_type;
    UnitMoveType force_move_type;

    static char const* move_type_name[MAX_MOVE_TYPE] = {  "Walk", "Run", "RunBack", "Swim", "SwimBack", "TurnRate", "Flight", "FlightBack" };

    switch (opcode)
    {
        case CMSG_FORCE_WALK_SPEED_CHANGE_ACK:          move_type = MOVE_WALK;          force_move_type = MOVE_WALK;        break;
        case CMSG_FORCE_RUN_SPEED_CHANGE_ACK:           move_type = MOVE_RUN;           force_move_type = MOVE_RUN;         break;
        case CMSG_FORCE_RUN_BACK_SPEED_CHANGE_ACK:      move_type = MOVE_RUN_BACK;      force_move_type = MOVE_RUN_BACK;    break;
        case CMSG_FORCE_SWIM_SPEED_CHANGE_ACK:          move_type = MOVE_SWIM;          force_move_type = MOVE_SWIM;        break;
        case CMSG_FORCE_SWIM_BACK_SPEED_CHANGE_ACK:     move_type = MOVE_SWIM_BACK;     force_move_type = MOVE_SWIM_BACK;   break;
        case CMSG_FORCE_TURN_RATE_CHANGE_ACK:           move_type = MOVE_TURN_RATE;     force_move_type = MOVE_TURN_RATE;   break;
        case CMSG_FORCE_FLIGHT_SPEED_CHANGE_ACK:        move_type = MOVE_FLIGHT;        force_move_type = MOVE_FLIGHT;      break;
        case CMSG_FORCE_FLIGHT_BACK_SPEED_CHANGE_ACK:   move_type = MOVE_FLIGHT_BACK;   force_move_type = MOVE_FLIGHT_BACK; break;
        default:
            TC_LOG_ERROR("network", "WorldSession::HandleForceSpeedChangeAck: Unknown move type opcode: %u", opcode);
            return;
    }

    // skip all forced speed changes except last and unexpected
    // in run/mounted case used one ACK and it must be skipped.m_forced_speed_changes[MOVE_RUN} store both.
    if (_player->m_forced_speed_changes[force_move_type] > 0)
    {
        --_player->m_forced_speed_changes[force_move_type];
        if (_player->m_forced_speed_changes[force_move_type] > 0)
            return;
    }

    if (!_player->GetSession()->IsReplaying() && !_player->GetTransport() && fabs(_player->GetSpeed(move_type) - newspeed) > 0.01f)
    {
        if (_player->GetSpeed(move_type) > newspeed)         // must be greater - just correct
        {
            TC_LOG_ERROR("network", "%sSpeedChange player %s is NOT correct (must be %f instead %f), force set to correct value",
                move_type_name[move_type], _player->GetName().c_str(), _player->GetSpeed(move_type), newspeed);
            _player->SetSpeedRate(move_type, _player->GetSpeedRate(move_type));
        }
        else                                                // must be lesser - cheating
        {
            TC_LOG_DEBUG("misc", "Player %s from account id %u kicked for incorrect speed (must be %f instead %f)",
                _player->GetName().c_str(), _player->GetSession()->GetAccountId(), _player->GetSpeed(move_type), newspeed);
            _player->GetSession()->KickPlayer();
        }
    }
}

// sent by client when gaining control of a unit
void WorldSession::HandleSetActiveMoverOpcode(WorldPacket &recvData)
{
    //TC_LOG_DEBUG("network", "WORLD: Recvd CMSG_SET_ACTIVE_MOVER");

    //LK OK
    uint64 guid;
    recvData >> guid; //Client started controlling this unit

	/*TC 
	if (GetPlayer()->IsInWorld())
        if (_player->m_unitMovedByMe->GetGUID() != guid)
            TC_LOG_DEBUG("network", "HandleSetActiveMoverOpcode: incorrect mover guid: mover is %s and should be %s" , guid.ToString().c_str(), _player->m_unitMovedByMe->GetGUID().ToString().c_str());
			*/
    Unit* movedUnit = ObjectAccessor::GetUnit(*_player, guid);
    if(movedUnit)
        _player->SetMover(movedUnit);
    else
        TC_LOG_ERROR("network", "Player " UI64FMTD " - WorldSession::HandleSetActiveMoverOpcode could not find player with guid " UI64FMTD, _player->GetGUID(), guid);
}

//CMSG_MOVE_NOT_ACTIVE_MOVER
//sent by client when loosing control of a unit
void WorldSession::HandleMoveNotActiveMover(WorldPacket &recvData)
{
    //TC_LOG_DEBUG("network", "WORLD: Recvd CMSG_MOVE_NOT_ACTIVE_MOVER");

    uint64 old_mover_guid;
#ifdef LICH_KING
    recvData.readPackGUID(old_mover_guid);
#else
    recvData >> old_mover_guid;
#endif

    if (!_player->m_unitMovedByMe || !_player->m_unitMovedByMe->IsInWorld() || old_mover_guid != _player->m_unitMovedByMe->GetGUID())
    {
        recvData.rfinish(); // prevent warnings spam
        return;
    }

    MovementInfo movementInfo;
#ifdef LICH_KING
    mi.guid = old_mover_guid;
#endif
    ReadMovementInfo(recvData, &movementInfo);

    _player->m_movementInfo = movementInfo;
}

void WorldSession::HandleMountSpecialAnimOpcode(WorldPacket& /*recvData*/)
{
    WorldPacket data(SMSG_MOUNTSPECIAL_ANIM, 8);
    data << uint64(GetPlayer()->GetGUID());

    GetPlayer()->SendMessageToSet(&data, false);
}

// CMSG_MOVE_KNOCK_BACK_ACK
void WorldSession::HandleMoveKnockBackAck(WorldPacket& recvData)
{
    //TC_LOG_DEBUG("network", "CMSG_MOVE_KNOCK_BACK_ACK");

    uint64 guid;
#ifdef LICH_KING
    recvData.readPackGUID(guid);
#else
    recvData >> guid;
#endif

    if (!_player->m_unitMovedByMe || !_player->m_unitMovedByMe->IsInWorld() || guid != _player->m_unitMovedByMe->GetGUID())
    {
        recvData.rfinish(); // prevent warnings spam
        return;
    }

    recvData.read_skip<uint32>();                          // Always set to 0

    MovementInfo movementInfo;
#ifdef LICH_KING
    movementInfo.guid = guid;
#endif
    ReadMovementInfo(recvData, &movementInfo);

    _player->m_movementInfo = movementInfo;

    // Calculate timestamp
    uint32 move_time, mstime;
    mstime = GetMSTime();
    if (m_clientTimeDelay == 0)
        m_clientTimeDelay = mstime - movementInfo.time;
    move_time = (movementInfo.time - (mstime - m_clientTimeDelay)) + mstime + 500;
    movementInfo.time = move_time;
    
    // Save movement flags
    GetPlayer()->SetUnitMovementFlags(movementInfo.GetMovementFlags());

    // Send packet
    WorldPacket data(MSG_MOVE_KNOCK_BACK, uint16(recvData.size() + 4));
    data.appendPackGUID(guid);
    _player->BuildMovementPacket(&data);

    /* This is sent in addition to the rest of the movement data (yes, angle+velocity are sent twice) */
    data << movementInfo.jump.sinAngle;
    data << movementInfo.jump.cosAngle;
    data << movementInfo.jump.xyspeed;
    data << movementInfo.jump.zspeed; //InitVertSpeed 

    /* Do we really need to send the data to everyone? Seemed to work better */
    _player->SendMessageToSet(&data, false);
}

//CMSG_MOVE_HOVER_ACK
//BC structure confirmed
void WorldSession::HandleMoveHoverAck(WorldPacket& recvData)
{
    //TC_LOG_DEBUG("network", "CMSG_MOVE_HOVER_ACK");

    uint64 guid;                                            // guid - unused
#ifdef LICH_KING
    recvData.readPackGUID(guid);
#else
    recvData >> guid;
#endif

    recvData.read_skip<uint32>();                          // unk

    MovementInfo movementInfo;
    WriteMovementInfo(&recvData, &movementInfo);

    recvData.read_skip<uint32>();                          // unk2
}

//CMSG_MOVE_WATER_WALK_ACK
//BC structure confirmed
void WorldSession::HandleMoveWaterWalkAck(WorldPacket& recvData)
{
    //TC_LOG_DEBUG("network", "CMSG_MOVE_WATER_WALK_ACK");

    uint64 guid;                                            // guid - unused
    recvData >> guid;

    recvData.read_skip<uint32>();                          // unk

    MovementInfo movementInfo;
    ReadMovementInfo(recvData, &movementInfo);

    recvData.read_skip<uint32>();                          // unk2
}

void WorldSession::HandleSummonResponseOpcode(WorldPacket& recvData)
{
    if (!_player->IsAlive() || _player->IsInCombat())
        return;

    uint64 summoner_guid;
    bool agree;
    recvData >> summoner_guid;
    recvData >> agree;

    _player->SummonIfPossible(agree);
}

bool WorldSession::Anti__CheatOccurred(uint32 CurTime,const char* Reason,float Speed,const char* Op,
                                       float Val1,uint32 Val2,MovementInfo* MvInfo)
{
    if(!Reason)
    {
        TC_LOG_ERROR("FIXME","Anti__CheatOccurred: Missing Ply or Reason paremeter!");
        return false;
    }

    GetPlayer()->m_anti_lastalarmtime = CurTime;
    GetPlayer()->m_anti_alarmcount = GetPlayer()->m_anti_alarmcount + 1;

    if (GetPlayer()->m_anti_alarmcount > sWorld->GetMvAnticheatAlarmCount())
    {
        Anti__ReportCheat(Reason,Speed,Op,Val1,Val2,MvInfo);
        return true;
    }
    return false;
}

bool WorldSession::Anti__ReportCheat(const char* Reason,float Speed,const char* Op,float Val1,uint32 Val2,MovementInfo* MvInfo)
{
    if(!Reason)
    {
        TC_LOG_ERROR("warden","Anti__ReportCheat: Missing Player or Reason paremeter!");
        return false;
    }
    const char* player=GetPlayer()->GetName().c_str();
    uint32 Acc=GetPlayer()->GetSession()->GetAccountId();
    uint32 Map=GetPlayer()->GetMapId();
    if(!player)
    {
        TC_LOG_ERROR("warden","Anti__ReportCheat: Player with no name?!?");
        return false;
    }

    if(sWorld->GetMvAnticheatWarn())
    {
        if(lastCheatWarn + 120 < time(NULL)) //2m cooldown
        {
            lastCheatWarn = time(NULL);
            std::stringstream msg;
            msg << "New anticheat entry for player " << player << " (guid : " << GetPlayer()->GetGUIDLow() << ").";

            ChatHandler(GetPlayer()).SendGlobalGMSysMessage(msg.str().c_str());
        }
    }

    QueryResult Res=CharacterDatabase.PQuery("SELECT speed,Val1,Val2 FROM cheaters WHERE player='%s' AND reason LIKE '%s' AND Map='%u' AND last_date >= NOW()-300",player,Reason,Map);
    if(Res)
    {
        Field* Fields = Res->Fetch();

        std::stringstream Query;
        Query << "UPDATE cheaters SET count=count+1,last_date=NOW()";
        Query.precision(5);
        if(Speed>0.0f && Speed > Fields[0].GetFloat())
        {
            Query << ",speed='";
            Query << std::fixed << Speed;
            Query << "'";
        }

        if(Val1>0.0f && Val1 > Fields[1].GetFloat())
        {
            Query << ",Val1='";
            Query << std::fixed << Val1;
            Query << "'";
        }

        Query << " WHERE player='" << player << "' AND reason='" << Reason << "' AND Map='" << Map << "' AND last_date >= NOW()-300 ORDER BY entry LIMIT 1";

        CharacterDatabase.Execute(Query.str().c_str());
    }
    else
    {
        if(!Op)
        { Op=""; }
        std::stringstream Pos;
        Pos << "OldPos: " << GetPlayer()->GetPositionX() << " " << GetPlayer()->GetPositionY() << " "
            << GetPlayer()->GetPositionZ();
        if(MvInfo)
        {
            Pos << "\nNew: " << MvInfo->pos.GetPositionX() << " " << MvInfo->pos.GetPositionY() << " " << MvInfo->pos.GetPositionZ() << "\n"
                << "t_guid: " << MvInfo->transport.guid << " falltime: " << MvInfo->fallTime;
        }
        CharacterDatabase.PExecute("INSERT INTO cheaters (player,acctid,reason,speed,count,first_date,last_date,`Op`,Val1,Val2,Map,Pos,Level) "
            "VALUES ('%s','%u','%s','%f','1',NOW(),NOW(),'%s','%f','%u','%u','%s','%u')",
            player,Acc,Reason,Speed,Op,Val1,Val2,Map,
            Pos.str().c_str(),GetPlayer()->GetLevel());
    }

    if(sWorld->GetMvAnticheatKill() && GetPlayer()->IsAlive())
    {
        GetPlayer()->DealDamage(GetPlayer(), GetPlayer()->GetHealth(), NULL, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false);
    }
    if(sWorld->GetMvAnticheatKick())
    {
        GetPlayer()->GetSession()->KickPlayer();
    }
    if(sWorld->GetMvAnticheatBan() & 1)
    {
        sWorld->BanAccount(SANCTION_BAN_CHARACTER,player,sWorld->GetMvAnticheatBanTime(),"Cheat","Anticheat", nullptr);
    }
    if(sWorld->GetMvAnticheatBan() & 2)
    {
        QueryResult result = LoginDatabase.PQuery("SELECT last_ip FROM account WHERE id=%u", Acc);
        if(result)
        {

            Field *fields = result->Fetch();
            std::string LastIP = fields[0].GetString();
            if(!LastIP.empty())
                sWorld->BanAccount(SANCTION_BAN_IP,LastIP,sWorld->GetMvAnticheatBanTime(),"Cheat","Anticheat", nullptr);
        }
    }
    return true;
}
