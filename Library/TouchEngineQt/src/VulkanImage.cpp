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

#ifdef _WIN32

#include "VulkanImage.h"

VulkanImage::VulkanImage()
{
}

VulkanImage::VulkanImage(VulkanImage&& o) noexcept
    : myFrontBuffer(std::move(o.myFrontBuffer))
    , myBackBuffer(std::move(o.myBackBuffer))
    , myHasNewFrame(o.myHasNewFrame.load())
    , myDirty(o.myDirty)
{
}

VulkanImage&
VulkanImage::operator=(VulkanImage&& o) noexcept
{
    std::lock_guard<std::mutex> lock(mySwapMutex);
    myFrontBuffer = std::move(o.myFrontBuffer);
    myBackBuffer = std::move(o.myBackBuffer);
    myHasNewFrame.store(o.myHasNewFrame.load());
    myDirty = o.myDirty;
    return *this;
}

VulkanImage::~VulkanImage()
{
}

bool
VulkanImage::setup()
{
    return true;
}

void
VulkanImage::updateBackBuffer(VulkanTexture&& texture)
{
    std::lock_guard<std::mutex> lock(mySwapMutex);
    myBackBuffer = std::move(texture);
    myHasNewFrame.store(true);
    myDirty = true;
}

void
VulkanImage::swapBuffers()
{
    std::lock_guard<std::mutex> lock(mySwapMutex);
    if (myHasNewFrame.load())
    {
        std::swap(myFrontBuffer, myBackBuffer);
        myHasNewFrame.store(false);
    }
}

const VulkanTexture&
VulkanImage::getFrontBuffer() const
{
    std::lock_guard<std::mutex> lock(mySwapMutex);
    return myFrontBuffer;
}

void
VulkanImage::update(VulkanTexture&& texture)
{
    // Legacy API - directly updates and swaps for backward compatibility
    updateBackBuffer(std::move(texture));
    swapBuffers();
}

void
VulkanImage::release()
{
    std::lock_guard<std::mutex> lock(mySwapMutex);
    myFrontBuffer.release();
    myBackBuffer.release();
}

#endif // _WIN32
