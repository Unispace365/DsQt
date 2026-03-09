#include "dsSpoutTextureImporter.h"
#include "dsSpoutD3D11Importer.h"
#include "dsSpoutD3D12Importer.h"
#include "dsSpoutOpenGLImporter.h"

#ifdef DSSPOUT_VULKAN_ENABLED
#include "dsSpoutVulkanImporter.h"
#endif

#include <QDebug>

std::unique_ptr<DsSpoutTextureImporter> DsSpoutTextureImporter::create(
    QSGRendererInterface::GraphicsApi api)
{
    switch (api) {
    case QSGRendererInterface::Direct3D11:
        return std::make_unique<DsSpoutD3D11Importer>();

    case QSGRendererInterface::Direct3D12:
        return std::make_unique<DsSpoutD3D12Importer>();

    case QSGRendererInterface::OpenGL:
        return std::make_unique<DsSpoutOpenGLImporter>();

    case QSGRendererInterface::Vulkan:
#ifdef DSSPOUT_VULKAN_ENABLED
        return std::make_unique<DsSpoutVulkanImporter>();
#else
        qWarning() << "DsSpoutTextureImporter: Vulkan backend requested but "
                       "DsSpout was built without Vulkan support";
        return nullptr;
#endif

    default:
        qWarning() << "DsSpoutTextureImporter: Unsupported graphics API" << api;
        return nullptr;
    }
}

QRhiTexture::Format DsSpoutTextureImporter::dxgiToRhiFormat(DXGI_FORMAT format)
{
    switch (format) {
    // 4-channel 8-bit
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        return QRhiTexture::RGBA8;
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        return QRhiTexture::BGRA8;

    // 4-channel float
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
        return QRhiTexture::RGBA16F;
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
        return QRhiTexture::RGBA32F;

    // 1-channel (Mono / R / A)
    case DXGI_FORMAT_R8_UNORM:
    case DXGI_FORMAT_A8_UNORM:
        return QRhiTexture::R8;
    case DXGI_FORMAT_R16_UNORM:
        return QRhiTexture::R16;
    case DXGI_FORMAT_R16_FLOAT:
        return QRhiTexture::R16F;
    case DXGI_FORMAT_R32_FLOAT:
        return QRhiTexture::R32F;

    // 2-channel (RG)
    case DXGI_FORMAT_R8G8_UNORM:
        return QRhiTexture::RG8;
    case DXGI_FORMAT_R16G16_UNORM:
    case DXGI_FORMAT_R16G16_FLOAT:
        // R16G16_FLOAT has no exact QRhi match; RG16 (unsigned normalized)
        // is the closest 2x16-bit format Qt offers.
        return QRhiTexture::RG16;

    // Packed formats
    case DXGI_FORMAT_R10G10B10A2_UNORM:
        return QRhiTexture::RGB10A2;

    // Formats without a direct QRhi equivalent — fall through to warning
    // DXGI_FORMAT_R16G16B16A16_UNORM  (16-bit fixed RGBA — no QRhi match)
    // DXGI_FORMAT_R11G11B10_FLOAT     (11-bit RGB — no QRhi match)
    // DXGI_FORMAT_R32G32_FLOAT        (RG 32-bit float — no QRhi match)
    default:
        qWarning() << "DsSpoutTextureImporter: Unsupported DXGI format" << format;
        return QRhiTexture::UnknownFormat;
    }
}
