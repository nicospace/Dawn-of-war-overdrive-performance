#include "pch.h"
#include <SDL3/SDL.h>
#include <windows.h>
#include "ThreadSafeQueue.h"
#include "RenderRequest.h"
#include "SDLThread.h"
#include <chrono>
#include <cstdlib>

// Global definitions
SDL_Renderer* gRenderer = nullptr;
SDL_Texture* gameTexture = nullptr;
ThreadSafeQueue gRenderQueue;
volatile bool gSDLRunning = true;

extern "C" __declspec(dllexport) DWORD WINAPI SDLThread(LPVOID lpParam)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        MessageBoxA(NULL, SDL_GetError(), "SDL Initialization Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "GDI to SDL Renderer",  // title
        1024,                   // width
        768,                    // height
        0                       // flags (default visible)
    );

    if (!window) {
        MessageBoxA(NULL, SDL_GetError(), "Window Creation Error", MB_OK | MB_ICONERROR);
        SDL_Quit();
        return 1;
    }

    // Fixed: Use NULL instead of -1 for default renderer
    gRenderer = SDL_CreateRenderer(
        window,                 // window
        NULL                    // renderer name (NULL for default)
    );

    if (!gRenderer) {
        MessageBoxA(NULL, SDL_GetError(), "Renderer Creation Error", MB_OK | MB_ICONERROR);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    RenderRequest request;
    while (gSDLRunning)
    {
        if (gRenderQueue.try_pop(request))
        {
            if (request.isValid)
            {
                gameTexture = SDL_CreateTexture(
                    gRenderer,
                    SDL_PIXELFORMAT_BGRA32,
                    SDL_TEXTUREACCESS_STREAMING,
                    request.bmi.bmiHeader.biWidth,
                    abs(request.bmi.bmiHeader.biHeight)
                );

                if (gameTexture)
                {
                    void* pixels;
                    int pitch;
                    if (SDL_LockTexture(gameTexture, NULL, &pixels, &pitch) == 0)
                    {
                        HDC hdcMem = CreateCompatibleDC(NULL);
                        if (hdcMem)
                        {
                            HGDIOBJ hOld = SelectObject(hdcMem, request.hBitmap);
                            GetDIBits(hdcMem,
                                request.hBitmap,
                                0,
                                abs(request.bmi.bmiHeader.biHeight),
                                pixels,
                                &request.bmi,
                                DIB_RGB_COLORS);
                            SelectObject(hdcMem, hOld);
                            DeleteDC(hdcMem);
                        }
                        SDL_UnlockTexture(gameTexture);

                        SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, 255);
                        SDL_RenderClear(gRenderer);

                        SDL_FRect destRectF = {
                            static_cast<float>(request.destRect.x),
                            static_cast<float>(request.destRect.y),
                            static_cast<float>(request.destRect.w),
                            static_cast<float>(request.destRect.h)
                        };

                        SDL_RenderTextureRotated(gRenderer, gameTexture, NULL, &destRectF, 0, NULL, SDL_FLIP_NONE);
                        SDL_RenderPresent(gRenderer);
                    }

                    SDL_DestroyTexture(gameTexture);
                    gameTexture = nullptr;
                }
            }
        }
        else
        {
            SDL_Delay(1);
        }
    }

    if (gameTexture) {
        SDL_DestroyTexture(gameTexture);
        gameTexture = nullptr;
    }
    if (gRenderer) {
        SDL_DestroyRenderer(gRenderer);
        gRenderer = nullptr;
    }
    if (window) {
        SDL_DestroyWindow(window);
    }
    SDL_Quit();
    return 0;
}