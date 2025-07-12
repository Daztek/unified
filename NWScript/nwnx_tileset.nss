const string NWNX_Tileset = "NWNX_Tileset";

struct NWNX_Tileset_TilesetData
{
    int nNumTileData; // The number of tiles in the tileset.
    float fHeightTransition; // The height difference between tiles on different heights.
    int nNumTerrain; // The number of terrains in the tileset.
    int nNumCrossers; // The number of crossers in the tileset.
    int nNumGroups; // The number of groups in the tileset.
    string sBorderTerrain; // The default border terrain of the tileset.
    string sDefaultTerrain; // The default terrain of the tileset.
    string sFloorTerrain; // The default floor terrain of the tileset.
    int nDisplayNameStrRef; // The name of the tileset as strref, -1 if not set.
    string sUnlocalizedName; // The unlocalized name of the tileset, "" if not set.
    int bInterior; // The type of tileset. TRUE for interior, FALSE for exterior.
    int bHasHeightTransition; // TRUE if the tileset supports multiple height levels. FALSE if not.
};

struct NWNX_Tileset_TilesetGroupData
{
    string sName; // The name of the group.
    int nStrRef; // The StrRef of the group.
    int nRows; // The number of rows the group has.
    int nColumns; // The number of columns the group has.
};

struct NWNX_Tileset_TileEdgesAndCorners
{
    string sTopLeft; // The top left corner.
    string sTop; // The top edge.
    string sTopRight; // The top right corner.
    string sRight; // The right edge.
    string sBottomRight; // The bottom right corner.
    string sBottom; // The bottom edge.
    string sBottomLeft; // The bottom left corner.
    string sLeft; // The left edge.
};

struct NWNX_Tileset_TileDoorData
{
    int nType; // The type of door, returns an index into doortypes.2da.
    float fX; // The X position of the door.
    float fY; // The Y position of the door.
    float fZ; // The Z position of the door.
    float fOrientation; // The orientation of the door.
};

// Get general data of sTileset.
// - sTileset: The tileset.
// * Returns a NWNX_Tileset_TilesetData struct.
struct NWNX_Tileset_TilesetData NWNX_Tileset_GetTilesetData(string sTileset);

// Get the name of sTileset's terrain at nIndex.
// - sTileset: The tileset.
// - nIndex: The index of the terrain.
// * Returns the terrain name or "" on error.
string NWNX_Tileset_GetTilesetTerrain(string sTileset, int nIndex);

// Get the name of sTileset's crosser at nIndex.
// - sTileset: The tileset.
// - nIndex: The index of the crosser.
// * Returns the crosser name or "" on error.
string NWNX_Tileset_GetTilesetCrosser(string sTileset, int nIndex);

// Get general data of the group at nIndex in sTileset.
// - sTileset: The tileset.
// - nIndex: The index of the group.
// * Returns a NWNX_Tileset_TilesetGroupData struct.
struct NWNX_Tileset_TilesetGroupData NWNX_Tileset_GetTilesetGroupData(string sTileset, int nIndex);

// Get the tile ID at nTileIndex in nGroupIndex of sTileset.
// - sTileset: The tileset.
// - nGroupIndex: The index of the group.
// - nTileIndex: The index of the tile.
// * Returns the tile ID or 0 on error.
int NWNX_Tileset_GetTilesetGroupTile(string sTileset, int nGroupIndex, int nTileIndex);

// Get the model name of a tile in sTileset.
// - sTileset: The tileset.
// - nTileID: The tile ID.
// * Returns the model name or "" on error.
string NWNX_Tileset_GetTileModel(string sTileset, int nTileID);

// Get the minimap texture name of a tile in sTileset.
// - sTileset: The tileset.
// - nTileID: The tile ID.
// * Returns the minimap texture name or "" on error.
string NWNX_Tileset_GetTileMinimapTexture(string sTileset, int nTileID);

// Get the edges and corners of a tile in sTileset.
// - sTileset: The tileset.
// - nTileID: The tile ID.
// * Returns a NWNX_Tileset_TileEdgesAndCorners struct.
struct NWNX_Tileset_TileEdgesAndCorners NWNX_Tileset_GetTileEdgesAndCorners(string sTileset, int nTileID);

// Get the number of doors of a tile in sTileset.
// - sTileset: The tileset.
// - nTileID: The tile ID.
// * Returns the amount of doors.
int NWNX_Tileset_GetTileNumDoors(string sTileset, int nTileID);

// Get the door data of a tile in sTileset.
// - sTileset: The tileset.
// - nTileID: The tile ID.
// - nIndex: The index of the door.
// * Returns a NWNX_Tileset_TileDoorData struct.
struct NWNX_Tileset_TileDoorData NWNX_Tileset_GetTileDoorData(string sTileset, int nTileID, int nIndex = 0);

struct NWNX_Tileset_TilesetData NWNX_Tileset_GetTilesetData(string sTileset)
{
    NWNXPushString(sTileset);
    NWNXCall(NWNX_Tileset, "GetTilesetData");
    struct NWNX_Tileset_TilesetData str;
    str.bHasHeightTransition = NWNXPopInt();
    str.bInterior = NWNXPopInt();
    str.sUnlocalizedName = NWNXPopString();
    str.nDisplayNameStrRef = NWNXPopInt();
    str.sFloorTerrain = NWNXPopString();
    str.sDefaultTerrain = NWNXPopString();
    str.sBorderTerrain = NWNXPopString();
    str.nNumGroups = NWNXPopInt();
    str.nNumCrossers = NWNXPopInt();
    str.nNumTerrain = NWNXPopInt();
    str.fHeightTransition = NWNXPopFloat();
    str.nNumTileData = NWNXPopInt();
    return str;
}

string NWNX_Tileset_GetTilesetTerrain(string sTileset, int nIndex)
{
    NWNXPushInt(nIndex);
    NWNXPushString(sTileset);
    NWNXCall(NWNX_Tileset, "GetTilesetTerrain");
    return NWNXPopString();
}

string NWNX_Tileset_GetTilesetCrosser(string sTileset, int nIndex)
{
    NWNXPushInt(nIndex);
    NWNXPushString(sTileset);
    NWNXCall(NWNX_Tileset, "GetTilesetCrosser");
    return NWNXPopString();
}

struct NWNX_Tileset_TilesetGroupData NWNX_Tileset_GetTilesetGroupData(string sTileset, int nIndex)
{
    NWNXPushInt(nIndex);
    NWNXPushString(sTileset);
    NWNXCall(NWNX_Tileset, "GetTilesetGroupData");
    struct NWNX_Tileset_TilesetGroupData str;
    str.nColumns = NWNXPopInt();
    str.nRows = NWNXPopInt();
    str.nStrRef = NWNXPopInt();
    str.sName = NWNXPopString();
    return str;
}

int NWNX_Tileset_GetTilesetGroupTile(string sTileset, int nGroupIndex, int nTileIndex)
{
    NWNXPushInt(nTileIndex);
    NWNXPushInt(nGroupIndex);
    NWNXPushString(sTileset);
    NWNXCall(NWNX_Tileset, "GetTilesetGroupTile");
    return NWNXPopInt();
}

string NWNX_Tileset_GetTileModel(string sTileset, int nTileID)
{
    NWNXPushInt(nTileID);
    NWNXPushString(sTileset);
    NWNXCall(NWNX_Tileset, "GetTileModel");
    return NWNXPopString();
}

string NWNX_Tileset_GetTileMinimapTexture(string sTileset, int nTileID)
{
    NWNXPushInt(nTileID);
    NWNXPushString(sTileset);
    NWNXCall(NWNX_Tileset, "GetTileMinimapTexture");
    return NWNXPopString();
}

struct NWNX_Tileset_TileEdgesAndCorners NWNX_Tileset_GetTileEdgesAndCorners(string sTileset, int nTileID)
{
    NWNXPushInt(nTileID);
    NWNXPushString(sTileset);
    NWNXCall(NWNX_Tileset, "GetTileEdgesAndCorners");
    struct NWNX_Tileset_TileEdgesAndCorners str;
    str.sLeft = NWNXPopString();
    str.sBottomLeft = NWNXPopString();
    str.sBottom = NWNXPopString();
    str.sBottomRight = NWNXPopString();
    str.sRight = NWNXPopString();
    str.sTopRight = NWNXPopString();
    str.sTop = NWNXPopString();
    str.sTopLeft = NWNXPopString();
    return str;
}

int NWNX_Tileset_GetTileNumDoors(string sTileset, int nTileID)
{
    NWNXPushInt(nTileID);
    NWNXPushString(sTileset);
    NWNXCall(NWNX_Tileset, "GetTileNumDoors");
    return NWNXPopInt();
}

struct NWNX_Tileset_TileDoorData NWNX_Tileset_GetTileDoorData(string sTileset, int nTileID, int nIndex = 0)
{
    NWNXPushInt(nIndex);
    NWNXPushInt(nTileID);
    NWNXPushString(sTileset);
    NWNXCall(NWNX_Tileset, "GetTileDoorData");
    struct NWNX_Tileset_TileDoorData str;
    str.fOrientation = NWNXPopFloat();
    str.fZ = NWNXPopFloat();
    str.fY = NWNXPopFloat();
    str.fX = NWNXPopFloat();
    str.nType = NWNXPopInt();
    return str;
}
