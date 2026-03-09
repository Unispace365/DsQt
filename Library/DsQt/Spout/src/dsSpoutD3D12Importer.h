#pragma once

#include "dsSpoutTextureImporter.h"

#include <d3d11.h>
#include <d3d12.h>
#include <d3d11on12.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

/**
 * DsSpoutD3D12Importer - Import Spout's D3D11 KMT handle into a D3D12 RHI.
 *
 * Spout shares via D3D11 KMT handles, but D3D12's OpenSharedHandle only
 * accepts NT handles. We bridge via D3D11On12: create a D3D11 device on top
 * of Qt's D3D12 device, open the KMT handle on the D3D11 side, and GPU-copy
 * into a D3D12 render target that we wrap as a QRhiTexture.
 */
class DsSpoutD3D12Importer : public DsSpoutTextureImporter
{
public:
    ~DsSpoutD3D12Importer() override;

    QSharedPointer<QRhiTexture> import(
        QRhi* rhi,
        HANDLE handle,
        const QSize& size,
        DXGI_FORMAT format,
        const QImage& cpuFallback = {}) override;

    void releaseResources() override;

private:
    bool ensureBridge(QRhi* rhi);
    bool ensureRenderTarget(QRhi* rhi, const QSize& size, DXGI_FORMAT format);

    // D3D11On12 bridge (created once, reused)
    ComPtr<ID3D11On12Device> m_d3d11On12;
    ComPtr<ID3D11Device>     m_d3d11Device;
    ComPtr<ID3D11DeviceContext> m_d3d11Ctx;

    // D3D12 render target + wrapped D3D11 version (recreated on size change)
    ComPtr<ID3D12Resource>   m_d3d12RenderTarget;
    ComPtr<ID3D11Resource>   m_wrappedResource;
    QSize                    m_rtSize;
    DXGI_FORMAT              m_rtFormat = DXGI_FORMAT_UNKNOWN;

    QSharedPointer<QRhiTexture> m_rhiTexture;
    HANDLE m_currentHandle = nullptr;
};
