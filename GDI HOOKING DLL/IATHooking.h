#pragma once

#include <windows.h>
#include <ImageHlp.h>  // Required for ImageDirectoryEntryToData
#include <stddef.h>    // For size_t

// Structure for an IAT hook entry.
struct DGI_IAT_Hook {
    const char* funcName;   // Name of the function to hook
    FARPROC replacement;    // Pointer to our replacement function
};

// Function prototype for IAT hooking.
void HookIATForModule(HMODULE hModule, const char* targetDLL, DGI_IAT_Hook hooks[], size_t hookCount);
