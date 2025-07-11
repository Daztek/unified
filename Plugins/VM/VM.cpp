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

NWNX_EXPORT ArgumentStack GetInstructionLimit(ArgumentStack&&)
{
    return (int32_t)Globals::VirtualMachine()->m_nInstructionLimit;
}

NWNX_EXPORT ArgumentStack SetInstructionLimit(ArgumentStack&& args)
{
    const static uint32_t defaultInstructionLimit = Globals::VirtualMachine()->m_nInstructionLimit;
    const auto limit = args.extract<int32_t>();

    if (limit < 0)
        Tasks::QueueOnMainThread([]() { Globals::VirtualMachine()->m_nInstructionLimit = defaultInstructionLimit; });
    else
        Globals::VirtualMachine()->m_nInstructionLimit = limit;

    return {};
}

NWNX_EXPORT ArgumentStack SetInstructionsExecuted(ArgumentStack&& args)
{
    const auto instructions = args.extract<int32_t>();
    Globals::VirtualMachine()->m_nInstructionsExecuted = instructions >= 0 ? instructions : 0;
    return {};
}

NWNX_EXPORT ArgumentStack GetScriptParamSet(ArgumentStack&& args)
{
    int32_t retVal = false;
    const auto paramName = args.extract<std::string>();
    ASSERT_OR_THROW(!paramName.empty());

    if (Globals::VirtualMachine()->m_nRecursionLevel >= 0)
    {
        auto& scriptParams = Globals::VirtualMachine()->m_lScriptParams[Globals::VirtualMachine()->m_nRecursionLevel];
        for (const auto& scriptParam : scriptParams)
        {
            if (scriptParam.key.CStr() == paramName)
            {
                retVal = true;
                break;
            }
        }
    }

    return retVal;
}
