// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#ifndef PCH_H
#define PCH_H

// add headers that you want to pre-compile here
#include "framework.h"

#endif //PCH_H
#ifndef PCH_H
#define PCH_H

// Windows API headers
#include <windows.h>
#include <wingdi.h>      // GDI functions like CreateCompatibleDC, SelectObject
#include <imagehlp.h>    // ImageDirectoryEntryToData
#include <gdiplus.h>     // GDI+ (if needed)

// Standard C++ headers (optional but useful)
#include <iostream>
#include <vector>
#include <memory>

#endif // PCH_H
