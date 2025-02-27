#pragma once
#ifndef DLL_UTILS_H
#define DLL_UTILS_H

#include "pch.h"

class DllError : public std::exception {
    DWORD errorCode;
    std::string message;
public:
    DllError(DWORD code, const std::string& msg) : errorCode(code), message(msg) {}
    const char* what() const noexcept override { return message.c_str(); }
};

class DllHandle {
public:
    DllHandle() : handle_(nullptr) {}
    explicit DllHandle(HMODULE handle) : handle_(handle) {}
    ~DllHandle() { if (handle_) FreeLibrary(handle_); }
    DllHandle(const DllHandle&) = delete;
    DllHandle& operator=(const DllHandle&) = delete;
    DllHandle(DllHandle&& other) noexcept : handle_(other.handle_) { other.handle_ = nullptr; }
    DllHandle& operator=(DllHandle&& other) noexcept {
        if (this != &other) {
            if (handle_) FreeLibrary(handle_);
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }
    operator HMODULE() const { return handle_; }
    HMODULE get() const { return handle_; }
private:
    HMODULE handle_;
};

DllHandle loadDll(const std::wstring& path);

#endif // DLL_UTILS_H