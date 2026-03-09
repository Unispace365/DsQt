#pragma once

#include "dsSpoutTextureImporter.h"

#include <d3d11.h>

/**
 * DsSpoutD3D11Importer - Zero-copy import when Qt's RHI is D3D11.
 *
 * Opens the Spout KMT shared texture handle directly on Qt's D3D11 device
 * and wraps it as a QRhiTexture via createFrom().
 */
class DsSpoutD3D11Importer : public DsSpoutTextureImporter
{
public:
    ~DsSpoutD3D11Importer() override;

    QSharedPointer<QRhiTexture> import(
        QRhi* rhi,
        HANDLE handle,
        const QSize& size,
        DXGI_FORMAT format,
        const QImage& cpuFallback = {}) override;

    void releaseResources() override;

private:
    QSharedPointer<QRhiTexture> m_rhiTexture;
    HANDLE m_currentHandle = nullptr;
};
