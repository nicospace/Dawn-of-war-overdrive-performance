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
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <unordered_map>
#include <array>
#include <atomic>
#include "ZeroPEChecksum.h"
#include <SDL3/SDL.h>
#include <cstddef>  // For size_t

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(linker, "/SECTION:.injsec,RWE")
#endif


#include "pch.h"

// Section definition from your pch.h


#include "pch.h"
#include "DebugLogger.h"
#include "DllUtils.h"


#define PATCH_ADDRESS 0x007D1496
#define PATCH_SIZE 2


#pragma section(".injsec", read, execute, shared)
__declspec(allocate(".injsec")) char dummyInjSec = 1;  // Instead of 0









// Line 65-66: Explicit array definitions
static const BYTE PATCH_SIGNATURE[2] = { 0x75, 0x40 }; // 2 bytes: JNZ instruction
static const BYTE PATCH_BYTES[2] = { 0x90, 0x90 };     // 2 bytes: NOP NOP

// Function declarations
DWORD_PTR FindPatchAddress(const BYTE* signature, size_t size);
BYTE* FindPattern(BYTE* base, DWORD size, const BYTE* pattern, const char* mask);

// Line ~84: FindPatchAddress definition starts
DWORD_PTR FindPatchAddress(BYTE* signature, size_t sigSize) {
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    DWORD_PTR baseAddress = (DWORD_PTR)sysInfo.lpMinimumApplicationAddress;
    DWORD_PTR maxAddress = (DWORD_PTR)sysInfo.lpMaximumApplicationAddress;
    MEMORY_BASIC_INFORMATION mbi;
    BYTE* buffer = new BYTE[sigSize];
    while (baseAddress < maxAddress) {
        if (VirtualQuery((LPCVOID)baseAddress, &mbi, sizeof(mbi)) == 0) {
            baseAddress += 0x1000;
            continue;
        }
        if (mbi.State == MEM_COMMIT && (mbi.Protect & (PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_READ))) {
            DWORD_PTR regionEnd = (DWORD_PTR)mbi.BaseAddress + mbi.RegionSize - sigSize;
            for (DWORD_PTR addr = (DWORD_PTR)mbi.BaseAddress; addr < regionEnd; addr++) {
                SIZE_T bytesRead = 0;
                BOOL result = FALSE;
                __try {
                    result = ReadProcessMemory(GetCurrentProcess(), (LPCVOID)addr, buffer, sigSize, &bytesRead);
                }
                __except (EXCEPTION_EXECUTE_HANDLER) {
                    result = FALSE;
                }
                if (result && bytesRead == sigSize && memcmp(buffer, signature, sigSize) == 0) {
                    delete[] buffer;
                    return addr;
                }
            }
        }
        baseAddress += mbi.RegionSize;
    }
    delete[] buffer;
    return 0;
}

// Line ~110: PatchMultiplayerLobby starts
void PatchMultiplayerLobby(HANDLE hProcess, LPVOID patchAddr) {
    DWORD oldProtect;
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQueryEx(hProcess, patchAddr, &mbi, sizeof(mbi))) {
        DebugLogger::Log(DebugLogger::INFO, "Current Memory Protection: %lu", mbi.Protect);
    }
    else {
        DebugLogger::Log(DebugLogger::CRITICAL, "Failed to query memory protection.");
        return;
    }
    if (VirtualProtectEx(hProcess, patchAddr, PATCH_SIZE, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        SIZE_T bytesWritten;
        // Line ~123: WriteProcessMemory call
        if (WriteProcessMemory(hProcess, patchAddr, PATCH_BYTES, PATCH_SIZE, &bytesWritten)) {
            if (!VirtualProtectEx(hProcess, patchAddr, PATCH_SIZE, oldProtect, &oldProtect)) {
                DebugLogger::Log(DebugLogger::WARNING, "Failed to restore original memory protection.");
            }
            else {
                MessageBoxA(nullptr, "[+] Successfully patched multiplayer lobby!", "Success", MB_OK);
                return;
            }
        }
        else {
            DWORD dwError = GetLastError();
            DebugLogger::Log(DebugLogger::CRITICAL, "Failed to write memory: %lu", dwError);
        }
    }
    else {
        DWORD dwError = GetLastError();
        DebugLogger::Log(DebugLogger::CRITICAL, "Failed to change memory protection: %lu", dwError);
    }
}

void PatchMultiplayerLobbylog() {
    HANDLE hProcess = GetCurrentProcess();
    if (!hProcess) {
        MessageBoxA(nullptr, "[-] Failed to open process!", "Error", MB_ICONERROR);
        return;
    }
    DWORD_PTR patchAddr = FindPatchAddress(PATCH_SIGNATURE, PATCH_SIZE);
    if (!patchAddr) {
        MessageBoxA(nullptr, "[-] Could not find patch address!", "Error", MB_ICONERROR);
        return;
    }
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery((LPCVOID)patchAddr, &mbi, sizeof(mbi)) == 0) {
        MessageBoxA(nullptr, "[-] VirtualQuery() failed! Address is invalid.", "Error", MB_ICONERROR);
        return;
    }
    DWORD oldProtect;
    if (VirtualProtect((LPVOID)patchAddr, PATCH_SIZE, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        SIZE_T bytesWritten;
        if (WriteProcessMemory(hProcess, (LPVOID)patchAddr, PATCH_BYTES, PATCH_SIZE, &bytesWritten)) {
            VirtualProtect((LPVOID)patchAddr, PATCH_SIZE, oldProtect, &oldProtect);
            MessageBoxA(nullptr, "[+] Successfully patched multiplayer lobby!", "Success", MB_OK);
            return;
        }
    }
    MessageBoxA(nullptr, "[-] Failed to patch memory!", "Error", MB_ICONERROR);
}

BYTE* FindPattern(BYTE* base, DWORD size, const BYTE* pattern, const char* mask) {
    DWORD patternLength = static_cast<DWORD>(strlen(mask));
    for (DWORD i = 0; i <= size - patternLength; i++) {
        bool found = true;
        for (DWORD j = 0; j < patternLength; j++) {
            if (mask[j] == 'x' && pattern[j] != *(base + i + j)) {
                found = false;
                break;
            }
        }
        if (found) return base + i;
    }
    return nullptr;
}

struct Patch {
    DWORD offset;
    std::vector<BYTE> bytes;
    Patch(DWORD _offset = 0) : offset(_offset) {}
};

constexpr size_t RESERVED_SYSTEM_MEMORY = 512ull * 1024 * 1024;
constexpr size_t PRIVATE_MEMORY_SIZE = 1ull * 1024 * 1024 * 1024;
constexpr size_t TEXTURE_MEMORY_SIZE = 1ull * 1024 * 1024 * 1024;
constexpr size_t TOTAL_POOL_SIZE = PRIVATE_MEMORY_SIZE + TEXTURE_MEMORY_SIZE;

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
    std::atomic<MemoryBlock*> freeList;
    MemoryPool() : pool(nullptr), offset(0), head(nullptr), freeList(nullptr) {}
};

static MemoryPool gameMemoryPool;
static std::mutex headMutex;

void InitMemoryPool() {
    static bool initialized = false;
    if (!initialized) {
        char* p = static_cast<char*>(VirtualAlloc(nullptr, TOTAL_POOL_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        if (!p) {
            MessageBoxA(nullptr, "Failed to allocate game memory pool!", "Error", MB_OK | MB_ICONERROR);
            exit(1);
        }
        gameMemoryPool.pool = p;
        gameMemoryPool.offset.store(0, std::memory_order_relaxed);
        gameMemoryPool.head.store(nullptr, std::memory_order_relaxed);
        gameMemoryPool.freeList.store(nullptr, std::memory_order_relaxed);
        initialized = true;
    }
}

void CleanupMemoryPool() {
    if (gameMemoryPool.pool) {
        std::lock_guard<std::mutex> lock(headMutex);
        MemoryBlock* current = gameMemoryPool.head.load(std::memory_order_acquire);
        while (current) {
            MemoryBlock* next = current->next;
            delete current;
            current = next;
        }
        VirtualFree(gameMemoryPool.pool, 0, MEM_RELEASE);
        gameMemoryPool.pool = nullptr;
        gameMemoryPool.offset.store(0, std::memory_order_relaxed);
        gameMemoryPool.head.store(nullptr, std::memory_order_relaxed);
        gameMemoryPool.freeList.store(nullptr, std::memory_order_relaxed);
    }
}

bool IsInCustomPool(void* p) {
    return gameMemoryPool.pool && (p >= gameMemoryPool.pool && p < gameMemoryPool.pool + TOTAL_POOL_SIZE);
}

MemoryBlock* PopFreeBlock() {
    MemoryBlock* block = gameMemoryPool.freeList.load(std::memory_order_acquire);
    while (block) {
        MemoryBlock* next = block->next;
        if (gameMemoryPool.freeList.compare_exchange_weak(block, next, std::memory_order_acq_rel, std::memory_order_acquire))
            return block;
    }
    return nullptr;
}

void PushFreeBlock(MemoryBlock* block) {
    MemoryBlock* oldFree = gameMemoryPool.freeList.load(std::memory_order_acquire);
    do {
        block->next = oldFree;
    } while (!gameMemoryPool.freeList.compare_exchange_weak(oldFree, block, std::memory_order_acq_rel, std::memory_order_acquire));
}

extern "C" __declspec(dllexport) void* CustomMalloc(size_t size) {
    if (size == 0) return nullptr;
    if (size < 1024 || size >(TOTAL_POOL_SIZE / 3)) return malloc(size);
    InitMemoryPool();
    size_t alignedSize = (size + 15) & ~static_cast<size_t>(15);
    MemoryBlock* block = PopFreeBlock();
    while (block) {
        if (block->size >= alignedSize) {
            block->used = true;
            return block->address;
        }
        block = PopFreeBlock();
    }
    size_t currentOffset = gameMemoryPool.offset.fetch_add(alignedSize, std::memory_order_relaxed);
    if (currentOffset + alignedSize > TOTAL_POOL_SIZE) return nullptr;
    void* allocatedMemory = gameMemoryPool.pool + currentOffset;
    MemoryBlock* newBlock = new MemoryBlock{ alignedSize, true, allocatedMemory, nullptr };
    std::lock_guard<std::mutex> lock(headMutex);
    newBlock->next = gameMemoryPool.head.load(std::memory_order_acquire);
    gameMemoryPool.head.store(newBlock, std::memory_order_release);
    return allocatedMemory;
}

extern "C" __declspec(dllexport) void CustomFree(void* p) {
    if (!p || !gameMemoryPool.pool) return;
    if (!IsInCustomPool(p)) {
        free(p);
        return;
    }
    MemoryBlock* targetBlock = nullptr;
    {
        std::lock_guard<std::mutex> lock(headMutex);
        MemoryBlock* current = gameMemoryPool.head.load(std::memory_order_acquire);
        while (current) {
            if (current->address == p) {
                if (!current->used) return;
                targetBlock = current;
                break;
            }
            current = current->next;
        }
    }
    if (targetBlock) {
        targetBlock->used = false;
        PushFreeBlock(targetBlock);
    }
}

extern "C" __declspec(dllexport) void* CustomRealloc(void* p, size_t size) {
    if (!p) return CustomMalloc(size);
    if (!IsInCustomPool(p)) return realloc(p, size);
    MemoryBlock* origBlock = nullptr;
    {
        std::lock_guard<std::mutex> lock(headMutex);
        MemoryBlock* current = gameMemoryPool.head.load(std::memory_order_acquire);
        while (current) {
            if (current->address == p) {
                origBlock = current;
                break;
            }
            current = current->next;
        }
    }
    if (!origBlock) return realloc(p, size);
    void* newPtr = CustomMalloc(size);
    if (newPtr) {
        size_t copySize = (size < origBlock->size) ? size : origBlock->size;
        memcpy(newPtr, p, copySize);
        CustomFree(p);
    }
    return newPtr;
}

extern "C" __declspec(dllexport) void* CustomCalloc(size_t n, size_t s) {
    size_t total = n * s;
    void* p = CustomMalloc(total);
    if (p) memset(p, 0, total);
    return p;
}

extern "C" __declspec(dllexport) void DebugMemoryUsage() {
    size_t usedMemory = 0, freeMemory = 0;
    {
        std::lock_guard<std::mutex> lock(headMutex);
        MemoryBlock* current = gameMemoryPool.head.load(std::memory_order_acquire);
        while (current) {
            if (current->used) usedMemory += current->size;
            else freeMemory += current->size;
            current = current->next;
        }
    }
    DebugLogger::Log(DebugLogger::INFO, "Memory Usage: Used=%zu, Free=%zu", usedMemory, freeMemory);
}

bool IsRunningAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = nullptr;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(nullptr, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return (isAdmin != FALSE);
}

void RelaunchAsAdmin() {
    TCHAR szPath[MAX_PATH];
    if (GetModuleFileName(nullptr, szPath, MAX_PATH)) {
        SHELLEXECUTEINFO sei = { sizeof(sei) };
        sei.lpVerb = _T("runas");
        sei.lpFile = szPath;
        sei.hwnd = nullptr;
        sei.nShow = SW_NORMAL;
        if (!ShellExecuteEx(&sei)) {
            MessageBox(nullptr, _T("Must be run as administrator."), _T("Error"), MB_ICONERROR);
        }
        ExitProcess(0);
    }
}

static std::string PickSoulstormExe() {
    OPENFILENAMEA ofn = {};
    char fileName[MAX_PATH] = "";
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = "Exe Files\0*.exe\0All Files\0*.*\0";
    ofn.lpstrTitle = "Select Soulstorm.exe";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameA(&ofn)) return std::string(fileName);
    return {};
}

bool EnableLAA(const std::string& exePath) {
    DebugLogger::Log(DebugLogger::INFO, "EnableLAA: Opening file %s", exePath.c_str());
    std::fstream f(exePath.c_str(), std::ios::in | std::ios::out | std::ios::binary);
    if (!f.is_open()) {
        DebugLogger::Log(DebugLogger::CRITICAL, "EnableLAA: Failed to open file.");
        return false;
    }
    IMAGE_DOS_HEADER dosH = {};
    f.read(reinterpret_cast<char*>(&dosH), sizeof(dosH));
    if (dosH.e_magic != IMAGE_DOS_SIGNATURE) {
        DebugLogger::Log(DebugLogger::CRITICAL, "EnableLAA: Invalid DOS signature.");
        f.close();
        return false;
    }
    f.seekg(dosH.e_lfanew, std::ios::beg);
    IMAGE_NT_HEADERS32 nth = {};
    f.read(reinterpret_cast<char*>(&nth), sizeof(nth));
    if (nth.Signature != IMAGE_NT_SIGNATURE || nth.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        DebugLogger::Log(DebugLogger::CRITICAL, "EnableLAA: Invalid NT header.");
        f.close();
        return false;
    }
    nth.FileHeader.Characteristics |= IMAGE_FILE_LARGE_ADDRESS_AWARE;
    f.clear();
    f.seekp(dosH.e_lfanew, std::ios::beg);
    f.write(reinterpret_cast<const char*>(&nth), sizeof(nth));
    f.close();
    DebugLogger::Log(DebugLogger::INFO, "EnableLAA: Succeeded.");
    return true;
}

DWORD AlignValue(DWORD value, DWORD alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

bool InjectInjsec(const std::string& exePath) {
    DebugLogger::Log(DebugLogger::INFO, "InjectInjsec: Opening file %s", exePath.c_str());
    std::fstream f(exePath, std::ios::in | std::ios::out | std::ios::binary);
    if (!f.is_open()) {
        DebugLogger::Log(DebugLogger::CRITICAL, "InjectInjsec: Failed to open file.");
        return false;
    }
    IMAGE_DOS_HEADER dosH = {};
    f.read(reinterpret_cast<char*>(&dosH), sizeof(dosH));
    if (dosH.e_magic != IMAGE_DOS_SIGNATURE) {
        DebugLogger::Log(DebugLogger::CRITICAL, "InjectInjsec: Invalid DOS signature.");
        f.close();
        return false;
    }
    f.seekg(dosH.e_lfanew, std::ios::beg);
    IMAGE_NT_HEADERS32 nth = {};
    f.read(reinterpret_cast<char*>(&nth), sizeof(nth));
    if (nth.Signature != IMAGE_NT_SIGNATURE || nth.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        DebugLogger::Log(DebugLogger::CRITICAL, "InjectInjsec: Invalid NT header.");
        f.close();
        return false;
    }
    WORD secCount = nth.FileHeader.NumberOfSections;
    DebugLogger::Log(DebugLogger::INFO, "InjectInjsec: Section count: %d", secCount);
    std::vector<IMAGE_SECTION_HEADER> secs(secCount);
    f.read(reinterpret_cast<char*>(secs.data()), secCount * sizeof(IMAGE_SECTION_HEADER));
    DWORD fAlign = nth.OptionalHeader.FileAlignment;
    DWORD sAlign = nth.OptionalHeader.SectionAlignment;
    IMAGE_SECTION_HEADER& lastSec = secs.back();
    DWORD newSecRaw = AlignValue(lastSec.PointerToRawData + lastSec.SizeOfRawData, fAlign);
    DWORD newSecVA = AlignValue(lastSec.VirtualAddress + lastSec.Misc.VirtualSize, sAlign);
    IMAGE_SECTION_HEADER inj = {};
    memcpy(inj.Name, ".injsec", 7);
    inj.VirtualAddress = newSecVA;
    inj.PointerToRawData = newSecRaw;
    inj.Misc.VirtualSize = 0x3000;
    inj.SizeOfRawData = AlignValue(inj.Misc.VirtualSize, fAlign);
    inj.Characteristics = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_CNT_CODE;
    nth.FileHeader.NumberOfSections++;
    nth.OptionalHeader.SizeOfImage = newSecVA + AlignValue(inj.Misc.VirtualSize, sAlign);
    f.clear();
    f.seekp(dosH.e_lfanew, std::ios::beg);
    f.write(reinterpret_cast<const char*>(&nth), sizeof(nth));
    f.write(reinterpret_cast<const char*>(secs.data()), secCount * sizeof(IMAGE_SECTION_HEADER));
    f.write(reinterpret_cast<const char*>(&inj), sizeof(inj));
    f.seekp(inj.PointerToRawData, std::ios::beg);
    std::vector<char> blank(inj.SizeOfRawData, 0);
    f.write(blank.data(), blank.size());
    f.close();
    DebugLogger::Log(DebugLogger::INFO, "InjectInjsec: Succeeded.");
    return true;
}

// Line ~454: RunPatch starts
void RunPatch() {
    DebugLogger::Log(DebugLogger::INFO, "RunPatch: Executing in-game modifications...");
    HANDLE hProcess = GetCurrentProcess();
    DWORD_PTR patchAddr = FindPatchAddress(PATCH_SIGNATURE, PATCH_SIZE);
    if (!patchAddr) {
        DebugLogger::Log(DebugLogger::CRITICAL, "RunPatch: Could not find patch address!");
        MessageBoxA(nullptr, "[-] Could not find patch address!", "Error", MB_ICONERROR);
        return;
    }
    PatchMultiplayerLobby(hProcess, (LPVOID)patchAddr);
}

bool PatchSoulstorm(const std::string& path) {
    DebugLogger::Log(DebugLogger::INFO, "PatchSoulstorm: Starting patch for %s", path.c_str());
    if (!EnableLAA(path)) {
        DebugLogger::Log(DebugLogger::CRITICAL, "PatchSoulstorm: EnableLAA failed.");
        return false;
    }
    DebugLogger::Log(DebugLogger::INFO, "[OK] EnableLAA applied successfully.");
    if (!InjectInjsec(path)) {
        DebugLogger::Log(DebugLogger::CRITICAL, "PatchSoulstorm: InjectInjsec failed.");
        return false;
    }
    DebugLogger::Log(DebugLogger::INFO, "[OK] InjectInjsec applied successfully.");
    DebugLogger::Log(DebugLogger::INFO, "[->] Executing RunPatch() for runtime modifications...");
    RunPatch();
    DebugLogger::Log(DebugLogger::INFO, "[OK] RunPatch() executed successfully.");
    DebugLogger::Log(DebugLogger::INFO, "PatchSoulstorm: All patch steps completed.");
    return true;
}

std::wstring GetSoulstormPath() {
    HKEY hKey;
    const wchar_t* regPath = L"SOFTWARE\\WOW6432Node\\THQ\\Dawn of War - Soulstorm";
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, regPath, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        throw std::runtime_error("Failed to open registry key for Soulstorm");
    }
    wchar_t buffer[MAX_PATH];
    DWORD bufferSize = sizeof(buffer);
    DWORD type = REG_SZ;
    if (RegQueryValueExW(hKey, L"InstallLocation", 0, &type, (LPBYTE)buffer, &bufferSize) != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        throw std::runtime_error("Failed to read InstallLocation from registry");
    }
    RegCloseKey(hKey);
    std::wstring path = buffer;
    if (!path.empty() && path.back() != L'\\') {
        path += L'\\';
    }
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        throw std::runtime_error("Soulstorm installation directory not found");
    }
    return path;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    DebugLogger::Init();
    DebugLogger::Log(DebugLogger::INFO, "WinMain started.");
    if (!IsRunningAsAdmin()) {
        DebugLogger::Log(DebugLogger::CRITICAL, "Not running as admin. Relaunching...");
        RelaunchAsAdmin();
        return 0;
    }
    std::string soulExe = PickSoulstormExe();
    if (soulExe.empty()) {
        DebugLogger::Log(DebugLogger::CRITICAL, "No EXE selected by user.");
        MessageBoxA(nullptr, "No EXE selected.", "Error", MB_ICONERROR);
        return 1;
    }
    DebugLogger::Log(DebugLogger::INFO, "User selected EXE: %s", soulExe.c_str());
    try {
        DebugLogger::Log(DebugLogger::INFO, "Getting Soulstorm path from registry...");
        std::wstring soulstormPath;
        try {
            soulstormPath = GetSoulstormPath();
            DebugLogger::Log(DebugLogger::INFO, "Found Soulstorm installation path in registry");
        }
        catch (const std::exception& e) {
            DebugLogger::Log(DebugLogger::CRITICAL, "Failed to get Soulstorm path: %s", e.what());
            MessageBoxA(nullptr, "Failed to locate Soulstorm installation in registry.", "Error", MB_ICONERROR);
            return 1;
        }
        const std::wstring gdiHookPath = soulstormPath + L"GDI HOOKING DLL.dll";
        if (GetFileAttributesW(gdiHookPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
            DebugLogger::Log(DebugLogger::CRITICAL, "GDI HOOKING DLL.dll not found in Soulstorm directory");
            MessageBoxA(nullptr, "GDI HOOKING DLL.dll not found in Soulstorm directory.", "Error", MB_ICONERROR);
            return 1;
        }
        DebugLogger::Log(DebugLogger::INFO, "Loading GDI Hooking DLL...");
        DllHandle gdiHookDll = loadDll(gdiHookPath);
        if (!gdiHookDll.get()) {
            DebugLogger::Log(DebugLogger::CRITICAL, "Failed to load GDI HOOKING DLL");
            MessageBoxA(nullptr, "Failed to load GDI HOOKING DLL.", "Error", MB_ICONERROR);
            return 1;
        }
        if (!PatchSoulstorm(soulExe)) {
            DebugLogger::Log(DebugLogger::CRITICAL, "PatchSoulstorm failed.");
            MessageBoxA(nullptr, "Patch failed – multiplayer net-lobby might remain hidden.", "Patch Error", MB_ICONERROR);
            return 1;
        }
        MessageBoxA(nullptr,
            "Soulstorm Patch Applied:\n\n"
            " - LAA enabled\n"
            " - .injsec added\n"
            " - Multiplayer lobby JNZ patched\n"
            " - Custom memory functions in .injsec\n\n"
            "Success!",
            "Done", MB_OK);
        DebugLogger::Log(DebugLogger::INFO, "All done.");
    }
    catch (const std::exception& e) {
        DebugLogger::Log(DebugLogger::CRITICAL, "Exception caught: %s", e.what());
        MessageBoxA(nullptr, e.what(), "Error", MB_ICONERROR);
        return 1;
    }
    catch (...) {
        DebugLogger::Log(DebugLogger::CRITICAL, "Unknown exception caught.");
        MessageBoxA(nullptr, "Unknown error occurred.", "Error", MB_ICONERROR);
        return 1;
    }
    DebugLogger::Cleanup();
    return 0;
}

DWORD_PTR FindPatchAddress(const BYTE* signature, size_t size) {
    MEMORY_BASIC_INFORMATION mbi;
    BYTE* base = nullptr;
    while (VirtualQuery(base, &mbi, sizeof(mbi))) {
        if (mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS))) {
            BYTE* found = FindPattern((BYTE*)mbi.BaseAddress, mbi.RegionSize, signature, "xx");
            if (found) return (DWORD_PTR)found;
        }
        base = (BYTE*)mbi.BaseAddress + mbi.RegionSize;
    }
    return 0;
}