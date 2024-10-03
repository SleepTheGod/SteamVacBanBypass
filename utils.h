#pragma once

#include <Windows.h>

// Utility function declarations
PVOID Utils_findPattern(PCWSTR moduleName, PCSTR pattern, SIZE_T patternSize);
VOID Utils_hookImport(PCWSTR moduleName, PCSTR libraryName, PCSTR functionName, PVOID newFunction);
