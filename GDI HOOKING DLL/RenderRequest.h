// RenderRequest.h
#pragma once

#include <windows.h>
#include <SDL.h>

struct RenderRequest {
    SDL_Rect destRect;
    HBITMAP hBitmap;
    BITMAPINFO bmi;
    bool isValid;

    RenderRequest()
        : destRect({ 0, 0, 0, 0 }),   // Initialize SDL_Rect with zero values
        hBitmap(nullptr),           // Initialize HBITMAP to nullptr
        bmi({ 0 }),                    // Zero-initialize BITMAPINFO
        isValid(false)              // Initialize isValid to false
    {
    }

    RenderRequest(const RenderRequest&) = delete;
    RenderRequest& operator=(const RenderRequest&) = delete;
    RenderRequest(RenderRequest&&) = default;
    RenderRequest& operator=(RenderRequest&&) = default;
};
