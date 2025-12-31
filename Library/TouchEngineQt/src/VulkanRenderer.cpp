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

#include "VulkanRenderer.h"
#include <TouchEngine/TouchEngine.h>
#include <TouchEngine/TEVulkan.h>
#include <QtCore>
#include <QVulkanInstance>
#include <vector>

VulkanRenderer::VulkanRenderer()
{
}

VulkanRenderer::~VulkanRenderer()
{
    stop();
}

bool
VulkanRenderer::setup(QQuickWindow* window)
{
    bool success = Renderer::setup(window);

    if (!success)
        return false;

    // Get the RHI from the window
    QRhi* rhi = window->rhi();
    if (!rhi)
    {
        qDebug() << "VulkanRenderer: No RHI available from window";
        return false;
    }

    if (rhi->backend() != QRhi::Vulkan)
    {
        qDebug() << "VulkanRenderer: RHI backend is not Vulkan";
        return false;
    }

    // Get native Vulkan handles from Qt's RHI - use the SAME device as Qt
    const QRhiVulkanNativeHandles* nativeHandles = static_cast<const QRhiVulkanNativeHandles*>(rhi->nativeHandles());
    if (!nativeHandles || !nativeHandles->dev)
    {
        qDebug() << "VulkanRenderer: Failed to get Vulkan native handles from RHI";
        return false;
    }

    // Use Qt's Vulkan device - this ensures textures are on the same device
    // nativeHandles->inst is QVulkanInstance*, need to get the VkInstance from it
    myVkInstance = nativeHandles->inst ? nativeHandles->inst->vkInstance() : VK_NULL_HANDLE;
    myPhysicalDevice = nativeHandles->physDev;
    myDevice = nativeHandles->dev;
    myGraphicsQueue = nativeHandles->gfxQueue;
    myGraphicsQueueFamily = nativeHandles->gfxQueueFamilyIdx;

    // Get device properties including UUIDs for TouchEngine
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(myPhysicalDevice, &props);
    myDeviceName = QString::fromUtf8(props.deviceName);

    // Get device ID properties (UUIDs and LUID)
    VkPhysicalDeviceIDProperties idProps = {};
    idProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;

    VkPhysicalDeviceProperties2 props2 = {};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &idProps;
    vkGetPhysicalDeviceProperties2(myPhysicalDevice, &props2);

    memcpy(myDeviceUUID, idProps.deviceUUID, VK_UUID_SIZE);
    memcpy(myDriverUUID, idProps.driverUUID, VK_UUID_SIZE);
    memcpy(myDeviceLUID, idProps.deviceLUID, VK_LUID_SIZE);
    myLUIDValid = idProps.deviceLUIDValid;

    // Use the window's RHI directly
    myRhi = QSharedPointer<QRhi>(rhi, [](QRhi*) {});

    // Create TouchEngine Vulkan context
    TEResult result = TEVulkanContextCreate(
        myDeviceUUID,
        myDriverUUID,
        myDeviceLUID,
        myLUIDValid,
        TETextureOriginTopLeft,
        myContext.take()
    );

    if (result != TEResultSuccess)
    {
        qDebug() << "VulkanRenderer: Failed to create TouchEngine Vulkan context";
        return false;
    }

    myIsSetupDone = true;
    return true;
}

bool
VulkanRenderer::configure(TEInstance* instance, QString& error)
{
    if (!myDevice)
    {
        error = "Vulkan device not initialized";
        return false;
    }

    int32_t count = 0;
    TEInstanceGetSupportedVkFormats(instance, nullptr, &count);

    // When querying count (nullptr for array), result may be TEResultInsufficientMemory (1)
    // which is expected. We just need count > 0 to confirm Vulkan is supported.
    if (count == 0)
    {
        error = "Vulkan is not supported. No supported formats found.";
        error += "\nThe selected GPU is: ";
        error += myDeviceName;
        return false;
    }

    return true;
}

void
VulkanRenderer::resize(int width, int height)
{
    Renderer::resize(width, height);
}

void
VulkanRenderer::stop()
{
    myOutputImages.clear();
    myInputImages.clear();
    myContext = nullptr;

    // Don't destroy the Vulkan device - it's owned by Qt's RHI
    myDevice = VK_NULL_HANDLE;
    myVkInstance = VK_NULL_HANDLE;
    myPhysicalDevice = VK_NULL_HANDLE;
    myGraphicsQueue = VK_NULL_HANDLE;

    Renderer::stop();
}

bool
VulkanRenderer::render()
{
    return true;
}

QSharedPointer<QRhiTexture>
VulkanRenderer::getOutputRhiTexture(size_t index)
{
    if (index < myOutputImages.size())
    {
        return myOutputImages[index].getTexture().getRhiTexture();
    }
    return QSharedPointer<QRhiTexture>();
}

void
VulkanRenderer::clearInputImages()
{
    myInputImages.clear();
    Renderer::clearInputImages();
}

void
VulkanRenderer::addOutputImage()
{
    myOutputImages.emplace_back();
    myOutputImages.back().setup();
    Renderer::addOutputImage();
}

bool
VulkanRenderer::updateOutputImage(const TouchObject<TEInstance>& instance, size_t index, const std::string& identifier)
{
    bool success = false;

    TouchObject<TETexture> texture;
    TEResult result = TEInstanceLinkGetTextureValue(instance, identifier.c_str(), TELinkValueCurrent, texture.take());

    if (result == TEResultSuccess)
    {
        setOutputImage(index, texture);

        if (texture && TETextureGetType(texture) == TETextureTypeVulkan)
        {
            TEVulkanTexture* vulkanTexture = static_cast<TEVulkanTexture*>(texture.get());

            TouchObject<TEVulkanTexture> vkTexture;
            vkTexture.set(vulkanTexture);

            VulkanTexture tex(myWindow->rhi(), vkTexture, myDevice, myPhysicalDevice);
            if (tex.isValid())
            {
                myOutputImages.at(index).update(std::move(tex));
                success = true;
            }
        }
        else if (texture && TETextureGetType(texture) == TETextureTypeD3DShared)
        {
            qDebug() << "VulkanRenderer: Received D3D shared texture, need to handle interop";
        }
    }

    if (!success)
    {
        myOutputImages.at(index).update(VulkanTexture{});
        setOutputImage(index, nullptr);
    }

    return success;
}

void
VulkanRenderer::clearOutputImages()
{
    Renderer::clearOutputImages();
}

void
VulkanRenderer::releaseTextures()
{
    // Release all Vulkan texture resources before the device is destroyed
    for (auto& image : myOutputImages)
    {
        image.release();
    }
    for (auto& image : myInputImages)
    {
        image.release();
    }
}

#endif // _WIN32
