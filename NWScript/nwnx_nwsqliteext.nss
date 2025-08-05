const string NWNX_NWSQLiteExtensions = "NWNX_NWSQLiteExtensions"; ///< @private

const int NWNX_NWSQLITEEXTENSIONS_RETURN_TYPE_INT       = 1;
const int NWNX_NWSQLITEEXTENSIONS_RETURN_TYPE_FLOAT     = 2;
const int NWNX_NWSQLITEEXTENSIONS_RETURN_TYPE_STRING    = 3;
const int NWNX_NWSQLITEEXTENSIONS_RETURN_TYPE_OBJECT    = 4;

// Create a virtual table for s2DA in the module sqlite database.
// - s2DA: The 2DA name, cannot be empty.
// - sColumnTypeHints: A string containing type hints for the 2DA columns. See this plugin's readme file for more info.
// - sTableName: The table name, will use the 2da name if empty.
// * Returns TRUE if the virtual table was created.
int NWNX_NWSQLiteExtensions_CreateVirtual2DATable(string s2DA, string sColumnTypeHints = "", string sTableName = "");

// Create a custom SQLite function that runs a script chunk.
// - sName: The name of the function, must be unique.
// - sScriptChunk: The scriptchunk to run, must be an int/float/string/object main() {}.
// - nArgCount: The number of extra arguments the function requires, the first one is implicit and always OBJECT_SELF.
//              Arguments are set as local variables on the module with CF_ARG_{X} as varname, where {X} is the index.
//              For function TEST(oid, 'hi', 1)
//                  Index 0 is an object and used as OBJECT_SELF
//                  Index 1 is a string and can be retrieved with GetLocalString();
//                  Index 2 is an integer and can be retrieved with GetLocalInt() or GetLocalObject().
// - nReturnType: One of NWNX_NWSQLITEEXTENSIONS_RETURN_TYPE_*.
// - bDeterministic: Set to true if repeated calls to the function with the same arguments return the same value.
// * Returns TRUE if the custom function was successfully registered.
int NWNX_NWSQLiteExtensions_RegisterCustomFunction(string sName, string sScriptChunk, int nArgCount, int nReturnType, int bDeterministic);

// Clear the result cache of custom SQLite functions.
// Should be called before each SQL query that uses them.
void NWNX_NWSQLiteExtensions_ClearFunctionResultCache();

int NWNX_NWSQLiteExtensions_CreateVirtual2DATable(string s2DA, string sColumnTypeHints = "", string sTableName = "")
{
    NWNXPushString(sTableName);
    NWNXPushString(sColumnTypeHints);
    NWNXPushString(s2DA);
    NWNXCall(NWNX_NWSQLiteExtensions, "CreateVirtual2DATable");
    return NWNXPopInt();
}

int NWNX_NWSQLiteExtensions_RegisterCustomFunction(string sName, string sScriptChunk, int nArgCount, int nReturnType, int bDeterministic)
{
    NWNXPushInt(bDeterministic);
    NWNXPushInt(nReturnType);
    NWNXPushInt(nArgCount);
    NWNXPushString(sScriptChunk);
    NWNXPushString(sName);
    NWNXCall(NWNX_NWSQLiteExtensions, "RegisterCustomFunction");
    return NWNXPopInt();
}

void NWNX_NWSQLiteExtensions_ClearFunctionResultCache()
{
    NWNXCall(NWNX_NWSQLiteExtensions, "ClearFunctionResultCache");
}
