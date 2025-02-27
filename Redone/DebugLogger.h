#ifndef DEBUG_LOGGER_H
#define DEBUG_LOGGER_H

#include "pch.h"

class DebugLogger {
private:
    static const int LOG_BUFFER_SIZE = 1024;
    static const char* LOG_FILE_NAME;
    static FILE* logFile;
public:
    enum LogLevel { INFO, WARNING, CRITICAL };
    static void Init(const char* fileName = nullptr);
    static void Log(LogLevel level, const char* fmt, ...);
    static void Cleanup();
};

#endif // DEBUG_LOGGER_H