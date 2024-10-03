#pragma once

#include <Windows.h>
#include <string>
#include <vector>

// Advanced utility function declarations
PVOID Utils_findPattern(PCWSTR moduleName, PCSTR pattern, SIZE_T patternSize);
VOID Utils_hookImport(PCWSTR moduleName, PCSTR libraryName, PCSTR functionName, PVOID newFunction);
std::vector<std::string> Utils_getLoadedModules();
void Utils_logMessage(const char* format, ...);
