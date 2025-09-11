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
            return *static_cast<JsonEngineStructure*>(pValue);
    }
    return JsonEngineStructure{};
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

NWNX_EXPORT ArgumentStack GetCurrentStack(ArgumentStack&& args)
{
    auto depth = args.extract<int32_t>();

    JsonEngineStructure j;
    j.m_shared->m_json = json::object();

    const auto currentStack = Globals::VirtualMachine()->GetCurrentStack(depth);
    for (const auto&[varName, varData]: currentStack)
    {
        json stackVar = json::object();
        stackVar["type"] = varData.auxType;
        stackVar["stack_location"] = varData.stackLocation;
        stackVar["struct_name"] = varData.structName;
        j.m_shared->m_json[varName] = stackVar;
    }

    return j;
}

NWNX_EXPORT ArgumentStack SetStackIntegerValue(ArgumentStack&& args)
{
    const auto stackLocation = args.extract<int32_t>();
    const auto value = args.extract<int32_t>();
    Globals::VirtualMachine()->SetStackIntegerValue(stackLocation, value);
    return {};
}

NWNX_EXPORT ArgumentStack GetStackIntegerValue(ArgumentStack&& args)
{
    const auto stackLocation = args.extract<int32_t>();
    return Globals::VirtualMachine()->GetStackIntegerValue(stackLocation);
}

NWNX_EXPORT ArgumentStack SetStackFloatValue(ArgumentStack&& args)
{
    const auto stackLocation = args.extract<int32_t>();
    const auto value = args.extract<float>();
    Globals::VirtualMachine()->SetStackFloatValue(stackLocation, value);
    return {};
}

NWNX_EXPORT ArgumentStack GetStackFloatValue(ArgumentStack&& args)
{
    const auto stackLocation = args.extract<int32_t>();
    return Globals::VirtualMachine()->GetStackFloatValue(stackLocation);
}

NWNX_EXPORT ArgumentStack SetStackObjectValue(ArgumentStack&& args)
{
    const auto stackLocation = args.extract<int32_t>();
    const auto value = args.extract<ObjectID>();
    Globals::VirtualMachine()->SetStackObjectValue(stackLocation, value);
    return {};
}

NWNX_EXPORT ArgumentStack GetStackObjectValue(ArgumentStack&& args)
{
    const auto stackLocation = args.extract<int32_t>();
    return Globals::VirtualMachine()->GetStackObjectValue(stackLocation);
}

NWNX_EXPORT ArgumentStack SetStackStringValue(ArgumentStack&& args)
{
    const auto stackLocation = args.extract<int32_t>();
    const auto value = args.extract<std::string>();
    Globals::VirtualMachine()->SetStackStringValue(stackLocation, value);
    return {};
}

NWNX_EXPORT ArgumentStack GetStackStringValue(ArgumentStack&& args)
{
    const auto stackLocation = args.extract<int32_t>();
    return Globals::VirtualMachine()->GetStackStringValue(stackLocation);
}

NWNX_EXPORT ArgumentStack SetStackLocationValue(ArgumentStack&& args)
{
    const auto stackLocation = args.extract<int32_t>();
    auto value = args.extract<CScriptLocation>();
    Globals::VirtualMachine()->SetStackLocationValue(stackLocation, &value);
    return {};
}

NWNX_EXPORT ArgumentStack GetStackLocationValue(ArgumentStack&& args)
{
    const auto stackLocation = args.extract<int32_t>();
    return Globals::VirtualMachine()->GetStackLocationValue(stackLocation);
}

NWNX_EXPORT ArgumentStack SetStackJsonValue(ArgumentStack&& args)
{
    const auto stackLocation = args.extract<int32_t>();
    auto value = args.extract<JsonEngineStructure>();
    Globals::VirtualMachine()->SetStackJsonValue(stackLocation, &value);
    return {};
}

NWNX_EXPORT ArgumentStack GetStackJsonValue(ArgumentStack&& args)
{
    const auto stackLocation = args.extract<int32_t>();
    return Globals::VirtualMachine()->GetStackJsonValue(stackLocation);
}
