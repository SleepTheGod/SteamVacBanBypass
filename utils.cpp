#include <Windows.h>
#include <Psapi.h>
#include <stdarg.h>
#include <vector>
#include <string>
#include <fstream>
#include "utils.h"

// Logging to file for debugging
void Utils_logMessage(const char* format, ...)
{
    static std::ofstream logFile("hook_log.txt", std::ios::app);

    va_list args;
    va_start(args, format);

    char buffer[256];
    vsprintf_s(buffer, format, args);

    logFile << buffer << std::endl;

    va_end(args);
}

// Function to find a byte pattern in a module
PVOID Utils_findPattern(PCWSTR moduleName, PCSTR pattern, SIZE_T patternSize)
{
    HMODULE hModule = GetModuleHandleW(moduleName);
    if (!hModule) {
        Utils_logMessage("[ERROR] Module %ws not found", moduleName);
        return NULL;
    }

    MODULEINFO moduleInfo;
    GetModuleInformation(GetCurrentProcess(), hModule, &moduleInfo, sizeof(moduleInfo));

    for (PBYTE pAddress = (PBYTE)moduleInfo.lpBaseOfDll;
         pAddress < (PBYTE)moduleInfo.lpBaseOfDll + moduleInfo.SizeOfImage - patternSize;
         ++pAddress) {
        if (!memcmp(pAddress, pattern, patternSize)) {
            Utils_logMessage("[INFO] Pattern found in module %ws", moduleName);
            return pAddress;
        }
    }

    Utils_logMessage("[ERROR] Pattern not found in module %ws", moduleName);
    return NULL;
}

// Function to hook import table entries
VOID Utils_hookImport(PCWSTR moduleName, PCSTR libraryName, PCSTR functionName, PVOID newFunction)
{
    HMODULE hModule = moduleName ? GetModuleHandleW(moduleName) : GetModuleHandleW(NULL);
    if (!hModule) return;

    ULONG size;
    PIMAGE_IMPORT_DESCRIPTOR pImportDesc = (PIMAGE_IMPORT_DESCRIPTOR)ImageDirectoryEntryToDataEx(
        hModule, TRUE, IMAGE_DIRECTORY_ENTRY_IMPORT, &size, NULL);

    if (!pImportDesc) {
        Utils_logMessage("[ERROR] Import table not found in module %ws", moduleName);
        return;
    }

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
                    Utils_logMessage("[INFO] Hooked %s in %ws", functionName, moduleName);
                    break;
                }
            }
            break;
        }
    }
}

// Function to get all loaded modules (debugging utility)
std::vector<std::string> Utils_getLoadedModules()
{
    std::vector<std::string> modules;
    HMODULE hModules[1024];
    DWORD cbNeeded;
    HANDLE hProcess = GetCurrentProcess();

    if (EnumProcessModules(hProcess, hModules, sizeof(hModules), &cbNeeded)) {
        for (int i = 0; i < (cbNeeded / sizeof(HMODULE)); i++) {
            char szModule[MAX_PATH];
            if (GetModuleFileNameExA(hProcess, hModules[i], szModule, sizeof(szModule) / sizeof(char))) {
                modules.push_back(szModule);
            }
        }
    }
    return modules;
}
