#include "nwnx.hpp"
#include "API/CScriptCompiler.hpp"
#include "API/CScriptParseTreeNode.hpp"
#include "API/CScriptCompilerIdListEntry.hpp"

using namespace NWNXLib;
using namespace NWNXLib::API;

namespace NWNXLib::VM::CustomMagicConstants
{
    static constexpr const char* MAGIC_CONSTANT_SENTINEL_FUNCTION = "\x1FNWNX_MAGIC_CONSTANT_TYPE::_FUNCTION_::CA75CA75";
    static constexpr const char* MAGIC_CONSTANT_SENTINEL_FILE = "\x1FNWNX_MAGIC_CONSTANT_TYPE::_FILE_::CA75CA75";
    static constexpr const char* MAGIC_CONSTANT_SENTINEL_ORIGIN = "\x1FNWNX_MAGIC_CONSTANT_TYPE::_ORIGIN_::CA75CA75";
    static constexpr int32_t MAGIC_CONSTANT_SENTINEL_LINE = INT32_MIN + 0xCA75;
    static constexpr int32_t MAGIC_CONSTANT_SENTINEL_FUNCTIONHASH = INT32_MIN + 0xCA75 + 1;

    #define CSCRIPTCOMPILER_OPERATION_ACTION 13
    #define CSCRIPTCOMPILER_OPERATION_CONSTANT_INTEGER 17
    #define CSCRIPTCOMPILER_OPERATION_CONSTANT_STRING 19
    #define CSCRIPTCOMPILER_TOKEN_KEYWORD_INT 29
    #define CSCRIPTCOMPILER_TOKEN_KEYWORD_STRING 31
    #define CSCRIPTCOMPILER_TOKEN_INTEGER_IDENTIFIER 34
    #define CSCRIPTCOMPILER_TOKEN_STRING_IDENTIFIER 36

    struct PatchedMagicConstantDefault
    {
        enum Type { Int, String, };
        CScriptCompilerIdListEntry* pEntry = nullptr;
        int32_t nParameter = -1;
        Type nType;
        int32_t nOldInt = 0;
        CExoString sOldString;
    };


    CExoString GetCallSiteFile(CScriptCompiler* pThis, CScriptParseTreeNode* pNode)
    {
        if (pNode && pNode->m_nFileReference >= 0)
            return *pThis->m_ppsParseTreeFileNames[pNode->m_nFileReference];
        return pThis->m_pcIncludeFileStack[pThis->m_nCompileFileLevel].m_sCompiledScriptName;
    }

    void PatchCustomMagicConstants(CScriptCompiler* pThis, int32_t nStartIdentifier)
    {
        for (int32_t i = nStartIdentifier; i < pThis->m_nOccupiedIdentifiers; ++i)
        {
            CScriptCompilerIdListEntry& entry = pThis->m_pcIdentifierList[i];

            if (entry.m_psIdentifier == "_FUNCTION_" && entry.m_nReturnType == CSCRIPTCOMPILER_TOKEN_STRING_IDENTIFIER)
            {
                entry.m_psStringData = MAGIC_CONSTANT_SENTINEL_FUNCTION;
            }
            else if (entry.m_psIdentifier == "_FILE_" && entry.m_nReturnType == CSCRIPTCOMPILER_TOKEN_STRING_IDENTIFIER)
            {
                entry.m_psStringData = MAGIC_CONSTANT_SENTINEL_FILE;
            }
            else if (entry.m_psIdentifier == "_ORIGIN_" && entry.m_nReturnType == CSCRIPTCOMPILER_TOKEN_STRING_IDENTIFIER)
            {
                entry.m_psStringData = MAGIC_CONSTANT_SENTINEL_ORIGIN;
            }
            else if (entry.m_psIdentifier == "_LINE_" && entry.m_nReturnType == CSCRIPTCOMPILER_TOKEN_INTEGER_IDENTIFIER)
            {
                entry.m_nIntegerData = MAGIC_CONSTANT_SENTINEL_LINE;
                entry.m_psStringData.Format("%d", MAGIC_CONSTANT_SENTINEL_LINE);
            }
            else if (entry.m_psIdentifier == "_FUNCTIONHASH_" && entry.m_nReturnType == CSCRIPTCOMPILER_TOKEN_INTEGER_IDENTIFIER)
            {
                entry.m_nIntegerData = MAGIC_CONSTANT_SENTINEL_FUNCTIONHASH;
                entry.m_psStringData.Format("%d", MAGIC_CONSTANT_SENTINEL_FUNCTIONHASH);
            }
        }
    }

    void PatchCustomMagicConstantNode(CScriptCompiler* pThis, CScriptParseTreeNode* pNode)
    {
        if (!pThis || !pNode)
            return;

        if (pNode->nOperation == CSCRIPTCOMPILER_OPERATION_CONSTANT_STRING && pNode->m_psStringData)
        {
            if (strcmp(pNode->m_psStringData->CStr(), MAGIC_CONSTANT_SENTINEL_FUNCTION) == 0)
            {
                *pNode->m_psStringData = pThis->m_sFunctionImpName;
            }
            else if (strcmp(pNode->m_psStringData->CStr(), MAGIC_CONSTANT_SENTINEL_FILE) == 0)
            {
                *pNode->m_psStringData = GetCallSiteFile(pThis, pNode);
            }
            else if (strcmp(pNode->m_psStringData->CStr(), MAGIC_CONSTANT_SENTINEL_ORIGIN) == 0)
            {
                *pNode->m_psStringData = GetCallSiteFile(pThis, pNode) + ":" + pThis->m_sFunctionImpName + ":" + std::to_string(pNode->nLine);
            }
        }
        else if (pNode->nOperation == CSCRIPTCOMPILER_OPERATION_CONSTANT_INTEGER)
        {
            if (pNode->nIntegerData == MAGIC_CONSTANT_SENTINEL_LINE)
                pNode->nIntegerData = pNode->nLine;
            else if (pNode->nIntegerData == MAGIC_CONSTANT_SENTINEL_FUNCTIONHASH)
                pNode->nIntegerData = pThis->m_sFunctionImpName.GetHash();
        }
    }

    void PatchCustomMagicConstantsForAction(CScriptCompiler* pThis, CScriptParseTreeNode* pNode, std::vector<PatchedMagicConstantDefault>& vPatches)
    {
        if (!pThis || !pNode || pNode->nOperation != CSCRIPTCOMPILER_OPERATION_ACTION)
            return;
        if (!pNode->pRight || !pNode->pRight->m_psStringData)
            return;
        int32_t nIdentifier = pThis->GetIdentifierByName(*pNode->pRight->m_psStringData);
        if (nIdentifier < 0)
            return;

        CScriptCompilerIdListEntry& entry = pThis->m_pcIdentifierList[nIdentifier];

        if (entry.m_nParameters <= 0 || !entry.m_pbOptionalParameters)
            return;

        const CExoString sCallerFunction = pThis->m_sFunctionImpName;
        const CExoString sCallerFile = GetCallSiteFile(pThis, pNode);
        const CExoString sCallerOrigin = GetCallSiteFile(pThis, pNode) + ":" + pThis->m_sFunctionImpName + ":" + std::to_string(pNode->nLine);
        const int32_t nCallerLine = pNode->nLine;
        const int32_t nCallerFunctionHash = sCallerFunction.GetHash();

        for (int32_t nParam = 0; nParam < entry.m_nParameters; ++nParam)
        {
            if (!entry.m_pbOptionalParameters[nParam])
                continue;

            const int32_t nParamType = entry.m_pchParameters[nParam];

            if (nParamType == CSCRIPTCOMPILER_TOKEN_KEYWORD_STRING && entry.m_psOptionalParameterStringData)
            {
                CExoString& sDefault = entry.m_psOptionalParameterStringData[nParam];
                if (strcmp(sDefault.CStr(), MAGIC_CONSTANT_SENTINEL_FUNCTION) == 0)
                {
                    PatchedMagicConstantDefault patch;
                    patch.pEntry = &entry;
                    patch.nParameter = nParam;
                    patch.nType = PatchedMagicConstantDefault::String;
                    patch.sOldString = sDefault;

                    sDefault = sCallerFunction;
                    vPatches.push_back(patch);
                }
                else if (strcmp(sDefault.CStr(), MAGIC_CONSTANT_SENTINEL_FILE) == 0)
                {
                    PatchedMagicConstantDefault patch;
                    patch.pEntry = &entry;
                    patch.nParameter = nParam;
                    patch.nType = PatchedMagicConstantDefault::String;
                    patch.sOldString = sDefault;

                    sDefault = sCallerFile;
                    vPatches.push_back(patch);
                }
                else if (strcmp(sDefault.CStr(), MAGIC_CONSTANT_SENTINEL_ORIGIN) == 0)
                {
                    PatchedMagicConstantDefault patch;
                    patch.pEntry = &entry;
                    patch.nParameter = nParam;
                    patch.nType = PatchedMagicConstantDefault::String;
                    patch.sOldString = sDefault;

                    sDefault = sCallerOrigin;
                    vPatches.push_back(patch);
                }
            }
            else if (nParamType == CSCRIPTCOMPILER_TOKEN_KEYWORD_INT && entry.m_pnOptionalParameterIntegerData)
            {
                int32_t& nDefault = entry.m_pnOptionalParameterIntegerData[nParam];
                if (nDefault == MAGIC_CONSTANT_SENTINEL_LINE)
                {
                    PatchedMagicConstantDefault patch;
                    patch.pEntry = &entry;
                    patch.nParameter = nParam;
                    patch.nType = PatchedMagicConstantDefault::Int;
                    patch.nOldInt = nDefault;

                    nDefault = nCallerLine;
                    vPatches.push_back(patch);
                }
                else if (nDefault == MAGIC_CONSTANT_SENTINEL_FUNCTIONHASH)
                {
                    PatchedMagicConstantDefault patch;
                    patch.pEntry = &entry;
                    patch.nParameter = nParam;
                    patch.nType = PatchedMagicConstantDefault::Int;
                    patch.nOldInt = nDefault;

                    nDefault = nCallerFunctionHash;
                    vPatches.push_back(patch);
                }
            }
        }
    }

    void InitializeHooks()
    {
        static Hooks::Hook s_GenerateIdentifiersFromConstantVariablesHook =
        Hooks::HookFunction(&CScriptCompiler::GenerateIdentifiersFromConstantVariables,
        +[](CScriptCompiler* pThis, CScriptParseTreeNode* pNode) -> int32_t
        {
            const int32_t nStartIdentifier = pThis->m_nOccupiedIdentifiers;
            int32_t retVal = s_GenerateIdentifiersFromConstantVariablesHook->CallOriginal<int32_t>(pThis, pNode);

            if (retVal >= 0)
                PatchCustomMagicConstants(pThis, nStartIdentifier);

            return retVal;
        }, Hooks::Order::Early);

        static Hooks::Hook s_PreVisitGenerateCodeHook = Hooks::HookFunction(&CScriptCompiler::PreVisitGenerateCode,
        +[](CScriptCompiler* pThis, CScriptParseTreeNode* pNode) -> int32_t
        {
            PatchCustomMagicConstantNode(pThis, pNode);

            if (!pNode || pNode->nOperation != CSCRIPTCOMPILER_OPERATION_ACTION)
                return s_PreVisitGenerateCodeHook->CallOriginal<int32_t>(pThis, pNode);

            std::vector<PatchedMagicConstantDefault> vPatches;
            PatchCustomMagicConstantsForAction(pThis, pNode, vPatches);
            auto retVal = s_PreVisitGenerateCodeHook->CallOriginal<int32_t>(pThis, pNode);

            for (const auto& patch : vPatches)
            {
                if (patch.nType == PatchedMagicConstantDefault::String)
                    patch.pEntry->m_psOptionalParameterStringData[patch.nParameter] = patch.sOldString;
                else
                    patch.pEntry->m_pnOptionalParameterIntegerData[patch.nParameter] = patch.nOldInt;
            }

            return retVal;
        }, Hooks::Order::Early);
    }
}
