#pragma once
#include <windows.h>
#include <ImageHlp.h>
#include <stddef.h>

#ifndef IATHOOKING_H
#define IATHOOKING_H

typedef struct {
    const char* funcName;
    FARPROC replacement;
} DGI_IAT_Hook;

void HookIATForModule(HMODULE hModule, const char* targetDLL, DGI_IAT_Hook hooks[], size_t hookCount);

#endif