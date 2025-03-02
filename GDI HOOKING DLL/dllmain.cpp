#include "pch.h"
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <vector>
#include "SDLThread.h" // Assumed to define gSDLRunning and SDLThread
#include "IATHooking.h" // Assumed to define DGI_IAT_Hook and HookIATForModule
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <d3d9.h>

#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Imagehlp.lib")
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "vulkan-1.lib") // Link Vulkan library

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#endif
// If your SDL3 lacks these functions, or they're named differently:
#ifndef SDL_SetCursorVisible
    // Map SDL_SetCursorVisible(toggle) to SDL_ShowCursor/SDL_HideCursor with no arguments.
#define SDL_SetCursorVisible(toggle) \
        ( (toggle) ? SDL_ShowCursor() : SDL_HideCursor() )
#endif

#ifndef SDL_GetWindowNative
    // Return nullptr so SDL_GetWindowNative(window, "win32") compiles but does nothing real.
#define SDL_GetWindowNative(win, type) \
        (nullptr)
#endif

#ifndef SDL_GetNativeWindow
    // Same idea for SDL_GetNativeWindow. Return nullptr.
#define SDL_GetNativeWindow(win, type) \
        (nullptr)
#endif

// GDI Function Pointers
typedef NTSTATUS(__stdcall* pNtGdiBitBlt)(HDC, int, int, int, int, HDC, int, int, DWORD);
typedef NTSTATUS(__stdcall* pNtGdiStretchBlt)(HDC, int, int, int, int, HDC, int, int, int, int, DWORD);
typedef NTSTATUS(__stdcall* pNtGdiPatBlt)(HDC, int, int, int, int, DWORD);

pNtGdiBitBlt OriginalNtGdiBitBlt = nullptr;
pNtGdiStretchBlt OriginalNtGdiStretchBlt = nullptr;
pNtGdiPatBlt OriginalNtGdiPatBlt = nullptr;


struct IDirect3D8;

// Direct3D Function Pointers
typedef IDirect3D9* (WINAPI* pDirect3DCreate9)(UINT);
typedef HRESULT(WINAPI* pDirect3DCreate9Ex)(UINT, IDirect3D9Ex**);
typedef IDirect3D8* (WINAPI* pDirect3DCreate8)(UINT);

pDirect3DCreate9 OriginalDirect3DCreate9 = nullptr;
pDirect3DCreate9Ex OriginalDirect3DCreate9Ex = nullptr;
pDirect3DCreate8 OriginalDirect3DCreate8 = nullptr;

// GDI Hooked Functions (Basic Implementations)
NTSTATUS __stdcall HookedNtGdiBitBlt(HDC hdcDest, int xDest, int yDest, int width, int height, HDC hdcSrc, int xSrc, int ySrc, DWORD rop) {
    OutputDebugStringA("[GDI HOOK] HookedNtGdiBitBlt called.\n");
    if (OriginalNtGdiBitBlt) return OriginalNtGdiBitBlt(hdcDest, xDest, yDest, width, height, hdcSrc, xSrc, ySrc, rop);
    return STATUS_SUCCESS;
}

NTSTATUS __stdcall HookedNtGdiStretchBlt(HDC hdcDest, int xDest, int yDest, int widthDest, int heightDest, HDC hdcSrc, int xSrc, int ySrc, int widthSrc, int heightSrc, DWORD rop) {
    OutputDebugStringA("[GDI HOOK] HookedNtGdiStretchBlt called.\n");
    if (OriginalNtGdiStretchBlt) return OriginalNtGdiStretchBlt(hdcDest, xDest, yDest, widthDest, heightDest, hdcSrc, xSrc, ySrc, widthSrc, heightSrc, rop);
    return STATUS_SUCCESS;
}

NTSTATUS __stdcall HookedNtGdiPatBlt(HDC hdc, int x, int y, int width, int height, DWORD rop) {
    OutputDebugStringA("[GDI HOOK] HookedNtGdiPatBlt called.\n");
    if (OriginalNtGdiPatBlt) return OriginalNtGdiPatBlt(hdc, x, y, width, height, rop);
    return STATUS_SUCCESS;
}

// Vulkan-enabled IDirect3D9 implementation
class SDL_Vulkan_D3D9 : public IDirect3D9 {
public:
    SDL_Window* window;
    VkInstance vkInstance;
    VkSurfaceKHR vkSurface;
    VkPhysicalDevice vkPhysicalDevice;
    VkDevice vkDevice;
    VkSwapchainKHR vkSwapchain;
    VkQueue vkGraphicsQueue;
    VkQueue vkPresentQueue;
    VkCommandPool vkCommandPool;
    VkCommandBuffer vkCommandBuffer;
    VkRenderPass vkRenderPass;
    VkFramebuffer vkFramebuffer;
    VkImage vkSwapchainImage;
    VkImageView vkSwapchainImageView;
    uint32_t refCount;
#ifndef VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#define VK_KHR_WIN32_SURFACE_EXTENSION_NAME "VK_KHR_win32_surface"
#endif

    SDL_Vulkan_D3D9(UINT SDKVersion) : refCount(1) {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            OutputDebugStringA("[D3D9 HOOK] SDL_Init failed.\n");
            throw std::runtime_error(SDL_GetError());
        }

        window = SDL_CreateWindow("SDL Vulkan D3D9", 1024, 768, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
        if (!window) {
            OutputDebugStringA("[D3D9 HOOK] SDL_CreateWindow failed.\n");
            throw std::runtime_error(SDL_GetError());
        }
        const char* extensions[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME };

        VkApplicationInfo appInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
        appInfo.pApplicationName = "D3D9 Hook";
        appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
        appInfo.pEngineName = "SDL";
        appInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

       
        VkInstanceCreateInfo createInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = 2;
        createInfo.ppEnabledExtensionNames = extensions;

        if (vkCreateInstance(&createInfo, nullptr, &vkInstance) != VK_SUCCESS) {
            OutputDebugStringA("[D3D9 HOOK] vkCreateInstance failed.\n");
            throw std::runtime_error("Vulkan instance creation failed");
        }

        if (!SDL_Vulkan_CreateSurface(window, vkInstance, nullptr, &vkSurface)) {
            OutputDebugStringA("[D3D9 HOOK] SDL_Vulkan_CreateSurface failed.\n");
            throw std::runtime_error(SDL_GetError());
        }

        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(vkInstance, &deviceCount, nullptr);
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(vkInstance, &deviceCount, devices.data());
        vkPhysicalDevice = devices[0];

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(vkPhysicalDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(vkPhysicalDevice, &queueFamilyCount, queueFamilies.data());
        uint32_t graphicsFamily = UINT32_MAX, presentFamily = UINT32_MAX;
        for (uint32_t i = 0; i < queueFamilyCount; i++) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) graphicsFamily = i;
            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(vkPhysicalDevice, i, vkSurface, &presentSupport);
            if (presentSupport) presentFamily = i;
            if (graphicsFamily != UINT32_MAX && presentFamily != UINT32_MAX) break;
        }

        float queuePriority = 1.0f;
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        VkDeviceQueueCreateInfo queueInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
        queueInfo.queueFamilyIndex = graphicsFamily;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueInfo);
        if (graphicsFamily != presentFamily) {
            queueInfo.queueFamilyIndex = presentFamily;
            queueCreateInfos.push_back(queueInfo);
        }
        VkPhysicalDeviceFeatures deviceFeatures = {};
        VkDeviceCreateInfo deviceInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
        deviceInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        deviceInfo.pQueueCreateInfos = queueCreateInfos.data();
        deviceInfo.pEnabledFeatures = &deviceFeatures;

        if (vkCreateDevice(vkPhysicalDevice, &deviceInfo, nullptr, &vkDevice) != VK_SUCCESS) {
            OutputDebugStringA("[D3D9 HOOK] vkCreateDevice failed.\n");
            throw std::runtime_error("Vulkan device creation failed");
        }

        vkGetDeviceQueue(vkDevice, graphicsFamily, 0, &vkGraphicsQueue);
        vkGetDeviceQueue(vkDevice, presentFamily, 0, &vkPresentQueue);

        VkSurfaceCapabilitiesKHR capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkPhysicalDevice, vkSurface, &capabilities);
        VkSwapchainCreateInfoKHR swapchainInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
        swapchainInfo.surface = vkSurface;
        swapchainInfo.minImageCount = 2;
        swapchainInfo.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
        swapchainInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        swapchainInfo.imageExtent = capabilities.currentExtent;
        swapchainInfo.imageArrayLayers = 1;
        swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchainInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchainInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        swapchainInfo.clipped = VK_TRUE;

        if (vkCreateSwapchainKHR(vkDevice, &swapchainInfo, nullptr, &vkSwapchain) != VK_SUCCESS) {
            OutputDebugStringA("[D3D9 HOOK] vkCreateSwapchainKHR failed.\n");
            throw std::runtime_error("Swapchain creation failed");
        }

        uint32_t imageCount = 0;
        vkGetSwapchainImagesKHR(vkDevice, vkSwapchain, &imageCount, nullptr);
        std::vector<VkImage> swapchainImages(imageCount);
        vkGetSwapchainImagesKHR(vkDevice, vkSwapchain, &imageCount, swapchainImages.data());
        vkSwapchainImage = swapchainImages[0];

        VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        viewInfo.image = vkSwapchainImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(vkDevice, &viewInfo, nullptr, &vkSwapchainImageView) != VK_SUCCESS) {
            OutputDebugStringA("[D3D9 HOOK] vkCreateImageView failed.\n");
            throw std::runtime_error("Image view creation failed");
        }

        VkAttachmentDescription colorAttachment = {};
        colorAttachment.format = VK_FORMAT_B8G8R8A8_UNORM;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorAttachmentRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        VkRenderPassCreateInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;

        if (vkCreateRenderPass(vkDevice, &renderPassInfo, nullptr, &vkRenderPass) != VK_SUCCESS) {
            OutputDebugStringA("[D3D9 HOOK] vkCreateRenderPass failed.\n");
            throw std::runtime_error("Render pass creation failed");
        }

        VkFramebufferCreateInfo framebufferInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        framebufferInfo.renderPass = vkRenderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &vkSwapchainImageView;
        framebufferInfo.width = capabilities.currentExtent.width;
        framebufferInfo.height = capabilities.currentExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(vkDevice, &framebufferInfo, nullptr, &vkFramebuffer) != VK_SUCCESS) {
            OutputDebugStringA("[D3D9 HOOK] vkCreateFramebuffer failed.\n");
            throw std::runtime_error("Framebuffer creation failed");
        }

        VkCommandPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = graphicsFamily;

        if (vkCreateCommandPool(vkDevice, &poolInfo, nullptr, &vkCommandPool) != VK_SUCCESS) {
            OutputDebugStringA("[D3D9 HOOK] vkCreateCommandPool failed.\n");
            throw std::runtime_error("Command pool creation failed");
        }

        VkCommandBufferAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        allocInfo.commandPool = vkCommandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(vkDevice, &allocInfo, &vkCommandBuffer) != VK_SUCCESS) {
            OutputDebugStringA("[D3D9 HOOK] vkAllocateCommandBuffers failed.\n");
            throw std::runtime_error("Command buffer allocation failed");
        }

        OutputDebugStringA("[D3D9 HOOK] Vulkan initialized successfully.\n");
    }

    virtual ~SDL_Vulkan_D3D9() {
        vkDestroyFramebuffer(vkDevice, vkFramebuffer, nullptr);
        vkDestroyRenderPass(vkDevice, vkRenderPass, nullptr);
        vkDestroyImageView(vkDevice, vkSwapchainImageView, nullptr);
        vkDestroySwapchainKHR(vkDevice, vkSwapchain, nullptr);
        vkDestroyCommandPool(vkDevice, vkCommandPool, nullptr);
        vkDestroyDevice(vkDevice, nullptr);
        vkDestroySurfaceKHR(vkInstance, vkSurface, nullptr);
        vkDestroyInstance(vkInstance, nullptr);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }
    //Problems below





    STDMETHOD(QueryInterface)(REFIID riid, void** ppvObj) override {
        if (riid == IID_IUnknown || riid == IID_IDirect3D9) {
            *ppvObj = this;
            AddRef();
            return S_OK;
        }
        *ppvObj = nullptr;
        return E_NOINTERFACE;
    }

    STDMETHOD_(ULONG, AddRef)() override {
        return InterlockedIncrement(&refCount);
    }

    STDMETHOD_(ULONG, Release)() override {
        ULONG count = InterlockedDecrement(&refCount);
        if (count == 0) delete this;
        return count;
    }

    STDMETHOD(RegisterSoftwareDevice)(void* pInitializeFunction) override {
        OutputDebugStringA("[D3D9 HOOK] RegisterSoftwareDevice called - no-op.\n");
        return S_OK;
    }

    STDMETHOD_(UINT, GetAdapterCount)() override {
        return 1;
    }

    STDMETHOD(GetAdapterIdentifier)(UINT Adapter, DWORD Flags, D3DADAPTER_IDENTIFIER9* pIdentifier) override {
        if (Adapter >= 1) return D3DERR_INVALIDCALL;
        strcpy_s(pIdentifier->Driver, "Vulkan Driver");
        strcpy_s(pIdentifier->Description, "SDL Vulkan Adapter");
        pIdentifier->VendorId = 0x1002; // AMD
        pIdentifier->DeviceId = 0x1234;
        pIdentifier->SubSysId = 0;
        pIdentifier->Revision = 1;
        pIdentifier->DriverVersion.QuadPart = VK_MAKE_API_VERSION(0, 1, 0, 0);
        return S_OK;
    }

    STDMETHOD_(UINT, GetAdapterModeCount)(UINT Adapter, D3DFORMAT Format) override {
        if (Adapter >= 1) return 0;
        return 1;
    }

    STDMETHOD(EnumAdapterModes)(UINT Adapter, D3DFORMAT Format, UINT Mode, D3DDISPLAYMODE* pMode) override {
        if (Adapter >= 1 || Mode >= 1) return D3DERR_INVALIDCALL;
        pMode->Width = 1024;
        pMode->Height = 768;
        pMode->RefreshRate = 60;
        pMode->Format = Format;
        return S_OK;
    }

    STDMETHOD(GetAdapterDisplayMode)(UINT Adapter, D3DDISPLAYMODE* pMode) override {
        if (Adapter >= 1) return D3DERR_INVALIDCALL;
        pMode->Width = 1024;
        pMode->Height = 768;
        pMode->RefreshRate = 60;
        pMode->Format = D3DFMT_X8R8G8B8;
        return S_OK;
    }

    STDMETHOD(CheckDeviceType)(UINT Adapter, D3DDEVTYPE DevType, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat, BOOL bWindowed) override {
        if (Adapter >= 1 || DevType != D3DDEVTYPE_HAL) return D3DERR_INVALIDCALL;
        return (AdapterFormat == BackBufferFormat) ? S_OK : D3DERR_NOTAVAILABLE;
    }

    STDMETHOD(CheckDeviceFormat)(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, DWORD Usage, D3DRESOURCETYPE RType, D3DFORMAT CheckFormat) override {
        if (Adapter >= 1 || DeviceType != D3DDEVTYPE_HAL) return D3DERR_INVALIDCALL;
        return (CheckFormat == D3DFMT_X8R8G8B8 || CheckFormat == D3DFMT_A8R8G8B8) ? S_OK : D3DERR_NOTAVAILABLE;
    }

    STDMETHOD(CheckDeviceMultiSampleType)(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SurfaceFormat, BOOL Windowed, D3DMULTISAMPLE_TYPE MultiSampleType, DWORD* pQualityLevels) override {
        if (Adapter >= 1 || DeviceType != D3DDEVTYPE_HAL) return D3DERR_INVALIDCALL;
        if (pQualityLevels) *pQualityLevels = 1;
        return (MultiSampleType == D3DMULTISAMPLE_NONE) ? S_OK : D3DERR_NOTAVAILABLE;
    }

    STDMETHOD(CheckDepthStencilMatch)(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, D3DFORMAT RenderTargetFormat, D3DFORMAT DepthStencilFormat) override {
        if (Adapter >= 1 || DeviceType != D3DDEVTYPE_HAL) return D3DERR_INVALIDCALL;
        return (DepthStencilFormat == D3DFMT_D24S8) ? S_OK : D3DERR_NOTAVAILABLE;
    }

    STDMETHOD(CheckDeviceFormatConversion)(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SourceFormat, D3DFORMAT TargetFormat) override {
        if (Adapter >= 1 || DeviceType != D3DDEVTYPE_HAL) return D3DERR_INVALIDCALL;
        return (SourceFormat == TargetFormat) ? S_OK : D3DERR_NOTAVAILABLE;
    }

    STDMETHOD(GetDeviceCaps)(UINT Adapter, D3DDEVTYPE DeviceType, D3DCAPS9* pCaps) override {
        if (Adapter >= 1 || DeviceType != D3DDEVTYPE_HAL) return D3DERR_INVALIDCALL;
        ZeroMemory(pCaps, sizeof(D3DCAPS9));
        pCaps->DeviceType = D3DDEVTYPE_HAL;
        pCaps->AdapterOrdinal = 0;
        pCaps->Caps = D3DCAPS_READ_SCANLINE;
        pCaps->Caps2 = D3DCAPS2_FULLSCREENGAMMA;
        pCaps->DevCaps = D3DDEVCAPS_HWTRANSFORMANDLIGHT;
        pCaps->VertexShaderVersion = D3DVS_VERSION(2, 0);
        pCaps->PixelShaderVersion = D3DPS_VERSION(2, 0);
        return S_OK;
    }

    STDMETHOD_(HMONITOR, GetAdapterMonitor)(UINT Adapter) override {
        if (Adapter >= 1) return nullptr;
        // Use SDL_GetNativeWindow (declared above) to retrieve the native Win32 window handle.
        HWND hwnd = static_cast<HWND>(SDL_GetNativeWindow(window, "win32"));
        return MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
    }

    // Declaration for CreateDevice (its definition is provided outside the class).
    STDMETHOD(CreateDevice)(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
        D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnedDeviceInterface) override;
};



// Vulkan-enabled IDirect3DDevice9 implementation
class SDL_Vulkan_D3D9Device : public IDirect3DDevice9 {

public:
    SDL_Vulkan_D3D9* parent;
    uint32_t refCount;
    VkPipelineLayout vkPipelineLayout;
    VkPipeline vkGraphicsPipeline;
    VkSemaphore vkImageAvailableSemaphore;
    VkSemaphore vkRenderFinishedSemaphore;
    VkFence vkInFlightFence;
    std::vector<VkBuffer> vertexBuffers;
    std::vector<VkDeviceMemory> vertexBufferMemory;
    std::vector<VkBuffer> indexBuffers;
    std::vector<VkDeviceMemory> indexBufferMemory;

    SDL_Vulkan_D3D9Device(SDL_Vulkan_D3D9* p, D3DPRESENT_PARAMETERS* pPresentationParameters) : parent(p), refCount(1) {
        VkSemaphoreCreateInfo semaphoreInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        vkCreateSemaphore(parent->vkDevice, &semaphoreInfo, nullptr, &vkImageAvailableSemaphore);
        vkCreateSemaphore(parent->vkDevice, &semaphoreInfo, nullptr, &vkRenderFinishedSemaphore);
        VkFenceCreateInfo fenceInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        vkCreateFence(parent->vkDevice, &fenceInfo, nullptr, &vkInFlightFence);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        if (vkCreatePipelineLayout(parent->vkDevice, &pipelineLayoutInfo, nullptr, &vkPipelineLayout) != VK_SUCCESS) {
            OutputDebugStringA("[D3D9 DEVICE] vkCreatePipelineLayout failed.\n");
            throw std::runtime_error("Pipeline layout creation failed");
        };

        VkShaderModule vertShader = CreateShaderModule({
            0x07230203, 0x00010000, 0x00080001, 0x0000000e, 0x00000000, 0x00020011, 0x00000001, 0x0006000b,
            0x00000001, 0x4c534c47, 0x64747320, 0x3035342e, 0x00000000, 0x0003000e, 0x00000000, 0x00000001,
            0x0007000f, 0x00000000, 0x00000004, 0x6e69616d, 0x00000000, 0x00000009, 0x0000000d, 0x00030003,
            0x00000002, 0x000001c2, 0x00040005, 0x00000004, 0x6e69616d, 0x00000000, 0x00050006, 0x00000007,
            0x00000000, 0x6f6c6f63, 0x00000072, 0x00030005, 0x00000009, 0x00006f63, 0x00040006, 0x0000000b,
            0x00000000, 0x00007675, 0x00030005, 0x0000000d, 0x00000076, 0x00050048, 0x00000007, 0x00000000,
            0x0000000b, 0x00000000, 0x00030047, 0x00000007, 0x00000002, 0x00040048, 0x0000000b, 0x00000000,
            0x00000005, 0x00030047, 0x0000000b, 0x00000002, 0x00020013, 0x00000002, 0x00030021, 0x00000003,
            0x00000002, 0x00030016, 0x00000006, 0x00000020, 0x00040017, 0x00000007, 0x00000006, 0x00000004,
            0x00040020, 0x00000008, 0x00000003, 0x00000007, 0x0004003b, 0x00000008, 0x00000009, 0x00000003,
            0x00040017, 0x0000000a, 0x00000006, 0x00000002, 0x0004001e, 0x0000000b, 0x0000000a, 0x0000000a,
            0x00040020, 0x0000000c, 0x00000001, 0x0000000b, 0x0004003b, 0x0000000c, 0x0000000d, 0x00000001,
            0x0004002b, 0x00000006, 0x0000000f, 0x3f800000, 0x00050036, 0x00000002, 0x00000004, 0x00000000,
            0x00000003, 0x000200f8, 0x00000005, 0x0004003d, 0x0000000b, 0x0000000e, 0x0000000d, 0x0007004f,
            0x0000000a, 0x00000010, 0x0000000e, 0x0000000e, 0x00000000, 0x00000001, 0x00050051, 0x00000006,
            0x00000011, 0x0000000e, 0x00000002, 0x00070050, 0x00000007, 0x00000012, 0x00000010, 0x00000011,
            0x0000000f, 0x0000000f, 0x0003003e, 0x00000009, 0x00000012, 0x000100fd, 0x00010038
            });
        VkShaderModule fragShader = CreateShaderModule({
            0x07230203, 0x00010000, 0x00080001, 0x0000000a, 0x00000000, 0x00020011, 0x00000001, 0x0006000b,
            0x00000001, 0x4c534c47, 0x64747320, 0x3035342e, 0x00000000, 0x0003000e, 0x00000000, 0x00000001,
            0x0007000f, 0x00000004, 0x00000004, 0x6e69616d, 0x00000000, 0x00000008, 0x00000009, 0x00030003,
            0x00000002, 0x000001c2, 0x00040005, 0x00000004, 0x6e69616d, 0x00000000, 0x00030005, 0x00000006,
            0x00006f63, 0x00040005, 0x00000008, 0x67617266, 0x00000000, 0x00030005, 0x00000009, 0x00000076,
            0x00020013, 0x00000002, 0x00030021, 0x00000003, 0x00000002, 0x00030016, 0x00000005, 0x00000020,
            0x00040017, 0x00000006, 0x00000005, 0x00000004, 0x00040020, 0x00000007, 0x00000003, 0x00000006,
            0x0004003b, 0x00000007, 0x00000008, 0x00000003, 0x00040020, 0x00000009, 0x00000001, 0x00000006,
            0x0004003b, 0x00000009, 0x00000009, 0x00000001, 0x0004002b, 0x00000005, 0x0000000b, 0x3f800000,
            0x00050036, 0x00000002, 0x00000004, 0x00000000, 0x00000003, 0x000200f8, 0x00000005, 0x0004003d,
            0x00000006, 0x0000000a, 0x00000009, 0x00060050, 0x00000006, 0x0000000c, 0x0000000b, 0x0000000a,
            0x0000000b, 0x0000000b, 0x0003003e, 0x00000008, 0x0000000c, 0x000100fd, 0x00010038
            });
        VkPipelineShaderStageCreateInfo shaderStages[] = {
            { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertShader, "main", nullptr },
            { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragShader, "main", nullptr }
        };
        VkVertexInputBindingDescription bindingDesc = { 0, sizeof(float) * 6, VK_VERTEX_INPUT_RATE_VERTEX };
        VkVertexInputAttributeDescription attrDescs[] = {
            { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 },
            { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3 }
        };
        VkPipelineVertexInputStateCreateInfo vertexInputInfo = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDesc;
        vertexInputInfo.vertexAttributeDescriptionCount = 2;
        vertexInputInfo.pVertexAttributeDescriptions = attrDescs;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport = { 0.0f, 0.0f, (float)pPresentationParameters->BackBufferWidth, (float)pPresentationParameters->BackBufferHeight, 0.0f, 1.0f };
        VkRect2D scissor = { {0, 0}, {pPresentationParameters->BackBufferWidth, pPresentationParameters->BackBufferHeight} };
        VkPipelineViewportStateCreateInfo viewportState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterizer = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisampling = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo colorBlending = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        VkGraphicsPipelineCreateInfo pipelineInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.layout = vkPipelineLayout;
        pipelineInfo.renderPass = parent->vkRenderPass;
        pipelineInfo.subpass = 0;

        if (vkCreateGraphicsPipelines(parent->vkDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &vkGraphicsPipeline) != VK_SUCCESS) {
            OutputDebugStringA("[D3D9 DEVICE] vkCreateGraphicsPipelines failed.\n");
            throw std::runtime_error("Graphics pipeline creation failed");
        }
        vkDestroyShaderModule(parent->vkDevice, vertShader, nullptr);
        vkDestroyShaderModule(parent->vkDevice, fragShader, nullptr);
    }

    virtual ~SDL_Vulkan_D3D9Device() {
        for (size_t i = 0; i < vertexBuffers.size(); i++) {
            vkDestroyBuffer(parent->vkDevice, vertexBuffers[i], nullptr);
            vkFreeMemory(parent->vkDevice, vertexBufferMemory[i], nullptr);
        }
        for (size_t i = 0; i < indexBuffers.size(); i++) {
            vkDestroyBuffer(parent->vkDevice, indexBuffers[i], nullptr);
            vkFreeMemory(parent->vkDevice, indexBufferMemory[i], nullptr);
        }
        vkDestroyPipeline(parent->vkDevice, vkGraphicsPipeline, nullptr);
        vkDestroyPipelineLayout(parent->vkDevice, vkPipelineLayout, nullptr);
        vkDestroySemaphore(parent->vkDevice, vkImageAvailableSemaphore, nullptr);
        vkDestroySemaphore(parent->vkDevice, vkRenderFinishedSemaphore, nullptr);
        vkDestroyFence(parent->vkDevice, vkInFlightFence, nullptr);
    }

    VkShaderModule CreateShaderModule(const std::vector<uint32_t>& code) {
        VkShaderModuleCreateInfo createInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
        createInfo.codeSize = code.size() * sizeof(uint32_t);
        createInfo.pCode = code.data();
        VkShaderModule module;
        if (vkCreateShaderModule(parent->vkDevice, &createInfo, nullptr, &module) != VK_SUCCESS) {
            throw std::runtime_error("Shader module creation failed");
        }
        return module;
    }

    STDMETHOD(QueryInterface)(REFIID riid, void** ppvObj) override {
        if (riid == IID_IUnknown || riid == IID_IDirect3DDevice9) {
            *ppvObj = this;
            AddRef();
            return S_OK;
        }
        *ppvObj = nullptr;
        return E_NOINTERFACE;
    }

    STDMETHOD_(ULONG, AddRef)() override {
        return InterlockedIncrement(&refCount);
    }

    STDMETHOD_(ULONG, Release)() override {
        ULONG count = InterlockedDecrement(&refCount);
        if (count == 0) delete this;
        return count;
    }

    STDMETHOD(TestCooperativeLevel)() override {
        return D3D_OK;
    }

    STDMETHOD_(UINT, GetAvailableTextureMem)() override {
        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(parent->vkPhysicalDevice, &memProps);
        return static_cast<UINT>(memProps.memoryHeaps[0].size / (1024 * 1024));
    }

    STDMETHOD(EvictManagedResources)() override {
        return D3D_OK;
    }

    STDMETHOD(GetDirect3D)(IDirect3D9** ppD3D9) override {
        *ppD3D9 = parent;
        parent->AddRef();
        return D3D_OK;
    }

    STDMETHOD(GetDeviceCaps)(D3DCAPS9* pCaps) override {
        return parent->GetDeviceCaps(0, D3DDEVTYPE_HAL, pCaps);
    }

    STDMETHOD(GetDisplayMode)(UINT iSwapChain, D3DDISPLAYMODE* pMode) override {
        if (iSwapChain != 0) return D3DERR_INVALIDCALL;
        return parent->GetAdapterDisplayMode(0, pMode);
    }
    STDMETHOD(GetCreationParameters)(D3DDEVICE_CREATION_PARAMETERS* pParameters) override {
        pParameters->AdapterOrdinal = 0;
        pParameters->DeviceType = D3DDEVTYPE_HAL;
        pParameters->hFocusWindow = static_cast<HWND>(SDL_GetWindowNative(parent->window, "win32"));
        pParameters->BehaviorFlags = D3DCREATE_HARDWARE_VERTEXPROCESSING;
        return D3D_OK;
    }

    STDMETHOD(SetCursorProperties)(UINT XHotSpot, UINT YHotSpot, IDirect3DSurface9* pCursorBitmap) override {
        return D3D_OK;
    }

    STDMETHOD_(void, SetCursorPosition)(int X, int Y, DWORD Flags) override {
        SDL_WarpMouseInWindow(parent->window, X, Y);
    }

    STDMETHOD_(BOOL, ShowCursor)(BOOL bShow) override {
        SDL_SetCursorVisible(bShow);
        return bShow;
    }

    STDMETHOD(CreateAdditionalSwapChain)(D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DSwapChain9** pSwapChain) override {
        return D3DERR_NOTAVAILABLE;
    }

    STDMETHOD(GetSwapChain)(UINT iSwapChain, IDirect3DSwapChain9** pSwapChain) override {
        if (iSwapChain != 0) return D3DERR_INVALIDCALL;
        return D3DERR_NOTAVAILABLE;
    }

    STDMETHOD_(UINT, GetNumberOfSwapChains)() override {
        return 1;
    }

    STDMETHOD(Reset)(D3DPRESENT_PARAMETERS* pPresentationParameters) override {
        vkDeviceWaitIdle(parent->vkDevice);
        return D3D_OK;
    }

    STDMETHOD(Present)(CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion) override {
        VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &vkRenderFinishedSemaphore;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &parent->vkSwapchain;
        VkResult result = vkQueuePresentKHR(parent->vkPresentQueue, &presentInfo);
        return (result == VK_SUCCESS) ? D3D_OK : D3DERR_DEVICELOST;
    }

    STDMETHOD(GetBackBuffer)(UINT iSwapChain, UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9** ppBackBuffer) override {
        if (iSwapChain != 0 || iBackBuffer != 0 || Type != D3DBACKBUFFER_TYPE_MONO) return D3DERR_INVALIDCALL;
        return D3DERR_NOTAVAILABLE;
    }

    STDMETHOD(GetRasterStatus)(UINT iSwapChain, D3DRASTER_STATUS* pRasterStatus) override {
        if (iSwapChain != 0) return D3DERR_INVALIDCALL;
        pRasterStatus->InVBlank = VK_FALSE;
        pRasterStatus->ScanLine = 0;
        return D3D_OK;
    }

    STDMETHOD(SetDialogBoxMode)(BOOL bEnableDialogs) override {
        return D3D_OK;
    }

    STDMETHOD_(void, SetGammaRamp)(UINT iSwapChain, DWORD Flags, CONST D3DGAMMARAMP* pRamp) override {}

    STDMETHOD_(void, GetGammaRamp)(UINT iSwapChain, D3DGAMMARAMP* pRamp) override {
        if (iSwapChain != 0) return;
        ZeroMemory(pRamp, sizeof(D3DGAMMARAMP));
        for (int i = 0; i < 256; i++) {
            pRamp->red[i] = pRamp->green[i] = pRamp->blue[i] = static_cast<WORD>(i << 8);
        }
    }

    STDMETHOD(CreateTexture)(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9** ppTexture, HANDLE* pSharedHandle) override {
        VkImage image;
        VkDeviceMemory memory;
        VkImageCreateInfo imageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        imageInfo.extent = { Width, Height, 1 };
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        vkCreateImage(parent->vkDevice, &imageInfo, nullptr, &image);
        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(parent->vkDevice, image, &memReqs);
        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(parent->vkPhysicalDevice, &memProps);
        uint32_t memTypeIndex = FindMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memProps);
        VkMemoryAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = memTypeIndex;
        vkAllocateMemory(parent->vkDevice, &allocInfo, nullptr, &memory);
        vkBindImageMemory(parent->vkDevice, image, memory, 0);
        *ppTexture = reinterpret_cast<IDirect3DTexture9*>(image);
        return D3D_OK;
    }

    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties, VkPhysicalDeviceMemoryProperties& memProps) {
        for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }
        throw std::runtime_error("Failed to find suitable memory type");
    }

    STDMETHOD(CreateVolumeTexture)(UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DVolumeTexture9** ppVolumeTexture, HANDLE* pSharedHandle) override {
        return D3DERR_NOTAVAILABLE;
    }

    STDMETHOD(CreateCubeTexture)(UINT EdgeLength, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DCubeTexture9** ppCubeTexture, HANDLE* pSharedHandle) override {
        return D3DERR_NOTAVAILABLE;
    }

    STDMETHOD(CreateVertexBuffer)(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer9** ppVertexBuffer, HANDLE* pSharedHandle) override {
        VkBuffer buffer;
        VkDeviceMemory memory;
        VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bufferInfo.size = Length;
        bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateBuffer(parent->vkDevice, &bufferInfo, nullptr, &buffer);
        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(parent->vkDevice, buffer, &memReqs);
        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(parent->vkPhysicalDevice, &memProps);
        uint32_t memTypeIndex = FindMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, memProps);
        VkMemoryAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = memTypeIndex;
        vkAllocateMemory(parent->vkDevice, &allocInfo, nullptr, &memory);
        vkBindBufferMemory(parent->vkDevice, buffer, memory, 0);
        vertexBuffers.push_back(buffer);
        vertexBufferMemory.push_back(memory);
        *ppVertexBuffer = reinterpret_cast<IDirect3DVertexBuffer9*>(buffer);
        return D3D_OK;
    }

    STDMETHOD(CreateIndexBuffer)(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer9** ppIndexBuffer, HANDLE* pSharedHandle) override {
        VkBuffer buffer;
        VkDeviceMemory memory;
        VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bufferInfo.size = Length;
        bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateBuffer(parent->vkDevice, &bufferInfo, nullptr, &buffer);
        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(parent->vkDevice, buffer, &memReqs);
        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(parent->vkPhysicalDevice, &memProps);
        uint32_t memTypeIndex = FindMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, memProps);
        VkMemoryAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = memTypeIndex;
        vkAllocateMemory(parent->vkDevice, &allocInfo, nullptr, &memory);
        vkBindBufferMemory(parent->vkDevice, buffer, memory, 0);
        indexBuffers.push_back(buffer);
        indexBufferMemory.push_back(memory);
        *ppIndexBuffer = reinterpret_cast<IDirect3DIndexBuffer9*>(buffer);
        return D3D_OK;
    }

    STDMETHOD(CreateRenderTarget)(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) override {
        return D3DERR_NOTAVAILABLE;
    }

    STDMETHOD(CreateDepthStencilSurface)(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) override {
        return D3DERR_NOTAVAILABLE;
    }

    STDMETHOD(UpdateSurface)(IDirect3DSurface9* pSourceSurface, CONST RECT* pSourceRect, IDirect3DSurface9* pDestinationSurface, CONST POINT* pDestPoint) override {
        return D3D_OK;
    }

    STDMETHOD(UpdateTexture)(IDirect3DBaseTexture9* pSourceTexture, IDirect3DBaseTexture9* pDestinationTexture) override {
        return D3D_OK;
    }

    STDMETHOD(GetRenderTargetData)(IDirect3DSurface9* pRenderTarget, IDirect3DSurface9* pDestSurface) override {
        return D3DERR_NOTAVAILABLE;
    }

    STDMETHOD(GetFrontBufferData)(UINT iSwapChain, IDirect3DSurface9* pDestSurface) override {
        return D3DERR_NOTAVAILABLE;
    }

    STDMETHOD(StretchRect)(IDirect3DSurface9* pSourceSurface, CONST RECT* pSourceRect, IDirect3DSurface9* pDestSurface, CONST RECT* pDestRect, D3DTEXTUREFILTERTYPE Filter) override {
        return D3D_OK;
    }

    STDMETHOD(ColorFill)(IDirect3DSurface9* pSurface, CONST RECT* pRect, D3DCOLOR color) override {
        VkClearColorValue clearColor = { {(float)((color >> 16) & 0xFF) / 255.0f, (float)((color >> 8) & 0xFF) / 255.0f, (float)(color & 0xFF) / 255.0f, (float)((color >> 24) & 0xFF) / 255.0f} };
        VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        vkCmdClearColorImage(parent->vkCommandBuffer, parent->vkSwapchainImage, VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &range);
        return D3D_OK;
    }

    STDMETHOD(CreateOffscreenPlainSurface)(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) override {
        return D3DERR_NOTAVAILABLE;
    }

    STDMETHOD(SetRenderTarget)(DWORD RenderTargetIndex, IDirect3DSurface9* pRenderTarget) override {
        if (RenderTargetIndex != 0) return D3DERR_INVALIDCALL;
        return D3D_OK;
    }

    STDMETHOD(GetRenderTarget)(DWORD RenderTargetIndex, IDirect3DSurface9** ppRenderTarget) override {
        if (RenderTargetIndex != 0) return D3DERR_INVALIDCALL;
        return D3DERR_NOTAVAILABLE;
    }

    STDMETHOD(SetDepthStencilSurface)(IDirect3DSurface9* pNewZStencil) override {
        return D3D_OK;
    }

    STDMETHOD(GetDepthStencilSurface)(IDirect3DSurface9** ppZStencilSurface) override {
        return D3DERR_NOTAVAILABLE;
    }

    STDMETHOD(BeginScene)() override {
        vkAcquireNextImageKHR(parent->vkDevice, parent->vkSwapchain, UINT64_MAX, vkImageAvailableSemaphore, VK_NULL_HANDLE, nullptr);
        VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(parent->vkCommandBuffer, &beginInfo);
        VkRenderPassBeginInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
        renderPassInfo.renderPass = parent->vkRenderPass;
        renderPassInfo.framebuffer = parent->vkFramebuffer;
        renderPassInfo.renderArea = { {0, 0}, {1024, 768} };
        VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;
        vkCmdBeginRenderPass(parent->vkCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(parent->vkCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkGraphicsPipeline);
        return D3D_OK;
    }

    STDMETHOD(EndScene)() override {
        vkCmdEndRenderPass(parent->vkCommandBuffer);
        vkEndCommandBuffer(parent->vkCommandBuffer);
        VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &vkImageAvailableSemaphore;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &parent->vkCommandBuffer;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &vkRenderFinishedSemaphore;
        vkQueueSubmit(parent->vkGraphicsQueue, 1, &submitInfo, vkInFlightFence);
        vkWaitForFences(parent->vkDevice, 1, &vkInFlightFence, VK_TRUE, UINT64_MAX);
        vkResetFences(parent->vkDevice, 1, &vkInFlightFence);
        return D3D_OK;
    }

    STDMETHOD(Clear)(DWORD Count, CONST D3DRECT* pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil) override {
        VkClearColorValue clearColor = { {(float)((Color >> 16) & 0xFF) / 255.0f, (float)((Color >> 8) & 0xFF) / 255.0f, (float)(Color & 0xFF) / 255.0f, (float)((Color >> 24) & 0xFF) / 255.0f} };
        VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        vkCmdClearColorImage(parent->vkCommandBuffer, parent->vkSwapchainImage, VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &range);
        return D3D_OK;
    }

    STDMETHOD(SetTransform)(D3DTRANSFORMSTATETYPE State, CONST D3DMATRIX* pMatrix) override {
        return D3D_OK;
    }

    STDMETHOD(GetTransform)(D3DTRANSFORMSTATETYPE State, D3DMATRIX* pMatrix) override {
        ZeroMemory(pMatrix, sizeof(D3DMATRIX));
        pMatrix->_11 = pMatrix->_22 = pMatrix->_33 = pMatrix->_44 = 1.0f;
        return D3D_OK;
    }

    STDMETHOD(MultiplyTransform)(D3DTRANSFORMSTATETYPE, CONST D3DMATRIX*) override {
        return D3D_OK;
    }

    STDMETHOD(SetViewport)(CONST D3DVIEWPORT9* pViewport) override {
        VkViewport viewport = { (float)pViewport->X, (float)pViewport->Y, (float)pViewport->Width, (float)pViewport->Height, pViewport->MinZ, pViewport->MaxZ };
        vkCmdSetViewport(parent->vkCommandBuffer, 0, 1, &viewport);
        return D3D_OK;
    }

    STDMETHOD(GetViewport)(D3DVIEWPORT9* pViewport) override {
        pViewport->X = 0;
        pViewport->Y = 0;
        pViewport->Width = 1024;
        pViewport->Height = 768;
        pViewport->MinZ = 0.0f;
        pViewport->MaxZ = 1.0f;
        return D3D_OK;
    }

    STDMETHOD(SetMaterial)(CONST D3DMATERIAL9* pMaterial) override {
        return D3D_OK;
    }

    STDMETHOD(GetMaterial)(D3DMATERIAL9* pMaterial) override {
        ZeroMemory(pMaterial, sizeof(D3DMATERIAL9));
        pMaterial->Diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
        return D3D_OK;
    }

    STDMETHOD(SetLight)(DWORD Index, CONST D3DLIGHT9*) override {
        return D3D_OK;
    }

    STDMETHOD(GetLight)(DWORD Index, D3DLIGHT9* pLight) override {
        ZeroMemory(pLight, sizeof(D3DLIGHT9));
        return D3D_OK;
    }

    STDMETHOD(LightEnable)(DWORD Index, BOOL Enable) override {
        return D3D_OK;
    }

    STDMETHOD(GetLightEnable)(DWORD Index, BOOL* pEnable) override {
        *pEnable = FALSE;
        return D3D_OK;
    }

    STDMETHOD(SetClipPlane)(DWORD Index, CONST float* pPlane) override {
        return D3D_OK;
    }

    STDMETHOD(GetClipPlane)(DWORD Index, float* pPlane) override {
        ZeroMemory(pPlane, sizeof(float) * 4);
        return D3D_OK;
    }

    STDMETHOD(SetRenderState)(D3DRENDERSTATETYPE State, DWORD Value) override {
        return D3D_OK;
    }

    STDMETHOD(GetRenderState)(D3DRENDERSTATETYPE State, DWORD* pValue) override {
        *pValue = 0;
        return D3D_OK;
    }

    STDMETHOD(CreateStateBlock)(D3DSTATEBLOCKTYPE Type, IDirect3DStateBlock9** ppSB) override {
        return D3DERR_NOTAVAILABLE;
    }

    STDMETHOD(BeginStateBlock)() override {
        return D3D_OK;
    }

    STDMETHOD(EndStateBlock)(IDirect3DStateBlock9** ppSB) override {
        return D3DERR_NOTAVAILABLE;
    }

    STDMETHOD(SetClipStatus)(CONST D3DCLIPSTATUS9* pClipStatus) override {
        return D3D_OK;
    }

    STDMETHOD(GetClipStatus)(D3DCLIPSTATUS9* pClipStatus) override {
        ZeroMemory(pClipStatus, sizeof(D3DCLIPSTATUS9));
        return D3D_OK;
    }

    STDMETHOD(GetTexture)(DWORD Stage, IDirect3DBaseTexture9** ppTexture) override {
        *ppTexture = nullptr;
        return D3D_OK;
    }

    STDMETHOD(SetTexture)(DWORD Stage, IDirect3DBaseTexture9* pTexture) override {
        return D3D_OK;
    }

    STDMETHOD(GetTextureStageState)(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD* pValue) override {
        *pValue = 0;
        return D3D_OK;
    }

    STDMETHOD(SetTextureStageState)(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value) override {
        return D3D_OK;
    }

    STDMETHOD(GetSamplerState)(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD* pValue) override {
        *pValue = 0;
        return D3D_OK;
    }

    STDMETHOD(SetSamplerState)(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value) override {
        return D3D_OK;
    }

    STDMETHOD(ValidateDevice)(DWORD* pNumPasses) override {
        *pNumPasses = 1;
        return D3D_OK;
    }

    STDMETHOD(SetPaletteEntries)(UINT PaletteNumber, CONST PALETTEENTRY* pEntries) override {
        return D3D_OK;
    }

    STDMETHOD(GetPaletteEntries)(UINT PaletteNumber, PALETTEENTRY* pEntries) override {
        ZeroMemory(pEntries, sizeof(PALETTEENTRY) * 256);
        return D3D_OK;
    }

    STDMETHOD(SetCurrentTexturePalette)(UINT PaletteNumber) override {
        return D3D_OK;
    }

    STDMETHOD(GetCurrentTexturePalette)(UINT* PaletteNumber) override {
        *PaletteNumber = 0;
        return D3D_OK;
    }

    STDMETHOD(SetScissorRect)(CONST RECT* pRect) override {
        VkRect2D scissor = { {pRect->left, pRect->top}, {(uint32_t)(pRect->right - pRect->left), (uint32_t)(pRect->bottom - pRect->top)} };
        vkCmdSetScissor(parent->vkCommandBuffer, 0, 1, &scissor);
        return D3D_OK;
    }

    STDMETHOD(GetScissorRect)(RECT* pRect) override {
        pRect->left = 0;
        pRect->top = 0;
        pRect->right = 1024;
        pRect->bottom = 768;
        return D3D_OK;
    }

    STDMETHOD(SetSoftwareVertexProcessing)(BOOL bSoftware) override {
        return D3D_OK;
    }

    STDMETHOD_(BOOL, GetSoftwareVertexProcessing)() override {
        return FALSE;
    }

    STDMETHOD(SetNPatchMode)(float nSegments) override {
        return D3D_OK;
    }

    STDMETHOD_(float, GetNPatchMode)() override {
        return 0.0f;
    }

    STDMETHOD(DrawPrimitive)(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount) override {
        if (vertexBuffers.empty()) return D3D_OK; // Prevent crash if no buffers
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(parent->vkCommandBuffer, 0, 1, &vertexBuffers[0], offsets);
        vkCmdDraw(parent->vkCommandBuffer, PrimitiveCount * 3, 1, StartVertex, 0);
        return D3D_OK;
    }

    STDMETHOD(DrawIndexedPrimitive)(D3DPRIMITIVETYPE, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount) override {
        if (vertexBuffers.empty() || indexBuffers.empty()) return D3D_OK; // Prevent crash if no buffers
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(parent->vkCommandBuffer, 0, 1, &vertexBuffers[0], offsets);
        vkCmdBindIndexBuffer(parent->vkCommandBuffer, indexBuffers[0], 0, VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(parent->vkCommandBuffer, primCount * 3, 1, startIndex, BaseVertexIndex, 0);
        return D3D_OK;
    }

    STDMETHOD(DrawPrimitiveUP)(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, CONST void* pVertexStreamZeroData, UINT VertexStreamZeroStride) override {
        VkBuffer buffer;
        VkDeviceMemory memory;
        VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bufferInfo.size = PrimitiveCount * VertexStreamZeroStride * 3;
        bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateBuffer(parent->vkDevice, &bufferInfo, nullptr, &buffer);
        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(parent->vkDevice, buffer, &memReqs);
        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(parent->vkPhysicalDevice, &memProps);
        uint32_t memTypeIndex = FindMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, memProps);
        VkMemoryAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = memTypeIndex;
        vkAllocateMemory(parent->vkDevice, &allocInfo, nullptr, &memory);
        vkBindBufferMemory(parent->vkDevice, buffer, memory, 0);
        void* data;
        vkMapMemory(parent->vkDevice, memory, 0, memReqs.size, 0, &data);
        memcpy(data, pVertexStreamZeroData, PrimitiveCount * VertexStreamZeroStride * 3);
        vkUnmapMemory(parent->vkDevice, memory);
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(parent->vkCommandBuffer, 0, 1, &buffer, offsets);
        vkCmdDraw(parent->vkCommandBuffer, PrimitiveCount * 3, 1, 0, 0);
        vkDestroyBuffer(parent->vkDevice, buffer, nullptr);
        vkFreeMemory(parent->vkDevice, memory, nullptr);
        return D3D_OK;
    }

    STDMETHOD(DrawIndexedPrimitiveUP)(D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertices, UINT PrimitiveCount, CONST void* pIndexData, D3DFORMAT IndexDataFormat, CONST void* pVertexStreamZeroData, UINT VertexStreamZeroStride) override {
        VkBuffer vBuffer, iBuffer;
        VkDeviceMemory vMemory, iMemory;
        VkBufferCreateInfo vBufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        vBufferInfo.size = NumVertices * VertexStreamZeroStride;
        vBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        vBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VkBufferCreateInfo iBufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        iBufferInfo.size = PrimitiveCount * (IndexDataFormat == D3DFMT_INDEX16 ? 2 : 4) * 3;
        iBufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        iBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateBuffer(parent->vkDevice, &vBufferInfo, nullptr, &vBuffer);
        vkCreateBuffer(parent->vkDevice, &iBufferInfo, nullptr, &iBuffer);
        VkMemoryRequirements vMemReqs, iMemReqs;
        vkGetBufferMemoryRequirements(parent->vkDevice, vBuffer, &vMemReqs);
        vkGetBufferMemoryRequirements(parent->vkDevice, iBuffer, &iMemReqs);
        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(parent->vkPhysicalDevice, &memProps);
        uint32_t memTypeIndex = FindMemoryType(vMemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, memProps);
        VkMemoryAllocateInfo vAllocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        vAllocInfo.allocationSize = vMemReqs.size;
        vAllocInfo.memoryTypeIndex = memTypeIndex;
        VkMemoryAllocateInfo iAllocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        iAllocInfo.allocationSize = iMemReqs.size;
        iAllocInfo.memoryTypeIndex = memTypeIndex;
        vkAllocateMemory(parent->vkDevice, &vAllocInfo, nullptr, &vMemory);
        vkAllocateMemory(parent->vkDevice, &iAllocInfo, nullptr, &iMemory);
        vkBindBufferMemory(parent->vkDevice, vBuffer, vMemory, 0);
        vkBindBufferMemory(parent->vkDevice, iBuffer, iMemory, 0);
        void* vData, * iData;
        vkMapMemory(parent->vkDevice, vMemory, 0, vMemReqs.size, 0, &vData);
        vkMapMemory(parent->vkDevice, iMemory, 0, iMemReqs.size, 0, &iData);
        memcpy(vData, pVertexStreamZeroData, NumVertices * VertexStreamZeroStride);
        memcpy(iData, pIndexData, PrimitiveCount * (IndexDataFormat == D3DFMT_INDEX16 ? 2 : 4) * 3);
        vkUnmapMemory(parent->vkDevice, vMemory);
        vkUnmapMemory(parent->vkDevice, iMemory);
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(parent->vkCommandBuffer, 0, 1, &vBuffer, offsets);
        vkCmdBindIndexBuffer(parent->vkCommandBuffer, iBuffer, 0, IndexDataFormat == D3DFMT_INDEX16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(parent->vkCommandBuffer, PrimitiveCount * 3, 1, 0, 0, 0);
        vkDestroyBuffer(parent->vkDevice, vBuffer, nullptr);
        vkDestroyBuffer(parent->vkDevice, iBuffer, nullptr);
        vkFreeMemory(parent->vkDevice, vMemory, nullptr);
        vkFreeMemory(parent->vkDevice, iMemory, nullptr);
        return D3D_OK;
    }

    STDMETHOD(ProcessVertices)(UINT SrcStartIndex, UINT DestIndex, UINT VertexCount, IDirect3DVertexBuffer9* pDestBuffer, IDirect3DVertexDeclaration9* pVertexDecl, DWORD Flags) override {
        return D3D_OK;
    }

    STDMETHOD(CreateVertexDeclaration)(CONST D3DVERTEXELEMENT9* pVertexElements, IDirect3DVertexDeclaration9** ppDecl) override {
        return D3DERR_NOTAVAILABLE;
    }

    STDMETHOD(SetVertexDeclaration)(IDirect3DVertexDeclaration9* pDecl) override {
        return D3D_OK;
    }

    STDMETHOD(GetVertexDeclaration)(IDirect3DVertexDeclaration9** ppDecl) override {
        *ppDecl = nullptr;
        return D3D_OK;
    }

    STDMETHOD(SetFVF)(DWORD FVF) override {
        return D3D_OK;
    }

    STDMETHOD(GetFVF)(DWORD* pFVF) override {
        *pFVF = 0;
        return D3D_OK;
    }

    STDMETHOD(CreateVertexShader)(CONST DWORD* pFunction, IDirect3DVertexShader9** ppShader) override {
        return D3DERR_NOTAVAILABLE;
    }

    STDMETHOD(SetVertexShader)(IDirect3DVertexShader9* pShader) override {
        return D3D_OK;
    }

    STDMETHOD(GetVertexShader)(IDirect3DVertexShader9** ppShader) override {
        *ppShader = nullptr;
        return D3D_OK;
    }

    STDMETHOD(SetVertexShaderConstantF)(UINT StartRegister, CONST float* pConstantData, UINT Vector4fCount) override {
        return D3D_OK;
    }

    STDMETHOD(GetVertexShaderConstantF)(UINT StartRegister, float* pConstantData, UINT Vector4fCount) override {
        ZeroMemory(pConstantData, Vector4fCount * sizeof(float) * 4);
        return D3D_OK;
    }

    STDMETHOD(SetVertexShaderConstantI)(UINT StartRegister, CONST int* pConstantData, UINT Vector4iCount) override {
        return D3D_OK;
    }

    STDMETHOD(GetVertexShaderConstantI)(UINT StartRegister, int* pConstantData, UINT Vector4iCount) override {
        ZeroMemory(pConstantData, Vector4iCount * sizeof(int) * 4);
        return D3D_OK;
    }

    STDMETHOD(SetVertexShaderConstantB)(UINT StartRegister, CONST BOOL* pConstantData, UINT BoolCount) override {
        return D3D_OK;
    }

    STDMETHOD(GetVertexShaderConstantB)(UINT StartRegister, BOOL* pConstantData, UINT BoolCount) override {
        ZeroMemory(pConstantData, BoolCount * sizeof(BOOL));
        return D3D_OK;
    }

    STDMETHOD(SetStreamSource)(UINT StreamNumber, IDirect3DVertexBuffer9* pStreamData, UINT OffsetInBytes, UINT Stride) override {
        VkDeviceSize offsets[] = { OffsetInBytes };
        VkBuffer buffer = reinterpret_cast<VkBuffer>(pStreamData);
        vkCmdBindVertexBuffers(parent->vkCommandBuffer, StreamNumber, 1, &buffer, offsets);
        return D3D_OK;
    }

    STDMETHOD(GetStreamSource)(UINT StreamNumber, IDirect3DVertexBuffer9** ppStreamData, UINT* pOffsetInBytes, UINT* pStride) override {
        *ppStreamData = nullptr;
        *pOffsetInBytes = 0;
        *pStride = 0;
        return D3D_OK;
    }

    STDMETHOD(SetStreamSourceFreq)(UINT StreamNumber, UINT Setting) override {
        return D3D_OK;
    }

    STDMETHOD(GetStreamSourceFreq)(UINT StreamNumber, UINT* pSetting) override {
        *pSetting = 1;
        return D3D_OK;
    }

    STDMETHOD(SetIndices)(IDirect3DIndexBuffer9* pIndexData) override {
        VkBuffer buffer = reinterpret_cast<VkBuffer>(pIndexData);
        vkCmdBindIndexBuffer(parent->vkCommandBuffer, buffer, 0, VK_INDEX_TYPE_UINT16);
        return D3D_OK;
    }

    STDMETHOD(GetIndices)(IDirect3DIndexBuffer9** ppIndexData) override {
        *ppIndexData = nullptr;
        return D3D_OK;
    }

    STDMETHOD(CreatePixelShader)(CONST DWORD* pFunction, IDirect3DPixelShader9** ppShader) override {
        return D3DERR_NOTAVAILABLE;
    }

    STDMETHOD(SetPixelShader)(IDirect3DPixelShader9* pShader) override {
        return D3D_OK;
    }

    STDMETHOD(GetPixelShader)(IDirect3DPixelShader9** ppShader) override {
        *ppShader = nullptr;
        return D3D_OK;
    }

    };