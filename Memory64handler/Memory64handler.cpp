// Memory64Handler.cpp
// Compile as a 64-bit executable (x64 target)

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <unordered_map>

#include "rpmalloc.h"

#pragma comment(lib, "rpmalloc.lib") // Adjust path to 64-bit rpmalloc.lib

// Shared memory settings (must match RPMemoryManager.dll)
static const char* SHM_NAME = "Global\\GameMemorySHM";
static const char* SHM_EVENT_REQUEST = "Global\\GameMemoryRequest";
static const char* SHM_EVENT_RESPONSE = "Global\\GameMemoryResponse";
#define SHM_SIZE 1024

typedef struct {
    size_t size;
    BOOL isAlloc;
    void* ptr;
} SHMRequest;

// Configuration
#define TOTAL_64BIT_POOL_SIZE (8ULL * 1024 * 1024 * 1024) // 8 GB pool

static BOOL rpmallocInitialized = FALSE;
static std::unordered_map<void*, HANDLE> memoryMappings;

static FILE* logFile = NULL;

static void DebugLog(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    // Log to file
    if (!logFile) {
        fopen_s(&logFile, "Memory64Handler.log", "a");
    }
    if (logFile) {
        vfprintf(logFile, fmt, args);
        fprintf(logFile, "\n");
        fflush(logFile);
    }

    // Log to console (optional, can be disabled in production)
    vprintf(fmt, args);
    printf("\n");

    va_end(args);
}

static void InitRPMalloc() {
    if (!rpmallocInitialized) {
        int result = rpmalloc_initialize();
        if (result == 0) {
            rpmallocInitialized = TRUE;
            DebugLog("64-bit rpmalloc initialized successfully");
        }
        else {
            DebugLog("ERROR: Failed to initialize 64-bit rpmalloc (result=%d)", result);
            ExitProcess(1);
        }
    }
}

static void CleanupRPMalloc() {
    if (rpmallocInitialized) {
        rpmalloc_finalize();
        rpmallocInitialized = FALSE;
        DebugLog("64-bit rpmalloc finalized");
    }
}

static void* AllocateLargeMemory(size_t size) {
    if (size == 0) {
        DebugLog("WARNING: Attempted to allocate 0 bytes");
        return NULL;
    }

    void* ptr = rpmalloc(size);
    if (!ptr) {
        DebugLog("ERROR: Failed to allocate %zu bytes with rpmalloc", size);
        return NULL;
    }

    HANDLE hMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
        (DWORD)(size >> 32), (DWORD)(size & 0xFFFFFFFF), NULL);
    if (!hMap) {
        rpfree(ptr);
        DebugLog("ERROR: Failed to create shared memory mapping for %zu bytes (error=%d)", size, GetLastError());
        return NULL;
    }

    void* mappedPtr = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, size);
    if (!mappedPtr) {
        CloseHandle(hMap);
        rpfree(ptr);
        DebugLog("ERROR: Failed to map shared memory for %zu bytes (error=%d)", size, GetLastError());
        return NULL;
    }

    memcpy(mappedPtr, ptr, size);
    memoryMappings[mappedPtr] = hMap;
    rpfree(ptr);

    DebugLog("Allocated %zu bytes at %p (shared)", size, mappedPtr);
    return mappedPtr;
}

static void FreeLargeMemory(void* ptr) {
    if (!ptr) {
        DebugLog("WARNING: Attempted to free NULL pointer");
        return;
    }

    auto it = memoryMappings.find(ptr);
    if (it != memoryMappings.end()) {
        UnmapViewOfFile(it->first);
        CloseHandle(it->second);
        memoryMappings.erase(it);
        DebugLog("Freed %p (shared)", ptr);
    }
    else {
        DebugLog("ERROR: Attempted to free unmapped pointer %p", ptr);
    }
}

static void Cleanup() {
    for (auto& mapping : memoryMappings) {
        UnmapViewOfFile(mapping.first);
        CloseHandle(mapping.second);
    }
    memoryMappings.clear();

    if (logFile) {
        fclose(logFile);
        logFile = NULL;
    }

    CleanupRPMalloc();
}

int main() {
    InitRPMalloc();

    HANDLE hShm = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, SHM_NAME);
    if (!hShm) {
        DebugLog("ERROR: Failed to open shared memory (error=%d)", GetLastError());
        MessageBoxA(NULL, "Failed to open shared memory", "Memory64Handler Error", MB_ICONERROR);
        Cleanup();
        return 1;
    }

    LPVOID shmBuffer = MapViewOfFile(hShm, FILE_MAP_ALL_ACCESS, 0, 0, SHM_SIZE);
    if (!shmBuffer) {
        DebugLog("ERROR: Failed to map shared memory (error=%d)", GetLastError());
        CloseHandle(hShm);
        Cleanup();
        return 1;
    }

    HANDLE hRequestEvent = OpenEventA(EVENT_ALL_ACCESS, FALSE, SHM_EVENT_REQUEST);
    HANDLE hResponseEvent = OpenEventA(EVENT_ALL_ACCESS, FALSE, SHM_EVENT_RESPONSE);
    if (!hRequestEvent || !hResponseEvent) {
        DebugLog("ERROR: Failed to open events (request=%p, response=%p, error=%d)",
            hRequestEvent, hResponseEvent, GetLastError());
        UnmapViewOfFile(shmBuffer);
        CloseHandle(hShm);
        Cleanup();
        return 1;
    }

    DebugLog("64-bit Memory Handler started successfully");

    // Main loop
    while (TRUE) {
        DWORD waitResult = WaitForSingleObject(hRequestEvent, INFINITE);
        if (waitResult != WAIT_OBJECT_0) {
            DebugLog("ERROR: WaitForSingleObject failed (result=%d, error=%d)", waitResult, GetLastError());
            break;
        }

        SHMRequest* request = (SHMRequest*)shmBuffer;
        SHMRequest response = *request;

        if (request->isAlloc) {
            response.ptr = AllocateLargeMemory(request->size);
        }
        else {
            FreeLargeMemory(request->ptr);
            response.ptr = NULL;
        }

        memcpy(shmBuffer, &response, sizeof(SHMRequest));
        if (!SetEvent(hResponseEvent)) {
            DebugLog("ERROR: Failed to signal response event (error=%d)", GetLastError());
            break;
        }
    }

    // Cleanup on exit
    UnmapViewOfFile(shmBuffer);
    CloseHandle(hShm);
    CloseHandle(hRequestEvent);
    CloseHandle(hResponseEvent);
    Cleanup();

    DebugLog("64-bit Memory Handler shutting down");
    return 0;
}