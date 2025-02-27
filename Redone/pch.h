#ifndef PCH_H
#define PCH_H

#define _WIN32_WINNT 0x0600
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
// Removed NO_IMAGEHLP to avoid conflicts with DbgHelp.h
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

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(linker, "/SECTION:.injsec,RWE")

#endif // PCH_H