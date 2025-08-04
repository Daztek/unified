#include "nwnx.hpp"
#include "API/CVirtualMachine.hpp"
#include "API/CScriptCompiler.hpp"
#include "API/StackElement.hpp"
#include "API/CNWSDialog.hpp"
#include "API/CNWSObject.hpp"
#include "API/CScriptParseTreeNode.hpp"
#include "API/CScriptCompilerIdListEntry.hpp"
#include "API/CScriptCompilerVarStackEntry.hpp"

using namespace NWNXLib;
using namespace NWNXLib::API;

namespace NWNXLib::VM::ReturnTypeExtension
{
    constexpr int32_t MAX_RECURSION_LEVELS = 8;
    static StackElement s_cRunScriptReturnValue[MAX_RECURSION_LEVELS];

    void InitializeHooks()
    {
        static Hooks::Hook s_RunScriptFileHook = Hooks::HookFunction(&CVirtualMachine::RunScriptFile,
        +[](CVirtualMachine* pThis, int32_t nInstructionPointer) -> int32_t
        {
            int32_t nStackLevel = pThis->m_cRunTimeStack.GetStackPointer();
            int32_t nCallStackPointer = pThis->m_nInstructPtrLevel;
            s_cRunScriptReturnValue[pThis->m_nRecursionLevel].Clear(pThis->m_pCmdImplementer);
            pThis->PushInstructionPtr(0xFFFFFFFF);
            int32_t nMyPointer = nInstructionPointer;
            int32_t nReturnValue = pThis->ExecuteCode(&nMyPointer, pThis->m_pVirtualMachineScript[pThis->m_nRecursionLevel].m_pCode);

            if (nReturnValue == Constants::VMError::FakeAbortScript)
            {
                if (!pThis->m_sAbortCustomError.IsEmpty())
                {
                    CExoString sFileName = pThis->m_pVirtualMachineScript[pThis->m_nRecursionLevel].m_sScriptName.CStr();
                    pThis->m_pCmdImplementer->ReportError(sFileName, -nReturnValue, pThis->m_sAbortCustomError);
                    pThis->m_sAbortCustomError = "";
                }
                nReturnValue = 0;
            }
            else if (nReturnValue < 0 || nCallStackPointer != pThis->m_nInstructPtrLevel)
            {
                CExoString sFileName = pThis->m_pVirtualMachineScript[pThis->m_nRecursionLevel].m_sScriptName.CStr();
                pThis->m_pCmdImplementer->ReportError(sFileName, -nReturnValue, pThis->m_sAbortCustomError);
                if (pThis->m_cRunTimeStack.GetStackPointer() > nStackLevel)
                    pThis->m_cRunTimeStack.SetStackPointer(nStackLevel);
                else
                    pThis->m_cRunTimeStack.m_nStackPointer = nStackLevel;
                pThis->m_nInstructPtrLevel = nCallStackPointer;
            }

            return nReturnValue;
        }, Hooks::Order::Final);

        static Hooks::Hook s_RunScriptHook = Hooks::HookFunction(&CVirtualMachine::RunScript,
        +[](CVirtualMachine* pThis, CExoString* psFileName, OBJECT_ID oid, BOOL bOidValid, int32_t nScriptEventID) -> BOOL
        {
            if (psFileName->IsEmpty())
                return false;

            INSTR_SCOPE();
            INSTR_SCOPE_TEXT(psFileName->CStr(), psFileName->GetLength());
            INSTR_SCOPE_PROP_HEX32("OBJECT_SELF", oid);

            if (pThis->m_nRecursionLevel == -1)
            {
                pThis->m_cRunTimeStack.InitializeStack();
                pThis->m_cRunTimeStack.m_pVMachine = pThis;
                pThis->m_nInstructionsExecuted = 0;
                pThis->m_nInstructPtrLevel = 0;
            }

            if (pThis->ReadScriptFile(psFileName, nScriptEventID) == 0)
            {
                pThis->m_bValidObjectRunScript[pThis->m_nRecursionLevel] = bOidValid;
                pThis->m_oidObjectRunScript[pThis->m_nRecursionLevel] = oid;
                pThis->m_pCmdImplementer->m_bValidObjectRunScript = pThis->m_bValidObjectRunScript[pThis->m_nRecursionLevel];
                pThis->m_pCmdImplementer->m_oidObjectRunScript = pThis->m_oidObjectRunScript[pThis->m_nRecursionLevel];
                pThis->m_pCmdImplementer->RunScriptCallback(psFileName, pThis->m_nRecursionLevel);
                int32_t nStackLevelBeforeScript = pThis->m_cRunTimeStack.GetStackPointer();
                int32_t nRunValue = pThis->RunScriptFile(0);
                pThis->m_pCmdImplementer->RunScriptEndCallback(psFileName, pThis->m_nRecursionLevel);
                pThis->m_lScriptParams->SetSize(0);
                pThis->DeleteScript(&pThis->m_pVirtualMachineScript[pThis->m_nRecursionLevel]);
                --pThis->m_nRecursionLevel;

                if (pThis->m_nRecursionLevel != -1)
                {
                    pThis->m_pCmdImplementer->m_bValidObjectRunScript = pThis->m_bValidObjectRunScript[pThis->m_nRecursionLevel];
                    pThis->m_pCmdImplementer->m_oidObjectRunScript = pThis->m_oidObjectRunScript[pThis->m_nRecursionLevel];
                }

                if (nRunValue == 0)
                {
                    if (pThis->m_cRunTimeStack.GetStackPointer() == nStackLevelBeforeScript + 1)
                    {
                        s_cRunScriptReturnValue[pThis->m_nRecursionLevel + 1].CopyFrom(pThis->m_cRunTimeStack.GetStackNode(nStackLevelBeforeScript), pThis->m_pCmdImplementer);
                        pThis->m_cRunTimeStack.SetStackPointer(nStackLevelBeforeScript);
                        if (pThis->m_nRecursionLevel == -1)
                            pThis->m_cRunTimeStack.InitializeStack();
                        return true;
                    }
                    else if (pThis->m_cRunTimeStack.GetStackPointer() == nStackLevelBeforeScript)
                    {
                        if (pThis->m_nRecursionLevel == -1)
                            pThis->m_cRunTimeStack.InitializeStack();
                        return true;
                    }

                    pThis->m_cRunTimeStack.SetStackPointer(nStackLevelBeforeScript);
                    if (pThis->m_nRecursionLevel == -1)
                        pThis->m_cRunTimeStack.InitializeStack();
                    return false;
                }

                return false;
            }

            return false;
        }, Hooks::Order::Final);

        static Hooks::Hook s_RunScriptChunkHook = Hooks::HookFunction(&CVirtualMachine::RunScriptChunk,
        +[](CVirtualMachine* pThis, const CExoString& sScriptChunk, OBJECT_ID oid, BOOL bOidValid, BOOL bWrapIntoMain) -> int32_t
        {
            INSTR_SCOPE();
            INSTR_SCOPE_TEXT(sScriptChunk.CStr(), sScriptChunk.GetLength());
            INSTR_SCOPE_PROP_HEX32("OBJECT_SELF", oid);

            if (pThis->m_nRecursionLevel == -1)
            {
                pThis->m_cRunTimeStack.InitializeStack();
                pThis->m_cRunTimeStack.m_pVMachine = pThis;
                pThis->m_nInstructionsExecuted = 0;
                pThis->m_nInstructPtrLevel = 0;
            }

            if (pThis->SetUpJITCompiledScript(sScriptChunk, bWrapIntoMain) == 0)
            {
                pThis->m_bValidObjectRunScript[pThis->m_nRecursionLevel] = bOidValid;
                pThis->m_oidObjectRunScript[pThis->m_nRecursionLevel] = oid;
                pThis->m_pCmdImplementer->m_bValidObjectRunScript = pThis->m_bValidObjectRunScript[pThis->m_nRecursionLevel];
                pThis->m_pCmdImplementer->m_oidObjectRunScript = pThis->m_oidObjectRunScript[pThis->m_nRecursionLevel];
                pThis->m_pCmdImplementer->RunScriptCallback(nullptr, pThis->m_nRecursionLevel);
                int32_t nStackLevelBeforeScript = pThis->m_cRunTimeStack.GetStackPointer();
                int32_t nRunValue = pThis->RunScriptFile(0);
                pThis->m_pCmdImplementer->RunScriptEndCallback(nullptr, pThis->m_nRecursionLevel);
                pThis->DeleteScript(&pThis->m_pVirtualMachineScript[pThis->m_nRecursionLevel]);
                --pThis->m_nRecursionLevel;

                if (pThis->m_nRecursionLevel != -1)
                {
                    pThis->m_pCmdImplementer->m_bValidObjectRunScript = pThis->m_bValidObjectRunScript[pThis->m_nRecursionLevel];
                    pThis->m_pCmdImplementer->m_oidObjectRunScript = pThis->m_oidObjectRunScript[pThis->m_nRecursionLevel];
                }

                if (nRunValue == 0)
                {
                    if (pThis->m_cRunTimeStack.GetStackPointer() == nStackLevelBeforeScript + 1)
                    {
                        s_cRunScriptReturnValue[pThis->m_nRecursionLevel + 1].CopyFrom(pThis->m_cRunTimeStack.GetStackNode(nStackLevelBeforeScript), pThis->m_pCmdImplementer);
                        pThis->m_cRunTimeStack.SetStackPointer(nStackLevelBeforeScript);
                        if (pThis->m_nRecursionLevel == -1)
                            pThis->m_cRunTimeStack.InitializeStack();
                        return 0;
                    }
                    else if (pThis->m_cRunTimeStack.GetStackPointer() == nStackLevelBeforeScript)
                    {
                        if (pThis->m_nRecursionLevel == -1)
                            pThis->m_cRunTimeStack.InitializeStack();
                        return 0;
                    }

                    pThis->m_cRunTimeStack.SetStackPointer(nStackLevelBeforeScript);
                    if (pThis->m_nRecursionLevel == -1)
                        pThis->m_cRunTimeStack.InitializeStack();

                    pThis->m_sAbortCustomError = "Huh?";
                    return -645;
                }

                return -634;
            }

            return -635;
        }, Hooks::Order::Final);

        static Hooks::Hook s_InstallLoaderHook = Hooks::HookFunction(&CScriptCompiler::InstallLoader,
        +[](CScriptCompiler* pThis) -> int32_t
        {
            static auto InsertMainReturnValue = [](CScriptCompiler* pThis, int32_t nReturnType, int32_t nTokenType) -> void
            {
                pThis->m_pcIdentifierList[pThis->m_nOccupiedIdentifiers].m_nReturnType = nReturnType;
                ++pThis->m_nOccupiedVariables;
                ++pThis->m_nVarStackRecursionLevel;
                ++pThis->m_nGlobalVariables;
                pThis->m_pcVarStackList[pThis->m_nOccupiedVariables].m_psVarName = "#retval";
                pThis->m_pcVarStackList[pThis->m_nOccupiedVariables].m_nVarType = nTokenType;
                pThis->m_pcVarStackList[pThis->m_nOccupiedVariables].m_nVarLevel = pThis->m_nVarStackRecursionLevel;
                pThis->m_pcVarStackList[pThis->m_nOccupiedVariables].m_nVarRunTimeLocation = pThis->m_nStackCurrentDepth * 4;
                int32_t nOccupiedVariables = pThis->m_nOccupiedVariables;
                int32_t nStackCurrentDepth = pThis->m_nStackCurrentDepth;
                int32_t nGlobalVariableSize = pThis->m_nGlobalVariableSize;
                pThis->AddVariableToStack(nTokenType, nullptr, true);
                pThis->AddToSymbolTableVarStack(nOccupiedVariables, nStackCurrentDepth, nGlobalVariableSize);
                --pThis->m_nStackCurrentDepth;
            };

            pThis->m_aOutputCodeInstructionBoundaries.push_back(pThis->m_nOutputCodeLength);
            int32_t nMainIdentifier;

            if (pThis->m_bCompileConditionalOrMain == true)
            {
                pThis->m_bOldCompileConditionalFile = pThis->m_bCompileConditionalFile;
                nMainIdentifier = pThis->GetIdentifierByName("main");
                if (nMainIdentifier >= 0)
                    pThis->m_bCompileConditionalFile = false;
                else
                {
                    nMainIdentifier = pThis->GetIdentifierByName("StartingConditional");
                    if (nMainIdentifier >= 0)
                        pThis->m_bCompileConditionalFile = true;
                    else
                        pThis->m_bCompileConditionalFile = false;
                }
            }

            if (pThis->m_bCompileConditionalFile == false)
            {
                nMainIdentifier = pThis->GetIdentifierByName("main");
                if (nMainIdentifier < 0)
                {
                    return -623;
                }

                if (pThis->m_pcIdentifierList[nMainIdentifier].m_nReturnType != 38 &&
                    pThis->m_pcIdentifierList[nMainIdentifier].m_nReturnType != 34 &&
                    pThis->m_pcIdentifierList[nMainIdentifier].m_nReturnType != 35 &&
                    pThis->m_pcIdentifierList[nMainIdentifier].m_nReturnType != 36 &&
                    pThis->m_pcIdentifierList[nMainIdentifier].m_nReturnType != 37 &&
                    pThis->m_pcIdentifierList[nMainIdentifier].m_nReturnType != 80)
                {
                    return -624;
                }

                if (pThis->m_pcIdentifierList[nMainIdentifier].m_nParameters != 0)
                {
                    return -625;
                }
            }
            else
            {
                nMainIdentifier = pThis->GetIdentifierByName("StartingConditional");
                if (nMainIdentifier < 0)
                {
                    return -5182;
                }

                if (pThis->m_pcIdentifierList[nMainIdentifier].m_nReturnType != 34)
                {
                    return -5183;
                }

                if (pThis->m_pcIdentifierList[nMainIdentifier].m_nParameters != 0)
                {
                    return -5184;
                }
            }

            BOOL bGlobalVariablesPresent = false;
            if (pThis->m_pGlobalVariableParseTree != NULL)
            {
                bGlobalVariablesPresent = true;
            }

            pThis->m_pcIdentifierList[pThis->m_nOccupiedIdentifiers].m_psIdentifier = "#loader";
            pThis->m_pcIdentifierList[pThis->m_nOccupiedIdentifiers].m_nIdentifierHash = pThis->HashString("#loader");
            pThis->m_pcIdentifierList[pThis->m_nOccupiedIdentifiers].m_nIdentifierLength = 7;
            pThis->m_pcIdentifierList[pThis->m_nOccupiedIdentifiers].m_nBinarySourceStart = pThis->m_nOutputCodeLength;
            pThis->m_pcIdentifierList[pThis->m_nOccupiedIdentifiers].m_nBinaryDestinationStart = -1;
            pThis->m_pcIdentifierList[pThis->m_nOccupiedIdentifiers].m_nBinaryDestinationFinish = -1;
            pThis->m_pcIdentifierList[pThis->m_nOccupiedIdentifiers].m_nParameters = 0;
            pThis->m_pcIdentifierList[pThis->m_nOccupiedIdentifiers].m_nReturnType = 38;
            pThis->HashManagerAdd(1, pThis->m_nOccupiedIdentifiers);

            switch (pThis->m_pcIdentifierList[nMainIdentifier].m_nReturnType)
            {
                case 34:
                    InsertMainReturnValue(pThis, pThis->m_pcIdentifierList[nMainIdentifier].m_nReturnType, 29);
                    break;

                case 35:
                    InsertMainReturnValue(pThis, pThis->m_pcIdentifierList[nMainIdentifier].m_nReturnType, 30);
                    break;

                case 36:
                    InsertMainReturnValue(pThis, pThis->m_pcIdentifierList[nMainIdentifier].m_nReturnType, 31);
                    break;

                case 37:
                    InsertMainReturnValue(pThis, pThis->m_pcIdentifierList[nMainIdentifier].m_nReturnType, 32);
                    break;

                case 80:
                    InsertMainReturnValue(pThis, pThis->m_pcIdentifierList[nMainIdentifier].m_nReturnType, 70);
                    break;
            }

            pThis->m_pchOutputCode[pThis->m_nOutputCodeLength + 0] = 0x1e;
            pThis->m_pchOutputCode[pThis->m_nOutputCodeLength + 1] = 0;
            int32_t nSymbolSubType1 = 0;
            int32_t nSymbolSubType2 = 0;

            if (bGlobalVariablesPresent)
                nSymbolSubType2 = 1;
            else
                nSymbolSubType2 = 2;

            pThis->AddSymbolToQueryList(pThis->m_nOutputCodeLength + 2, 1, nSymbolSubType1, nSymbolSubType2);
            pThis->m_nOutputCodeLength += 2 + 4;
            pThis->m_aOutputCodeInstructionBoundaries.push_back(pThis->m_nOutputCodeLength);
            pThis->m_pchOutputCode[pThis->m_nOutputCodeLength + 0] = 0x20;
            pThis->m_pchOutputCode[pThis->m_nOutputCodeLength + 1] = 0;
            pThis->m_nOutputCodeLength += 2;
            pThis->m_aOutputCodeInstructionBoundaries.push_back(pThis->m_nOutputCodeLength);
            pThis->m_pcIdentifierList[pThis->m_nOccupiedIdentifiers].m_nBinarySourceFinish = pThis->m_nOutputCodeLength;
            ++pThis->m_nOccupiedIdentifiers;

            if (pThis->m_nOccupiedIdentifiers >= 65536)
                return pThis->OutputWalkTreeError(-577, nullptr);

            return 0;
        }, Hooks::Order::Final);

        static Hooks::Hook s_PostVisitGenerateCodeHook = Hooks::HookFunction(&CScriptCompiler::PostVisitGenerateCode,
        +[](CScriptCompiler* pThis, CScriptParseTreeNode* pNode) -> int32_t
        {
            if (pNode->nOperation == 73)
            {
                pThis->m_bGlobalVariableDefinition = false;
                pThis->m_pchOutputCode[pThis->m_nOutputCodeLength + 0] = 0x2A;
                pThis->m_pchOutputCode[pThis->m_nOutputCodeLength + 1] = 0;
                pThis->m_nOutputCodeLength += 2;
                pThis->m_aOutputCodeInstructionBoundaries.push_back(pThis->m_nOutputCodeLength);

                int32_t nMainIdentifier = pThis->GetIdentifierByName("main");
                {
                    int32_t nAuxType = Constants::VMAuxCodeType::Void;
                    if (pThis->m_bCompileConditionalFile)
                        nAuxType = Constants::VMAuxCodeType::Integer;
                    else
                    {
                        if (nMainIdentifier >= 0)
                        {
                            int32_t nMainReturnType = pThis->m_pcIdentifierList[nMainIdentifier].m_nReturnType;

                            switch (nMainReturnType)
                            {
                                case 34:
                                    nAuxType = Constants::VMAuxCodeType::Integer;
                                    break;
                                case 35:
                                    nAuxType = Constants::VMAuxCodeType::Float;
                                    break;
                                case 36:
                                    nAuxType = Constants::VMAuxCodeType::String;
                                    break;
                                case 37:
                                    nAuxType = Constants::VMAuxCodeType::Object;
                                    break;
                                case 80:
                                    nAuxType = Constants::VMAuxCodeType::EngSt7;
                                    break;

                                default:
                                    nAuxType = Constants::VMAuxCodeType::Void;
                                    break;
                            }
                        }
                    }

                    if (nAuxType != Constants::VMAuxCodeType::Void)
                    {
                        pThis->m_pchOutputCode[pThis->m_nOutputCodeLength + 0] = 0x02;
                        pThis->m_pchOutputCode[pThis->m_nOutputCodeLength + 1] = nAuxType;
                        pThis->m_nOutputCodeLength += 2;
                        pThis->m_aOutputCodeInstructionBoundaries.push_back(pThis->m_nOutputCodeLength);
                    }
                }

                pThis->m_pchOutputCode[pThis->m_nOutputCodeLength + 0] = 0x1e;
                pThis->m_pchOutputCode[pThis->m_nOutputCodeLength + 1] = 0;
                pThis->AddSymbolToQueryList(pThis->m_nOutputCodeLength + 2, 1, 0, 2);
                pThis->m_nOutputCodeLength += 2 + 4;
                pThis->m_aOutputCodeInstructionBoundaries.push_back(pThis->m_nOutputCodeLength);

                if (pThis->m_bCompileConditionalFile == true || (nMainIdentifier >= 0 && pThis->m_pcIdentifierList[nMainIdentifier].m_nReturnType != 38))
                {
                    int32_t nStackElementsDown = -(pThis->m_nGlobalVariableSize + 12);
                    int32_t nSize = 4;
                    if (pThis->m_nFunctionImpReturnType == 54)
                    {
                        nSize = pThis->GetStructureSize(pThis->m_sFunctionImpReturnStructureName);
                    }
                    pThis->m_pchOutputCode[pThis->m_nOutputCodeLength + 0] = 0x01;
                    pThis->m_pchOutputCode[pThis->m_nOutputCodeLength + 1] = Constants::VMAuxCodeType::Void;
                    pThis->m_pchOutputCode[pThis->m_nOutputCodeLength + 2] = (char)(((nStackElementsDown) >> 24) & 0x0ff);
                    pThis->m_pchOutputCode[pThis->m_nOutputCodeLength + 2 + 1] = (char)(((nStackElementsDown) >> 16) & 0x0ff);
                    pThis->m_pchOutputCode[pThis->m_nOutputCodeLength + 2 + 2] = (char)(((nStackElementsDown) >> 8) & 0x0ff);
                    pThis->m_pchOutputCode[pThis->m_nOutputCodeLength + 2 + 3] = (char)(((nStackElementsDown)) & 0x0ff);
                    pThis->m_pchOutputCode[pThis->m_nOutputCodeLength + 2 + 4] = (char)(((nSize) >> 8) & 0x0ff);
                    pThis->m_pchOutputCode[pThis->m_nOutputCodeLength + 2 + 5] = (char)(((nSize)) & 0x0ff);
                    pThis->m_nOutputCodeLength += 2 + 6;
                    pThis->m_aOutputCodeInstructionBoundaries.push_back(pThis->m_nOutputCodeLength);
                    pThis->EmitModifyStackPointer(-4);
                }

                pThis->m_pchOutputCode[pThis->m_nOutputCodeLength + 0] = 0x2B;
                pThis->m_pchOutputCode[pThis->m_nOutputCodeLength + 1] = 0;
                pThis->m_nOutputCodeLength += 2;
                pThis->m_aOutputCodeInstructionBoundaries.push_back(pThis->m_nOutputCodeLength);

                int32_t nStackPointer = -pThis->m_nGlobalVariableSize;
                if (nStackPointer != 0)
                {
                    pThis->EmitModifyStackPointer(nStackPointer);
                }

                pThis->m_pchOutputCode[pThis->m_nOutputCodeLength + 0] = 0x20;
                pThis->m_pchOutputCode[pThis->m_nOutputCodeLength + 1] = 0;
                pThis->m_nOutputCodeLength += 2;
                pThis->m_aOutputCodeInstructionBoundaries.push_back(pThis->m_nOutputCodeLength);
                pThis->m_pcIdentifierList[pThis->m_nOccupiedIdentifiers].m_nBinarySourceFinish = pThis->m_nOutputCodeLength;

                ++pThis->m_nOccupiedIdentifiers;
                if (pThis->m_nOccupiedIdentifiers >= 65536)
                {
                    return pThis->OutputWalkTreeError(-577, pNode);
                }

                return 0;
            }

            return s_PostVisitGenerateCodeHook->CallOriginal<int32_t>(pThis, pNode);
        }, Hooks::Order::Late);

        static Hooks::Hook s_CheckScriptHook = Hooks::HookFunction(&CNWSDialog::CheckScript,
        +[](CNWSDialog*, CNWSObject* pSpeaker, const CResRef& sActive, const CExoArrayList<ScriptParam>& scriptParams) -> BOOL
        {
            if (sActive == "")
                return true;
            CExoString sString;
            sActive.CopyToString(sString);
            Globals::VirtualMachine()->SetScriptParameters(scriptParams);
            BOOL nReturnValue = Globals::VirtualMachine()->RunScript(&sString, pSpeaker->m_idSelf);
            if (nReturnValue == 1)
            {
                int32_t nScriptRetVal;
                if (Globals::VirtualMachine()->GetRunScriptReturnValueInteger(&nScriptRetVal))
                    nReturnValue = !!nScriptRetVal;
                else
                    nReturnValue = false;
            }
            return nReturnValue;
        }, Hooks::Order::Final);
    }

    void Cleanup()
    {
        for (auto &stackElement : s_cRunScriptReturnValue)
        {
            stackElement.Clear(Globals::VirtualMachine()->m_pCmdImplementer);
        }
    }

    uint8_t GetRunScriptReturnValueType()
    {
        auto* pVM = Globals::VirtualMachine();
        return pVM->m_nRecursionLevel + 1 >= MAX_RECURSION_LEVELS ? 0 : s_cRunScriptReturnValue[pVM->m_nRecursionLevel + 1].m_nType;
    }

    bool GetRunScriptReturnValueInteger(int32_t* pInteger)
    {
        auto* pVM = Globals::VirtualMachine();
        if (pVM->m_nRecursionLevel + 1 >= MAX_RECURSION_LEVELS ||
            s_cRunScriptReturnValue[pVM->m_nRecursionLevel + 1].m_nType != Constants::VMAuxCodeType::Integer)
        {
            return false;
        }
        *pInteger = s_cRunScriptReturnValue[pVM->m_nRecursionLevel + 1].m_nStackInt;
        return true;
    }

    bool GetRunScriptReturnValueFloat(float* pFloat)
    {
        auto* pVM = Globals::VirtualMachine();
        if (pVM->m_nRecursionLevel + 1 >= MAX_RECURSION_LEVELS ||
            s_cRunScriptReturnValue[pVM->m_nRecursionLevel + 1].m_nType != Constants::VMAuxCodeType::Float)
        {
            return false;
        }
        *pFloat = s_cRunScriptReturnValue[pVM->m_nRecursionLevel + 1].m_fStackFloat;
        return true;
    }

    bool GetRunScriptReturnValueString(CExoString* pString)
    {
        auto* pVM = Globals::VirtualMachine();
        if (pVM->m_nRecursionLevel + 1 >= MAX_RECURSION_LEVELS ||
            s_cRunScriptReturnValue[pVM->m_nRecursionLevel + 1].m_nType != Constants::VMAuxCodeType::String)
        {
            return false;
        }
        *pString = s_cRunScriptReturnValue[pVM->m_nRecursionLevel + 1].m_sString;
        return true;
    }

    bool GetRunScriptReturnValueObject(ObjectID* pObjectID)
    {
        auto* pVM = Globals::VirtualMachine();
        if (pVM->m_nRecursionLevel + 1 >= MAX_RECURSION_LEVELS ||
            s_cRunScriptReturnValue[pVM->m_nRecursionLevel + 1].m_nType != Constants::VMAuxCodeType::Object)
        {
            return false;
        }
        *pObjectID = s_cRunScriptReturnValue[pVM->m_nRecursionLevel + 1].m_nStackObjectID;
        return true;
    }

    bool GetRunScriptReturnValueEngineStructure(int32_t nEngineStructureType, void** pEngst)
    {
        auto* pVM = Globals::VirtualMachine();
        if (pVM->m_nRecursionLevel + 1 < MAX_RECURSION_LEVELS &&
            s_cRunScriptReturnValue[pVM->m_nRecursionLevel + 1].m_nType >= Constants::VMAuxCodeType::EngSt0 &&
            s_cRunScriptReturnValue[pVM->m_nRecursionLevel + 1].m_nType <= Constants::VMAuxCodeType::EngSt9)
        {
            if (s_cRunScriptReturnValue[pVM->m_nRecursionLevel + 1].m_nType == nEngineStructureType)
            {
                *pEngst = s_cRunScriptReturnValue[pVM->m_nRecursionLevel + 1].m_pStackPtr;
                return true;
            }
        }
        return false;
    }
}

uint8_t CVirtualMachine::GetRunScriptReturnValueType()
{
    return VM::ReturnTypeExtension::GetRunScriptReturnValueType();
}

bool CVirtualMachine::GetRunScriptReturnValueInteger(int32_t* pInteger)
{
    return VM::ReturnTypeExtension::GetRunScriptReturnValueInteger(pInteger);
}

bool CVirtualMachine::GetRunScriptReturnValueFloat(float* pFloat)
{
    return VM::ReturnTypeExtension::GetRunScriptReturnValueFloat(pFloat);
}

bool CVirtualMachine::GetRunScriptReturnValueString(CExoString* pString)
{
    return VM::ReturnTypeExtension::GetRunScriptReturnValueString(pString);
}

bool CVirtualMachine::GetRunScriptReturnValueObject(ObjectID* pObjectID)
{
    return VM::ReturnTypeExtension::GetRunScriptReturnValueObject(pObjectID);
}

bool CVirtualMachine::GetRunScriptReturnValueEngineStructure(int32_t nEngineStructureType, void** pEngst)
{
    return VM::ReturnTypeExtension::GetRunScriptReturnValueEngineStructure(nEngineStructureType, pEngst);
}
