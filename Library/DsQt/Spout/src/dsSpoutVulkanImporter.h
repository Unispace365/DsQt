#pragma once

// Vulkan headers MUST come before Qt headers to avoid VK_NO_PROTOTYPES
// (Qt's qvulkanfunctions.h defines it, which suppresses function prototypes)
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include "dsSpoutTextureImporter.h"

#ifdef DSSPOUT_VULKAN_ENABLED

/**
 * DsSpoutVulkanImporter - Import Spout's D3D11 KMT handle via Vulkan
 * external memory (VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT).
 *
 * Follows the TouchEngine VulkanTexture pattern: create a VkImage backed by
 * imported external memory, transition to SHADER_READ_ONLY_OPTIMAL, and wrap
 * via QRhiTexture::createFrom().
 */
class DsSpoutVulkanImporter : public DsSpoutTextureImporter
{
public:
    ~DsSpoutVulkanImporter() override;

    QSharedPointer<QRhiTexture> import(
        QRhi* rhi,
        HANDLE handle,
        const QSize& size,
        DXGI_FORMAT format,
        const QImage& cpuFallback = {}) override;

    void releaseResources() override;

private:
    bool ensureCommandPool(QRhi* rhi);
    void cleanupVulkanResources();
    static VkFormat dxgiToVkFormat(DXGI_FORMAT format);

    // Cached from RHI (not owned)
    VkDevice         m_device         = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkQueue          m_queue          = VK_NULL_HANDLE;

    // Owned resources
    VkCommandPool    m_commandPool = VK_NULL_HANDLE;
    VkImage          m_image       = VK_NULL_HANDLE;
    VkDeviceMemory   m_memory      = VK_NULL_HANDLE;

    QSharedPointer<QRhiTexture> m_rhiTexture;
    HANDLE m_currentHandle = nullptr;
    QSize  m_currentSize;
};

#endif // DSSPOUT_VULKAN_ENABLED
