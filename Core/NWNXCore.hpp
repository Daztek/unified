#pragma once

#include "nwnx.hpp"
#include "API/CServerExoAppInternal.hpp"
#include "API/CNWSVirtualMachineCommands.hpp"

#include <functional>
#include <map>
#include <memory>

namespace Core {

class NWNXCore
{
public:
    NWNXCore();
    ~NWNXCore();

    const std::vector<std::string>& GetCustomResourceDirectoryAliases() const { return m_CustomResourceDirectoryAliases; }

private:
    NWNXLib::Hooks::Hook m_createServerHook;
    NWNXLib::Hooks::Hook m_destroyServerHook;
    NWNXLib::Hooks::Hook m_mainLoopInternalHook;
    NWNXLib::Hooks::Hook m_vmPlaySoundHook;
    NWNXLib::Hooks::Hook m_nwnxFunctionManagementHook;

    void CleanupPreload();
    void ConfigureLogLevel(const std::string& plugin);

    void InitialSetupHooks();
    void InitialVersionCheck();
    void InitialSetupPlugins();
    void InitialSetupResourceDirectories();
    void InitialSetupCommands();

    void Shutdown();

    static void CreateServerHandler(CAppManager*);
    static void DestroyServerHandler(CAppManager*);
    static int32_t MainLoopInternalHandler(CServerExoAppInternal*);
    static int32_t PlaySoundHandler(CNWSVirtualMachineCommands*, int32_t, int32_t);
    static int32_t NWNXFunctionManagementHandler(CNWSVirtualMachineCommands*, int32_t, int32_t);

    int m_ScriptChunkRecursion;
    std::vector<std::string> m_CustomResourceDirectoryAliases;
};

}
