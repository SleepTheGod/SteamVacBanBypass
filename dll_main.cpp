#include <windows.h>
#include "utils.h"  // Includes pattern search and hook import functions
#include "hooks.h"  // Includes the hook function declarations

// Utility function to find a byte pattern in a module
PVOID Utils_findPattern(PCWSTR moduleName, PCSTR pattern, SIZE_T patternSize) {
    HMODULE hModule = GetModuleHandleW(moduleName);
    if (!hModule) return NULL;

    // Fetch the module base address and size
    MODULEINFO moduleInfo;
    GetModuleInformation(GetCurrentProcess(), hModule, &moduleInfo, sizeof(moduleInfo));

    // Scan through the module memory to find the pattern
    for (PBYTE pAddress = (PBYTE)moduleInfo.lpBaseOfDll;
         pAddress < (PBYTE)moduleInfo.lpBaseOfDll + moduleInfo.SizeOfImage - patternSize;
         ++pAddress) {
        if (!memcmp(pAddress, pattern, patternSize)) {
            return pAddress;
        }
    }
    return NULL;
}

// Utility function to hook an import
VOID Utils_hookImport(PCWSTR moduleName, PCSTR libraryName, PCSTR functionName, PVOID newFunction) {
    HMODULE hModule = moduleName ? GetModuleHandleW(moduleName) : GetModuleHandleW(NULL);
    if (!hModule) return;

    // Fetch the address of the import address table (IAT)
    ULONG size;
    PIMAGE_IMPORT_DESCRIPTOR pImportDesc = (PIMAGE_IMPORT_DESCRIPTOR)ImageDirectoryEntryToDataEx(
        hModule, TRUE, IMAGE_DIRECTORY_ENTRY_IMPORT, &size, NULL);

    if (!pImportDesc) return;

    // Iterate over import descriptors to find the target library
    for (; pImportDesc->Name; pImportDesc++) {
        PCSTR libName = (PCSTR)((PBYTE)hModule + pImportDesc->Name);
        if (!_stricmp(libName, libraryName)) {
            PIMAGE_THUNK_DATA pThunk = (PIMAGE_THUNK_DATA)((PBYTE)hModule + pImportDesc->FirstThunk);
            for (; pThunk->u1.Function; pThunk++) {
                PROC* ppfn = (PROC*)&pThunk->u1.Function;
                PROC pfn = *ppfn;
                if (pfn == GetProcAddress(GetModuleHandleA(libraryName), functionName)) {
                    DWORD oldProtect;
                    VirtualProtect(ppfn, sizeof(PROC), PAGE_EXECUTE_READWRITE, &oldProtect);
                    *ppfn = (PROC)newFunction;
                    VirtualProtect(ppfn, sizeof(PROC), oldProtect, &oldProtect);
                    break;
                }
            }
            break;
        }
    }
}

// DllMain entry function
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);

            // Check if "steamservice" module is present
            if (GetModuleHandleW(L"steamservice"))
            {
                // Attempt to find the pattern in the module
                PBYTE toPatch = (PBYTE)Utils_findPattern(L"steamservice", "\x74\x47\x6A\x01\x6A", 0);
                
                if (toPatch)
                {
                    // Modify memory permissions to allow patching
                    DWORD oldProtect;
                    if (VirtualProtect(toPatch, 1, PAGE_EXECUTE_READWRITE, &oldProtect))
                    {
                        *toPatch = 0xEB; // Patch the byte to bypass VAC
                        VirtualProtect(toPatch, 1, oldProtect, &oldProtect); // Restore protection
                    }
                    else
                    {
                        MessageBoxW(NULL, L"Failed to change memory protection.", L"VAC bypass", MB_OK | MB_ICONERROR);
                        return FALSE;
                    }

                    // Hook the LoadLibraryExW function
                    if (!Utils_hookImport(L"steamservice", "kernel32.dll", "LoadLibraryExW", Hooks_LoadLibraryExW))
                    {
                        MessageBoxW(NULL, L"Failed to hook LoadLibraryExW.", L"VAC bypass", MB_OK | MB_ICONERROR);
                        return FALSE;
                    }

                    MessageBoxW(NULL, L"Initialization was successful!", L"VAC bypass", MB_OK | MB_ICONINFORMATION);
                }
                else
                {
                    MessageBoxW(NULL, L"Pattern not found in steamservice.", L"VAC bypass", MB_OK | MB_ICONERROR);
                    return FALSE;
                }
            }
            else
            {
                // If "steamservice" isn't loaded, hook LoadLibraryExW globally
                if (!Utils_hookImport(NULL, "kernel32.dll", "LoadLibraryExW", Hooks_LoadLibraryExW_SteamClient))
                {
                    MessageBoxW(NULL, L"Failed to hook LoadLibraryExW globally.", L"VAC bypass", MB_OK | MB_ICONERROR);
                    return FALSE;
                }
            }
            break;

        case DLL_PROCESS_DETACH:
            // Cleanup if necessary
            break;
    }
    
    return TRUE;
}
