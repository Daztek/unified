#include "nwnx.hpp"
#include "API/CVirtualMachine.hpp"
#include "API/CVirtualMachineDebuggerInstance.hpp"

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
    auto *pVM = Globals::VirtualMachine();
    auto dbg = pVM->GetDebuggerInstance();
    auto depth = args.extract<int32_t>();

    JsonEngineStructure j;
    j.m_shared->m_json = json::array();

    int32_t finalInstructionPointer;
    if (depth == 0)
        finalInstructionPointer = *pVM->m_pCurrentInstructionPointer[pVM->m_nRecursionLevel];
    else if (depth >= 1 && depth <= pVM->m_nInstructPtrLevel - 1)
        finalInstructionPointer = pVM->m_pnRunTimeInstructPtr[pVM->m_nInstructPtrLevel - depth];
    else
        return j;

    int32_t functionIdentifier = dbg->GenerateFunctionIDFromInstructionPointer(*pVM->m_pCurrentInstructionPointer[pVM->m_nRecursionLevel]);
    int32_t currentStackPointer = pVM->m_cRunTimeStack.GetStackPointer();
    int32_t stackSize = dbg->GenerateStackSizeAtInstructionPointer(functionIdentifier,*pVM->m_pCurrentInstructionPointer[pVM->m_nRecursionLevel]);
    int32_t functionCount = pVM->m_nInstructPtrLevel;

    while (depth > 0)
    {
        --depth;
        --functionCount;
        currentStackPointer -= (stackSize >> 2);
        int32_t runTimePtr = pVM->m_pnRunTimeInstructPtr[functionCount];
        functionIdentifier = dbg->GenerateFunctionIDFromInstructionPointer(runTimePtr);
        stackSize = dbg->GenerateStackSizeAtInstructionPointer(functionIdentifier,runTimePtr);
    }

    int32_t baseStackLocation = currentStackPointer - (stackSize >> 2);
    int32_t topMostStackEntry;
    if (dbg->GenerateTypeSize(&dbg->m_pDebugFunctionReturnTypeNames[functionIdentifier]) == 0)
        topMostStackEntry = -1;
    else
        topMostStackEntry = 0;

    int32_t debugVariableLocation, totalParameters = dbg->m_pDebugFunctionParameters[functionIdentifier];
    for (int32_t parameterCount = 0; parameterCount < totalParameters; ++parameterCount)
    {
        debugVariableLocation = dbg->GenerateDebugVariableLocationForParameter(functionIdentifier, parameterCount);
        int32_t stackLocation = baseStackLocation + (dbg->m_pDebugVariableStackLocation[debugVariableLocation] >> 2);

        if (dbg->m_pDebugVariableStackLocation[debugVariableLocation] > topMostStackEntry)
            topMostStackEntry = dbg->m_pDebugVariableStackLocation[debugVariableLocation];

        json jStackVar = json::object();
        jStackVar["stack_location"] = stackLocation;
        jStackVar["name"] = dbg->m_pDebugVariableNames[debugVariableLocation];
        jStackVar["type"] = dbg->m_pDebugVariableTypeNames[debugVariableLocation];
        j.m_shared->m_json.emplace_back(jStackVar);
    }

    debugVariableLocation = dbg->GetNextDebugVariable(functionIdentifier, finalInstructionPointer, topMostStackEntry);
    while (debugVariableLocation != -1)
    {
        int32_t stackLocation = (baseStackLocation + (dbg->m_pDebugVariableStackLocation[debugVariableLocation] >> 2));
        topMostStackEntry = dbg->m_pDebugVariableStackLocation[debugVariableLocation];


        json jStackVar = json::object();
        jStackVar["stack_location"] = stackLocation;
        jStackVar["name"] = dbg->m_pDebugVariableNames[debugVariableLocation];
        jStackVar["type"] = dbg->m_pDebugVariableTypeNames[debugVariableLocation];
        j.m_shared->m_json.emplace_back(jStackVar);

        debugVariableLocation = dbg->GetNextDebugVariable(functionIdentifier, finalInstructionPointer, topMostStackEntry);
    }

    return j;
}

NWNX_EXPORT ArgumentStack SetStackIntegerValue(ArgumentStack&& args)
{
    auto *pVM = Globals::VirtualMachine();
    const auto stackLocation = args.extract<int32_t>();
    const auto value = args.extract<int32_t>();

    if (stackLocation >= 0 && stackLocation < pVM->m_cRunTimeStack.m_nTotalSize)
    {
        auto &stackNode = pVM->m_cRunTimeStack.GetStackNode(stackLocation);
        if (stackNode.m_nType == StackElement::INTEGER)
            stackNode.m_nStackInt = value;
    }
    return {};
}

NWNX_EXPORT ArgumentStack GetStackIntegerValue(ArgumentStack&& args)
{
    auto *pVM = Globals::VirtualMachine();
    const auto stackLocation = args.extract<int32_t>();

    if (stackLocation >= 0 && stackLocation < pVM->m_cRunTimeStack.m_nTotalSize)
    {
        auto &stackNode = pVM->m_cRunTimeStack.GetStackNode(stackLocation);
        if (stackNode.m_nType == StackElement::INTEGER)
            return stackNode.m_nStackInt;
    }
    return 0;
}

NWNX_EXPORT ArgumentStack SetStackFloatValue(ArgumentStack&& args)
{
    auto *pVM = Globals::VirtualMachine();
    const auto stackLocation = args.extract<int32_t>();
    const auto value = args.extract<float>();

    if (stackLocation >= 0 && stackLocation < pVM->m_cRunTimeStack.m_nTotalSize)
    {
        auto &stackNode = pVM->m_cRunTimeStack.GetStackNode(stackLocation);
        if (stackNode.m_nType == StackElement::FLOAT)
            stackNode.m_fStackFloat = value;
    }
    return {};
}

NWNX_EXPORT ArgumentStack GetStackFloatValue(ArgumentStack&& args)
{
    auto *pVM = Globals::VirtualMachine();
    const auto stackLocation = args.extract<int32_t>();

    if (stackLocation >= 0 && stackLocation < pVM->m_cRunTimeStack.m_nTotalSize)
    {
        auto &stackNode = pVM->m_cRunTimeStack.GetStackNode(stackLocation);
        if (stackNode.m_nType == StackElement::FLOAT)
            return stackNode.m_fStackFloat;
    }
    return 0.0f;
}

NWNX_EXPORT ArgumentStack SetStackObjectValue(ArgumentStack&& args)
{
    auto *pVM = Globals::VirtualMachine();
    const auto stackLocation = args.extract<int32_t>();
    const auto value = args.extract<ObjectID>();

    if (stackLocation >= 0 && stackLocation < pVM->m_cRunTimeStack.m_nTotalSize)
    {
        auto &stackNode = pVM->m_cRunTimeStack.GetStackNode(stackLocation);
        if (stackNode.m_nType == StackElement::OBJECT)
            stackNode.m_nStackObjectID = value;
    }
    return {};
}

NWNX_EXPORT ArgumentStack GetStackObjectValue(ArgumentStack&& args)
{
    auto *pVM = Globals::VirtualMachine();
    const auto stackLocation = args.extract<int32_t>();

    if (stackLocation >= 0 && stackLocation < pVM->m_cRunTimeStack.m_nTotalSize)
    {
        auto &stackNode = pVM->m_cRunTimeStack.GetStackNode(stackLocation);
        if (stackNode.m_nType == StackElement::OBJECT)
            return stackNode.m_nStackObjectID;
    }
    return Constants::OBJECT_INVALID;
}

NWNX_EXPORT ArgumentStack SetStackStringValue(ArgumentStack&& args)
{
    auto *pVM = Globals::VirtualMachine();
    const auto stackLocation = args.extract<int32_t>();
    const auto value = args.extract<std::string>();

    if (stackLocation >= 0 && stackLocation < pVM->m_cRunTimeStack.m_nTotalSize)
    {
        auto &stackNode = pVM->m_cRunTimeStack.GetStackNode(stackLocation);
        if (stackNode.m_nType == StackElement::STRING)
            stackNode.m_sString = value;
    }
    return {};
}

NWNX_EXPORT ArgumentStack GetStackStringValue(ArgumentStack&& args)
{
    auto *pVM = Globals::VirtualMachine();
    const auto stackLocation = args.extract<int32_t>();

    if (stackLocation >= 0 && stackLocation < pVM->m_cRunTimeStack.m_nTotalSize)
    {
        auto &stackNode = pVM->m_cRunTimeStack.GetStackNode(stackLocation);
        if (stackNode.m_nType == StackElement::STRING)
            return stackNode.m_sString;
    }
    return "";
}
