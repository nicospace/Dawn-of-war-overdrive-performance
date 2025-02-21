#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <string>

// Address to patch: Example target for multiplayer check removal
#define PATCH_ADDRESS 0x007D1496

int main()
{
    // Print a header so we know the program launched
    printf("=== Soulstorm Lobby Patcher ===\n");

    // Acquire handle to our own process
    HANDLE hProcess = GetCurrentProcess();
    if (!hProcess) {
        printf("[-] Failed to open process!\n");
        system("pause");
        return 1;
    }

    // Query memory protection before modifying
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery((LPCVOID)PATCH_ADDRESS, &mbi, sizeof(mbi)) == 0) {
        printf("[-] VirtualQuery() failed! Address is invalid.\n");
        system("pause");
        return 1;
    }

    // Show memory info
    printf("Memory Info:\n");
    printf("  Base Address: 0x%p\n", mbi.BaseAddress);
    printf("  Region Size:  0x%X\n", (unsigned)mbi.RegionSize);
    printf("  Allocation Protect: 0x%X\n", mbi.AllocationProtect);
    printf("  State: 0x%X\n", mbi.State);
    printf("  Protect: 0x%X\n", mbi.Protect);
    printf("  Type: 0x%X\n", mbi.Type);

    // 1) Attempt to change memory protection to PAGE_EXECUTE_READWRITE
    DWORD oldProtect;
    if (VirtualProtect(mbi.BaseAddress, mbi.RegionSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        // 2) Overwrite instruction bytes with NOP NOP
        BYTE patchBytes[2] = { 0x90, 0x90 };
        SIZE_T bytesWritten;
        if (WriteProcessMemory(hProcess, (LPVOID)PATCH_ADDRESS, patchBytes, sizeof(patchBytes), &bytesWritten)) {
            // 3) Restore original protection
            VirtualProtect(mbi.BaseAddress, mbi.RegionSize, oldProtect, &oldProtect);
            printf("[+] Successfully patched multiplayer lobby!\n");
            system("pause");
            return 0;
        }
    }

    // 4) If VirtualProtect fails, attempt an alternative patch method
    printf("[-] Failed to set memory protection. Attempting alternative patching...\n");

    // Alternative: Allocate new memory, then write a jump from the original address
    BYTE jumpPatch[5] = { 0xE9 };
    LPVOID newMemory = VirtualAlloc(nullptr, 5, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (newMemory) {
        // Calculate relative jump to newMemory
        *(DWORD*)(jumpPatch + 1) = (DWORD)newMemory - (DWORD)PATCH_ADDRESS - 5;

        // Write the jump patch into the original address
        if (WriteProcessMemory(hProcess, (LPVOID)PATCH_ADDRESS, jumpPatch, sizeof(jumpPatch), nullptr)) {
            printf("[+] Successfully patched using alternative method!\n");
            system("pause");
            return 0;
        }
    }

    printf("[-] Final patching attempt failed!\n");
    system("pause");
    return 1;
}
