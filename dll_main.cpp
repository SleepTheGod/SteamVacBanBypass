#include <windows.h>
#include "utils.h"
#include "hooks.h"

// DllMain entry function
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            Utils_logMessage("[INFO] DLL_PROCESS_ATTACH triggered");

            // Log loaded modules
            std::vector<std::string> modules = Utils_getLoadedModules();
            for (const auto& module : modules) {
                Utils_logMessage("[INFO] Loaded module: %s", module.c_str());
            }

            // Check if "steamservice" is loaded
            if (GetModuleHandleW(L"steamservice"))
            {
                Utils_logMessage("[INFO] steamservice module found");
                
                PBYTE toPatch = (PBYTE)Utils_findPattern(L"steamservice", "\x74\x47\x6A\x01\x6A", 5);
                if (toPatch)
                {
                    DWORD oldProtect;
                    if (VirtualProtect(toPatch, 1, PAGE_EXECUTE_READWRITE, &oldProtect))
                    {
                        *toPatch = 0xEB;  // Patch the byte to bypass VAC
                        VirtualProtect(toPatch, 1, oldProtect, &oldProtect);
                        Utils_logMessage("[INFO] VAC bypass patch applied");
                    }
                    else
                    {
                        Utils_logMessage("[ERROR] Failed to change memory protection");
                        return FALSE;
                    }

                    if (!Utils_hookImport(L"steamservice", "kernel32.dll", "LoadLibraryExW", Hooks_LoadLibraryExW))
                    {
                        Utils_logMessage("[ERROR] Failed to hook LoadLibraryExW");
                        return FALSE;
                    }

                    MessageBoxW(NULL, L"Initialization was successful!", L"VAC bypass", MB_OK | MB_ICONINFORMATION);
                }
                else
                {
                    Utils_logMessage("[ERROR] Pattern not found in steamservice");
                    return FALSE;
                }
            }
            else
            {
                Utils_logMessage("[INFO] steamservice not loaded, hooking globally");
                if (!Utils_hookImport(NULL, "kernel32.dll", "LoadLibraryExW", Hooks_LoadLibraryExW_SteamClient))
                {
                    Utils_logMessage("[ERROR] Failed to hook LoadLibraryExW globally");
                    return FALSE;
                }
            }
            break;

        case DLL_PROCESS_DETACH:
            Utils_logMessage("[INFO] DLL_PROCESS_DETACH triggered");
            // Cleanup if necessary
            break;
    }
    
    return TRUE;
}
