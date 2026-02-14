#include "dsSpoutD3D11Importer.h"

#include <QDebug>
#include <d3d11_1.h>

DsSpoutD3D11Importer::~DsSpoutD3D11Importer()
{
    releaseResources();
}

QSharedPointer<QRhiTexture> DsSpoutD3D11Importer::import(
    QRhi* rhi,
    HANDLE handle,
    const QSize& size,
    DXGI_FORMAT format,
    const QImage& /*cpuFallback*/)
{
    if (!rhi || !handle || size.isEmpty())
        return {};

    // Reuse existing texture if handle hasn't changed
    if (handle == m_currentHandle && m_rhiTexture)
        return m_rhiTexture;

    m_currentHandle = handle;
    m_rhiTexture.reset();

    // Get Qt's D3D11 device
    auto nativeHandles = static_cast<const QRhiD3D11NativeHandles*>(rhi->nativeHandles());
    if (!nativeHandles || !nativeHandles->dev) {
        qWarning() << "DsSpoutD3D11Importer: Could not get D3D11 device from RHI";
        return {};
    }

    auto device = static_cast<ID3D11Device*>(nativeHandles->dev);

    // Open the Spout shared texture handle
    ID3D11Texture2D* sharedTexture = nullptr;
    HRESULT hr = device->OpenSharedResource(
        handle,
        __uuidof(ID3D11Texture2D),
        reinterpret_cast<void**>(&sharedTexture));

    if (FAILED(hr) || !sharedTexture) {
        qWarning() << "DsSpoutD3D11Importer: Failed to open shared resource, hr ="
                   << Qt::hex << hr;
        return {};
    }

    QRhiTexture::Format rhiFormat = dxgiToRhiFormat(format);
    if (rhiFormat == QRhiTexture::UnknownFormat) {
        sharedTexture->Release();
        return {};
    }

    QRhiTexture* rhiTex = rhi->newTexture(rhiFormat, size, 1, {});
    if (!rhiTex->createFrom({quint64(sharedTexture), 0})) {
        qWarning() << "DsSpoutD3D11Importer: Failed to create QRhiTexture from shared texture";
        sharedTexture->Release();
        delete rhiTex;
        return {};
    }

    // QRhiTexture now holds a reference
    sharedTexture->Release();

    m_rhiTexture = QSharedPointer<QRhiTexture>(rhiTex);
    return m_rhiTexture;
}

void DsSpoutD3D11Importer::releaseResources()
{
    m_rhiTexture.reset();
    m_currentHandle = nullptr;
}
