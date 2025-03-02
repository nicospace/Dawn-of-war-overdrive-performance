// SDLCompat.cpp
#include "pch.h" // Must be first if using precompiled headers
#include <windows.h>
#include <d3d9.h>
#include <SDL3/SDL.h>
#include <cstring>

#ifndef D3DERR_NOTIMPL
#define D3DERR_NOTIMPL ((HRESULT)0x80004001L)
#endif

// Make sure SDL types are properly defined
typedef struct SDL_Window SDL_Window;

extern "C" void* SDL_GetWindowNative(SDL_Window* window, const char* property)
{
    (void)window;
    (void)property;
    return nullptr;
}

extern "C" void* SDL_GetNativeWindow(SDL_Window* window, const char* nativeWindowType)
{
    (void)window;
    (void)nativeWindowType;
    return nullptr;
}

extern "C" int SDL_SetCursorVisible(int toggle)
{
    if (toggle) {
        return SDL_ShowCursor();
    }
    else {
        return SDL_HideCursor();
    }
}

class SDL_Vulkan_D3D9 : public IDirect3D9
{
private:
    ULONG m_refCount;
    SDL_Window* m_window;

public:
    SDL_Vulkan_D3D9() : m_refCount(1), m_window(nullptr) {}
    virtual ~SDL_Vulkan_D3D9() {}

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObj) override
    {
        if (!ppvObj) return E_INVALIDARG;
        *ppvObj = nullptr;
        if (riid == IID_IUnknown || riid == IID_IDirect3D9) {
            *ppvObj = this;
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return InterlockedIncrement(&m_refCount);
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        ULONG r = InterlockedDecrement(&m_refCount);
        if (r == 0) delete this;
        return r;
    }

    // IDirect3D9
    HRESULT STDMETHODCALLTYPE RegisterSoftwareDevice(void* pInitializeFunction) override
    {
        return D3DERR_NOTIMPL;
    }

    UINT STDMETHODCALLTYPE GetAdapterCount() override
    {
        return 1;
    }

    HRESULT STDMETHODCALLTYPE GetAdapterIdentifier(UINT Adapter, DWORD Flags, D3DADAPTER_IDENTIFIER9* pIdentifier) override
    {
        return D3DERR_NOTIMPL;
    }

    UINT STDMETHODCALLTYPE GetAdapterModeCount(UINT Adapter, D3DFORMAT Format) override
    {
        return 1;
    }

    HRESULT STDMETHODCALLTYPE EnumAdapterModes(UINT Adapter, D3DFORMAT Format, UINT Mode, D3DDISPLAYMODE* pMode) override
    {
        return D3DERR_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE GetAdapterDisplayMode(UINT Adapter, D3DDISPLAYMODE* pMode) override
    {
        return D3DERR_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE CheckDeviceType(UINT Adapter, D3DDEVTYPE DevType, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat, BOOL bWindowed) override
    {
        return D3DERR_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE CheckDeviceFormat(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, DWORD Usage, D3DRESOURCETYPE RType, D3DFORMAT CheckFormat) override
    {
        return D3DERR_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE CheckDeviceMultiSampleType(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SurfaceFormat, BOOL Windowed, D3DMULTISAMPLE_TYPE MultiSampleType, DWORD* pQualityLevels) override
    {
        return D3DERR_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE CheckDepthStencilMatch(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, D3DFORMAT RenderTargetFormat, D3DFORMAT DepthStencilFormat) override
    {
        return D3DERR_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE CheckDeviceFormatConversion(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SourceFormat, D3DFORMAT TargetFormat) override
    {
        return D3DERR_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE GetDeviceCaps(UINT Adapter, D3DDEVTYPE DeviceType, D3DCAPS9* pCaps) override
    {
        return D3DERR_NOTIMPL;
    }

    HMONITOR STDMETHODCALLTYPE GetAdapterMonitor(UINT Adapter) override
    {
        return nullptr;
    }

    HRESULT STDMETHODCALLTYPE CreateDevice(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnedDeviceInterface) override
    {
        return D3DERR_NOTIMPL;
    }
};