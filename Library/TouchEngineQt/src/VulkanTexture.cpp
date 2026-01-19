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

#include "VulkanTexture.h"
#include <QtCore>

VulkanTexture::VulkanTexture()
{
}

VulkanTexture::VulkanTexture(VulkanTexture&& o) noexcept
    : mySource(std::move(o.mySource))
    , myImage(o.myImage)
    , myImageView(o.myImageView)
    , myMemory(o.myMemory)
    , myDevice(o.myDevice)
    , myRhiTexture(std::move(o.myRhiTexture))
    , myWidth(o.myWidth)
    , myHeight(o.myHeight)
{
    o.myImage = VK_NULL_HANDLE;
    o.myImageView = VK_NULL_HANDLE;
    o.myMemory = VK_NULL_HANDLE;
    o.myDevice = VK_NULL_HANDLE;
}

VulkanTexture&
VulkanTexture::operator=(VulkanTexture&& o) noexcept
{
    if (this != &o)
    {
        cleanup();

        mySource = std::move(o.mySource);
        myImage = o.myImage;
        myImageView = o.myImageView;
        myMemory = o.myMemory;
        myDevice = o.myDevice;
        myRhiTexture = std::move(o.myRhiTexture);
        myWidth = o.myWidth;
        myHeight = o.myHeight;

        o.myImage = VK_NULL_HANDLE;
        o.myImageView = VK_NULL_HANDLE;
        o.myMemory = VK_NULL_HANDLE;
        o.myDevice = VK_NULL_HANDLE;
    }
    return *this;
}

void
VulkanTexture::cleanup()
{
    if (myDevice != VK_NULL_HANDLE)
    {
        if (myImageView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(myDevice, myImageView, nullptr);
            myImageView = VK_NULL_HANDLE;
        }
        if (myImage != VK_NULL_HANDLE)
        {
            vkDestroyImage(myDevice, myImage, nullptr);
            myImage = VK_NULL_HANDLE;
        }
        if (myMemory != VK_NULL_HANDLE)
        {
            vkFreeMemory(myDevice, myMemory, nullptr);
            myMemory = VK_NULL_HANDLE;
        }
    }
}

VulkanTexture::VulkanTexture(QRhi* rhi, const TouchObject<TEVulkanTexture>& source, VkDevice device, VkPhysicalDevice physicalDevice, VkQueue graphicsQueue, VkCommandPool commandPool)
    : mySource(source), myDevice(device)
{
    if (!source || !device || !physicalDevice || !rhi || !graphicsQueue || !commandPool)
        return;

    // Wait for all GPU work on our device to complete before importing the texture
    // This ensures proper synchronization when importing shared textures from TouchEngine
    vkDeviceWaitIdle(device);

    HANDLE sharedHandle = TEVulkanTextureGetHandle(source);
    if (!sharedHandle)
        return;

    myWidth = TEVulkanTextureGetWidth(source);
    myHeight = TEVulkanTextureGetHeight(source);
    VkFormat format = TEVulkanTextureGetFormat(source);
    VkExternalMemoryHandleTypeFlagBits handleType = TEVulkanTextureGetHandleType(source);

    VkExternalMemoryImageCreateInfo externalMemoryInfo = {};
    externalMemoryInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    externalMemoryInfo.handleTypes = handleType;

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.pNext = &externalMemoryInfo;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = format;
    imageInfo.extent.width = myWidth;
    imageInfo.extent.height = myHeight;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkResult result = vkCreateImage(device, &imageInfo, nullptr, &myImage);
    if (result != VK_SUCCESS)
    {
        qDebug() << "VulkanTexture: Failed to create image";
        return;
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, myImage, &memRequirements);

    VkImportMemoryWin32HandleInfoKHR importInfo = {};
    importInfo.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR;
    importInfo.handleType = handleType;
    importInfo.handle = sharedHandle;

    VkMemoryDedicatedAllocateInfo dedicatedInfo = {};
    dedicatedInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
    dedicatedInfo.pNext = &importInfo;
    dedicatedInfo.image = myImage;

    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    uint32_t memoryTypeIndex = UINT32_MAX;
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((memRequirements.memoryTypeBits & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
        {
            memoryTypeIndex = i;
            break;
        }
    }

    if (memoryTypeIndex == UINT32_MAX)
    {
        qDebug() << "VulkanTexture: Failed to find suitable memory type";
        vkDestroyImage(device, myImage, nullptr);
        myImage = VK_NULL_HANDLE;
        return;
    }

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.pNext = &dedicatedInfo;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;

    result = vkAllocateMemory(device, &allocInfo, nullptr, &myMemory);
    if (result != VK_SUCCESS)
    {
        qDebug() << "VulkanTexture: Failed to allocate memory";
        vkDestroyImage(device, myImage, nullptr);
        myImage = VK_NULL_HANDLE;
        return;
    }

    result = vkBindImageMemory(device, myImage, myMemory, 0);
    if (result != VK_SUCCESS)
    {
        qDebug() << "VulkanTexture: Failed to bind image memory";
        vkFreeMemory(device, myMemory, nullptr);
        vkDestroyImage(device, myImage, nullptr);
        myImage = VK_NULL_HANDLE;
        myMemory = VK_NULL_HANDLE;
        return;
    }

    // Perform image layout transition from UNDEFINED to SHADER_READ_ONLY_OPTIMAL
    // This also creates an implicit memory barrier for synchronization
    {
        VkCommandBufferAllocateInfo cmdBufAllocInfo = {};
        cmdBufAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdBufAllocInfo.commandPool = commandPool;
        cmdBufAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdBufAllocInfo.commandBufferCount = 1;

        VkCommandBuffer cmdBuffer;
        result = vkAllocateCommandBuffers(device, &cmdBufAllocInfo, &cmdBuffer);
        if (result != VK_SUCCESS)
        {
            qDebug() << "VulkanTexture: Failed to allocate command buffer for layout transition";
            vkFreeMemory(device, myMemory, nullptr);
            vkDestroyImage(device, myImage, nullptr);
            myImage = VK_NULL_HANDLE;
            myMemory = VK_NULL_HANDLE;
            return;
        }

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(cmdBuffer, &beginInfo);

        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL;  // From external source (TouchEngine)
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = myImage;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;  // Wait for all previous writes
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(
            cmdBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

        vkEndCommandBuffer(cmdBuffer);

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdBuffer;

        result = vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        if (result != VK_SUCCESS)
        {
            qDebug() << "VulkanTexture: Failed to submit layout transition";
            vkFreeCommandBuffers(device, commandPool, 1, &cmdBuffer);
            vkFreeMemory(device, myMemory, nullptr);
            vkDestroyImage(device, myImage, nullptr);
            myImage = VK_NULL_HANDLE;
            myMemory = VK_NULL_HANDLE;
            return;
        }

        // Wait for the layout transition to complete
        vkQueueWaitIdle(graphicsQueue);
        vkFreeCommandBuffers(device, commandPool, 1, &cmdBuffer);
    }

    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = myImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.components = TEVulkanTextureGetVkComponentMapping(source);
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    result = vkCreateImageView(device, &viewInfo, nullptr, &myImageView);
    if (result != VK_SUCCESS)
    {
        qDebug() << "VulkanTexture: Failed to create image view";
        vkFreeMemory(device, myMemory, nullptr);
        vkDestroyImage(device, myImage, nullptr);
        myImage = VK_NULL_HANDLE;
        myMemory = VK_NULL_HANDLE;
        return;
    }

    QRhiTexture::Format rhiFormat = QRhiTexture::RGBA8;
    switch (format)
    {
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SRGB:
        rhiFormat = QRhiTexture::RGBA8;
        break;
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SRGB:
        rhiFormat = QRhiTexture::BGRA8;
        break;
    case VK_FORMAT_R16G16B16A16_SFLOAT:
        rhiFormat = QRhiTexture::RGBA16F;
        break;
    case VK_FORMAT_R32G32B32A32_SFLOAT:
        rhiFormat = QRhiTexture::RGBA32F;
        break;
    default:
        rhiFormat = QRhiTexture::RGBA8;
        break;
    }

    myRhiTexture.reset(rhi->newTexture(rhiFormat, QSize(myWidth, myHeight), 1,
        QRhiTexture::UsedAsTransferSource | QRhiTexture::RenderTarget));

    QRhiTexture::NativeTexture nativeTex;
    nativeTex.object = reinterpret_cast<quint64>(myImage);
    nativeTex.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    if (!myRhiTexture->createFrom(nativeTex))
    {
        qDebug() << "VulkanTexture: Failed to create RHI texture from native";
        myRhiTexture.reset();
    }
}

VulkanTexture::~VulkanTexture()
{
    cleanup();
}

void
VulkanTexture::release()
{
    // Release RHI texture first
    myRhiTexture.reset();
    // Then cleanup Vulkan resources
    cleanup();
    // Clear source reference
    mySource = nullptr;
}

#endif // _WIN32
