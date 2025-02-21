#include "pch.h"
#include "IATHooking.h"
#include <ImageHlp.h>

void HookIATForModule(HMODULE hModule, const char* targetDLL, DGI_IAT_Hook hooks[], size_t hookCount) {
    ULONG size;
    PIMAGE_IMPORT_DESCRIPTOR importDesc = (PIMAGE_IMPORT_DESCRIPTOR)ImageDirectoryEntryToData(
        hModule, TRUE, IMAGE_DIRECTORY_ENTRY_IMPORT, &size);

    if (!importDesc) {
        OutputDebugStringA("[IAT Hook] Failed to get import descriptor\n");
        return;
    }

    // Find the import descriptor for our target DLL
    while (importDesc->Name) {
        char* moduleName = (char*)((PBYTE)hModule + importDesc->Name);
        if (_stricmp(moduleName, targetDLL) == 0) {
            break;
        }
        importDesc++;
    }

    if (!importDesc->Name) {
        OutputDebugStringA("[IAT Hook] Target DLL not found in imports\n");
        return;
    }

    // Get the IAT
    PIMAGE_THUNK_DATA originalFirstThunk = (PIMAGE_THUNK_DATA)((PBYTE)hModule + importDesc->OriginalFirstThunk);
    PIMAGE_THUNK_DATA firstThunk = (PIMAGE_THUNK_DATA)((PBYTE)hModule + importDesc->FirstThunk);

    // Iterate through the IAT
    while (originalFirstThunk->u1.AddressOfData) {
        PIMAGE_IMPORT_BY_NAME functionName = (PIMAGE_IMPORT_BY_NAME)((PBYTE)hModule + originalFirstThunk->u1.AddressOfData);

        // Check each hook
        for (size_t i = 0; i < hookCount; i++) {
            if (strcmp((char*)functionName->Name, hooks[i].funcName) == 0) {
                DWORD oldProtect;
                VirtualProtect(&firstThunk->u1.Function, sizeof(PVOID), PAGE_READWRITE, &oldProtect);
                firstThunk->u1.Function = (ULONGLONG)hooks[i].replacement;
                VirtualProtect(&firstThunk->u1.Function, sizeof(PVOID), oldProtect, &oldProtect);
                break;
            }
        }

        originalFirstThunk++;
        firstThunk++;
    }
}