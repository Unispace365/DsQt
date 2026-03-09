/* Shared Use License: This file is owned by Derivative Inc. (Derivative)
* and can only be used, and/or modified for use, in conjunction with
* Derivative's TouchDesigner software, and only if you are a licensee who has
* accepted Derivative's TouchDesigner license or assignment agreement
* (which also govern the use of this file). You may share or redistribute
* a modified version of this file provided the following conditions are met:
*
* 1. The shared file or redistribution must retain the information set out
* above and this list of conditions.
* 2. Derivative's name (Derivative Inc.) or its trademarks may not be used
* to endorse or promote products derived from this file without specific
* prior written permission from Derivative.
*/

#pragma once

#ifdef _WIN32

#include "VulkanTexture.h"
#include <atomic>
#include <mutex>

class VulkanImage
{
public:
    VulkanImage();
    VulkanImage(const VulkanImage& o) = delete;
    VulkanImage(VulkanImage&& o) noexcept;
    VulkanImage& operator=(const VulkanImage& o) = delete;
    VulkanImage& operator=(VulkanImage&& o) noexcept;
    ~VulkanImage();

    bool setup();
    void release();

    // Double-buffering API
    // Update back buffer with new texture (called when frame completes)
    void updateBackBuffer(VulkanTexture&& texture);
    // Swap front and back buffers (makes new texture available for reading)
    void swapBuffers();
    // Get the front buffer for reading (thread-safe)
    const VulkanTexture& getFrontBuffer() const;
    // Check if a new frame is ready in the back buffer
    bool hasNewFrame() const { return myHasNewFrame.load(); }

    // Legacy API - for compatibility during transition
    void update(VulkanTexture&& texture);
    const VulkanTexture& getTexture() const { return getFrontBuffer(); }

private:
    VulkanTexture myFrontBuffer;  // Currently displayed (Qt reads from this)
    VulkanTexture myBackBuffer;   // Being updated (TouchEngine writes complete)
    std::atomic<bool> myHasNewFrame{false};
    mutable std::mutex mySwapMutex;
    bool myDirty = true;
};

#endif // _WIN32
