#include "nwnx.hpp"
#include "API/CVirtualMachine.hpp"
#include "API/StackElement.hpp"
#include "API/CVirtualMachineDebuggerInstance.hpp"

using namespace NWNXLib;
using namespace NWNXLib::API;

namespace NWNXLib::VM::StackManipulation
{
    std::unordered_map<std::string, StackVariable> GetCurrentStack(int32_t nDepth)
    {
        auto *pVM = Globals::VirtualMachine();
        auto dbg = pVM->GetDebuggerInstance();
        std::unordered_map<std::string, StackVariable> currentStack;

        int32_t finalInstructionPointer;
        if (nDepth == 0)
            finalInstructionPointer = *pVM->m_pCurrentInstructionPointer[pVM->m_nRecursionLevel];
        else if (nDepth >= 1 && nDepth <= pVM->m_nInstructPtrLevel - 1)
            finalInstructionPointer = pVM->m_pnRunTimeInstructPtr[pVM->m_nInstructPtrLevel - nDepth];
        else
            return currentStack;

        int32_t functionIdentifier = dbg->GenerateFunctionIDFromInstructionPointer(*pVM->m_pCurrentInstructionPointer[pVM->m_nRecursionLevel]);
        int32_t currentStackPointer = pVM->m_cRunTimeStack.GetStackPointer();
        int32_t stackSize = dbg->GenerateStackSizeAtInstructionPointer(functionIdentifier,*pVM->m_pCurrentInstructionPointer[pVM->m_nRecursionLevel]);
        int32_t functionCount = pVM->m_nInstructPtrLevel;

        while (nDepth > 0)
        {
            --nDepth;
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

        static auto StringTypeToAuxType = [](const CExoString& stringType) -> Constants::VMAuxCodeType::TYPE
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
                    }
                }
            }

            return Constants::VMAuxCodeType::Invalid;
        };

        std::function<bool(const CExoString&, int, int, int)> ProcessStructInStruct = [&](const CExoString& parentStr, const int32_t strDef, const int32_t strField, const int32_t strStackLoc) -> bool
        {
            if (dbg->m_ppDebugStructureTypeNames[strDef][strField].CStr()[0] == 't')
            {
                const int32_t structureDefinition = atoi(dbg->m_ppDebugStructureTypeNames[strDef][strField].CStr() + 1);
                const int32_t structureFields = dbg->m_pDebugStructureFields[structureDefinition];
                int32_t currentSize = 0;
                const CExoString structureVariableName = parentStr + dbg->m_ppDebugStructureFieldNames[strDef][strField] + ".";

                currentStack.emplace(parentStr + dbg->m_ppDebugStructureFieldNames[strDef][strField],
                    StackVariable{Constants::VMAuxCodeType::Void, strStackLoc, dbg->m_pDebugStructureNames[structureDefinition]});

                for (int32_t structureField = 0; structureField < structureFields; structureField++)
                {
                    int32_t structVarStackLocation = strStackLoc + (currentSize >> 2);
                    if (!ProcessStructInStruct(structureVariableName, structureDefinition, structureField, structVarStackLocation))
                    {
                        const Constants::VMAuxCodeType::TYPE auxType = StringTypeToAuxType(dbg->m_ppDebugStructureTypeNames[structureDefinition][structureField]);
                        if (auxType != Constants::VMAuxCodeType::Invalid)
                        {
                            currentStack.emplace(structureVariableName + dbg->m_ppDebugStructureFieldNames[structureDefinition][structureField],
                                StackVariable{auxType, structVarStackLocation, ""});
                        }
                    }
                    currentSize += dbg->GenerateTypeSize(&dbg->m_ppDebugStructureTypeNames[structureDefinition][structureField]);
                }
                return true;
            }
            return false;
        };

        auto ProcessStruct = [&dbg, &currentStack, ProcessStructInStruct](const int32_t debugVariableLocation, const int32_t stackLocation) -> bool
        {
            if (dbg->m_pDebugVariableTypeNames[debugVariableLocation].CStr()[0] == 't')
            {
                const int32_t structureDefinition = atoi(dbg->m_pDebugVariableTypeNames[debugVariableLocation].CStr() + 1);
                const int32_t structureFields = dbg->m_pDebugStructureFields[structureDefinition];
                int32_t currentSize = 0;
                const CExoString structureVariableName = dbg->m_pDebugVariableNames[debugVariableLocation] + ".";

                currentStack.emplace(dbg->m_pDebugVariableNames[debugVariableLocation],
                    StackVariable{Constants::VMAuxCodeType::Void, stackLocation, dbg->m_pDebugStructureNames[structureDefinition]});

                for (int32_t structureField = 0; structureField < structureFields; structureField++)
                {
                    int32_t structVarStackLocation = stackLocation + (currentSize >> 2);
                    if (!ProcessStructInStruct(structureVariableName, structureDefinition, structureField, structVarStackLocation))
                    {
                        const Constants::VMAuxCodeType::TYPE auxType = StringTypeToAuxType(dbg->m_ppDebugStructureTypeNames[structureDefinition][structureField]);
                        if (auxType != Constants::VMAuxCodeType::Invalid)
                        {
                            currentStack.emplace(structureVariableName + dbg->m_ppDebugStructureFieldNames[structureDefinition][structureField],
                                StackVariable{auxType, structVarStackLocation, ""});
                        }
                    }
                    currentSize += dbg->GenerateTypeSize(&dbg->m_ppDebugStructureTypeNames[structureDefinition][structureField]);
                }
                return true;
            }
            return false;
        };

        int32_t debugVariableLocation, totalParameters = dbg->m_pDebugFunctionParameters[functionIdentifier];
        for (int32_t parameterCount = 0; parameterCount < totalParameters; parameterCount++)
        {
            debugVariableLocation = dbg->GenerateDebugVariableLocationForParameter(functionIdentifier, parameterCount);
            int32_t stackLocation = baseStackLocation + (dbg->m_pDebugVariableStackLocation[debugVariableLocation] >> 2);

            if (dbg->m_pDebugVariableStackLocation[debugVariableLocation] > topMostStackEntry)
                topMostStackEntry = dbg->m_pDebugVariableStackLocation[debugVariableLocation];

            if (!ProcessStruct(debugVariableLocation, stackLocation))
            {
                const Constants::VMAuxCodeType::TYPE auxType = StringTypeToAuxType(dbg->m_ppDebugFunctionParamTypeNames[functionIdentifier][parameterCount]);
                if (auxType != Constants::VMAuxCodeType::Invalid)
                {
                    currentStack.emplace(dbg->m_pDebugVariableNames[debugVariableLocation], StackVariable{auxType, stackLocation, ""});
                }
            }
        }

        debugVariableLocation = dbg->GetNextDebugVariable(functionIdentifier, finalInstructionPointer, topMostStackEntry);
        while (debugVariableLocation != -1)
        {
            int32_t stackLocation = baseStackLocation + (dbg->m_pDebugVariableStackLocation[debugVariableLocation] >> 2);
            topMostStackEntry = dbg->m_pDebugVariableStackLocation[debugVariableLocation];

            if (!ProcessStruct(debugVariableLocation, stackLocation))
            {
                const Constants::VMAuxCodeType::TYPE auxType = StringTypeToAuxType(dbg->m_pDebugVariableTypeNames[debugVariableLocation]);
                if (auxType != Constants::VMAuxCodeType::Invalid)
                {
                    currentStack.emplace(dbg->m_pDebugVariableNames[debugVariableLocation], StackVariable{auxType, stackLocation, ""});
                }
            }
            debugVariableLocation = dbg->GetNextDebugVariable(functionIdentifier, finalInstructionPointer, topMostStackEntry);
        }

        return currentStack;
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

std::unordered_map<std::string, VM::StackManipulation::StackVariable> CVirtualMachine::GetCurrentStack(int32_t nDepth)
{
    return VM::StackManipulation::GetCurrentStack(nDepth);
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
