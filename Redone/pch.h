#ifndef PCH_H
#define PCH_H

#define _WIN32_WINNT 0x0600
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NO_IMAGEHLP

#include <windows.h>
#include <DbgHelp.h>
#include <shlwapi.h>
#include <commdlg.h>
#include <shellapi.h>
#include <tchar.h>
#include <psapi.h>
#include <iostream>
#include <vector>
#include <string>
#include <cstdint>
#include <thread>
#include <mutex>
#include <cstring>
#include <fstream>
#include <algorithm>
#include <atomic>

// Include your DebugLogger.h
#include "DebugLogger.h"

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(linker, "/SECTION:.injsec,RWE")

// Constants
#define PATCH_ADDRESS 0x007D1496
#define PATCH_SIZE 2
extern const BYTE PATCH_SIGNATURE[PATCH_SIZE];
extern const BYTE PATCH_BYTES[PATCH_SIZE];

// Memory Pool Structures
struct MemoryBlock {
    size_t size;
    bool used;
    void* address;
    MemoryBlock* next;
};

struct MemoryPool {
    char* pool;
    std::atomic<size_t> offset;
    std::atomic<MemoryBlock*> head;
    std::mutex mutex;

    MemoryPool();
    ~MemoryPool();
    void Init();
    void Cleanup();
};

// Function Declarations
extern "C" __declspec(dllexport) void* CustomMalloc(size_t size);
extern "C" __declspec(dllexport) void CustomFree(void* p);
extern "C" __declspec(dllexport) void* CustomRealloc(void* p, size_t size);
extern "C" __declspec(dllexport) void* CustomCalloc(size_t n, size_t s);

DWORD AlignValue(DWORD value, DWORD alignment);
DWORD_PTR FindPatchAddress(const BYTE* signature, size_t size);
void PatchMultiplayerLobby(HANDLE hProcess, LPVOID patchAddr);
bool EnableLAA(const std::string& exePath);
bool InjectInjsec(const std::string& exePath);
bool IsRunningAsAdmin();
void RelaunchAsAdmin();
std::string PickSoulstormExe();
void RunPatch(const std::string& exePath);

#endif // PCH_H