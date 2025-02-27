#pragma once

#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

// -----------------------------------------------------------------------------
// 1) Define a structure for your Vulkan context so both files see the same type.
// -----------------------------------------------------------------------------
struct VulkanContext {
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue graphicsQueue;
    VkSwapchainKHR swapchain;
};

// -----------------------------------------------------------------------------
// 2) Extern declarations so SDLThread.cpp can see them from dllmain.cpp
// -----------------------------------------------------------------------------
extern VulkanContext g_VulkanContext; // Defined in dllmain.cpp
extern "C" void CleanupVulkan();      // Also defined in dllmain.cpp
