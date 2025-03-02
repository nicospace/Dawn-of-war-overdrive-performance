#include "pch.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include "SDLThread.h"
#include <chrono>

// Global definitions
volatile bool gSDLRunning = true;

extern "C" __declspec(dllexport) DWORD WINAPI SDLThread(LPVOID lpParam) {
    // SDL and Vulkan initialization moved to SDL_Vulkan_D3D9
    while (gSDLRunning) {
        SDL_Delay(1); // Keep thread alive, rendering handled by Direct3D wrappers
    }
    return 0;
}