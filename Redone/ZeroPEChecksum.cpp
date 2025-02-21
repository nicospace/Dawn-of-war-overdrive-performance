#include "ZeroPEChecksum.h"
#include "DebugLogger.h"
#include <fstream>
#include <windows.h>

// Implementation of ZeroPEChecksum
bool ZeroPEChecksum(const std::string& exePath) {
    DebugLogger::Log(DebugLogger::INFO, "ZeroPEChecksum: Opening file %s", exePath.c_str());

    std::fstream f(exePath, std::ios::in | std::ios::out | std::ios::binary);
    if (!f.is_open()) {
        DebugLogger::Log(DebugLogger::CRITICAL, "ZeroPEChecksum: Failed to open file.");
        return false;
    }

    IMAGE_DOS_HEADER dosH = {};
    f.read(reinterpret_cast<char*>(&dosH), sizeof(dosH));
    if (dosH.e_magic != IMAGE_DOS_SIGNATURE) {
        DebugLogger::Log(DebugLogger::CRITICAL, "ZeroPEChecksum: Invalid DOS signature.");
        f.close();
        return false;
    }

    f.seekg(dosH.e_lfanew, std::ios::beg);
    IMAGE_NT_HEADERS32 nth = {};
    f.read(reinterpret_cast<char*>(&nth), sizeof(nth));

    if (nth.Signature != IMAGE_NT_SIGNATURE ||
        nth.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        DebugLogger::Log(DebugLogger::CRITICAL, "ZeroPEChecksum: Invalid NT header.");
        f.close();
        return false;
    }

    nth.FileHeader.TimeDateStamp = 0;
    nth.OptionalHeader.CheckSum = 0;

    if (nth.OptionalHeader.NumberOfRvaAndSizes > IMAGE_DIRECTORY_ENTRY_DEBUG) {
        nth.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].VirtualAddress = 0;
        nth.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].Size = 0;
    }

    f.clear();
    f.seekp(dosH.e_lfanew, std::ios::beg);
    f.write(reinterpret_cast<const char*>(&nth), sizeof(nth));
    f.close();

    DebugLogger::Log(DebugLogger::INFO, "ZeroPEChecksum: Succeeded.");
    return true;
}
