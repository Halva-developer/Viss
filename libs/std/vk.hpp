#pragma once
#include "../vissrt.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

namespace viss {
    namespace vk {
        typedef void* (*PFN_vkVoidFunction)(void);
        typedef int (*PFN_vkCreateInstance)(const void*, const void*, void**);
        
        #ifdef _WIN32
        inline HMODULE vulkanLib = nullptr;
        inline PFN_vkCreateInstance createInstanceFunc = nullptr;

        inline Bool init() {
            if (vulkanLib) return true;
            vulkanLib = LoadLibrary("vulkan-1.dll");
            if (!vulkanLib) return false;
            
            createInstanceFunc = (PFN_vkCreateInstance)GetProcAddress(vulkanLib, "vkCreateInstance");
            return createInstanceFunc != nullptr;
        }

        inline Bool isSupported() {
            return init();
        }

        inline Str getStatus() {
            if (init()) {
                return "Vulkan driver (vulkan-1.dll) loaded successfully! GPU Vulkan support is active.";
            }
            return "Vulkan is not supported on this device (vulkan-1.dll not found).";
        }
        #else
        inline Bool init() { return false; }
        inline Bool isSupported() { return false; }
        inline Str getStatus() { return "Vulkan dynamic loading is only supported on Windows."; }
        #endif
    }
}
