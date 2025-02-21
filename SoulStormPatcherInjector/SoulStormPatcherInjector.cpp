#include <windows.h>
#include <shlwapi.h>
#include <fstream>
#include <vector>
#include <string>

#pragma comment(lib, "shlwapi.lib")

#pragma pack(push,1)
struct TLSCallbacks
{
    DWORD callback1;
    DWORD terminator;
};
#pragma pack(pop)

static DWORD AlignValue(DWORD val, DWORD alignment)
{
    return (val + alignment - 1) & ~(alignment - 1);
}

static bool PatchSoulstorm(const std::string& exePath)
{
    // 1) open
    std::fstream file(exePath, std::ios::in | std::ios::out | std::ios::binary);
    if (!file.is_open())
    {
        MessageBoxA(nullptr, "Cannot open Soulstorm.exe!", "Error", MB_ICONERROR);
        return false;
    }

    // 2) read DOS + PE
    IMAGE_DOS_HEADER dosH = {};
    file.read(reinterpret_cast<char*>(&dosH), sizeof(dosH));
    if (dosH.e_magic != IMAGE_DOS_SIGNATURE)
    {
        file.close();
        return false;
    }
    file.seekg(dosH.e_lfanew, std::ios::beg);
    IMAGE_NT_HEADERS32 ntH = {};
    file.read(reinterpret_cast<char*>(&ntH), sizeof(ntH));
    if (ntH.Signature != IMAGE_NT_SIGNATURE || ntH.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC)
    {
        file.close();
        return false;
    }

    // Mark LAA
    ntH.FileHeader.Characteristics |= IMAGE_FILE_LARGE_ADDRESS_AWARE;

    // read sections
    WORD secCount = ntH.FileHeader.NumberOfSections;
    std::vector<IMAGE_SECTION_HEADER> sections(secCount);
    file.read(reinterpret_cast<char*>(sections.data()), secCount * sizeof(IMAGE_SECTION_HEADER));

    auto AlignSec = [&](DWORD x, DWORD a) { return (x + a - 1) & ~(a - 1); };

    IMAGE_SECTION_HEADER& lastSec = sections.back();
    DWORD fAlign = ntH.OptionalHeader.FileAlignment;
    DWORD sAlign = ntH.OptionalHeader.SectionAlignment;

    DWORD newSecRaw = AlignSec(lastSec.PointerToRawData + lastSec.SizeOfRawData, fAlign);
    DWORD newSecVA = AlignSec(lastSec.VirtualAddress + lastSec.Misc.VirtualSize, sAlign);

    // create .injsec
    IMAGE_SECTION_HEADER newSec = {};
    memcpy(newSec.Name, ".injsec", 7);
    newSec.VirtualAddress = newSecVA;
    newSec.PointerToRawData = newSecRaw;
    newSec.Misc.VirtualSize = 0x3000;
    newSec.SizeOfRawData = AlignSec(newSec.Misc.VirtualSize, fAlign);
    newSec.Characteristics = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE;

    ntH.FileHeader.NumberOfSections++;
    ntH.OptionalHeader.SizeOfImage =
        newSecVA + AlignSec(newSec.Misc.VirtualSize, sAlign);

    // rewrite updated header
    file.clear();
    file.seekp(dosH.e_lfanew, std::ios::beg);
    file.write(reinterpret_cast<const char*>(&ntH), sizeof(ntH));
    file.write(reinterpret_cast<const char*>(sections.data()), secCount * sizeof(IMAGE_SECTION_HEADER));
    file.write(reinterpret_cast<const char*>(&newSec), sizeof(newSec));

    DWORD injsecRaw = newSec.PointerToRawData;
    DWORD injsecRVA = newSec.VirtualAddress;
    DWORD injsecVA = ntH.OptionalHeader.ImageBase + injsecRVA;

    // fill new section w/ zeros
    file.seekp(injsecRaw, std::ios::beg);
    std::vector<char> blank(newSec.SizeOfRawData, 0);
    file.write(blank.data(), blank.size());

    // We'll store "MemoryPool.dll" at offset 0x500
    const DWORD off_TLSCode = 0x100; // our callback code
    const DWORD off_CBArray = 0x80;  // array of callbacks
    const DWORD off_TLSD = 0x50;  // TLS Directory
    const DWORD off_DLLString = 0x500;

    DWORD tlsCodeRVA = injsecRVA + off_TLSCode;
    DWORD tlsCodeVA = ntH.OptionalHeader.ImageBase + tlsCodeRVA;

    DWORD cbArrRVA = injsecRVA + off_CBArray;
    DWORD cbArrVA = ntH.OptionalHeader.ImageBase + cbArrRVA;

    DWORD tdirRVA = injsecRVA + off_TLSD;
    DWORD tdirVA = ntH.OptionalHeader.ImageBase + tdirRVA;

    // 1) store "MemoryPool.dll"
    file.seekp(injsecRaw + off_DLLString, std::ios::beg);
    {
        const char sDLL[] = "MemoryPool.dll";
        file.write(sDLL, sizeof(sDLL));
    }
    DWORD dllStrVA = ntH.OptionalHeader.ImageBase + injsecRVA + off_DLLString;

    // 2) define minimal callback code:
    //   pushad
    //   push dllStrVA
    //   mov eax, [LoadLibraryA]
    //   call eax
    //   popad
    //   ret
    std::vector<unsigned char> code;
    auto putB = [&](BYTE b) { code.push_back(b); };
    auto putD = [&](DWORD d) {
        unsigned char tmp[4];
        memcpy(tmp, &d, 4);
        code.insert(code.end(), tmp, tmp + 4);
        };

    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    FARPROC pLoadLib = k32 ? GetProcAddress(k32, "LoadLibraryA") : nullptr;
    DWORD addrLL = pLoadLib ? reinterpret_cast<DWORD>(pLoadLib) : 0;

    // pushad
    putB(0x60);
    // push dllStrVA
    putB(0x68); putD(dllStrVA);
    // mov eax, addrLL
    putB(0xB8); putD(addrLL);
    // call eax
    putB(0xFF); putB(0xD0);
    // popad
    putB(0x61);
    // ret
    putB(0xC3);

    // write code
    file.seekp(injsecRaw + off_TLSCode, std::ios::beg);
    file.write(reinterpret_cast<const char*>(code.data()), code.size());

    // 3) define callback array => [tlsCodeVA, 0]
    file.seekp(injsecRaw + off_CBArray, std::ios::beg);
    {
        TLSCallbacks cb;
        cb.callback1 = tlsCodeVA;
        cb.terminator = 0;
        file.write(reinterpret_cast<const char*>(&cb), sizeof(cb));
    }

    // 4) define minimal TLS directory
#pragma pack(push,1)
    struct IMAGE_TLS_DIRECTORY32_F {
        DWORD StartAddressOfRawData;
        DWORD EndAddressOfRawData;
        DWORD AddressOfIndex;
        DWORD AddressOfCallBacks;
        DWORD SizeOfZeroFill;
        DWORD Characteristics;
    };
#pragma pack(pop)

    IMAGE_TLS_DIRECTORY32_F tdir = {};
    // We only set AddressOfCallBacks => cbArrVA
    tdir.AddressOfCallBacks = cbArrVA;

    file.seekp(injsecRaw + off_TLSD, std::ios::beg);
    file.write(reinterpret_cast<const char*>(&tdir), sizeof(tdir));

    // 5) update DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS]
    ntH.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].VirtualAddress = tdirRVA;
    ntH.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].Size = sizeof(tdir);

    file.clear();
    file.seekp(dosH.e_lfanew, std::ios::beg);
    file.write(reinterpret_cast<const char*>(&ntH), sizeof(ntH));

    file.close();
    return true;
}

// Standard signature => no annotation warning
int APIENTRY WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    char path[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    PathRemoveFileSpecA(path);

    std::string exePath = std::string(path) + "\\Soulstorm.exe";
    if (!PatchSoulstorm(exePath))
    {
        MessageBoxA(nullptr, "Patch failed (Soulstorm.exe invalid or permission error).", "Error", MB_ICONERROR);
        return 1;
    }

    MessageBoxA(nullptr,
        "Soulstorm.exe patched:\n"
        " - Large Address Aware\n"
        " - TLS callback auto-loads MemoryPool.dll\n"
        " - No changes to net code => multiplayer works\n",
        "Done",
        MB_OK);

    return 0;
}
