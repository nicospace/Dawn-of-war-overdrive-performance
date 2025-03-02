// SDLCompat.cpp
#include <windows.h>
#include <d3d9.h>      // For IDirect3D9 and D3DERR_* codes
#include <SDL3/SDL.h>  // Or #include <SDL.h> if your SDL is in a different path.
#include <cstring>

// If D3DERR_NOTIMPL isn't defined by your headers, define it now:
#ifndef D3DERR_NOTIMPL
#define D3DERR_NOTIMPL ((HRESULT)0x80004001L)
#endif

//------------------------------------------------------------------------------
// Stub for SDL_GetWindowNative (some code calls this).
// Returns nullptr so the call compiles, but does nothing real.
//------------------------------------------------------------------------------
extern "C" DECLSPEC void* SDLCALL SDL_GetWindowNative(SDL_Window* window, const char* property)
{
    (void)window;
    (void)property;
    return nullptr;
}

//------------------------------------------------------------------------------
// Stub for SDL_GetNativeWindow (another variant).
// Returns nullptr so your code compiles, but does nothing real.
//------------------------------------------------------------------------------
extern "C" DECLSPEC void* SDLCALL SDL_GetNativeWindow(SDL_Window* window, const char* nativeWindowType)
{
    (void)window;
    (void)nativeWindowType;
    return nullptr;
}

//------------------------------------------------------------------------------
// Stub for SDL_SetCursorVisible.
// In some older SDL3 versions, SDL_SetCursorMode doesn't exist.
// Here we use SDL_ShowCursor(toggle) instead so your code compiles.
//------------------------------------------------------------------------------
extern "C" DECLSPEC int SDLCALL SDL_SetCursorVisible(int toggle)
{
    // 0 = hide, non‐0 = show. We'll map that to SDL_ShowCursor(SDL_TRUE/SDL_FALSE).
    return SDL_ShowCursor(toggle ? SDL_TRUE : SDL_FALSE);
}

//------------------------------------------------------------------------------
// Minimal SDL_Vulkan_D3D9 class implementing IDirect3D9.
// Provides a CreateDevice stub so your code can link.
//------------------------------------------------------------------------------
class SDL_Vulkan_D3D9 : public IDirect3D9
{
public:
    // Minimal constructor / destructor.
    SDL_Vulkan_D3D9() : m_refCount(1), m_window(nullptr) {}
    virtual ~SDL_Vulkan_D3D9() {}

    // IUnknown methods
    STDMETHOD(QueryInterface)(REFIID riid, void** ppvObj) override
    {
        (void)riid;
        *ppvObj = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHOD_(ULONG, AddRef)() override
    {
        return ++m_refCount;
    }
    STDMETHOD_(ULONG, Release)() override
    {
        ULONG r = --m_refCount;
        if (r == 0) delete this;
        return r;
    }

    // IDirect3D9 methods (all stubs returning D3DERR_NOTIMPL, except for trivial ones)
    STDMETHOD(RegisterSoftwareDevice)(void* pInitializeFunction) override
    {
        (void)pInitializeFunction;
        return S_OK; // or D3DERR_NOTIMPL if you prefer
    }
    STDMETHOD_(UINT, GetAdapterCount)() override
    {
        return 1;
    }
    STDMETHOD(GetAdapterIdentifier)(UINT, DWORD, D3DADAPTER_IDENTIFIER9*) override
    {
        return D3DERR_NOTIMPL;
    }
    STDMETHOD_(UINT, GetAdapterModeCount)(UINT, D3DFORMAT) override
    {
        return 1;
    }
    STDMETHOD(EnumAdapterModes)(UINT, D3DFORMAT, UINT, D3DDISPLAYMODE*) override
    {
        return D3DERR_NOTIMPL;
    }
    STDMETHOD(GetAdapterDisplayMode)(UINT, D3DDISPLAYMODE*) override
    {
        return D3DERR_NOTIMPL;
    }
    STDMETHOD(CheckDeviceType)(UINT, D3DDEVTYPE, D3DFORMAT, D3DFORMAT, BOOL) override
    {
        return D3DERR_NOTIMPL;
    }
    STDMETHOD(CheckDeviceFormat)(UINT, D3DDEVTYPE, D3DFORMAT, DWORD, D3DRESOURCETYPE, D3DFORMAT) override
    {
        return D3DERR_NOTIMPL;
    }
    STDMETHOD(CheckDeviceMultiSampleType)(UINT, D3DDEVTYPE, D3DFORMAT, BOOL, D3DMULTISAMPLE_TYPE, DWORD*) override
    {
        return D3DERR_NOTIMPL;
    }
    STDMETHOD(CheckDepthStencilMatch)(UINT, D3DDEVTYPE, D3DFORMAT, D3DFORMAT, D3DFORMAT) override
    {
        return D3DERR_NOTIMPL;
    }
    STDMETHOD(CheckDeviceFormatConversion)(UINT, D3DDEVTYPE, D3DFORMAT, D3DFORMAT) override
    {
        return D3DERR_NOTIMPL;
    }
    STDMETHOD(GetDeviceCaps)(UINT, D3DDEVTYPE, D3DCAPS9*) override
    {
        return D3DERR_NOTIMPL;
    }
    STDMETHOD_(HMONITOR, GetAdapterMonitor)(UINT) override
    {
        return nullptr;
    }
    STDMETHOD(CreateDevice)(UINT, D3DDEVTYPE, HWND, DWORD,
        D3DPRESENT_PARAMETERS*, IDirect3DDevice9**) override
    {
        // Return D3DERR_NOTIMPL so it compiles & links even if unimplemented.
        return D3DERR_NOTIMPL;
    }

private:
    ULONG       m_refCount;
    SDL_Window* m_window;  // Not actually used in this stub.
};
