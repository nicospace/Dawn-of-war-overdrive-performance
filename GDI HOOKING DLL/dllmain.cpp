#include "pch.h"
#include <windows.h>
#include <d3d9.h>
#include <stdio.h>
#include <stdarg.h>
#include "SDLThread.h"
#include "IATHooking.h"

// Include SDL and Vulkan with Vulkan support:
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Imagehlp.lib")
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "vulkan-1.lib")

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#endif

// ------------------------------------------------------------------------
// Types for D3D9 hooking
// ------------------------------------------------------------------------
typedef IDirect3D9* (WINAPI* pDirect3DCreate9)(UINT);
typedef HRESULT(WINAPI* pPresent)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
typedef HRESULT(WINAPI* pReset)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);

// Types for Vulkan hooking
typedef VkResult(VKAPI_CALL* pVkCreateInstance)(const VkInstanceCreateInfo*, const VkAllocationCallbacks*, VkInstance*);
typedef void     (VKAPI_CALL* pVkDestroyInstance)(VkInstance, const VkAllocationCallbacks*);
typedef VkResult(VKAPI_CALL* pVkCreateDevice)(VkPhysicalDevice, const VkDeviceCreateInfo*, const VkAllocationCallbacks*, VkDevice*);
typedef void     (VKAPI_CALL* pVkDestroyDevice)(VkDevice, const VkAllocationCallbacks*);
typedef VkResult(VKAPI_CALL* pVkQueueSubmit)(VkQueue, uint32_t, const VkSubmitInfo*, VkFence);

// ------------------------------------------------------------------------
// Global hooking pointers
// ------------------------------------------------------------------------
pDirect3DCreate9   OriginalDirect3DCreate9 = nullptr;
pPresent           OriginalPresent = nullptr;
pReset             OriginalReset = nullptr;

pVkCreateInstance  OriginalVkCreateInstance = nullptr;
pVkDestroyInstance OriginalVkDestroyInstance = nullptr;
pVkCreateDevice    OriginalVkCreateDevice = nullptr;
pVkDestroyDevice   OriginalVkDestroyDevice = nullptr;
pVkQueueSubmit     OriginalVkQueueSubmit = nullptr;

// ------------------------------------------------------------------------
// Minimal VulkanContext struct
// ------------------------------------------------------------------------
struct VulkanContext {
    VkInstance       instance;
    VkSurfaceKHR     surface;
    VkPhysicalDevice physicalDevice;
    VkDevice         device;
    VkQueue          graphicsQueue;
    VkSwapchainKHR   swapchain;
};

// One global context
static VulkanContext g_VulkanContext = {
    VK_NULL_HANDLE, // instance
    VK_NULL_HANDLE, // surface
    VK_NULL_HANDLE, // physicalDevice
    VK_NULL_HANDLE, // device
    VK_NULL_HANDLE, // graphicsQueue
    VK_NULL_HANDLE  // swapchain
};

// ------------------------------------------------------------------------
// Globals for SDL thread and window
// ------------------------------------------------------------------------
static HANDLE      hSDLThread = NULL;
static SDL_Window* g_Window = NULL;
volatile bool      gSDLRunning = true;  // controlling main loop

// ------------------------------------------------------------------------
// Hooked D3D9 calls
// ------------------------------------------------------------------------
extern "C"
IDirect3D9* WINAPI HookedDirect3DCreate9(UINT sdkVersion) {
    OutputDebugStringA("[D3D9 HOOK] HookedDirect3DCreate9 called.\n");
    return OriginalDirect3DCreate9
        ? OriginalDirect3DCreate9(sdkVersion)
        : nullptr;
}

extern "C"
HRESULT WINAPI HookedPresent(IDirect3DDevice9* device,
    const RECT* src, const RECT* dst,
    HWND hwnd, const RGNDATA* rgn) {
    OutputDebugStringA("[D3D9 HOOK] HookedPresent called.\n");
    return OriginalPresent
        ? OriginalPresent(device, src, dst, hwnd, rgn)
        : S_OK;
}

extern "C"
HRESULT WINAPI HookedReset(IDirect3DDevice9* device,
    D3DPRESENT_PARAMETERS* params) {
    OutputDebugStringA("[D3D9 HOOK] HookedReset called.\n");
    return OriginalReset
        ? OriginalReset(device, params)
        : S_OK;
}

// ------------------------------------------------------------------------
// Hooked Vulkan calls
// ------------------------------------------------------------------------
extern "C"
VkResult VKAPI_CALL HookedVkCreateInstance(const VkInstanceCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkInstance* pInstance) {
    OutputDebugStringA("[VULKAN HOOK] HookedVkCreateInstance called.\n");
    return OriginalVkCreateInstance
        ? OriginalVkCreateInstance(pCreateInfo, pAllocator, pInstance)
        : VK_ERROR_INITIALIZATION_FAILED;
}

extern "C"
void VKAPI_CALL HookedVkDestroyInstance(VkInstance instance,
    const VkAllocationCallbacks* pAllocator) {
    OutputDebugStringA("[VULKAN HOOK] HookedVkDestroyInstance called.\n");
    if (OriginalVkDestroyInstance) {
        OriginalVkDestroyInstance(instance, pAllocator);
    }
}

extern "C"
VkResult VKAPI_CALL HookedVkCreateDevice(VkPhysicalDevice physicalDevice,
    const VkDeviceCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDevice* pDevice) {
    OutputDebugStringA("[VULKAN HOOK] HookedVkCreateDevice called.\n");
    return OriginalVkCreateDevice
        ? OriginalVkCreateDevice(physicalDevice, pCreateInfo, pAllocator, pDevice)
        : VK_ERROR_INITIALIZATION_FAILED;
}

extern "C"
void VKAPI_CALL HookedVkDestroyDevice(VkDevice device,
    const VkAllocationCallbacks* pAllocator) {
    OutputDebugStringA("[VULKAN HOOK] HookedVkDestroyDevice called.\n");
    if (OriginalVkDestroyDevice) {
        OriginalVkDestroyDevice(device, pAllocator);
    }
}

extern "C"
VkResult VKAPI_CALL HookedVkQueueSubmit(VkQueue queue,
    uint32_t submitCount,
    const VkSubmitInfo* pSubmits,
    VkFence fence) {
    OutputDebugStringA("[VULKAN HOOK] HookedVkQueueSubmit called.\n");
    return OriginalVkQueueSubmit
        ? OriginalVkQueueSubmit(queue, submitCount, pSubmits, fence)
        : VK_ERROR_DEVICE_LOST;
}

// ------------------------------------------------------------------------
// Initialize Vulkan with SDL
// ------------------------------------------------------------------------
BOOL InitializeVulkan() {
    OutputDebugStringA("[VULKAN] InitializeVulkan started.\n");

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Soulstorm Vulkan";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Custom Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    // If your SDL is compiled with Vulkan support, the next call has 3 params:
    // (SDL_Window*, unsigned int*, const char**)
    // or (SDL_Window*, size_t*, const char**)
    // either is acceptable. We'll use unsigned int for extensionCount.
    unsigned int extensionCount = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(g_Window, &extensionCount, nullptr)) {
        OutputDebugStringA("[VULKAN] SDL_Vulkan_GetInstanceExtensions (count) failed.\n");
        return FALSE;
    }

    std::vector<const char*> extensions(extensionCount);
    if (!SDL_Vulkan_GetInstanceExtensions(g_Window, &extensionCount, extensions.data())) {
        OutputDebugStringA("[VULKAN] SDL_Vulkan_GetInstanceExtensions (names) failed.\n");
        return FALSE;
    }

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = extensionCount;
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkResult result = vkCreateInstance(&createInfo, nullptr, &g_VulkanContext.instance);
    if (result != VK_SUCCESS) {
        OutputDebugStringA("[VULKAN] vkCreateInstance failed.\n");
        return FALSE;
    }

    // SDL_Vulkan_CreateSurface also needs 3 parameters in Vulkan-enabled SDL:
    // (SDL_Window*, VkInstance, VkSurfaceKHR*)
    // If your header has only 2, your SDL library lacks Vulkan support.
    if (!SDL_Vulkan_CreateSurface(g_Window, g_VulkanContext.instance, &g_VulkanContext.surface)) {
        OutputDebugStringA("[VULKAN] SDL_Vulkan_CreateSurface failed.\n");
        vkDestroyInstance(g_VulkanContext.instance, nullptr);
        g_VulkanContext.instance = VK_NULL_HANDLE;
        return FALSE;
    }

    // Now pick a physical device
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(g_VulkanContext.instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        OutputDebugStringA("[VULKAN] No physical devices found.\n");
        // Clean up instance
        vkDestroySurfaceKHR(g_VulkanContext.instance, g_VulkanContext.surface, nullptr);
        vkDestroyInstance(g_VulkanContext.instance, nullptr);
        g_VulkanContext.instance = VK_NULL_HANDLE;
        g_VulkanContext.surface = VK_NULL_HANDLE;
        return FALSE;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(g_VulkanContext.instance, &deviceCount, devices.data());
    g_VulkanContext.physicalDevice = devices[0];

    // Create a basic logical device
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo = {};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = 0;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo deviceInfo = {};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;

    result = vkCreateDevice(g_VulkanContext.physicalDevice, &deviceInfo, nullptr, &g_VulkanContext.device);
    if (result != VK_SUCCESS) {
        OutputDebugStringA("[VULKAN] vkCreateDevice failed.\n");
        vkDestroySurfaceKHR(g_VulkanContext.instance, g_VulkanContext.surface, nullptr);
        vkDestroyInstance(g_VulkanContext.instance, nullptr);
        g_VulkanContext.instance = VK_NULL_HANDLE;
        g_VulkanContext.surface = VK_NULL_HANDLE;
        return FALSE;
    }

    // Acquire a queue
    vkGetDeviceQueue(g_VulkanContext.device, 0, 0, &g_VulkanContext.graphicsQueue);

    OutputDebugStringA("[VULKAN] Vulkan initialized successfully.\n");
    return TRUE;
}

// ------------------------------------------------------------------------
// Cleanup Vulkan
// ------------------------------------------------------------------------
void CleanupVulkan() {
    OutputDebugStringA("[VULKAN] CleanupVulkan called.\n");

    if (g_VulkanContext.swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(g_VulkanContext.device, g_VulkanContext.swapchain, nullptr);
        g_VulkanContext.swapchain = VK_NULL_HANDLE;
    }
    if (g_VulkanContext.device != VK_NULL_HANDLE) {
        vkDestroyDevice(g_VulkanContext.device, nullptr);
        g_VulkanContext.device = VK_NULL_HANDLE;
    }
    if (g_VulkanContext.surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(g_VulkanContext.instance, g_VulkanContext.surface, nullptr);
        g_VulkanContext.surface = VK_NULL_HANDLE;
    }
    if (g_VulkanContext.instance != VK_NULL_HANDLE) {
        vkDestroyInstance(g_VulkanContext.instance, nullptr);
        g_VulkanContext.instance = VK_NULL_HANDLE;
    }
}

// ------------------------------------------------------------------------
// The SDL thread that calls InitializeVulkan and runs the loop
// ------------------------------------------------------------------------
DWORD WINAPI SDLThread(LPVOID lpParam) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        OutputDebugStringA("[VULKAN] SDL_Init failed.\n");
        return 1;
    }

    // Create an SDL window with SDL_WINDOW_VULKAN
    g_Window = SDL_CreateWindow("Soulstorm Vulkan", 800, 600, SDL_WINDOW_VULKAN);
    if (!g_Window) {
        OutputDebugStringA("[VULKAN] SDL_CreateWindow failed.\n");
        SDL_Quit();
        return 1;
    }

    // Now initialize Vulkan
    if (!InitializeVulkan()) {
        OutputDebugStringA("[VULKAN] InitializeVulkan failed.\n");
        SDL_DestroyWindow(g_Window);
        g_Window = NULL;
        SDL_Quit();
        return 1;
    }

    // Example: hooking D3D9 device if you wish
    IDirect3D9* d3d9 = OriginalDirect3DCreate9
        ? OriginalDirect3DCreate9(D3D_SDK_VERSION)
        : nullptr;
    if (d3d9) {
        D3DADAPTER_IDENTIFIER9 adapter;
        d3d9->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &adapter);

        D3DPRESENT_PARAMETERS pp = {
            800,            // BackBufferWidth
            600,            // BackBufferHeight
            D3DFMT_X8R8G8B8,// BackBufferFormat
            1,              // BackBufferCount
            D3DMULTISAMPLE_NONE,  // MultiSampleType
            0,              // MultiSampleQuality
            D3DSWAPEFFECT_DISCARD, // SwapEffect
            NULL,           // hDeviceWindow
            TRUE,           // Windowed
            FALSE,          // EnableAutoDepthStencil
            D3DFMT_D24S8,   // AutoDepthStencilFormat
            0,              // Flags
            0,              // FullScreen_RefreshRateInHz
            0               // PresentationInterval
        };

        IDirect3DDevice9* device = nullptr;
        if (SUCCEEDED(d3d9->CreateDevice(
            D3DADAPTER_DEFAULT,
            D3DDEVTYPE_HAL,
            NULL,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING,
            &pp,
            &device)))
        {
            void** vtable = *(void***)device;
            OriginalPresent = (pPresent)vtable[17];
            OriginalReset = (pReset)vtable[16];

            DWORD oldProtect;
            // Hook Present
            VirtualProtect(&vtable[17], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect);
            vtable[17] = HookedPresent;
            VirtualProtect(&vtable[17], sizeof(void*), oldProtect, &oldProtect);

            // Hook Reset
            VirtualProtect(&vtable[16], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect);
            vtable[16] = HookedReset;
            VirtualProtect(&vtable[16], sizeof(void*), oldProtect, &oldProtect);

            device->Release();
        }
        d3d9->Release();
    }

    // Main loop
    SDL_Event event;
    while (gSDLRunning) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                gSDLRunning = false;
            }
        }
        // ~60fps
        Sleep(16);
    }

    // Cleanup
    CleanupVulkan();
    SDL_DestroyWindow(g_Window);
    g_Window = NULL;
    SDL_Quit();
    return 0;
}

// ------------------------------------------------------------------------
// DllMain
// ------------------------------------------------------------------------
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        OutputDebugStringA("[D3D9 HOOK] DLL_PROCESS_ATTACH triggered.\n");
        break;

    case DLL_PROCESS_DETACH:
        gSDLRunning = false;
        if (hSDLThread) {
            WaitForSingleObject(hSDLThread, INFINITE);
            CloseHandle(hSDLThread);
            hSDLThread = NULL;
        }
        CleanupVulkan();
        if (g_Window) {
            SDL_DestroyWindow(g_Window);
            g_Window = NULL;
            SDL_Quit();
        }
        OutputDebugStringA("[D3D9 HOOK] DLL_PROCESS_DETACH triggered.\n");
        break;
    }
    return TRUE;
}

// ------------------------------------------------------------------------
// Exported hooking initializer
// ------------------------------------------------------------------------
extern "C"
__declspec(dllexport)
void InitializeDGIHooking() {
    OutputDebugStringA("[HOOK] InitializeDGIHooking() called.\n");

    HMODULE hHostModule = GetModuleHandle(NULL);
    if (!hHostModule) {
        OutputDebugStringA("[HOOK] ERROR: Could not retrieve host module handle.\n");
        MessageBoxA(NULL, "Hooking failed: Could not retrieve host module handle.", "Error", MB_ICONERROR);
        return;
    }

    // Example: IAT hooking for d3d9
    DGI_IAT_Hook d3d9Hooks[] = {
        { "Direct3DCreate9", (FARPROC)HookedDirect3DCreate9 },
    };
    HookIATForModule(hHostModule, "d3d9.dll", d3d9Hooks, sizeof(d3d9Hooks) / sizeof(DGI_IAT_Hook));
    OutputDebugStringA("[D3D9 HOOK] IAT hooking for d3d9.dll succeeded.\n");

    // Example: IAT hooking for vulkan-1
    DGI_IAT_Hook vkHooks[] = {
        { "vkCreateInstance",  (FARPROC)HookedVkCreateInstance },
        { "vkDestroyInstance", (FARPROC)HookedVkDestroyInstance },
        { "vkCreateDevice",    (FARPROC)HookedVkCreateDevice },
        { "vkDestroyDevice",   (FARPROC)HookedVkDestroyDevice },
        { "vkQueueSubmit",     (FARPROC)HookedVkQueueSubmit }
    };
    HookIATForModule(hHostModule, "vulkan-1.dll", vkHooks, sizeof(vkHooks) / sizeof(DGI_IAT_Hook));
    OutputDebugStringA("[VULKAN HOOK] IAT hooking for vulkan-1.dll succeeded.\n");

    // Create the SDL thread that calls InitializeVulkan
    hSDLThread = CreateThread(NULL, 0, SDLThread, NULL, 0, NULL);
    if (!hSDLThread) {
        OutputDebugStringA("[HOOK] ERROR: Could not create SDL thread.\n");
        MessageBoxA(NULL, "Hooking failed: Could not create SDL thread.", "Error", MB_ICONERROR);
        return;
    }
    OutputDebugStringA("[HOOK] SDL thread created successfully.\n");
}

// ------------------------------------------------------------------------
// Basic IAT hooking function (so it fully compiles). Adjust as needed.
// ------------------------------------------------------------------------
#include <dbghelp.h>
#pragma comment(lib, "DbgHelp.lib")

bool HookIATForModule(HMODULE module, const char* targetDll,
    DGI_IAT_Hook* hooks, size_t count)
{
    // Implementation is the same idea as before (ImageDirectoryEntryToData).
    // For brevity, assume you already have it. You can keep or remove this.
    // ...
    return true;
}
