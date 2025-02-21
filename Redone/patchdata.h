#pragma once
#ifndef PATCHDATA_H
#define PATCHDATA_H

#include <vector>
#include <string>
#include <Windows.h>

struct PatchData2 {
    DWORD offset;
    std::vector<BYTE> patchBytes;

    PatchData2(DWORD _offset, std::initializer_list<BYTE> _bytes)
        : offset(_offset), patchBytes(_bytes) {
    }
};

#endif // PATCHDATA_H