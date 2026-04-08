#include "nwnx.hpp"
#include "API/CVirtualMachine.hpp"
#include "API/StackElement.hpp"
#include "API/CVirtualMachineDebuggerInstance.hpp"

using namespace NWNXLib;
using namespace NWNXLib::API;

namespace NWNXLib::VM::StackManipulation
{
    static int32_t s_InstrPtrLevelForRecursionLevel[Constants::MAX_RECURSION_LEVEL];
    static int32_t s_StackPointerForRecursionLevel[Constants::MAX_RECURSION_LEVEL];

    void InitializeHooks()
    {
        static Hooks::Hook s_RunScriptFileHook = Hooks::HookFunction(&CVirtualMachine::RunScriptFile,
        +[](CVirtualMachine *pThis, int32_t nInstructionPointer) -> int32_t
        {
            s_StackPointerForRecursionLevel[pThis->m_nRecursionLevel] = pThis->m_cRunTimeStack.GetStackPointer();
            s_InstrPtrLevelForRecursionLevel[pThis->m_nRecursionLevel] = pThis->m_nInstructPtrLevel;
            return s_RunScriptFileHook->CallOriginal<int32_t>(pThis, nInstructionPointer);
        }, Hooks::Order::Early);
    }

    static Constants::VMAuxCodeType::TYPE StringTypeToAuxType(const CExoString& stringType)
    {
        if (stringType.IsEmpty())
            return Constants::VMAuxCodeType::Invalid;

        switch (stringType.CStr()[0])
        {
            case 'v': return Constants::VMAuxCodeType::Void;
            case 'i': return Constants::VMAuxCodeType::Integer;
            case 'f': return Constants::VMAuxCodeType::Float;
            case 'o': return Constants::VMAuxCodeType::Object;
            case 's': return Constants::VMAuxCodeType::String;
            case 'e':
            {
                switch (stringType.CStr()[1])
                {
                    case '0': return Constants::VMAuxCodeType::EngSt0;
                    case '1': return Constants::VMAuxCodeType::EngSt1;
                    case '2': return Constants::VMAuxCodeType::EngSt2;
                    case '3': return Constants::VMAuxCodeType::EngSt3;
                    case '4': return Constants::VMAuxCodeType::EngSt4;
                    case '5': return Constants::VMAuxCodeType::EngSt5;
                    case '6': return Constants::VMAuxCodeType::EngSt6;
                    case '7': return Constants::VMAuxCodeType::EngSt7;
                    case '8': return Constants::VMAuxCodeType::EngSt8;
                    case '9': return Constants::VMAuxCodeType::EngSt9;
                    default:  return Constants::VMAuxCodeType::Invalid;
                }
            }
            default: return Constants::VMAuxCodeType::Invalid;
        }
    }

    static bool ProcessStruct(const std::shared_ptr<CVirtualMachineDebuggerInstance>& dbg, StackFrame& stackFrame, const CExoString& typeName,
                              const CExoString& varName, int32_t strStackLoc, bool isParameter)
    {
        if (typeName.CStr()[0] != 't')
            return false;

        const int32_t structureDefinition = atoi(typeName.CStr() + 1);
        const int32_t structureFields = dbg->m_pDebugStructureFields[structureDefinition];
        int32_t currentSize = 0;
        const CExoString structureVariableName = varName + ".";

        stackFrame.stackVariables.emplace_back(varName, StackVariable{Constants::VMAuxCodeType::Void, strStackLoc, dbg->m_pDebugStructureNames[structureDefinition], isParameter});

        for (int32_t structureField = 0; structureField < structureFields; structureField++)
        {
            int32_t structVarStackLocation = strStackLoc + (currentSize >> 2);
            const CExoString& fieldTypeName = dbg->m_ppDebugStructureTypeNames[structureDefinition][structureField];
            const CExoString fieldVarName  = structureVariableName + dbg->m_ppDebugStructureFieldNames[structureDefinition][structureField];

            if (!ProcessStruct(dbg, stackFrame, fieldTypeName, fieldVarName, structVarStackLocation, isParameter))
            {
                const Constants::VMAuxCodeType::TYPE auxType = StringTypeToAuxType(fieldTypeName);
                if (auxType != Constants::VMAuxCodeType::Invalid)
                {
                    stackFrame.stackVariables.emplace_back(fieldVarName, StackVariable{auxType, structVarStackLocation, "", isParameter});
                }
            }
            currentSize += dbg->GenerateTypeSize(&dbg->m_ppDebugStructureTypeNames[structureDefinition][structureField]);
        }
        return true;
    }

    StackFrame GetStackFrame(int32_t nDepth, int32_t nRecursionLevel)
    {
        auto *pVM = Globals::VirtualMachine();
        int32_t nActualRecursionLevel = pVM->m_nRecursionLevel;
        if (nRecursionLevel == -1)
            nRecursionLevel = nActualRecursionLevel;

        StackFrame stackFrame;
        stackFrame.depth = nDepth;
        stackFrame.recursionLevel = nRecursionLevel;

        if (nRecursionLevel < 0 || nRecursionLevel > nActualRecursionLevel)
            return stackFrame;

        pVM->m_nRecursionLevel = nRecursionLevel;
        auto dbg = pVM->GetDebuggerInstance();
        pVM->m_nRecursionLevel = nActualRecursionLevel;

        if (!dbg)
            return stackFrame;

        int32_t currentStackPointer = (nRecursionLevel == nActualRecursionLevel) ? pVM->m_cRunTimeStack.GetStackPointer() : s_StackPointerForRecursionLevel[nRecursionLevel + 1];
        int32_t functionCount = (nRecursionLevel == nActualRecursionLevel) ? pVM->m_nInstructPtrLevel : s_InstrPtrLevelForRecursionLevel[nRecursionLevel + 1];
        int32_t functionIdentifier = dbg->GenerateFunctionIDFromInstructionPointer(*pVM->m_pCurrentInstructionPointer[nRecursionLevel]);

        if (functionIdentifier == -1)
        {
            LOG_ERROR("Bad NDB Data?");
            return stackFrame;
        }

        int32_t stackSize = dbg->GenerateStackSizeAtInstructionPointer(functionIdentifier, *pVM->m_pCurrentInstructionPointer[nRecursionLevel]);

        int32_t finalInstructionPointer;
        if (nDepth == 0)
            finalInstructionPointer = *pVM->m_pCurrentInstructionPointer[nRecursionLevel];
        else if (nDepth >= 1 && nDepth <= pVM->m_nInstructPtrLevel - 1)
            finalInstructionPointer = pVM->m_pnRunTimeInstructPtr[functionCount - nDepth];
        else
            return stackFrame;

        while (nDepth > 0)
        {
            --nDepth;
            --functionCount;
            currentStackPointer -= (stackSize >> 2);
            int32_t runTimePtr = pVM->m_pnRunTimeInstructPtr[functionCount];
            functionIdentifier = dbg->GenerateFunctionIDFromInstructionPointer(runTimePtr);
            if (functionIdentifier != -1)
                stackSize = dbg->GenerateStackSizeAtInstructionPointer(functionIdentifier,runTimePtr);
        }

        if (functionIdentifier == -1)
            return stackFrame;

        stackFrame.functionName = dbg->m_pDebugFunctionNames[functionIdentifier];

        const int32_t baseStackLocation = currentStackPointer - (stackSize >> 2);
        int32_t topMostStackEntry;
        if (dbg->GenerateTypeSize(&dbg->m_pDebugFunctionReturnTypeNames[functionIdentifier]) == 0)
            topMostStackEntry = -1;
        else
            topMostStackEntry = 0;

        int32_t debugVariableLocation;
        int32_t totalParameters = dbg->m_pDebugFunctionParameters[functionIdentifier];
        for (int32_t parameterCount = 0; parameterCount < totalParameters; parameterCount++)
        {
            debugVariableLocation = dbg->GenerateDebugVariableLocationForParameter(functionIdentifier, parameterCount);
            int32_t stackLocation = baseStackLocation + (dbg->m_pDebugVariableStackLocation[debugVariableLocation] >> 2);

            if (dbg->m_pDebugVariableStackLocation[debugVariableLocation] > topMostStackEntry)
                topMostStackEntry = dbg->m_pDebugVariableStackLocation[debugVariableLocation];

            if (!ProcessStruct(dbg, stackFrame, dbg->m_pDebugVariableTypeNames[debugVariableLocation], dbg->m_pDebugVariableNames[debugVariableLocation], stackLocation, true))
            {
                const Constants::VMAuxCodeType::TYPE auxType = StringTypeToAuxType(dbg->m_ppDebugFunctionParamTypeNames[functionIdentifier][parameterCount]);
                if (auxType != Constants::VMAuxCodeType::Invalid)
                {
                    stackFrame.stackVariables.emplace_back(dbg->m_pDebugVariableNames[debugVariableLocation], StackVariable{auxType, stackLocation, "", true});
                }
            }
        }

        debugVariableLocation = dbg->GetNextDebugVariable(functionIdentifier, finalInstructionPointer, topMostStackEntry);
        while (debugVariableLocation != -1)
        {
            int32_t stackLocation = baseStackLocation + (dbg->m_pDebugVariableStackLocation[debugVariableLocation] >> 2);
            topMostStackEntry = dbg->m_pDebugVariableStackLocation[debugVariableLocation];

            if (!ProcessStruct(dbg, stackFrame, dbg->m_pDebugVariableTypeNames[debugVariableLocation], dbg->m_pDebugVariableNames[debugVariableLocation], stackLocation, false))
            {
                const Constants::VMAuxCodeType::TYPE auxType = StringTypeToAuxType(dbg->m_pDebugVariableTypeNames[debugVariableLocation]);
                if (auxType != Constants::VMAuxCodeType::Invalid)
                {
                    stackFrame.stackVariables.emplace_back(dbg->m_pDebugVariableNames[debugVariableLocation], StackVariable{auxType, stackLocation, "", false});
                }
            }
            debugVariableLocation = dbg->GetNextDebugVariable(functionIdentifier, finalInstructionPointer, topMostStackEntry);
        }

        return stackFrame;
    }

    void SetStackIntegerValue(int32_t nStackLocation, int32_t nValue)
    {
        if (nStackLocation >= 0 && nStackLocation < Globals::VirtualMachine()->m_cRunTimeStack.m_nTotalSize)
        {
            auto &stackNode = Globals::VirtualMachine()->m_cRunTimeStack.GetStackNode(nStackLocation);
            if (stackNode.m_nType == StackElement::INTEGER)
                stackNode.m_nStackInt = nValue;
        }
    }

    int32_t GetStackIntegerValue(int32_t nStackLocation)
    {
        if (nStackLocation >= 0 && nStackLocation < Globals::VirtualMachine()->m_cRunTimeStack.m_nTotalSize)
        {
            auto &stackNode = Globals::VirtualMachine()->m_cRunTimeStack.GetStackNode(nStackLocation);
            if (stackNode.m_nType == StackElement::INTEGER)
                return stackNode.m_nStackInt;
        }
        return 0;
    }

    void SetStackFloatValue(int32_t nStackLocation, float fValue)
    {
        if (nStackLocation >= 0 && nStackLocation < Globals::VirtualMachine()->m_cRunTimeStack.m_nTotalSize)
        {
            auto &stackNode = Globals::VirtualMachine()->m_cRunTimeStack.GetStackNode(nStackLocation);
            if (stackNode.m_nType == StackElement::FLOAT)
                stackNode.m_fStackFloat = fValue;
        }
    }

    float GetStackFloatValue(int32_t nStackLocation)
    {
        if (nStackLocation >= 0 && nStackLocation < Globals::VirtualMachine()->m_cRunTimeStack.m_nTotalSize)
        {
            auto &stackNode = Globals::VirtualMachine()->m_cRunTimeStack.GetStackNode(nStackLocation);
            if (stackNode.m_nType == StackElement::FLOAT)
                return stackNode.m_fStackFloat;
        }
        return 0.0f;
    }

    void SetStackObjectValue(int32_t nStackLocation, ObjectID oidValue)
    {
        if (nStackLocation >= 0 && nStackLocation < Globals::VirtualMachine()->m_cRunTimeStack.m_nTotalSize)
        {
            auto &stackNode = Globals::VirtualMachine()->m_cRunTimeStack.GetStackNode(nStackLocation);
            if (stackNode.m_nType == StackElement::OBJECT)
                stackNode.m_nStackObjectID = oidValue;
        }
    }

    ObjectID GetStackObjectValue(int32_t nStackLocation)
    {
        if (nStackLocation >= 0 && nStackLocation < Globals::VirtualMachine()->m_cRunTimeStack.m_nTotalSize)
        {
            auto &stackNode = Globals::VirtualMachine()->m_cRunTimeStack.GetStackNode(nStackLocation);
            if (stackNode.m_nType == StackElement::OBJECT)
                return stackNode.m_nStackObjectID;
        }
        return Constants::OBJECT_INVALID;
    }

    void SetStackStringValue(int32_t nStackLocation, const CExoString& sValue)
    {
        if (nStackLocation >= 0 && nStackLocation < Globals::VirtualMachine()->m_cRunTimeStack.m_nTotalSize)
        {
            auto &stackNode = Globals::VirtualMachine()->m_cRunTimeStack.GetStackNode(nStackLocation);
            if (stackNode.m_nType == StackElement::STRING)
                stackNode.m_sString = sValue;
        }
    }

    CExoString GetStackStringValue(int32_t nStackLocation)
    {
        if (nStackLocation >= 0 && nStackLocation < Globals::VirtualMachine()->m_cRunTimeStack.m_nTotalSize)
        {
            auto &stackNode = Globals::VirtualMachine()->m_cRunTimeStack.GetStackNode(nStackLocation);
            if (stackNode.m_nType == StackElement::STRING)
                return stackNode.m_sString;
        }
        return "";
    }

    void SetStackLocationValue(int32_t nStackLocation, CScriptLocation* locValue)
    {
        if (nStackLocation >= 0 && nStackLocation < Globals::VirtualMachine()->m_cRunTimeStack.m_nTotalSize)
        {
            auto &stackNode = Globals::VirtualMachine()->m_cRunTimeStack.GetStackNode(nStackLocation);
            if (stackNode.m_nType == StackElement::ENGST2)
            {
                StackElement tempStack;
                tempStack.Init(StackElement::ENGST2);
                tempStack.m_pStackPtr = locValue;
                stackNode.Clear(Globals::VirtualMachine()->m_pCmdImplementer);
                stackNode.CopyFrom(tempStack, Globals::VirtualMachine()->m_pCmdImplementer);
            }
        }
    }

    CScriptLocation GetStackLocationValue(int32_t nStackLocation)
    {
        if (nStackLocation >= 0 && nStackLocation < Globals::VirtualMachine()->m_cRunTimeStack.m_nTotalSize)
        {
            auto &stackNode = Globals::VirtualMachine()->m_cRunTimeStack.GetStackNode(nStackLocation);
            if (stackNode.m_nType == StackElement::ENGST2)
                return *static_cast<CScriptLocation*>(stackNode.m_pStackPtr);
        }
        return CScriptLocation{};
    }

    void SetStackJsonValue(int32_t nStackLocation, JsonEngineStructure* jValue)
    {
        if (nStackLocation >= 0 && nStackLocation < Globals::VirtualMachine()->m_cRunTimeStack.m_nTotalSize)
        {
            auto &stackNode = Globals::VirtualMachine()->m_cRunTimeStack.GetStackNode(nStackLocation);
            if (stackNode.m_nType == StackElement::ENGST7)
            {
                StackElement tempStack;
                tempStack.Init(StackElement::ENGST7);
                tempStack.m_pStackPtr = jValue;
                stackNode.Clear(Globals::VirtualMachine()->m_pCmdImplementer);
                stackNode.CopyFrom(tempStack, Globals::VirtualMachine()->m_pCmdImplementer);
            }
        }
    }

    JsonEngineStructure GetStackJsonValue(int32_t nStackLocation)
    {
        if (nStackLocation >= 0 && nStackLocation < Globals::VirtualMachine()->m_cRunTimeStack.m_nTotalSize)
        {
            auto &stackNode = Globals::VirtualMachine()->m_cRunTimeStack.GetStackNode(nStackLocation);
            if (stackNode.m_nType == StackElement::ENGST7)
                return *static_cast<JsonEngineStructure*>(stackNode.m_pStackPtr);
        }
        return JsonEngineStructure{};
    }
}

VM::StackManipulation::StackFrame CVirtualMachine::GetStackFrame(int32_t nDepth, int32_t nRecursionLevel)
{
    return VM::StackManipulation::GetStackFrame(nDepth, nRecursionLevel);
}

void CVirtualMachine::SetStackIntegerValue(int32_t nStackLocation, int32_t nValue)
{
    VM::StackManipulation::SetStackIntegerValue(nStackLocation, nValue);
}

int32_t CVirtualMachine::GetStackIntegerValue(int32_t nStackLocation)
{
    return VM::StackManipulation::GetStackIntegerValue(nStackLocation);
}

void CVirtualMachine::SetStackFloatValue(int32_t nStackLocation, float fValue)
{
    VM::StackManipulation::SetStackFloatValue(nStackLocation, fValue);
}

float CVirtualMachine::GetStackFloatValue(int32_t nStackLocation)
{
    return VM::StackManipulation::GetStackFloatValue(nStackLocation);
}

void CVirtualMachine::SetStackObjectValue(int32_t nStackLocation, ObjectID oidValue)
{
    VM::StackManipulation::SetStackObjectValue(nStackLocation, oidValue);
}

ObjectID CVirtualMachine::GetStackObjectValue(int32_t nStackLocation)
{
    return VM::StackManipulation::GetStackObjectValue(nStackLocation);
}

void CVirtualMachine::SetStackStringValue(int32_t nStackLocation, const CExoString& sValue)
{
    VM::StackManipulation::SetStackStringValue(nStackLocation, sValue);
}

CExoString CVirtualMachine::GetStackStringValue(int32_t nStackLocation)
{
    return VM::StackManipulation::GetStackStringValue(nStackLocation);
}

void CVirtualMachine::SetStackLocationValue(int32_t nStackLocation, CScriptLocation* locValue)
{
    VM::StackManipulation::SetStackLocationValue(nStackLocation, locValue);
}

CScriptLocation CVirtualMachine::GetStackLocationValue(int32_t nStackLocation)
{
    return VM::StackManipulation::GetStackLocationValue(nStackLocation);
}

void CVirtualMachine::SetStackJsonValue(int32_t nStackLocation, JsonEngineStructure* jValue)
{
    VM::StackManipulation::SetStackJsonValue(nStackLocation, jValue);
}

JsonEngineStructure CVirtualMachine::GetStackJsonValue(int32_t nStackLocation)
{
    return VM::StackManipulation::GetStackJsonValue(nStackLocation);
}
