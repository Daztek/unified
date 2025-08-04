const string NWNX_Json = "NWNX_Json";

// Set the element pointed at by sPointer to jValue in jParent.
void NWNX_Json_SetAtPointerInplace(json jParent, string sPointer, json jValue);

void NWNX_Json_SetAtPointerInplace(json jParent, string sPointer, json jValue)
{
    NWNXPushJson(jValue);
    NWNXPushString(sPointer);
    NWNXPushJson(jParent);
    NWNXCall(NWNX_Json, "SetAtPointerInplace");
}
