#pragma once

// Platform-specific headers for TouchEngine
// Must be included before TouchEngine headers

#ifdef _WIN32
    // Define WIN32_LEAN_AND_MEAN to exclude rarely-used Windows APIs
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    
    // Exclude some Windows headers we don't need
    #ifndef NOMINMAX
        #define NOMINMAX  // Prevent Windows.h from defining min/max macros
    #endif
    
    // Include Windows headers for HANDLE and DirectX types
    #include <windows.h>
    
    // DirectX 11
    #include <d3d11.h>
    #include <dxgi.h>
    
    // DirectX 12 (optional, only if using D3D12)
    #ifdef TOUCHENGINE_USE_D3D12
        #include <d3d12.h>
    #endif
    
#elif defined(__APPLE__)
    // macOS/iOS - Metal headers
    #ifdef __OBJC__
        #import <Metal/Metal.h>
    #else
        // For C++ files, we need forward declarations
        struct MTLDevice;
        struct MTLTexture;
    #endif
    
#elif defined(__linux__)
    // Linux - Vulkan or OpenGL
    #ifdef TOUCHENGINE_USE_VULKAN
        #include <vulkan/vulkan.h>
    #endif
    
#endif

// OpenGL headers (cross-platform)
//#ifdef TOUCHENGINE_USE_OPENGL
    #ifdef _WIN32
        #include <GL/gl.h>
    #elif defined(__APPLE__)
        #include <OpenGL/gl.h>
    #else
        #include <GL/gl.h>
    #endif
//#endif
