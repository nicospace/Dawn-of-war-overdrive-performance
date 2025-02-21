// SDLThread.cpp
#include "pch.h"
#include <SDL.h>
#include <windows.h>
#include "ThreadSafeQueue.h"
#include "RenderRequest.h"
#include "SDLThread.h"
#include <chrono>
#include <cstdlib>



#ifndef SDLTHREAD_H
#define SDLTHREAD_H

// Global definitions (shared across modules)
SDL_Renderer* gRenderer = nullptr;
SDL_Texture* gameTexture = nullptr;
ThreadSafeQueue gRenderQueue; // Instantiate the render queue.
volatile bool gSDLRunning = true; // Global flag to signal shutdown

DWORD WINAPI SDLThread(LPVOID lpParam)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        MessageBoxA(NULL, SDL_GetError(), "SDL Initialization Error", MB_ICONERROR);
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "GDI to SDL Renderer",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1024, 768,
        SDL_WINDOW_SHOWN
    );

    if (!window) {
        SDL_Quit();
        return 1;
    }

    gRenderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!gRenderer) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    RenderRequest request;
    // Loop until gSDLRunning becomes false (e.g., on DLL unload)
    while (gSDLRunning)
    {
        // Wait up to ~16ms (for roughly 60 FPS) for a render request.
        if (gRenderQueue.waitPop(request, std::chrono::milliseconds(16)))
        {
            if (request.isValid)
            {
                // Create an SDL_Surface to hold the bitmap pixels.
                SDL_Surface* tempSurface = SDL_CreateRGBSurfaceWithFormat(
                    0,
                    request.bmi.bmiHeader.biWidth,
                    abs(request.bmi.bmiHeader.biHeight),
                    32,
                    SDL_PIXELFORMAT_ARGB8888
                );
                if (tempSurface)
                {
                    // Create a memory DC to extract the bitmap data.
                    HDC hdcMem = CreateCompatibleDC(NULL);
                    if (hdcMem)
                    {
                        HGDIOBJ hOld = SelectObject(hdcMem, request.hBitmap);
                        // Note: GetDIBits returns the number of scan lines copied.
                        GetDIBits(hdcMem, request.hBitmap, 0,
                            abs(request.bmi.bmiHeader.biHeight),
                            tempSurface->pixels,
                            &request.bmi,
                            DIB_RGB_COLORS);
                        SelectObject(hdcMem, hOld);
                        DeleteDC(hdcMem);
                    }
                    if (gameTexture)
                    {
                        SDL_DestroyTexture(gameTexture);
                        gameTexture = nullptr;
                    }
                    gameTexture = SDL_CreateTextureFromSurface(gRenderer, tempSurface);
                    if (gameTexture)
                    {
                        SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, 255);
                        SDL_RenderClear(gRenderer);
                        SDL_RenderCopy(gRenderer, gameTexture, NULL, &request.destRect);
                        SDL_RenderPresent(gRenderer);
                    }
                    SDL_FreeSurface(tempSurface);
                }
            }
        }
        // (If waitPop times out, simply loop again.)
    }

    // Cleanup when exiting the thread.
    if (gameTexture)
    {
        SDL_DestroyTexture(gameTexture);
        gameTexture = nullptr;
    }
    SDL_DestroyRenderer(gRenderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
#endif