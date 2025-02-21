// DllMain.cpp
#include "pch.h"
#include <windows.h>
#include <stdio.h>          // Added for sprintf_s and vsprintf_s
#include <stdarg.h>         // Added for va_list usage with vsprintf_s
#include "SDLThread.h"
#include "IATHooking.h"


// In the GDI HOOKING DLL project
extern "C" __declspec(dllexport) bool __stdcall IsHooked();
// Hooked GDI functions (with proper annotation)
extern "C" _Must_inspect_result_ NTSTATUS __stdcall HookedNtGdiBitBlt(
    _In_ HDC hdcDest, _In_ int x, _In_ int y, _In_ int cx, _In_ int cy,
    _In_ HDC hdcSrc, _In_ int x1, _In_ int y1, _In_ DWORD rop);

extern "C" _Must_inspect_result_ NTSTATUS __stdcall HookedNtGdiStretchBlt(
    _In_ HDC hdcDest, _In_ int x, _In_ int y, _In_ int cx, _In_ int cy,
    _In_ HDC hdcSrc, _In_ int x1, _In_ int y1, _In_ int cx1, _In_ int cy1, _In_ DWORD rop);

extern "C" _Must_inspect_result_ NTSTATUS __stdcall HookedNtGdiPatBlt(
    _In_ HDC hdcDest, _In_ int x, _In_ int y, _In_ int cx, _In_ int cy, _In_ DWORD rop);

static HANDLE hSDLThread = NULL;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        // Prevent loading multiple times
        DisableThreadLibraryCalls(hModule);
        break;
    case DLL_PROCESS_DETACH:
        // Ensure SDL thread is properly terminated
        gSDLRunning = false;
        if (hSDLThread)
        {
            WaitForSingleObject(hSDLThread, INFINITE);  // Wait for the SDL thread to finish
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

    // Set up the IAT hook for the GDI functions
    static DGI_IAT_Hook g_IATHooks[] = {
        { "NtGdiBitBlt",     (FARPROC)HookedNtGdiBitBlt },
        { "NtGdiStretchBlt", (FARPROC)HookedNtGdiStretchBlt },
        { "NtGdiPatBlt",     (FARPROC)HookedNtGdiPatBlt }
    };

    // Retrieve the handle for the host module
    HMODULE hHostModule = GetModuleHandle(NULL);
    if (!hHostModule)
    {
        OutputDebugStringA("[DGI HOOK] ERROR: Could not retrieve host module handle.\n");
        MessageBoxA(NULL, "DGI hooking failed: Could not retrieve host module handle.", "Error", MB_ICONERROR);
        return;
    }

    // Hook the IAT for the specified module (win32u.dll)
    HookIATForModule(hHostModule, "win32u.dll", g_IATHooks, sizeof(g_IATHooks) / sizeof(DGI_IAT_Hook));
    OutputDebugStringA("[DGI HOOK] IAT hooking succeeded.\n");

    // Create a thread for SDL
    hSDLThread = CreateThread(NULL, 0, SDLThread, NULL, 0, NULL);
    if (!hSDLThread)
    {
        OutputDebugStringA("[DGI HOOK] ERROR: Could not create SDL thread.\n");
        MessageBoxA(NULL, "DGI hooking failed: Could not create SDL thread.", "Error", MB_ICONERROR);
        return;
    }
    OutputDebugStringA("[DGI HOOK] SDL thread created successfully.\n");
}