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

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

#include <memory>
#include <functional>
#include <TouchEngine/TouchObject.h>
#include <TouchEngine/TEVulkan.h>
#include <rhi/qrhi.h>

class VulkanTexture
{
public:
    VulkanTexture();
    VulkanTexture(QRhi* rhi, const TouchObject<TEVulkanTexture>& texture, VkDevice device, VkPhysicalDevice physicalDevice);
    VulkanTexture(const VulkanTexture& o) = delete;
    VulkanTexture(VulkanTexture&& o) noexcept;
    VulkanTexture& operator=(const VulkanTexture& o) = delete;
    VulkanTexture& operator=(VulkanTexture&& o) noexcept;
    ~VulkanTexture();

    VkImage getImage() const { return myImage; }
    VkImageView getImageView() const { return myImageView; }

    constexpr const TouchObject<TEVulkanTexture>&
    getSource() const
    {
        return mySource;
    }

    QSharedPointer<QRhiTexture> getRhiTexture() const { return myRhiTexture; }

    int getWidth() const { return myWidth; }
    int getHeight() const { return myHeight; }
    bool isValid() const { return myImage != VK_NULL_HANDLE; }

    // Release Vulkan resources early (before device is destroyed)
    void release();

private:
    void cleanup();

    TouchObject<TEVulkanTexture> mySource;
    VkImage myImage = VK_NULL_HANDLE;
    VkImageView myImageView = VK_NULL_HANDLE;
    VkDeviceMemory myMemory = VK_NULL_HANDLE;
    VkDevice myDevice = VK_NULL_HANDLE;
    QSharedPointer<QRhiTexture> myRhiTexture;
    int myWidth = 0;
    int myHeight = 0;
};

#endif // _WIN32
