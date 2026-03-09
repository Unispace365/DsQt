#include "dsSpoutD3D12Importer.h"

#include <QDebug>

DsSpoutD3D12Importer::~DsSpoutD3D12Importer()
{
    releaseResources();
}

bool DsSpoutD3D12Importer::ensureBridge(QRhi* rhi)
{
    if (m_d3d11On12)
        return true;

    // Get Qt's D3D12 device and command queue
    auto nativeHandles = static_cast<const QRhiD3D12NativeHandles*>(rhi->nativeHandles());
    if (!nativeHandles || !nativeHandles->dev) {
        qWarning() << "DsSpoutD3D12Importer: Could not get D3D12 native handles";
        return false;
    }

    auto d3d12Device = static_cast<ID3D12Device*>(nativeHandles->dev);
    auto cmdQueue = static_cast<ID3D12CommandQueue*>(nativeHandles->commandQueue);

    // Create a D3D11On12 device wrapping Qt's D3D12 device
    IUnknown* queues[] = { cmdQueue };
    ComPtr<ID3D11Device> d3d11Device;
    ComPtr<ID3D11DeviceContext> d3d11Ctx;

    HRESULT hr = D3D11On12CreateDevice(
        d3d12Device,
        0,               // flags
        nullptr, 0,      // feature levels (use default)
        queues, 1,       // command queues
        0,               // node mask
        &d3d11Device,
        &d3d11Ctx,
        nullptr);        // chosen feature level

    if (FAILED(hr)) {
        qWarning() << "DsSpoutD3D12Importer: D3D11On12CreateDevice failed, hr ="
                   << Qt::hex << hr;
        return false;
    }

    hr = d3d11Device.As(&m_d3d11On12);
    if (FAILED(hr)) {
        qWarning() << "DsSpoutD3D12Importer: QueryInterface for ID3D11On12Device failed";
        return false;
    }

    m_d3d11Device = d3d11Device;
    m_d3d11Ctx = d3d11Ctx;
    return true;
}

bool DsSpoutD3D12Importer::ensureRenderTarget(QRhi* rhi, const QSize& size, DXGI_FORMAT format)
{
    if (m_d3d12RenderTarget && m_rtSize == size && m_rtFormat == format)
        return true;

    // Release old wrapped resource first
    if (m_wrappedResource) {
        m_d3d11On12->ReleaseWrappedResources(m_wrappedResource.GetAddressOf(), 1);
        m_wrappedResource.Reset();
    }
    m_d3d12RenderTarget.Reset();
    m_rhiTexture.reset();

    // Get Qt's D3D12 device
    auto nativeHandles = static_cast<const QRhiD3D12NativeHandles*>(rhi->nativeHandles());
    auto d3d12Device = static_cast<ID3D12Device*>(nativeHandles->dev);

    // Create a committed D3D12 resource as render target
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = size.width();
    desc.Height = size.height();
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    HRESULT hr = d3d12Device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&m_d3d12RenderTarget));

    if (FAILED(hr)) {
        qWarning() << "DsSpoutD3D12Importer: Failed to create D3D12 render target, hr ="
                   << Qt::hex << hr;
        return false;
    }

    // Wrap the D3D12 resource for D3D11 access
    D3D11_RESOURCE_FLAGS d3d11Flags = {};
    hr = m_d3d11On12->CreateWrappedResource(
        m_d3d12RenderTarget.Get(),
        &d3d11Flags,
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_COMMON,
        IID_PPV_ARGS(&m_wrappedResource));

    if (FAILED(hr)) {
        qWarning() << "DsSpoutD3D12Importer: Failed to wrap D3D12 resource, hr ="
                   << Qt::hex << hr;
        m_d3d12RenderTarget.Reset();
        return false;
    }

    m_rtSize = size;
    m_rtFormat = format;

    QRhiTexture::Format rhiFormat = dxgiToRhiFormat(format);
    if (rhiFormat == QRhiTexture::UnknownFormat)
        return false;

    QRhiTexture* rhiTex = rhi->newTexture(rhiFormat, size, 1, {});
    if (!rhiTex->createFrom({quint64(m_d3d12RenderTarget.Get()), 0})) {
        qWarning() << "DsSpoutD3D12Importer: Failed to create QRhiTexture from D3D12 resource";
        delete rhiTex;
        return false;
    }

    m_rhiTexture = QSharedPointer<QRhiTexture>(rhiTex);
    return true;
}

QSharedPointer<QRhiTexture> DsSpoutD3D12Importer::import(
    QRhi* rhi,
    HANDLE handle,
    const QSize& size,
    DXGI_FORMAT format,
    const QImage& /*cpuFallback*/)
{
    if (!rhi || !handle || size.isEmpty())
        return {};

    if (!ensureBridge(rhi))
        return {};

    if (!ensureRenderTarget(rhi, size, format))
        return {};

    // Open the Spout KMT handle on the D3D11 side
    ID3D11Texture2D* spoutTexture = nullptr;
    HRESULT hr = m_d3d11Device->OpenSharedResource(
        handle,
        __uuidof(ID3D11Texture2D),
        reinterpret_cast<void**>(&spoutTexture));

    if (FAILED(hr) || !spoutTexture) {
        qWarning() << "DsSpoutD3D12Importer: Failed to open shared resource, hr ="
                   << Qt::hex << hr;
        return {};
    }

    // Acquire the wrapped D3D12 resource for D3D11 use, copy, then release
    m_d3d11On12->AcquireWrappedResources(m_wrappedResource.GetAddressOf(), 1);
    m_d3d11Ctx->CopyResource(m_wrappedResource.Get(), spoutTexture);
    m_d3d11On12->ReleaseWrappedResources(m_wrappedResource.GetAddressOf(), 1);
    m_d3d11Ctx->Flush();

    spoutTexture->Release();

    m_currentHandle = handle;
    return m_rhiTexture;
}

void DsSpoutD3D12Importer::releaseResources()
{
    m_rhiTexture.reset();

    if (m_wrappedResource && m_d3d11On12) {
        m_d3d11On12->ReleaseWrappedResources(m_wrappedResource.GetAddressOf(), 1);
    }
    m_wrappedResource.Reset();
    m_d3d12RenderTarget.Reset();

    m_d3d11Ctx.Reset();
    m_d3d11On12.Reset();
    m_d3d11Device.Reset();

    m_rtSize = QSize();
    m_rtFormat = DXGI_FORMAT_UNKNOWN;
    m_currentHandle = nullptr;
}
