const string NWNX_Json = "NWNX_Json";

// Set the element pointed at by sPointer to jValue in jParent.
// - jParent: The parent json.
// - sPointer: A json pointer.
// - jValue: The value to set.
void NWNX_Json_SetAtPointerInplace(json jParent, string sPointer, json jValue);

// Check if jObject contains sKey.
// - jObject: A json object.
// - sKey: The key to check for.
// * Returns: TRUE if jObject contains sKey, FALSE if not and on error.
int NWNX_Json_JsonObjectContainsKey(json jObject, string sKey);

void NWNX_Json_SetAtPointerInplace(json jParent, string sPointer, json jValue)
{
    NWNXPushJson(jValue);
    NWNXPushString(sPointer);
    NWNXPushJson(jParent);
    NWNXCall(NWNX_Json, "SetAtPointerInplace");
}

int NWNX_Json_JsonObjectContainsKey(json jObject, string sKey)
{
    NWNXPushString(sKey);
    NWNXPushJson(jObject);
    NWNXCall(NWNX_Json, "JsonObjectContainsKey");
    return NWNXPopInt();
}
