#include "nwnx.hpp"
#include "API/CVirtualMachine.hpp"

using namespace NWNXLib;
using namespace NWNXLib::API;

NWNX_EXPORT ArgumentStack GetScriptReturnValueType(ArgumentStack&&)
{
    return Globals::VirtualMachine()->GetRunScriptReturnValueType();
}

NWNX_EXPORT ArgumentStack GetScriptReturnValueInt(ArgumentStack&&)
{
    if (Globals::VirtualMachine()->GetRunScriptReturnValueType() == Constants::VMAuxCodeType::Integer)
    {
        int32_t nValue;
        if (Globals::VirtualMachine()->GetRunScriptReturnValueInteger(&nValue))
            return nValue;
    }
    return 0;
}

NWNX_EXPORT ArgumentStack GetScriptReturnValueFloat(ArgumentStack&&)
{
    if (Globals::VirtualMachine()->GetRunScriptReturnValueType() == Constants::VMAuxCodeType::Float)
    {
        float fValue;
        if (Globals::VirtualMachine()->GetRunScriptReturnValueFloat(&fValue))
            return fValue;
    }
    return 0.0f;
}

NWNX_EXPORT ArgumentStack GetScriptReturnValueString(ArgumentStack&&)
{
    if (Globals::VirtualMachine()->GetRunScriptReturnValueType() == Constants::VMAuxCodeType::String)
    {
        CExoString sValue;
        if (Globals::VirtualMachine()->GetRunScriptReturnValueString(&sValue))
            return sValue;
    }
    return "";
}

NWNX_EXPORT ArgumentStack GetScriptReturnValueObject(ArgumentStack&&)
{
    if (Globals::VirtualMachine()->GetRunScriptReturnValueType() == Constants::VMAuxCodeType::Object)
    {
        ObjectID oidValue;
        if (Globals::VirtualMachine()->GetRunScriptReturnValueObject(&oidValue))
            return oidValue;
    }
    return Constants::OBJECT_INVALID;
}

NWNX_EXPORT ArgumentStack GetScriptReturnValueJson(ArgumentStack&&)
{
    if (Globals::VirtualMachine()->GetRunScriptReturnValueType() == Constants::VMAuxCodeType::EngSt7)
    {
        void* pValue;
        if (Globals::VirtualMachine()->GetRunScriptReturnValueEngineStructure(Constants::VMAuxCodeType::EngSt7, &pValue))
            return *(JsonEngineStructure*)pValue;
    }
    JsonEngineStructure jRet;
    return jRet;
}
