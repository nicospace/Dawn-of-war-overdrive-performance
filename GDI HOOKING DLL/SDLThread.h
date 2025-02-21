// SDLThread.h
#pragma once
#include <windows.h>

// Declare the global variable (no definition here)
extern volatile bool gSDLRunning;

// Externally accessible SDL thread function.
DWORD WINAPI SDLThread(LPVOID lpParam);
