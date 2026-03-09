#pragma once

#include <memory>

#include <QSharedPointer>
#include <QSize>
#include <QImage>
#include <QSGRendererInterface>
#include <rhi/qrhi.h>

#include <windows.h>
#include <dxgi.h>

class DsSpoutReceiver;

/**
 * DsSpoutTextureImporter - Abstract base for importing Spout's D3D11 shared
 * texture handle into whatever RHI backend Qt is using.
 *
 * Spout always shares via D3D11 KMT handles. Each subclass bridges that handle
 * into its target graphics API (D3D11, D3D12, Vulkan, OpenGL).
 */
class DsSpoutTextureImporter
{
public:
    virtual ~DsSpoutTextureImporter() = default;

    /**
     * Import a Spout shared texture handle into a QRhiTexture.
     *
     * @param rhi        Qt's RHI instance
     * @param handle     D3D11 KMT shared texture handle from Spout
     * @param size       Texture dimensions
     * @param format     DXGI_FORMAT of the shared texture
     * @param cpuFallback Optional QImage for backends that can't do GPU interop
     * @return Shared pointer to the imported QRhiTexture, or null on failure
     */
    virtual QSharedPointer<QRhiTexture> import(
        QRhi* rhi,
        HANDLE handle,
        const QSize& size,
        DXGI_FORMAT format,
        const QImage& cpuFallback = {}) = 0;

    /** Release all cached GPU resources. */
    virtual void releaseResources() = 0;

    /**
     * Factory: create the appropriate importer for the current RHI backend.
     * @param api The graphics API in use (from QSGRendererInterface::graphicsApi())
     * @return A new importer instance, or nullptr if the backend is unsupported
     */
    static std::unique_ptr<DsSpoutTextureImporter> create(
        QSGRendererInterface::GraphicsApi api);

    /**
     * Convert a DXGI_FORMAT to QRhiTexture::Format.
     * Supports all formats that TouchDesigner can output via Spout.
     * @return QRhiTexture::UnknownFormat if the format is not supported.
     */
    static QRhiTexture::Format dxgiToRhiFormat(DXGI_FORMAT format);
};
