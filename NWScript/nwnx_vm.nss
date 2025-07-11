const string NWNX_VM = "NWNX_VM";

const int NWNX_VM_SCRIPT_RETURN_VALUE_TYPE_INVALID  = 0x0;
const int NWNX_VM_SCRIPT_RETURN_VALUE_TYPE_VOID     = 0x01;
const int NWNX_VM_SCRIPT_RETURN_VALUE_TYPE_INT      = 0x03;
const int NWNX_VM_SCRIPT_RETURN_VALUE_TYPE_FLOAT    = 0x04;
const int NWNX_VM_SCRIPT_RETURN_VALUE_TYPE_STRING   = 0x05;
const int NWNX_VM_SCRIPT_RETURN_VALUE_TYPE_OBJECT   = 0x06;
const int NWNX_VM_SCRIPT_RETURN_VALUE_TYPE_JSON     = 0x17;

// Get the return value type of the last script executed with ExecuteScript() or ExecuteScriptChunk().
// Returns a NWNX_VM_SCRIPT_RETURN_VALUE_TYPE_*
int NWNX_VM_GetScriptReturnValueType();

// Get the returned int value of the last script executed with ExecuteScript() or ExecuteScriptChunk() or 0 on error.
int NWNX_VM_GetScriptReturnValueInt();

// Get the returned float value of the last script executed with ExecuteScript() or ExecuteScriptChunk() or 0.0f on error.
float NWNX_VM_GetScriptReturnValueFloat();

// Get the returned string value of the last script executed with ExecuteScript() or ExecuteScriptChunk() or "" on error.
string NWNX_VM_GetScriptReturnValueString();

// Get the returned object value of the last script executed with ExecuteScript() or ExecuteScriptChunk() or OBJECT_INVALID on error.
object NWNX_VM_GetScriptReturnValueObject();

// Get the returned json value of the last script executed with ExecuteScript() or ExecuteScriptChunk() or JsonNull() on error.
json NWNX_VM_GetScriptReturnValueJson();

// Get the NWScript VM instruction limit.
int NWNX_VM_GetInstructionLimit();

// Set the NWScript VM instruction limit.
// - nInstructionLimit The new limit or -1 to reset to default.
void NWNX_VM_SetInstructionLimit(int nInstructionLimit);

// Set the number of NWScript VM instructions currently executed.
// - nInstructions: The number of instructions, must be >= 0.
void NWNX_VM_SetInstructionsExecuted(int nInstructions);

// Check if a script param is set.
// - sParamName: The script parameter name to check.
// * Returns TRUE if the script param is set, FALSE if not or on error.
int NWNX_VM_GetScriptParamSet(string sParamName);

int NWNX_VM_GetScriptReturnValueType()
{
    NWNXCall(NWNX_VM, "GetScriptReturnValueType");
    return NWNXPopInt();
}

int NWNX_VM_GetScriptReturnValueInt()
{
    NWNXCall(NWNX_VM, "GetScriptReturnValueInt");
    return NWNXPopInt();
}

float NWNX_VM_GetScriptReturnValueFloat()
{
    NWNXCall(NWNX_VM, "GetScriptReturnValueFloat");
    return NWNXPopFloat();
}

string NWNX_VM_GetScriptReturnValueString()
{
    NWNXCall(NWNX_VM, "GetScriptReturnValueString");
    return NWNXPopString();
}

object NWNX_VM_GetScriptReturnValueObject()
{
    NWNXCall(NWNX_VM, "GetScriptReturnValueObject");
    return NWNXPopObject();
}

json NWNX_VM_GetScriptReturnValueJson()
{
    NWNXCall(NWNX_VM, "GetScriptReturnValueJson");
    return NWNXPopJson();
}

int NWNX_VM_GetInstructionLimit()
{
    NWNXCall(NWNX_VM, "GetInstructionLimit");
    return NWNXPopInt();
}

void NWNX_VM_SetInstructionLimit(int nInstructionLimit)
{
    NWNXPushInt(nInstructionLimit);
    NWNXCall(NWNX_VM, "SetInstructionLimit");
}

void NWNX_VM_SetInstructionsExecuted(int nInstructions)
{
    NWNXPushInt(nInstructions);
    NWNXCall(NWNX_VM, "SetInstructionsExecuted");
}

int NWNX_VM_GetScriptParamSet(string sParamName)
{
    NWNXPushString(sParamName);
    NWNXCall(NWNX_VM, "GetScriptParamSet");
    return NWNXPopInt();
}
