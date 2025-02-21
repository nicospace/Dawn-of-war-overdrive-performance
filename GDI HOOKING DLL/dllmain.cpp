#include "pch.h"
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include "SDLThread.h"
#include "IATHooking.h"
#include <SDL3/SDL.h>

// Define the global variable that was incomplete

#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Imagehlp.lib")

// Define STATUS_SUCCESS if not already defined
#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#endif

// Function pointer type definitions
typedef NTSTATUS(__stdcall* pNtGdiBitBlt)(
    HDC hdcDest,
    int x,
    int y,
    int cx,
    int cy,
    HDC hdcSrc,
    int x1,
    int y1,
    DWORD rop
    );

typedef NTSTATUS(__stdcall* pNtGdiStretchBlt)(
    HDC hdcDest,
    int x,
    int y,
    int cx,
    int cy,
    HDC hdcSrc,
    int x1,
    int y1,
    int cx1,
    int cy1,
    DWORD rop
    );

typedef NTSTATUS(__stdcall* pNtGdiPatBlt)(
    HDC hdcDest,
    int x,
    int y,
    int cx,
    int cy,
    DWORD rop
    );

// Global variables to store original function addresses
pNtGdiBitBlt OriginalNtGdiBitBlt = nullptr;
pNtGdiStretchBlt OriginalNtGdiStretchBlt = nullptr;
pNtGdiPatBlt OriginalNtGdiPatBlt = nullptr;

// Hooked function implementations
extern "C" _Must_inspect_result_ NTSTATUS __stdcall HookedNtGdiBitBlt(
    _In_ HDC hdcDest,
    _In_ int x,
    _In_ int y,
    _In_ int cx,
    _In_ int cy,
    _In_ HDC hdcSrc,
    _In_ int x1,
    _In_ int y1,
    _In_ DWORD rop)
{
    return OriginalNtGdiBitBlt ?
        OriginalNtGdiBitBlt(hdcDest, x, y, cx, cy, hdcSrc, x1, y1, rop) :
        STATUS_SUCCESS;
}

extern "C" _Must_inspect_result_ NTSTATUS __stdcall HookedNtGdiStretchBlt(
    _In_ HDC hdcDest,
    _In_ int x,
    _In_ int y,
    _In_ int cx,
    _In_ int cy,
    _In_ HDC hdcSrc,
    _In_ int x1,
    _In_ int y1,
    _In_ int cx1,
    _In_ int cy1,
    _In_ DWORD rop)
{
    return OriginalNtGdiStretchBlt ?
        OriginalNtGdiStretchBlt(hdcDest, x, y, cx, cy, hdcSrc, x1, y1, cx1, cy1, rop) :
        STATUS_SUCCESS;
}

extern "C" _Must_inspect_result_ NTSTATUS __stdcall HookedNtGdiPatBlt(
    _In_ HDC hdcDest,
    _In_ int x,
    _In_ int y,
    _In_ int cx,
    _In_ int cy,
    _In_ DWORD rop)
{
    return OriginalNtGdiPatBlt ?
        OriginalNtGdiPatBlt(hdcDest, x, y, cx, cy, rop) :
        STATUS_SUCCESS;
}

static HANDLE hSDLThread = NULL;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        break;
    case DLL_PROCESS_DETACH:
        gSDLRunning = false;
        if (hSDLThread)
        {
            WaitForSingleObject(hSDLThread, INFINITE);
            CloseHandle(hSDLThread);
            hSDLThread = NULL;
        }
        break;
    }
    return TRUE;
}

extern "C" __declspec(dllexport) void InitializeDGIHooking()
{
    OutputDebugStringA("[DGI HOOK] InitializeDGIHooking() called.\n");

    // Using DGI_IAT_Hook structure from IATHooking.h
    DGI_IAT_Hook g_IATHooks[] = {
        { "NtGdiBitBlt",     (FARPROC)HookedNtGdiBitBlt },
        { "NtGdiStretchBlt", (FARPROC)HookedNtGdiStretchBlt },
        { "NtGdiPatBlt",     (FARPROC)HookedNtGdiPatBlt }
    };

    HMODULE hHostModule = GetModuleHandle(NULL);
    if (!hHostModule)
    {
        OutputDebugStringA("[DGI HOOK] ERROR: Could not retrieve host module handle.\n");
        MessageBoxA(NULL, "DGI hooking failed: Could not retrieve host module handle.", "Error", MB_ICONERROR);
        return;
    }

    HookIATForModule(hHostModule, "win32u.dll", g_IATHooks, sizeof(g_IATHooks) / sizeof(DGI_IAT_Hook));
    OutputDebugStringA("[DGI HOOK] IAT hooking succeeded.\n");

    hSDLThread = CreateThread(NULL, 0, SDLThread, NULL, 0, NULL);
    if (!hSDLThread)
    {
        OutputDebugStringA("[DGI HOOK] ERROR: Could not create SDL thread.\n");
        MessageBoxA(NULL, "DGI hooking failed: Could not create SDL thread.", "Error", MB_ICONERROR);
        return;
    }
    OutputDebugStringA("[DGI HOOK] SDL thread created successfully.\n");
}