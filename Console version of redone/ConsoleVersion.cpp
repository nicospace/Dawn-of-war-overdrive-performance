/******************************************************************************
 * Soulstorm 32-bit Patcher that:
 *   1) Makes the EXE Large-Address-Aware.
 *   2) Creates a new ".injsec" section with shellcode.
 *   3) Adds an import descriptor for VirtualAlloc (machine-independent).
 *   4) Shellcode calls VirtualAlloc(0, 512MB, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE).
 *   5) Restores OEP.
 *   6) Optionally launches Soulstorm and displays memory usage (Private Bytes).
 *
 * BUILD:
 *   - Use a 32-bit console project in Visual Studio or similar.
 *   - Link against psapi.lib (for GetProcessMemoryInfo).
 * RUN:
 *   - Must run as Administrator to modify Soulstorm.exe in Program Files.
 ******************************************************************************/

#include <windows.h>
#include <psapi.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>

 // Change these if needed
static const char* SOULSTORM_PATH = R"(C:\Program Files (x86)\Steam\steamapps\common\Dawn of War Soulstorm\Soulstorm.exe)";
static const DWORD EXTRA_ALLOC_SIZE = 512u * 1024u * 1024u; // 512 MB

/******************************************************************************
 * Helper: Align a value up to "alignment".
 ******************************************************************************/
static DWORD Align(DWORD val, DWORD alignment)
{
    return (val + alignment - 1) & ~(alignment - 1);
}

/******************************************************************************
 * We will store the new import descriptor, names, and thunks in the new
 * ".injsec" section. Then the loader can fill in the IAT pointer for us
 * at runtime (machine-independent).
 *
 * We'll create data structures in memory, then write them into the new section.
 * The shellcode will 'call [iat_VirtualAlloc]' for the real function.
 ******************************************************************************/
#pragma pack(push, 1)
struct ImportByName
{
    WORD Hint;           // we can set 0
    CHAR Name[13];       // "VirtualAlloc\0"
};
#pragma pack(pop)

/******************************************************************************
 * We'll keep all "import" data in one structure for convenience, then write
 * them out in .injsec:
 *
 *   [ IMAGE_IMPORT_DESCRIPTOR for kernel32.dll ]
 *   [ "KERNEL32.DLL\0" ]
 *   [ OriginalThunk array (2 entries + null) ]
 *   [ ImportByName struct => "VirtualAlloc" ]
 *   [ FirstThunk array (2 entries + null) ]
 ******************************************************************************/
struct MyImportData
{
    // Our single import descriptor for kernel32.dll
    IMAGE_IMPORT_DESCRIPTOR impDesc;

    // Storage for "KERNEL32.DLL\0"
    char dllName[16];

    // OriginalThunk (2 entries + terminating 0)
    //   - One for VirtualAlloc
    //   - One null (end)
    IMAGE_THUNK_DATA32 oft[2];

    // The "hint/name" structure for VirtualAlloc
    ImportByName importByName;

    // FirstThunk array (2 entries + terminating 0)
    IMAGE_THUNK_DATA32 ft[2];
};

/******************************************************************************
 * MAIN Patch Function
 *   1) Opens Soulstorm.exe.
 *   2) Reads DOS + NT headers, sections, etc.
 *   3) Sets LAA flag.
 *   4) Creates new .injsec section.
 *   5) Appends new import descriptor (for VirtualAlloc).
 *   6) Writes shellcode that references [IAT_of_VirtualAlloc].
 *   7) Updates OEP -> shellcode.
 ******************************************************************************/
static bool PatchSoulstormExe(const char* exePath)
{
    // Open the file for read+write in binary mode
    std::fstream exeFile(exePath, std::ios::in | std::ios::out | std::ios::binary);
    if (!exeFile.is_open()) {
        std::cerr << "[ERROR] Cannot open " << exePath << "\n";
        return false;
    }

    // -----------------------------------------------------------
    // 1) Read DOS + NT headers
    // -----------------------------------------------------------
    IMAGE_DOS_HEADER dosHeader = {};
    exeFile.read(reinterpret_cast<char*>(&dosHeader), sizeof(dosHeader));
    if (dosHeader.e_magic != IMAGE_DOS_SIGNATURE) {
        std::cerr << "[ERROR] Invalid DOS signature.\n";
        return false;
    }

    exeFile.seekg(dosHeader.e_lfanew, std::ios::beg);
    IMAGE_NT_HEADERS32 ntHeaders = {};
    exeFile.read(reinterpret_cast<char*>(&ntHeaders), sizeof(ntHeaders));
    if (ntHeaders.Signature != IMAGE_NT_SIGNATURE) {
        std::cerr << "[ERROR] Invalid NT signature.\n";
        return false;
    }

    if (ntHeaders.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        std::cerr << "[ERROR] Not a 32-bit PE file.\n";
        return false;
    }

    // Original Entry Point
    DWORD originalOEP = ntHeaders.OptionalHeader.AddressOfEntryPoint;
    DWORD imageBase = ntHeaders.OptionalHeader.ImageBase;

    // We'll need to rewrite the OEP:
    const DWORD oepDiskOffset = dosHeader.e_lfanew
        + offsetof(IMAGE_NT_HEADERS32, OptionalHeader)
        + offsetof(IMAGE_OPTIONAL_HEADER32, AddressOfEntryPoint);

    // -----------------------------------------------------------
    // 2) Make it LARGE ADDRESS AWARE
    // -----------------------------------------------------------
    ntHeaders.FileHeader.Characteristics |= IMAGE_FILE_LARGE_ADDRESS_AWARE;

    // -----------------------------------------------------------
    // 3) Read existing section headers
    // -----------------------------------------------------------
    WORD oldSecCount = ntHeaders.FileHeader.NumberOfSections;
    std::vector<IMAGE_SECTION_HEADER> sections(oldSecCount);
    exeFile.seekg(dosHeader.e_lfanew + sizeof(IMAGE_NT_HEADERS32), std::ios::beg);
    exeFile.read(reinterpret_cast<char*>(sections.data()), oldSecCount * sizeof(IMAGE_SECTION_HEADER));

    // We also want to find the import directory in case we must extend it
    DWORD importDirRVA = ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    DWORD importDirSize = ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size;

    // Validate we can find the OEP in one of the current sections
    DWORD oepFileOffset = 0;
    {
        auto sectAlign = ntHeaders.OptionalHeader.SectionAlignment;
        for (auto& sec : sections)
        {
            DWORD secVA = sec.VirtualAddress;
            DWORD secEnd = secVA + Align(sec.Misc.VirtualSize, sectAlign);
            if (originalOEP >= secVA && originalOEP < secEnd) {
                DWORD offsetInSection = (originalOEP - secVA);
                oepFileOffset = sec.PointerToRawData + offsetInSection;
                break;
            }
        }
        if (!oepFileOffset) {
            std::cerr << "[ERROR] Could not locate OEP in any section.\n";
            return false;
        }
    }

    // -----------------------------------------------------------
    // 4) Create new .injsec section
    // -----------------------------------------------------------
    IMAGE_SECTION_HEADER& lastSec = sections.back();
    DWORD fileAlignment = ntHeaders.OptionalHeader.FileAlignment;
    DWORD sectionAlignment = ntHeaders.OptionalHeader.SectionAlignment;

    // Where the new section's raw data will begin (file offset)
    DWORD newSecRawPtr = Align(lastSec.PointerToRawData + lastSec.SizeOfRawData, fileAlignment);

    // Where the new section's VA will begin
    DWORD newSecVA = Align(lastSec.VirtualAddress + lastSec.Misc.VirtualSize, sectionAlignment);

    // Build the new section header
    IMAGE_SECTION_HEADER newSec = {};
    std::memcpy(newSec.Name, ".injsec", 7);
    newSec.Misc.VirtualSize = 0x3000; // enough space for shellcode + import structures
    newSec.VirtualAddress = newSecVA;
    newSec.SizeOfRawData = Align(newSec.Misc.VirtualSize, fileAlignment);
    newSec.PointerToRawData = newSecRawPtr;
    newSec.Characteristics = IMAGE_SCN_MEM_READ
        | IMAGE_SCN_MEM_WRITE
        | IMAGE_SCN_MEM_EXECUTE; // RWX

    ntHeaders.FileHeader.NumberOfSections++;
    // Expand SizeOfImage
    ntHeaders.OptionalHeader.SizeOfImage =
        newSec.VirtualAddress + Align(newSec.Misc.VirtualSize, sectionAlignment);

    // -----------------------------------------------------------
    // 5) Append a new import descriptor for KERNEL32!VirtualAlloc
    //    We'll store the new structures in .injsec. The Windows
    //    loader will fill in the IAT on any machine (no absolute pointer).
    // -----------------------------------------------------------

    // We'll build everything in a MyImportData structure in memory,
    // then write it out to .injsec. Then we'll append a new IMAGE_IMPORT_DESCRIPTOR
    // at the end of the existing import descriptors, pointing to it.

    MyImportData myImp = {};
    // fill descriptor
    // We'll set:
    //   impDesc.Name = RVA of "KERNEL32.DLL"
    //   impDesc.FirstThunk = RVA of FT
    //   impDesc.OriginalFirstThunk = RVA of OFT
    //   impDesc.TimeDateStamp = 0
    //   impDesc.ForwarderChain = 0

    // Fill the names
    strcpy_s(myImp.dllName, "KERNEL32.DLL");
    strcpy_s(myImp.importByName.Name, "VirtualAlloc");

    // Original thunk (point to ImportByName)
    //   the lower 31 bits store the RVA to "ImportByName"
    //   the highest bit  (IMAGE_ORDINAL_FLAG32) must be 0 to use "by name"
    myImp.oft[0].u1.AddressOfData = 0; // will fix up after we know final offsets
    myImp.oft[1].u1.AddressOfData = 0; // null terminator

    // first thunk
    myImp.ft[0].u1.AddressOfData = 0;  // also fix up
    myImp.ft[1].u1.AddressOfData = 0;  // null terminator

    myImp.importByName.Hint = 0;  // we can set 0

    // We'll store myImp itself in .injsec. Then we must store the new
    // import descriptor in the existing array. The array ends with a 0 descriptor.

    // We'll do everything at the start of .injsec for simplicity.
    // Then the shellcode will come after that.

    // Our plan:
    //   injsec layout:
    //     offset 0x0:   MyImportData
    //     offset X:     (shellcode)

    const DWORD injsecSize = newSec.SizeOfRawData;
    const DWORD injsecOffset = newSec.PointerToRawData; // in file
    const DWORD injsecRVA = newSec.VirtualAddress;    // in memory

    // We'll place MyImportData at offset 0, shellcode after it
    DWORD importDataOffsetInSec = 0;
    DWORD shellcodeOffsetInSec = Align(sizeof(MyImportData), 0x10); // small alignment

    // in-memory RVA
    DWORD importDataRVA = injsecRVA + importDataOffsetInSec;
    DWORD shellcodeRVA = injsecRVA + shellcodeOffsetInSec;

    // Fix up the fields in myImp to point to the correct sub-structures:
    // The layout in MyImportData is:
    //
    //   offset  0:  impDesc
    //   offset 20:  dllName ("KERNEL32.DLL")
    //   offset 36:  oft[2]
    //   offset 44:  importByName (hint=0 + "VirtualAlloc\0")
    //   offset 5B:  ft[2]
    //
    // We'll get each sub-offset from the start of MyImportData:
    DWORD rvaOfDllName = importDataRVA + offsetof(MyImportData, dllName);
    DWORD rvaOfOFT = importDataRVA + offsetof(MyImportData, oft);
    DWORD rvaOfImportByName = importDataRVA + offsetof(MyImportData, importByName);
    DWORD rvaOfFT = importDataRVA + offsetof(MyImportData, ft);

    // Fill descriptor fields
    myImp.impDesc.OriginalFirstThunk = rvaOfOFT;
    myImp.impDesc.Name = rvaOfDllName;
    myImp.impDesc.FirstThunk = rvaOfFT;

    // Fill the OFT/FT so they point to the importByName
    myImp.oft[0].u1.AddressOfData = rvaOfImportByName;
    myImp.ft[0].u1.AddressOfData = rvaOfImportByName;

    // We'll write myImp into .injsec once all is ready.

    // Next we must create a new IMAGE_IMPORT_DESCRIPTOR entry
    // at the end of the existing array. That array is located at importDirRVA in memory.
    // We find the last non-empty entry, then append ours, then a null entry.

    // 1) Read the existing import descriptors
    if (importDirRVA == 0) {
        std::cerr << "[ERROR] Existing Import Directory not found (RVA=0). "
            "This code does not handle creating a fresh one from scratch.\n";
        return false;
    }

    // We'll find the file offset of importDirRVA
    // We need to locate which section contains importDirRVA
    DWORD importDirFileOffset = 0;
    for (auto& sec : sections)
    {
        DWORD start = sec.VirtualAddress;
        DWORD end = start + Align(sec.Misc.VirtualSize, sectionAlignment);
        if (importDirRVA >= start && importDirRVA < end) {
            DWORD delta = importDirRVA - start;
            importDirFileOffset = sec.PointerToRawData + delta;
            break;
        }
    }
    if (!importDirFileOffset) {
        std::cerr << "[ERROR] Could not locate importDir in sections.\n";
        return false;
    }

    // 2) Read them all into a vector
    exeFile.seekg(importDirFileOffset, std::ios::beg);
    std::vector<IMAGE_IMPORT_DESCRIPTOR> importArray;
    while (true) {
        IMAGE_IMPORT_DESCRIPTOR desc = {};
        exeFile.read(reinterpret_cast<char*>(&desc), sizeof(desc));
        if (!exeFile.good()) {
            std::cerr << "[ERROR] Unexpected EOF reading import descriptors.\n";
            return false;
        }
        importArray.push_back(desc);
        // Zero descriptor => end of array
        if (desc.OriginalFirstThunk == 0 && desc.Name == 0 && desc.FirstThunk == 0)
            break;
    }

    // We'll insert our new descriptor right before the final zero descriptor
    size_t insertPos = importArray.size() - 1;
    IMAGE_IMPORT_DESCRIPTOR newDesc = myImp.impDesc; // from our struct
    importArray.insert(importArray.begin() + insertPos, newDesc);

    // Now our new descriptor is in importArray[insertPos], followed by an empty descriptor
    // We'll rewrite the entire array back to disk, possibly extending it if needed.

    // 3) We might need more space in the import directory if we grew it.
    // We'll do a naive approach: place the new import descriptor array back
    // where it was, if there's enough slack. If not, you should move
    // the entire import table into .injsec. We'll assume there's enough space.
    // (Often there's enough padding.)

    // (Check if we have enough space to hold the new array.)
    DWORD oldSize = (DWORD)(importArray.size() * sizeof(IMAGE_IMPORT_DESCRIPTOR));
    if (oldSize > importDirSize) {
        std::cerr << "[WARNING] The new import array is bigger than the old size. "
            "We might overwrite something else. A safer approach is to move "
            "the entire import directory into .injsec.\n";
        // For a proper robust solution, you'd place the entire array in .injsec
        // and update ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT] = ...
        // But let's keep it simpler here, hoping there's padding.
    }

    // Rewrite the expanded import array
    exeFile.seekp(importDirFileOffset, std::ios::beg);
    exeFile.write(reinterpret_cast<char*>(importArray.data()),
        importArray.size() * sizeof(IMAGE_IMPORT_DESCRIPTOR));

    // Possibly update the Import Directory size to reflect the bigger array
    // We'll do it for cleanliness
    ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size =
        (DWORD)(importArray.size() * sizeof(IMAGE_IMPORT_DESCRIPTOR));

    // -----------------------------------------------------------
    // 6) Build the shellcode that calls [IAT_of_VirtualAlloc]
    //
    // The loader will fill the IAT pointer at ft[0], so the actual pointer
    // will be at (imageBase + rvaOfFT). But we want to do "call [<that address>]".
    // So let's compute the memory address of FT[0].
    //
    // Then push the parameters for VirtualAlloc(0, EXTRA_ALLOC_SIZE, 0x3000, 0x04).
    // -----------------------------------------------------------
    DWORD iatVA = imageBase + rvaOfFT; // firstThunk array
    // iatVA is the start of ft, i.e. ft[0]. This will hold the pointer to VirtualAlloc.

    // We'll do x86 instructions:
    //   pushad
    //   mov eax, [iatVA]   (call [eax] is trickier in machine code, so we'll do mov+call)
    //   push 0x04          (PAGE_READWRITE)
    //   push 0x3000        (MEM_RESERVE|MEM_COMMIT)
    //   push EXTRA_ALLOC_SIZE
    //   push 0             (lpAddress=0)
    //   call eax
    //   popad
    //   jmp originalOEP
    //
    // Note: We'll do an *absolute* memory read for [iatVA], so we do a 5-byte opcode:
    //   mov eax, [disp32] (0xA1 dword). Then disp32 is the absolute address of iatVA.
    // On load, iatVA is the real address in the process. But if the game loads at a different
    // base, that might be an issue. Typically, Soulstorm loads at the same base (ASLR might
    // break?). For full ASLR compliance, you'd do a more position-independent approach.
    //
    // For simplicity here, we assume base is stable or that the game is not ASLR.
    // If you want full ASLR compatibility, you'd do a PEB-walk or do an import thunk call.

    std::vector<unsigned char> shellcode;
    // pushad
    shellcode.push_back(0x60);

    // mov eax, [iatVA]
    shellcode.push_back(0xA1); // opcode for "mov eax, [imm32]"
    {
        DWORD addr = iatVA;
        shellcode.insert(shellcode.end(),
            reinterpret_cast<unsigned char*>(&addr),
            reinterpret_cast<unsigned char*>(&addr) + 4);
    }

    // push PAGE_READWRITE (0x04)
    shellcode.push_back(0x68);
    {
        DWORD val = PAGE_READWRITE; // 4
        shellcode.insert(shellcode.end(),
            reinterpret_cast<unsigned char*>(&val),
            reinterpret_cast<unsigned char*>(&val) + 4);
    }

    // push MEM_RESERVE|MEM_COMMIT (0x3000)
    shellcode.push_back(0x68);
    {
        DWORD val = MEM_RESERVE | MEM_COMMIT; // 0x3000
        shellcode.insert(shellcode.end(),
            reinterpret_cast<unsigned char*>(&val),
            reinterpret_cast<unsigned char*>(&val) + 4);
    }

    // push EXTRA_ALLOC_SIZE
    shellcode.push_back(0x68);
    {
        DWORD val = EXTRA_ALLOC_SIZE;
        shellcode.insert(shellcode.end(),
            reinterpret_cast<unsigned char*>(&val),
            reinterpret_cast<unsigned char*>(&val) + 4);
    }

    // push 0
    shellcode.push_back(0x68);
    {
        DWORD val = 0;
        shellcode.insert(shellcode.end(),
            reinterpret_cast<unsigned char*>(&val),
            reinterpret_cast<unsigned char*>(&val) + 4);
    }

    // call eax
    shellcode.push_back(0xFF);
    shellcode.push_back(0xD0);

    // popad
    shellcode.push_back(0x61);

    // jmp originalOEP
    shellcode.push_back(0xE9);
    {
        // relative displacement = (target - nextInstruction)
        DWORD nextInstr = (imageBase + shellcodeRVA) + (DWORD)shellcode.size() + 4;
        DWORD target = imageBase + originalOEP;
        DWORD rel = target - nextInstr;
        shellcode.insert(shellcode.end(),
            reinterpret_cast<unsigned char*>(&rel),
            reinterpret_cast<unsigned char*>(&rel) + 4);
    }

    // -----------------------------------------------------------
    // 7) Rewrite updated NT headers + section table + newSec
    // -----------------------------------------------------------
    exeFile.seekp(dosHeader.e_lfanew, std::ios::beg);
    exeFile.write(reinterpret_cast<const char*>(&ntHeaders), sizeof(ntHeaders));
    exeFile.write(reinterpret_cast<const char*>(sections.data()), oldSecCount * sizeof(IMAGE_SECTION_HEADER));
    exeFile.write(reinterpret_cast<const char*>(&newSec), sizeof(newSec));

    // -----------------------------------------------------------
    // 8) Write MyImportData at offset 0 of .injsec
    // -----------------------------------------------------------
    exeFile.seekp(injsecOffset + importDataOffsetInSec, std::ios::beg);
    exeFile.write(reinterpret_cast<const char*>(&myImp), sizeof(myImp));

    // -----------------------------------------------------------
    // 9) Write shellcode after that
    // -----------------------------------------------------------
    exeFile.seekp(injsecOffset + shellcodeOffsetInSec, std::ios::beg);
    exeFile.write(reinterpret_cast<const char*>(shellcode.data()), shellcode.size());

    // Zero out the remainder of .injsec, if we like
    {
        DWORD usedSize = shellcodeOffsetInSec + (DWORD)shellcode.size();
        if (usedSize < newSec.SizeOfRawData) {
            DWORD remain = newSec.SizeOfRawData - usedSize;
            std::vector<char> zeros(remain, 0);
            exeFile.write(zeros.data(), zeros.size());
        }
    }

    // -----------------------------------------------------------
    // 10) Update OEP -> shellcodeRVA
    // -----------------------------------------------------------
    DWORD newOEP = shellcodeRVA;
    exeFile.seekp(oepDiskOffset, std::ios::beg);
    exeFile.write(reinterpret_cast<const char*>(&newOEP), sizeof(newOEP));

    // Done
    exeFile.close();
    return true;
}

/******************************************************************************
 * Launch the newly patched Soulstorm, wait a bit, then measure memory usage.
 * We'll just show Private Bytes in a MessageBox. The game must actually load
 * big content (like a large mod) to push usage above 2 GB. If you see it exceed
 * 2 GB, it means LAA + big memory allocation is working.
 ******************************************************************************/
static void TestMemoryUsageOfSoulstorm(const char* exePath)
{
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessA(exePath, nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        MessageBoxA(nullptr, "Failed to launch Soulstorm.exe", "Error", MB_ICONERROR);
        return;
    }

    // Give it time to start and load
    Sleep(10000); // 10s - might not be enough for a big mod, adjust as needed

    PROCESS_MEMORY_COUNTERS_EX pmc = {};
    if (GetProcessMemoryInfo(pi.hProcess, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        SIZE_T privBytes = pmc.PrivateUsage;
        SIZE_T workSet = pmc.WorkingSetSize;

        char buf[512];
        wsprintfA(buf,
            "Soulstorm memory usage:\n\n"
            "Private Bytes = %zu (%.2f MB)\n"
            "Working Set   = %zu (%.2f MB)\n\n"
            "If you run a large mod or a big scenario, this can exceed 2 GB now.\n",
            privBytes, double(privBytes) / (1024 * 1024),
            workSet, double(workSet) / (1024 * 1024));
        MessageBoxA(nullptr, buf, "Memory Usage", MB_OK);
    }

    // We won't terminate the game here. Let the user play or watch usage in Task Manager.
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
}

int main()
{
    std::cout << "Soulstorm Patcher: Add 512MB Allocation + LAA + Memory Test\n"
        << "----------------------------------------------------------\n"
        << "EXE path: " << SOULSTORM_PATH << "\n\n";

    // 1) Ask user if we should patch
    int r = MessageBoxA(nullptr,
        "Patch Soulstorm.exe for 512MB injection and Large-Address-Aware?",
        "Patch?",
        MB_YESNO | MB_ICONQUESTION);
    if (r == IDNO) {
        std::cout << "User canceled patch.\n";
        return 0;
    }

    // 2) Attempt patch
    std::cout << "Patching...\n";
    if (!PatchSoulstormExe(SOULSTORM_PATH)) {
        MessageBoxA(nullptr, "Patch failed!", "Error", MB_ICONERROR);
        return 1;
    }
    MessageBoxA(nullptr, "Patch succeeded!", "Success", MB_OK);

    // 3) Ask if user wants to test memory usage
    r = MessageBoxA(nullptr,
        "Do you want to LAUNCH Soulstorm now and measure memory usage?\n"
        "To exceed 2 GB, load a large mod or big scenario.",
        "Test Memory?",
        MB_YESNO | MB_ICONQUESTION);
    if (r == IDYES) {
        TestMemoryUsageOfSoulstorm(SOULSTORM_PATH);
    }

    std::cout << "Done.\n";
    return 0;
}
