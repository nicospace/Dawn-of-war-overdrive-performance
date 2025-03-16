// dllmain.cpp
// Ensure precompiled headers are disabled for this file in project settings

#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <DbgHelp.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stddef.h> // For size_t

#include "rpmalloc.h" // Must match your built rpmalloc.lib

#pragma comment(lib, "rpmalloc.lib")  // Link your 32-bit rpmalloc library
#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "Advapi32.lib")  // <-- Add THIS line for OpenProcessToken, etc.
// Memory thresholds and pool sizes
#define LARGE_ALLOC_THRESHOLD (512ULL * 1024ULL * 1024ULL) // 512 MB
#define TOTAL_32BIT_POOL_SIZE (2ULL * 1024ULL * 1024ULL * 1024ULL) // 2 GB

// Shared memory settings
static const char* SHM_NAME = "Global\\GameMemorySHM";
static const char* SHM_EVENT_REQUEST = "Global\\GameMemoryRequest";
static const char* SHM_EVENT_RESPONSE = "Global\\GameMemoryResponse";
#define SHM_SIZE 1024

typedef struct {
    size_t size;
    BOOL   isAlloc;
    void* ptr;
} SHMRequest;

static BOOL   rpmallocInitialized = FALSE;
static HANDLE hShm = NULL;
static HANDLE hRequestEvent = NULL;
static HANDLE hResponseEvent = NULL;
static LPVOID shmBuffer = NULL;

// Original CRT function pointers
static void* (__cdecl* originalMalloc)(size_t) = NULL;
static void(__cdecl* originalFree)(void*) = NULL;
static void* (__cdecl* originalRealloc)(void*, size_t) = NULL;
static void* (__cdecl* originalCalloc)(size_t, size_t) = NULL;

// Simple debug levels
#define DEBUG_INFO     0
#define DEBUG_CRITICAL 1

static void DebugLog(int level, const char* fmt, ...) {
    // Basic console/log output
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

static void DebugInit() {}
static void DebugCleanup() {}

BOOL InitIPC() {
    // Create a global named shared memory region
    hShm = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, SHM_SIZE, SHM_NAME);
    if (!hShm || GetLastError() == ERROR_ALREADY_EXISTS) {
        DebugLog(DEBUG_CRITICAL, "Failed to create shared memory");
        return FALSE;
    }
    shmBuffer = MapViewOfFile(hShm, FILE_MAP_ALL_ACCESS, 0, 0, SHM_SIZE);
    if (!shmBuffer) {
        DebugLog(DEBUG_CRITICAL, "Failed to map shared memory");
        CloseHandle(hShm);
        return FALSE;
    }
    hRequestEvent = CreateEventA(NULL, FALSE, FALSE, SHM_EVENT_REQUEST);
    hResponseEvent = CreateEventA(NULL, FALSE, FALSE, SHM_EVENT_RESPONSE);
    if (!hRequestEvent || !hResponseEvent) {
        DebugLog(DEBUG_CRITICAL, "Failed to create events");
        return FALSE;
    }
    return TRUE;
}

void CleanupIPC() {
    if (shmBuffer)      UnmapViewOfFile(shmBuffer);
    if (hShm)           CloseHandle(hShm);
    if (hRequestEvent)  CloseHandle(hRequestEvent);
    if (hResponseEvent) CloseHandle(hResponseEvent);
}

void* OffloadLargeAlloc(size_t size) {
    SHMRequest request = { size, TRUE, NULL };
    memcpy(shmBuffer, &request, sizeof(SHMRequest));
    SetEvent(hRequestEvent);
    WaitForSingleObject(hResponseEvent, INFINITE);
    SHMRequest* response = (SHMRequest*)shmBuffer;
    return response->ptr;
}

void OffloadLargeFree(void* ptr) {
    SHMRequest request = { 0, FALSE, ptr };
    memcpy(shmBuffer, &request, sizeof(SHMRequest));
    SetEvent(hRequestEvent);
    WaitForSingleObject(hResponseEvent, INFINITE);
}

//
// Hooked Allocation Routines
//
