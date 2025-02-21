#include <windows.h>
#include <cstdint>
#include <mutex>
#include <atomic>
#include <thread>
#include <memoryapi.h>
#include <cstdio>
#include <pch.h>



// Force GPU usage
extern "C" __declspec(dllexport) DWORD NvOptimusEnablement = 1;
extern "C" __declspec(dllexport) DWORD AmdPowerXpressRequestHighPerformance = 1;

// define ~1.5GB pool
static const size_t POOL_SIZE = 1536ULL * 1024ULL * 1024ULL;
static void* g_pool = NULL;
static size_t g_used = 0;

// CRITICAL_SECTION instead of <mutex>
static CRITICAL_SECTION g_lock;
static bool g_inited = false;

typedef void* (__cdecl* CRT_Malloc)(size_t);
typedef void(__cdecl* CRT_Free)(void*);

static CRT_Malloc s_origMalloc = NULL;
static CRT_Free   s_origFree = NULL;

// Our hooking versions
static void* MyMalloc(size_t sz)
{
    EnterCriticalSection(&g_lock);
    if (!g_pool || (g_used + sz > POOL_SIZE))
    {
        void* p = s_origMalloc ? s_origMalloc(sz) : NULL;
        LeaveCriticalSection(&g_lock);
        return p;
    }
    void* ret = (char*)g_pool + g_used;
    g_used += sz;
    LeaveCriticalSection(&g_lock);
    return ret;
}
static void MyFree(void* ptr)
{
    if (!ptr) return;
    // if in our pool, ignore
    if (ptr >= g_pool && ptr < (char*)g_pool + POOL_SIZE)
    {
        return;
    }
    // fallback
    if (s_origFree) s_origFree(ptr);
}

static void HookIAT(const char* dllName, const char* funcName, void* newFunc)
{
    HMODULE base = GetModuleHandle(NULL);
    if (!base) return;
    IMAGE_DOS_HEADER* dosH = (IMAGE_DOS_HEADER*)base;
    IMAGE_NT_HEADERS32* ntH = (IMAGE_NT_HEADERS32*)((BYTE*)base + dosH->e_lfanew);
    DWORD impRVA = ntH->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (!impRVA) return;

    IMAGE_IMPORT_DESCRIPTOR* impDesc = (IMAGE_IMPORT_DESCRIPTOR*)((BYTE*)base + impRVA);
    for (; impDesc->Name; ++impDesc)
    {
        const char* modName = (const char*)((BYTE*)base + impDesc->Name);
        // skip hooking system libs if you want to avoid white flashing
        if (!_stricmp(modName, "kernel32.dll") ||
            !_stricmp(modName, "user32.dll") ||
            !_stricmp(modName, "gdi32.dll") ||
            !_stricmp(modName, "d3d9.dll"))
        {
            continue;
        }
        if (!_stricmp(modName, dllName))
        {
            IMAGE_THUNK_DATA* thunkOrig = (IMAGE_THUNK_DATA*)((BYTE*)base + impDesc->OriginalFirstThunk);
            IMAGE_THUNK_DATA* thunk = (IMAGE_THUNK_DATA*)((BYTE*)base + impDesc->FirstThunk);
            while (thunkOrig->u1.AddressOfData)
            {
                if (!(thunkOrig->u1.Ordinal & IMAGE_ORDINAL_FLAG32))
                {
                    IMAGE_IMPORT_BY_NAME* impByName = (IMAGE_IMPORT_BY_NAME*)((BYTE*)base + thunkOrig->u1.AddressOfData);
                    if (!strcmp(impByName->Name, funcName))
                    {
                        DWORD oldProt;
                        VirtualProtect(&thunk->u1.Function, sizeof(void*), PAGE_READWRITE, &oldProt);
                        thunk->u1.Function = (ULONG_PTR)newFunc;
                        VirtualProtect(&thunk->u1.Function, sizeof(void*), oldProt, &oldProt);
                        return;
                    }
                }
                ++thunkOrig;
                ++thunk;
            }
        }
    }
}

extern "C" __declspec(dllexport) void HookAllocators()
{
    if (g_inited) return;
    g_inited = true;

    InitializeCriticalSection(&g_lock);

    // create big memory pool
    g_pool = VirtualAlloc(NULL, POOL_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

    // find original malloc/free
    HMODULE hCRT = GetModuleHandleA("msvcrt.dll");
    if (!hCRT) hCRT = GetModuleHandleA("ucrtbase.dll");
    if (hCRT)
    {
        s_origMalloc = (CRT_Malloc)GetProcAddress(hCRT, "malloc");
        s_origFree = (CRT_Free)GetProcAddress(hCRT, "free");
    }

    // hook only non-critical modules in msVCRT or ucrtbase
    HookIAT("msvcrt.dll", "malloc", (void*)MyMalloc);
    HookIAT("msvcrt.dll", "free", (void*)MyFree);
    HookIAT("ucrtbase.dll", "malloc", (void*)MyMalloc);
    HookIAT("ucrtbase.dll", "free", (void*)MyFree);
}

BOOL WINAPI DllMain(HINSTANCE hDLL, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hDLL);
        // Optionally call HookAllocators() right away:
        HookAllocators();
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        // optional cleanup
        if (g_pool)
        {
            VirtualFree(g_pool, 0, MEM_RELEASE);
            g_pool = NULL;
        }
        DeleteCriticalSection(&g_lock);
    }
    return TRUE;
}