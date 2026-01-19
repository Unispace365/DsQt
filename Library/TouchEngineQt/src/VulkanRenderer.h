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

#include "Renderer.h"
#include "VulkanImage.h"
#include <vector>
#include <rhi/qrhi.h>

class VulkanRenderer : public Renderer
{
public:
    VulkanRenderer();
    virtual ~VulkanRenderer();

    virtual TEGraphicsContext*
    getTEContext() const override
    {
        return reinterpret_cast<TEGraphicsContext*>(myContext.get());
    }

    virtual bool setup(QQuickWindow* window) override;
    virtual bool configure(TEInstance* instance, QString& error) override;
    virtual void resize(int width, int height) override;
    virtual void stop() override;
    virtual bool render();
    virtual void clearInputImages() override;
    virtual void addOutputImage() override;
    virtual bool updateOutputImage(const TouchObject<TEInstance>& instance, size_t index, const std::string& identifier) override;
    virtual void clearOutputImages() override;
    virtual void releaseTextures() override;
    virtual void swapOutputBuffers() override;

    virtual QSharedPointer<QRhiTexture> getOutputRhiTexture(size_t index) override;

    VkDevice getDevice() const { return myDevice; }
    VkPhysicalDevice getPhysicalDevice() const { return myPhysicalDevice; }
    VkQueue getGraphicsQueue() const { return myGraphicsQueue; }
    VkCommandPool getCommandPool() const { return myCommandPool; }
    const QString& getDeviceName() const { return myDeviceName; }

private:
    // These are borrowed from Qt's RHI - we don't own them
    VkInstance myVkInstance = VK_NULL_HANDLE;
    VkPhysicalDevice myPhysicalDevice = VK_NULL_HANDLE;
    VkDevice myDevice = VK_NULL_HANDLE;
    VkQueue myGraphicsQueue = VK_NULL_HANDLE;
    uint32_t myGraphicsQueueFamily = 0;

    // We own this command pool for layout transitions
    VkCommandPool myCommandPool = VK_NULL_HANDLE;

    TouchObject<TEVulkanContext> myContext;
    std::vector<VulkanImage> myInputImages;
    std::vector<VulkanImage> myOutputImages;
    QString myDeviceName;
    bool myIsSetupDone = false;

    uint8_t myDeviceUUID[VK_UUID_SIZE] = {};
    uint8_t myDriverUUID[VK_UUID_SIZE] = {};
    uint8_t myDeviceLUID[VK_LUID_SIZE] = {};
    bool myLUIDValid = false;
};

#endif // _WIN32
