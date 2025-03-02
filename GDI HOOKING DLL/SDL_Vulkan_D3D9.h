#pragma once
#pragma once

#include <windows.h>
#include <d3d9.h>
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>         // Adjust to your SDL3 header if necessary (e.g. <SDL3/SDL.h>)
#include <stdexcept>
#include <cstring>

// The SDL_Vulkan_D3D9 class implements the IDirect3D9 interface.
class SDL_Vulkan_D3D9 : public IDirect3D9 {
public:
    // Data members – add additional Vulkan handles as needed.
    SDL_Window* window;
    VkInstance          vkInstance;
    VkSurfaceKHR        vkSurface;
    VkPhysicalDevice    vkPhysicalDevice;
    VkDevice            vkDevice;
    VkSwapchainKHR      vkSwapchain;
    VkQueue             vkGraphicsQueue;
    VkQueue             vkPresentQueue;
    VkCommandPool       vkCommandPool;
    VkCommandBuffer     vkCommandBuffer;
    VkRenderPass        vkRenderPass;
    VkFramebuffer       vkFramebuffer;
    VkImage             vkSwapchainImage;
    VkImageView         vkSwapchainImageView;
    UINT                refCount;

    // Constructor and destructor.
    SDL_Vulkan_D3D9(UINT SDKVersion);
    virtual ~SDL_Vulkan_D3D9();

    // IUnknown methods:
    STDMETHOD(QueryInterface)(REFIID riid, void** ppvObj) override;
    STDMETHOD_(ULONG, AddRef)() override;
    STDMETHOD_(ULONG, Release)() override;

    // IDirect3D9 methods:
    STDMETHOD(RegisterSoftwareDevice)(void* pInitializeFunction) override;
    STDMETHOD_(UINT, GetAdapterCount)() override;
    STDMETHOD(GetAdapterIdentifier)(UINT Adapter, DWORD Flags, D3DADAPTER_IDENTIFIER9* pIdentifier) override;
    STDMETHOD_(UINT, GetAdapterModeCount)(UINT Adapter, D3DFORMAT Format) override;
    STDMETHOD(EnumAdapterModes)(UINT Adapter, D3DFORMAT Format, UINT Mode, D3DDISPLAYMODE* pMode) override;
    STDMETHOD(GetAdapterDisplayMode)(UINT Adapter, D3DDISPLAYMODE* pMode) override;
    STDMETHOD(CheckDeviceType)(UINT Adapter, D3DDEVTYPE DevType, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat, BOOL bWindowed) override;
    STDMETHOD(CheckDeviceFormat)(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat,
        DWORD Usage, D3DRESOURCETYPE RType, D3DFORMAT CheckFormat) override;
    STDMETHOD(CheckDeviceMultiSampleType)(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SurfaceFormat,
        BOOL Windowed, D3DMULTISAMPLE_TYPE MultiSampleType, DWORD* pQualityLevels) override;
    STDMETHOD(CheckDepthStencilMatch)(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat,
        D3DFORMAT RenderTargetFormat, D3DFORMAT DepthStencilFormat) override;
    STDMETHOD(CheckDeviceFormatConversion)(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SourceFormat,
        D3DFORMAT TargetFormat) override;
    STDMETHOD(GetDeviceCaps)(UINT Adapter, D3DDEVTYPE DeviceType, D3DCAPS9* pCaps) override;
    STDMETHOD_(HMONITOR, GetAdapterMonitor)(UINT Adapter) override;
    // CreateDevice is declared but will be defined in a separate source file.
    STDMETHOD(CreateDevice)(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
        D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnedDeviceInterface) override;
};
