#include "nwnx.hpp"

#include "API/CExoResMan.hpp"
#include "API/CNWSArea.hpp"
#include "API/CNWTileSetManager.hpp"
#include "API/CNWTileSet.hpp"
#include "API/CNWTileData.hpp"
#include "API/CAppManager.hpp"

using namespace NWNXLib;
using namespace NWNXLib::API;

struct CachedTilesetTileDoor
{
    int32_t type;
    Vector position;
    float orientation;
};
struct CachedTilesetTile
{
    std::string tileModel;
    std::string minimapTexture;
    std::string tl, t, tr, r, br, b, bl, l;
    int32_t numDoors;
    std::vector<CachedTilesetTileDoor> doors;
};
struct CachedTilesetGroup
{
    std::string name;
    int32_t strRef;
    int32_t rows;
    int32_t columns;
    std::vector<int32_t> tiles;
};
struct CachedTileset
{
    int32_t numTiles;
    int32_t numTerrains;
    int32_t numCrossers;
    int32_t numGroups;
    int32_t displayNameStrRef;
    int32_t isInterior;
    int32_t hasHeightTransition;
    float heightTransition;
    std::string borderTerrain;
    std::string defaultTerrain;
    std::string floorTerrain;
    std::string unlocalizedName;
    std::vector<std::string> terrains;
    std::vector<std::string> crossers;
    std::vector<CachedTilesetTile> tiles;
    std::vector<CachedTilesetGroup> groups;
};

static std::unordered_map<std::string, CachedTileset> s_CachedTilesets;

static CachedTileset* GetCachedTileset(const std::string& tileset)
{
    auto cachedTilesetIterator = s_CachedTilesets.find(tileset);
    if (cachedTilesetIterator != s_CachedTilesets.end())
    {
        return &cachedTilesetIterator->second;
    }

    auto *pTileset = Globals::AppManager()->m_pNWTileSetManager->GetTileSet(CResRef(tileset));

    if (!pTileset)
        return nullptr;

    if (!pTileset->m_pRes->Demand())
    {
        return nullptr;
    }

    CachedTileset cachedTileset{};
    char section[32];
    char entry[32];
    char value[256];

    cachedTileset.numTiles = pTileset->m_nNumTileData;
    cachedTileset.heightTransition = pTileset->m_fHeightTransition;
    pTileset->m_pRes->GetSectionEntryValue((char*)"TERRAIN TYPES", (char*)"Count", value);
    cachedTileset.numTerrains = atoi(value);
    pTileset->m_pRes->GetSectionEntryValue((char*)"CROSSER TYPES", (char*)"Count", value);
    cachedTileset.numCrossers = atoi(value);
    pTileset->m_pRes->GetSectionEntryValue((char*)"GROUPS", (char*)"Count", value);
    cachedTileset.numGroups = atoi(value);
    pTileset->m_pRes->GetSectionEntryValue((char*)"GENERAL", (char*)"Interior", value);
    cachedTileset.isInterior = atoi(value);
    pTileset->m_pRes->GetSectionEntryValue((char*)"GENERAL", (char*)"DisplayName", value);
    cachedTileset.displayNameStrRef = atoi(value);
    pTileset->m_pRes->GetSectionEntryValue((char*)"GENERAL", (char*)"UnlocalizedName", value);
    cachedTileset.unlocalizedName = value;
    pTileset->m_pRes->GetSectionEntryValue((char*)"GENERAL", (char*)"Border", value);
    cachedTileset.borderTerrain = value;
    pTileset->m_pRes->GetSectionEntryValue((char*)"GENERAL", (char*)"Default", value);
    cachedTileset.defaultTerrain = value;
    pTileset->m_pRes->GetSectionEntryValue((char*)"GENERAL", (char*)"Floor", value);
    cachedTileset.floorTerrain = value;
    pTileset->m_pRes->GetSectionEntryValue((char*)"GENERAL", (char*)"HasHeightTransition", value);
    cachedTileset.hasHeightTransition = atoi(value);

    cachedTileset.terrains.reserve(cachedTileset.numTerrains);
    for (int32_t terrain = 0; terrain < cachedTileset.numTerrains; terrain++)
    {
        std::sprintf(section, "TERRAIN%i", terrain);
        pTileset->m_pRes->GetSectionEntryValue(section, (char*)"Name", value);
        cachedTileset.terrains.emplace_back(value);
    }

    cachedTileset.crossers.reserve(cachedTileset.numCrossers);
    for (int32_t crosser = 0; crosser < cachedTileset.numCrossers; crosser++)
    {
        std::sprintf(section, "CROSSER%i", crosser);
        pTileset->m_pRes->GetSectionEntryValue(section, (char*)"Name", value);
        cachedTileset.crossers.emplace_back(value);
    }

    cachedTileset.groups.reserve(cachedTileset.numGroups);
    for (int32_t group = 0; group < cachedTileset.numGroups; group++)
    {
        std::sprintf(section, "GROUP%i", group);
        CachedTilesetGroup cachedTilesetGroup{};

        pTileset->m_pRes->GetSectionEntryValue(section, (char*)"Name", value);
        cachedTilesetGroup.name = value;
        pTileset->m_pRes->GetSectionEntryValue(section, (char*)"StrRef", value);
        cachedTilesetGroup.strRef = atoi(value);
        pTileset->m_pRes->GetSectionEntryValue(section, (char*)"Rows", value);
        cachedTilesetGroup.rows = atoi(value);
        pTileset->m_pRes->GetSectionEntryValue(section, (char*)"Columns", value);
        cachedTilesetGroup.columns = atoi(value);

        int32_t numTiles = cachedTilesetGroup.rows * cachedTilesetGroup.columns;
        cachedTilesetGroup.tiles.reserve(numTiles);
        for (int groupTile = 0; groupTile < numTiles; groupTile++)
        {
            std::sprintf(entry, "Tile%i", groupTile);
            pTileset->m_pRes->GetSectionEntryValue(section, entry, value);
            cachedTilesetGroup.tiles.emplace_back(atoi(value));
        }

        cachedTileset.groups.emplace_back(cachedTilesetGroup);
    }

    cachedTileset.tiles.reserve(cachedTileset.numTiles);
    for (int32_t tile = 0; tile < cachedTileset.numTiles; tile++)
    {
        CachedTilesetTile cachedTilesetTile{};

        if (auto *pTileData = pTileset->GetTileData(tile))
        {
            cachedTilesetTile.tileModel = pTileData->GetModelResRef().GetResRefStr();
            cachedTilesetTile.minimapTexture = pTileData->GetMapIcon().GetResRefStr();
            cachedTilesetTile.tl = pTileData->m_sCornerTopLeft.CStr();
            cachedTilesetTile.t = pTileData->m_sEdgeTop.CStr();
            cachedTilesetTile.tr = pTileData->m_sCornerTopRight.CStr();
            cachedTilesetTile.r = pTileData->m_sEdgeRight.CStr();
            cachedTilesetTile.br = pTileData->m_sCornerBottomRight.CStr();
            cachedTilesetTile.b = pTileData->m_sEdgeBottom.CStr();
            cachedTilesetTile.bl = pTileData->m_sCornerBottomLeft.CStr();
            cachedTilesetTile.l = pTileData->m_sEdgeLeft.CStr();
            cachedTilesetTile.numDoors = pTileData->m_nNumDoors;

            cachedTilesetTile.doors.reserve(cachedTilesetTile.numDoors);
            for(int32_t door = 0; door < cachedTilesetTile.numDoors; door++)
            {
                CachedTilesetTileDoor cachedTilesetTileDoor{};
                std::sprintf(section, "TILE%iDOOR%i", tile, door);

                pTileset->m_pRes->GetSectionEntryValue(section, (char*)"Type", value);
                cachedTilesetTileDoor.type = atoi(value);
                pTileset->m_pRes->GetSectionEntryValue(section, (char*)"X", value);
                cachedTilesetTileDoor.position.x = atof(value);
                pTileset->m_pRes->GetSectionEntryValue(section, (char*)"Y", value);
                cachedTilesetTileDoor.position.y = atof(value);
                pTileset->m_pRes->GetSectionEntryValue(section, (char*)"Z", value);
                cachedTilesetTileDoor.position.z = atof(value);
                pTileset->m_pRes->GetSectionEntryValue(section, (char*)"Orientation", value);
                cachedTilesetTileDoor.orientation = atof(value);

                cachedTilesetTile.doors.emplace_back(cachedTilesetTileDoor);
            }
        }

        cachedTileset.tiles.emplace_back(cachedTilesetTile);
    }

    pTileset->m_pRes->Release();
    pTileset->m_pRes->Dump();

    auto it = s_CachedTilesets.emplace(tileset, cachedTileset);

    return it.second ? &it.first->second : nullptr;
}

NWNX_EXPORT ArgumentStack GetTilesetData(ArgumentStack&& args)
{
    const auto tileset = args.extract<std::string>();
      ASSERT_OR_THROW(!tileset.empty());

    if (auto *pCachedTileset = GetCachedTileset(tileset))
    {
        return {pCachedTileset->numTiles, pCachedTileset->heightTransition, pCachedTileset->numTerrains, pCachedTileset->numCrossers,
                pCachedTileset->numGroups, pCachedTileset->borderTerrain, pCachedTileset->defaultTerrain, pCachedTileset->floorTerrain,
                pCachedTileset->displayNameStrRef, pCachedTileset->unlocalizedName, pCachedTileset->isInterior, pCachedTileset->hasHeightTransition};
    }

    return {0, 0.0f, 0, 0, 0, "", "", "", 0, "", 0, 0};
}

NWNX_EXPORT ArgumentStack GetTilesetTerrain(ArgumentStack&& args)
{
    const auto tileset = args.extract<std::string>();
      ASSERT_OR_THROW(!tileset.empty());
    const auto terrainIndex = args.extract<int32_t>();
      ASSERT_OR_THROW(terrainIndex >= 0);

    if (auto *pCachedTileset = GetCachedTileset(tileset))
    {
        if (terrainIndex < pCachedTileset->numTerrains)
        {
            return pCachedTileset->terrains[terrainIndex];
        }
    }

    return "";
}

NWNX_EXPORT ArgumentStack GetTilesetCrosser(ArgumentStack&& args)
{
    const auto tileset = args.extract<std::string>();
      ASSERT_OR_THROW(!tileset.empty());
    const auto crosserIndex = args.extract<int32_t>();
      ASSERT_OR_THROW(crosserIndex >= 0);

    if (auto *pCachedTileset = GetCachedTileset(tileset))
    {
        if (crosserIndex < pCachedTileset->numCrossers)
        {
            return pCachedTileset->crossers[crosserIndex];
        }
    }

    return "";
}

NWNX_EXPORT ArgumentStack GetTilesetGroupData(ArgumentStack&& args)
{
    const auto tileset = args.extract<std::string>();
      ASSERT_OR_THROW(!tileset.empty());
    const auto groupIndex = args.extract<int32_t>();
      ASSERT_OR_THROW(groupIndex >= 0);

    if (auto *pCachedTileset = GetCachedTileset(tileset))
    {
        if (groupIndex < pCachedTileset->numGroups)
        {
            return {pCachedTileset->groups[groupIndex].name, pCachedTileset->groups[groupIndex].strRef,
                    pCachedTileset->groups[groupIndex].rows, pCachedTileset->groups[groupIndex].columns};
        }
    }

    return {"", 0, 0, 0};
}

NWNX_EXPORT ArgumentStack GetTilesetGroupTile(ArgumentStack&& args)
{
    const auto tileset = args.extract<std::string>();
      ASSERT_OR_THROW(!tileset.empty());
    const auto groupIndex = args.extract<int32_t>();
      ASSERT_OR_THROW(groupIndex >= 0);
    const auto tileIndex = args.extract<int32_t>();
      ASSERT_OR_THROW(tileIndex >= 0);

    if (auto *pCachedTileset = GetCachedTileset(tileset))
    {
        if (groupIndex < pCachedTileset->numGroups)
        {
            if ((size_t)tileIndex < pCachedTileset->groups[groupIndex].tiles.size())
            {
                return pCachedTileset->groups[groupIndex].tiles[tileIndex];
            }
        }
    }

    return 0;
}

NWNX_EXPORT ArgumentStack GetTileModel(ArgumentStack&& args)
{
    const auto tileset = args.extract<std::string>();
      ASSERT_OR_THROW(!tileset.empty());
    const auto tileId = args.extract<int32_t>();
      ASSERT_OR_THROW(tileId >= 0);

    if (auto *pCachedTileset = GetCachedTileset(tileset))
    {
        if (tileId < pCachedTileset->numTiles)
        {
            return pCachedTileset->tiles[tileId].tileModel;
        }
    }

    return "";
}

NWNX_EXPORT ArgumentStack GetTileMinimapTexture(ArgumentStack&& args)
{
    const auto tileset = args.extract<std::string>();
      ASSERT_OR_THROW(!tileset.empty());
    const auto tileId = args.extract<int32_t>();
      ASSERT_OR_THROW(tileId >= 0);

    if (auto *pCachedTileset = GetCachedTileset(tileset))
    {
        if (tileId < pCachedTileset->numTiles)
        {
            return pCachedTileset->tiles[tileId].minimapTexture;
        }
    }

    return "";
}

NWNX_EXPORT ArgumentStack GetTileEdgesAndCorners(ArgumentStack&& args)
{
    const auto tileset = args.extract<std::string>();
      ASSERT_OR_THROW(!tileset.empty());
    const auto tileId = args.extract<int32_t>();
      ASSERT_OR_THROW(tileId >= 0);

    if (auto *pCachedTileset = GetCachedTileset(tileset))
    {
        if (tileId < pCachedTileset->numTiles)
        {
            return {pCachedTileset->tiles[tileId].tl, pCachedTileset->tiles[tileId].t, pCachedTileset->tiles[tileId].tr,
                    pCachedTileset->tiles[tileId].r, pCachedTileset->tiles[tileId].br, pCachedTileset->tiles[tileId].b,
                    pCachedTileset->tiles[tileId].bl, pCachedTileset->tiles[tileId].l};
        }
    }

    return {"", "", "", "", "", "", "", ""};
}

NWNX_EXPORT ArgumentStack GetTileNumDoors(ArgumentStack&& args)
{
    const auto tileset = args.extract<std::string>();
      ASSERT_OR_THROW(!tileset.empty());
    const auto tileId = args.extract<int32_t>();
      ASSERT_OR_THROW(tileId >= 0);

    if (auto *pCachedTileset = GetCachedTileset(tileset))
    {
        if (tileId < pCachedTileset->numTiles)
        {
            return pCachedTileset->tiles[tileId].numDoors;
        }
    }

    return 0;
}

NWNX_EXPORT ArgumentStack GetTileDoorData(ArgumentStack&& args)
{
    const auto tileset = args.extract<std::string>();
      ASSERT_OR_THROW(!tileset.empty());
    const auto tileId = args.extract<int32_t>();
      ASSERT_OR_THROW(tileId >= 0);
    const auto doorIndex = args.extract<int32_t>();
      ASSERT_OR_THROW(doorIndex >= 0);

    if (auto *pCachedTileset = GetCachedTileset(tileset))
    {
        if (tileId < pCachedTileset->numTiles)
        {
            if (doorIndex < pCachedTileset->tiles[tileId].numDoors)
            {
                return {pCachedTileset->tiles[tileId].doors[doorIndex].type,
                        pCachedTileset->tiles[tileId].doors[doorIndex].position.x,
                        pCachedTileset->tiles[tileId].doors[doorIndex].position.y,
                        pCachedTileset->tiles[tileId].doors[doorIndex].position.z,
                        pCachedTileset->tiles[tileId].doors[doorIndex].orientation};
            }
        }
    }

    return {-1, 0.0f, 0.0f, 0.0f, 0.0f};
}
