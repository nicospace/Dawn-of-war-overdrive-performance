// IATHooking.cpp
#include "pch.h"           // Precompiled header (if you use one; must be first)
#include "IATHooking.h"    // Include our IAT hook declarations
#include <cstring>         // For _stricmp

// This function hooks all functions in the IAT for the specified targetDLL.
void HookIATForModule(HMODULE hModule, const char* targetDLL, DGI_IAT_Hook hooks[], size_t hookCount)
{
    if (!hModule)
        return;

    ULONG size = 0;
    // ImageDirectoryEntryToData is defined in ImageHlp.h (make sure your project includes the SDL and Windows SDK include paths)
    PIMAGE_IMPORT_DESCRIPTOR importDesc = (PIMAGE_IMPORT_DESCRIPTOR)
        ImageDirectoryEntryToData(hModule, TRUE, IMAGE_DIRECTORY_ENTRY_IMPORT, &size);
    if (!importDesc)
        return;

    // Iterate over each import descriptor.
    for (; importDesc->Name; importDesc++) {
        const char* dllName = (const char*)((PBYTE)hModule + importDesc->Name);
        // Compare the DLL name (case-insensitive) with our target DLL.
        if (_stricmp(dllName, targetDLL) != 0)
            continue;

        // Get the IAT (FirstThunk) for this descriptor.
        PIMAGE_THUNK_DATA thunk = (PIMAGE_THUNK_DATA)((PBYTE)hModule + importDesc->FirstThunk);
        for (; thunk->u1.Function; thunk++) {
            FARPROC* funcAddr = (FARPROC*)&thunk->u1.Function;
            // Check if this is an import by name.
            if (!(thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG)) {
                PIMAGE_IMPORT_BY_NAME importByName = (PIMAGE_IMPORT_BY_NAME)((PBYTE)hModule + thunk->u1.AddressOfData);
                if (!importByName)
                    continue;
                const char* funcName = (const char*)importByName->Name;
                // Loop through our hooks array.
                for (size_t i = 0; i < hookCount; i++) {
                    if (_stricmp(funcName, hooks[i].funcName) == 0) {
                        DWORD oldProtect;
                        if (VirtualProtect(funcAddr, sizeof(FARPROC), PAGE_EXECUTE_READWRITE, &oldProtect)) {
                            *funcAddr = hooks[i].replacement;
                            VirtualProtect(funcAddr, sizeof(FARPROC), oldProtect, &oldProtect);
                        }
                    }
                }
            }
        }
    }
}
