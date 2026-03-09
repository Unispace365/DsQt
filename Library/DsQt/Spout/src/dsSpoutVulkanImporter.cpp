#ifdef DSSPOUT_VULKAN_ENABLED

#include "dsSpoutVulkanImporter.h"

#include <QDebug>
#include <QVulkanInstance>
#include <vulkan/vulkan_win32.h>

DsSpoutVulkanImporter::~DsSpoutVulkanImporter()
{
    releaseResources();
}

VkFormat DsSpoutVulkanImporter::dxgiToVkFormat(DXGI_FORMAT format)
{
    switch (format) {
    // 4-channel 8-bit
    case DXGI_FORMAT_R8G8B8A8_UNORM:      return VK_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;
    case DXGI_FORMAT_B8G8R8A8_UNORM:      return VK_FORMAT_B8G8R8A8_UNORM;
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return VK_FORMAT_B8G8R8A8_SRGB;

    // 4-channel float
    case DXGI_FORMAT_R16G16B16A16_FLOAT:  return VK_FORMAT_R16G16B16A16_SFLOAT;
    case DXGI_FORMAT_R32G32B32A32_FLOAT:  return VK_FORMAT_R32G32B32A32_SFLOAT;

    // 1-channel
    case DXGI_FORMAT_R8_UNORM:            return VK_FORMAT_R8_UNORM;
    case DXGI_FORMAT_A8_UNORM:            return VK_FORMAT_R8_UNORM; // Vulkan has no A8; use R8
    case DXGI_FORMAT_R16_UNORM:           return VK_FORMAT_R16_UNORM;
    case DXGI_FORMAT_R16_FLOAT:           return VK_FORMAT_R16_SFLOAT;
    case DXGI_FORMAT_R32_FLOAT:           return VK_FORMAT_R32_SFLOAT;

    // 2-channel
    case DXGI_FORMAT_R8G8_UNORM:          return VK_FORMAT_R8G8_UNORM;
    case DXGI_FORMAT_R16G16_UNORM:        return VK_FORMAT_R16G16_UNORM;
    case DXGI_FORMAT_R16G16_FLOAT:        return VK_FORMAT_R16G16_SFLOAT;

    // Packed
    case DXGI_FORMAT_R10G10B10A2_UNORM:   return VK_FORMAT_A2B10G10R10_UNORM_PACK32;

    default:                              return VK_FORMAT_UNDEFINED;
    }
}

bool DsSpoutVulkanImporter::ensureCommandPool(QRhi* rhi)
{
    if (m_commandPool != VK_NULL_HANDLE)
        return true;

    auto nh = static_cast<const QRhiVulkanNativeHandles*>(rhi->nativeHandles());
    if (!nh || !nh->dev) {
        qWarning() << "DsSpoutVulkanImporter: Could not get Vulkan native handles";
        return false;
    }

    m_device         = nh->dev;
    m_physicalDevice = nh->physDev;
    m_queue          = nh->gfxQueue;

    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = nh->gfxQueueFamilyIdx;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VkResult result = vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool);
    if (result != VK_SUCCESS) {
        qWarning() << "DsSpoutVulkanImporter: Failed to create command pool";
        return false;
    }

    return true;
}

void DsSpoutVulkanImporter::cleanupVulkanResources()
{
    if (m_device == VK_NULL_HANDLE)
        return;

    if (m_image != VK_NULL_HANDLE) {
        vkDestroyImage(m_device, m_image, nullptr);
        m_image = VK_NULL_HANDLE;
    }
    if (m_memory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_memory, nullptr);
        m_memory = VK_NULL_HANDLE;
    }
}

QSharedPointer<QRhiTexture> DsSpoutVulkanImporter::import(
    QRhi* rhi,
    HANDLE handle,
    const QSize& size,
    DXGI_FORMAT format,
    const QImage& /*cpuFallback*/)
{
    if (!rhi || !handle || size.isEmpty())
        return {};

    // Reuse if handle and size haven't changed
    if (handle == m_currentHandle && size == m_currentSize && m_rhiTexture)
        return m_rhiTexture;

    if (!ensureCommandPool(rhi))
        return {};

    VkFormat vkFormat = dxgiToVkFormat(format);
    if (vkFormat == VK_FORMAT_UNDEFINED) {
        qWarning() << "DsSpoutVulkanImporter: Unsupported DXGI format" << format;
        return {};
    }

    // Clean up previous texture resources
    m_rhiTexture.reset();
    cleanupVulkanResources();

    vkDeviceWaitIdle(m_device);

    // Create VkImage with external memory info for D3D11 KMT handles
    VkExternalMemoryImageCreateInfo externalMemoryInfo = {};
    externalMemoryInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    externalMemoryInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT;

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.pNext = &externalMemoryInfo;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = vkFormat;
    imageInfo.extent = { (uint32_t)size.width(), (uint32_t)size.height(), 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkResult result = vkCreateImage(m_device, &imageInfo, nullptr, &m_image);
    if (result != VK_SUCCESS) {
        qWarning() << "DsSpoutVulkanImporter: Failed to create VkImage";
        return {};
    }

    // Get memory requirements and import external memory
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(m_device, m_image, &memReqs);

    VkImportMemoryWin32HandleInfoKHR importInfo = {};
    importInfo.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR;
    importInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT;
    importInfo.handle = handle;

    VkMemoryDedicatedAllocateInfo dedicatedInfo = {};
    dedicatedInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
    dedicatedInfo.pNext = &importInfo;
    dedicatedInfo.image = m_image;

    // Find suitable memory type
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProps);

    uint32_t memTypeIndex = UINT32_MAX;
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((memReqs.memoryTypeBits & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            memTypeIndex = i;
            break;
        }
    }

    if (memTypeIndex == UINT32_MAX) {
        qWarning() << "DsSpoutVulkanImporter: No suitable memory type found";
        cleanupVulkanResources();
        return {};
    }

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.pNext = &dedicatedInfo;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = memTypeIndex;

    result = vkAllocateMemory(m_device, &allocInfo, nullptr, &m_memory);
    if (result != VK_SUCCESS) {
        qWarning() << "DsSpoutVulkanImporter: Failed to allocate memory";
        cleanupVulkanResources();
        return {};
    }

    result = vkBindImageMemory(m_device, m_image, m_memory, 0);
    if (result != VK_SUCCESS) {
        qWarning() << "DsSpoutVulkanImporter: Failed to bind image memory";
        cleanupVulkanResources();
        return {};
    }

    // Layout transition: UNDEFINED → SHADER_READ_ONLY_OPTIMAL
    {
        VkCommandBufferAllocateInfo cmdBufInfo = {};
        cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdBufInfo.commandPool = m_commandPool;
        cmdBufInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdBufInfo.commandBufferCount = 1;

        VkCommandBuffer cmdBuf;
        result = vkAllocateCommandBuffers(m_device, &cmdBufInfo, &cmdBuf);
        if (result != VK_SUCCESS) {
            qWarning() << "DsSpoutVulkanImporter: Failed to allocate command buffer";
            cleanupVulkanResources();
            return {};
        }

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmdBuf, &beginInfo);

        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(
            cmdBuf,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        vkEndCommandBuffer(cmdBuf);

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdBuf;

        result = vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE);
        if (result != VK_SUCCESS) {
            qWarning() << "DsSpoutVulkanImporter: Failed to submit layout transition";
            vkFreeCommandBuffers(m_device, m_commandPool, 1, &cmdBuf);
            cleanupVulkanResources();
            return {};
        }

        vkQueueWaitIdle(m_queue);
        vkFreeCommandBuffers(m_device, m_commandPool, 1, &cmdBuf);
    }

    QRhiTexture::Format rhiFormat = dxgiToRhiFormat(format);
    if (rhiFormat == QRhiTexture::UnknownFormat) {
        cleanupVulkanResources();
        return {};
    }

    QRhiTexture* rhiTex = rhi->newTexture(rhiFormat, size, 1, {});
    QRhiTexture::NativeTexture nativeTex;
    nativeTex.object = quint64(m_image);
    nativeTex.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    if (!rhiTex->createFrom(nativeTex)) {
        qWarning() << "DsSpoutVulkanImporter: Failed to create QRhiTexture from VkImage";
        delete rhiTex;
        cleanupVulkanResources();
        return {};
    }

    m_rhiTexture = QSharedPointer<QRhiTexture>(rhiTex);
    m_currentHandle = handle;
    m_currentSize = size;
    return m_rhiTexture;
}

void DsSpoutVulkanImporter::releaseResources()
{
    m_rhiTexture.reset();
    cleanupVulkanResources();

    if (m_commandPool != VK_NULL_HANDLE && m_device != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_device, m_commandPool, nullptr);
        m_commandPool = VK_NULL_HANDLE;
    }

    m_device = VK_NULL_HANDLE;
    m_physicalDevice = VK_NULL_HANDLE;
    m_queue = VK_NULL_HANDLE;
    m_currentHandle = nullptr;
    m_currentSize = QSize();
}

#endif // DSSPOUT_VULKAN_ENABLED
