#include "DebugLogger.h"

const char* DebugLogger::LOG_FILE_NAME = nullptr;
FILE* DebugLogger::logFile = nullptr;

void DebugLogger::Init(const char* fileName) {
    static char temp[256];
    if (fileName) LOG_FILE_NAME = fileName;
    else {
        SYSTEMTIME st;
        GetSystemTime(&st);
        sprintf_s(temp, "patch_debug_%04d%02d%02d_%02d%02d%02d.log",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        LOG_FILE_NAME = temp;
    }
    fopen_s(&logFile, LOG_FILE_NAME, "w+");
    if (logFile) Log(CRITICAL, "=== Starting debug session ===");
}

void DebugLogger::Log(LogLevel level, const char* fmt, ...) {
    if (!logFile) return;
    va_list args;
    va_start(args, fmt);
    char buffer[LOG_BUFFER_SIZE];
    vsnprintf_s(buffer, LOG_BUFFER_SIZE, _TRUNCATE, fmt, args);
    const char* levelStr = "";
    switch (level) {
    case INFO: levelStr = "INFO"; break;
    case WARNING: levelStr = "WARN"; break;
    case CRITICAL: levelStr = "CRIT"; break;
    }
    fprintf(logFile, "[%s] %s\n", levelStr, buffer);
    fflush(logFile);
    va_end(args);
}

void DebugLogger::Cleanup() {
    if (logFile) {
        fclose(logFile);
        logFile = nullptr;
    }
}