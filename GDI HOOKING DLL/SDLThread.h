#pragma once
#ifndef SDLTHREAD_H
#define SDLTHREAD_H

#include <SDL3/SDL.h>
#include <windows.h>
#include "ThreadSafeQueue.h"
#include "RenderRequest.h"

// Function declaration
extern "C" __declspec(dllexport) DWORD WINAPI SDLThread(LPVOID lpParam);

// Global declarations
extern SDL_Renderer* gRenderer;
extern SDL_Texture* gameTexture;
extern ThreadSafeQueue gRenderQueue;
extern volatile bool gSDLRunning;

#endif // SDLTHREAD_H