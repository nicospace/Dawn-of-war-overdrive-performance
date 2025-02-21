// GDIHookFunctions.cpp
#include "pch.h"
#include <windows.h>
#include "ThreadSafeQueue.h"
#include "RenderRequest.h"
#include <utility> // for std::move

// Forward declarations of globals defined in SDLThread.cpp
extern ThreadSafeQueue gRenderQueue;
extern SDL_Renderer* gRenderer;

typedef long NTSTATUS;

// Define the hooked GDI functions
extern "C" NTSTATUS __stdcall HookedNtGdiBitBlt(
    HDC hdcDest, int x, int y, int cx, int cy,
    HDC hdcSrc, int x1, int y1, DWORD rop)
{
    if (!gRenderer || !hdcSrc)
        return 0;

    // Retrieve the bitmap from the source HDC.
    HBITMAP hBitmap = (HBITMAP)GetCurrentObject(hdcSrc, OBJ_BITMAP);
    if (!hBitmap)
        return 0;

    BITMAP bmp = { 0 };
    if (!GetObject(hBitmap, sizeof(BITMAP), &bmp))
        return 0;

    // Build a render request.
    RenderRequest req;
    req.destRect = { x, y, cx, cy };

    // Prepare BITMAPINFO (using a negative height for a top–down bitmap).
    req.bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    req.bmi.bmiHeader.biWidth = bmp.bmWidth;
    req.bmi.bmiHeader.biHeight = -bmp.bmHeight;
    req.bmi.bmiHeader.biPlanes = 1;
    req.bmi.bmiHeader.biBitCount = 32;
    req.bmi.bmiHeader.biCompression = BI_RGB;
    req.hBitmap = hBitmap;
    req.isValid = true;

    // Push the request into the render queue.
    gRenderQueue.push(std::move(req));
    return 0;
}

extern "C" NTSTATUS __stdcall HookedNtGdiStretchBlt(
    HDC hdcDest, int x, int y, int cx, int cy,
    HDC hdcSrc, int x1, int y1, int cx1, int cy1, DWORD rop)
{
    // For simplicity, reuse the BitBlt hook logic.
    return HookedNtGdiBitBlt(hdcDest, x, y, cx, cy, hdcSrc, x1, y1, rop);
}

extern "C" NTSTATUS __stdcall HookedNtGdiPatBlt(
    HDC hdcDest, int x, int y, int cx, int cy, DWORD rop)
{
    // We don't process pattern blits.
    return 0;
}

// Export function to check if the hook is active
extern "C" __declspec(dllexport) bool __stdcall IsHooked() {
    return true; // This confirms the hook is active.
}
