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

#ifndef _ADDONMGR_H
#define _ADDONMGR_H

#include "Define.h"
#include <string>
#include <list>

enum ClientBuild : uint32;

struct AddonInfo
{
    AddonInfo(const std::string& name, uint8 enabled, uint32 crc, uint8 state, bool crcOrPubKey, uint32 build)
        : Name(name), CRC(crc), State(state), UsePublicKeyOrCRC(crcOrPubKey), Enabled(enabled), build(build)
        { }

    std::string Name;
    uint8 Enabled; //LK only
    uint32 CRC;
    uint32 build;
    uint8 State;
    bool UsePublicKeyOrCRC;
};

struct SavedAddon
{
    SavedAddon(std::string const& name, uint32 crc, uint32 build) :
        Name(name),
        CRC(crc),
        build(build)
    { }

    std::string Name;
    uint32 CRC;
    uint32 build;
};

struct BannedAddon
{
    uint32 Id;
    uint8 NameMD5[16];
    uint8 VersionMD5[16];
    uint32 Timestamp;
};

//LK
#define STANDARD_ADDON_CRC_12340 0x4C1C776DLL
//BC
#define STANDARD_ADDON_CRC_8606 0x1c776d01LL

namespace AddonMgr
{
    void LoadFromDB();
    void SaveAddon(AddonInfo const& addon);
    SavedAddon const* GetAddonInfo(const std::string& name, ClientBuild build);

    typedef std::list<BannedAddon> BannedAddonList;
    BannedAddonList const* GetBannedAddons();

    uint64 GetStandardAddonCRC(uint32 clientBuild);
}

#endif