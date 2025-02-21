#pragma once
#ifndef DEBUG_LOGGER_H
#define DEBUG_LOGGER_H

class DebugLogger {
private:
    static const int LOG_BUFFER_SIZE;
    static const char* LOG_FILE_NAME;
    static FILE* logFile;

public:
    enum LogLevel {
        INFO,
        WARNING,
        ERROR,
        CRITICAL
    };

    static void Init(const char* fileName = nullptr);
    static void Log(LogLevel level, const char* fmt, ...);
    static void Cleanup();
};

#endif // DEBUG_LOGGER_H