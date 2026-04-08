#include "nwnx.hpp"
#include "API/CVirtualMachine.hpp"
#include "API/CNWSVirtualMachineCommands.hpp"
#include "API/CScriptCompiler.hpp"
#include "API/CExoResMan.hpp"
#include "API/CTlkTable.hpp"


using namespace NWNXLib;
using namespace NWNXLib::API;

namespace NWNXLib::VM::FixCompileScriptNDBGeneration
{
    static CExoString s_OutputName = "!Chunk";

    void InitializeHooks()
    {
        static Hooks::Hook s_CompileScriptChunkHook = Hooks::HookFunction(&CScriptCompiler::CompileScriptChunk,
        +[](CScriptCompiler *pThis, const CExoString &sScriptChunk, BOOL bWrapIntoMain) -> int32_t
        {
            INSTR_SCOPE();
            INSTR_SCOPE_TEXT(sScriptChunk.CStr(), sScriptChunk.GetLength());

            pThis->Initialize();

            if (pThis->m_nCompileFileLevel != 0)
                return -605;

            pThis->m_pcIncludeFileStack[pThis->m_nCompileFileLevel].m_sCompiledScriptName = "!Chunk";

            char *pScript;
            uint32_t nScriptLength;

            if (bWrapIntoMain)
            {
                nScriptLength = sScriptChunk.GetLength() + 13;
                pScript = new char[nScriptLength];
                sprintf(pScript, "void main(){%s}", sScriptChunk.CStr());
            }
            else
            {
                nScriptLength = sScriptChunk.GetLength();
                pScript = new char[nScriptLength];
                memmove(pScript, sScriptChunk.CStr(), nScriptLength);
            }

            pThis->m_nCompileFileLevel++;
            int32_t nReturnValue = pThis->ParseSource(pScript, nScriptLength);

            if (nReturnValue < 0)
            {
                delete[] pScript;
                return nReturnValue;
            }

            pThis->m_nCompileFileLevel--;
            pThis->InitializeFinalCode();
            nReturnValue = pThis->GenerateFinalCodeFromParseTree(s_OutputName);

            if (nReturnValue < 0)
            {
                delete[] pScript;
                return nReturnValue;
            }

            pThis->FinalizeFinalCode();
            delete[] pScript;
            return 0;
        }, Hooks::Order::Final);

        static Hooks::Hook s_ExecuteCommandCompileScriptHook = Hooks::HookFunction(&CNWSVirtualMachineCommands::ExecuteCommandCompileScript,
        +[](CNWSVirtualMachineCommands*, int32_t, int32_t) -> int32_t
        {
            CExoString scriptName;
            CExoString scriptData;
            int32_t wrapIntoMain;
            int32_t generateNDB;

            auto *pVM = Globals::VirtualMachine();
            if (!pVM->StackPopString(&scriptName) ||
                !pVM->StackPopString(&scriptData) ||
                !pVM->StackPopInteger(&wrapIntoMain) ||
                !pVM->StackPopInteger(&generateNDB))
                return Constants::VMError::StackUnderflow;

            const CResRef resRef = scriptName;
            CExoString error;
            s_OutputName = scriptName;

            if (int32_t retVal = pVM->m_pJitCompiler->CompileScriptChunk(scriptData, wrapIntoMain != 0); retVal != 0)
                error = pVM->m_pJitCompiler->m_sCapturedError;
            else
            {
                retVal = pVM->m_pJitCompiler->WriteFinalCodeToFile(scriptName);
                if (retVal != 0)
                    error = Globals::TlkTable()->GetSimpleString(-retVal).CStr();
                else
                {
                    if (auto *pRes = Globals::ExoResMan()->GetResObject(resRef, Constants::ResRefType::NCS))
                        Globals::ExoResMan()->Dump(pRes);

                    if (generateNDB != 0)
                    {
                        if (auto *pRes = Globals::ExoResMan()->GetResObject(resRef, Constants::ResRefType::NDB))
                            Globals::ExoResMan()->Dump(pRes);
                    }
                }
            }

            s_OutputName = "!Chunk";

            if (!pVM->StackPushString(error))
                return Constants::VMError::StackOverflow;

            return 0;
        }, Hooks::Order::Final);
    }
}
