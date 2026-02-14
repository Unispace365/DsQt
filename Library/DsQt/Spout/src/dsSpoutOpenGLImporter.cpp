#include "dsSpoutOpenGLImporter.h"

#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLFunctions>

// GL constants we need
#ifndef GL_TEXTURE_2D
#define GL_TEXTURE_2D 0x0DE1
#endif
#ifndef WGL_ACCESS_READ_ONLY_NV
#define WGL_ACCESS_READ_ONLY_NV 0x0000
#endif

DsSpoutOpenGLImporter::~DsSpoutOpenGLImporter()
{
    releaseResources();
}

bool DsSpoutOpenGLImporter::probeInteropSupport()
{
    if (m_interopProbed)
        return m_interopSupported;

    m_interopProbed = true;
    m_interopSupported = false;

    // Get WGL extension function pointers via wglGetProcAddress
    auto wglGetProc = reinterpret_cast<PROC(WINAPI*)(LPCSTR)>(
        GetProcAddress(GetModuleHandleA("opengl32.dll"), "wglGetProcAddress"));

    if (!wglGetProc) {
        qDebug() << "DsSpoutOpenGLImporter: wglGetProcAddress not available";
        return false;
    }

    // Check for the extension string
    auto wglGetExtensionsStringARB = reinterpret_cast<const char* (WINAPI*)(HDC)>(
        wglGetProc("wglGetExtensionsStringARB"));

    if (wglGetExtensionsStringARB) {
        HDC hdc = wglGetCurrentDC();
        if (hdc) {
            const char* extensions = wglGetExtensionsStringARB(hdc);
            if (extensions && strstr(extensions, "WGL_NV_DX_interop2")) {
                // Load function pointers
                m_wglDXOpenDeviceNV = reinterpret_cast<PFN_wglDXOpenDeviceNV>(
                    wglGetProc("wglDXOpenDeviceNV"));
                m_wglDXCloseDeviceNV = reinterpret_cast<PFN_wglDXCloseDeviceNV>(
                    wglGetProc("wglDXCloseDeviceNV"));
                m_wglDXRegisterObjectNV = reinterpret_cast<PFN_wglDXRegisterObjectNV>(
                    wglGetProc("wglDXRegisterObjectNV"));
                m_wglDXUnregisterObjectNV = reinterpret_cast<PFN_wglDXUnregisterObjectNV>(
                    wglGetProc("wglDXUnregisterObjectNV"));
                m_wglDXLockObjectsNV = reinterpret_cast<PFN_wglDXLockObjectsNV>(
                    wglGetProc("wglDXLockObjectsNV"));
                m_wglDXUnlockObjectsNV = reinterpret_cast<PFN_wglDXUnlockObjectsNV>(
                    wglGetProc("wglDXUnlockObjectsNV"));

                m_interopSupported = m_wglDXOpenDeviceNV
                    && m_wglDXCloseDeviceNV
                    && m_wglDXRegisterObjectNV
                    && m_wglDXUnregisterObjectNV
                    && m_wglDXLockObjectsNV
                    && m_wglDXUnlockObjectsNV;
            }
        }
    }

    if (m_interopSupported) {
        qDebug() << "DsSpoutOpenGLImporter: WGL_NV_DX_interop2 available (zero-copy path)";
    } else {
        qDebug() << "DsSpoutOpenGLImporter: WGL_NV_DX_interop2 not available, using CPU fallback";
    }

    return m_interopSupported;
}

bool DsSpoutOpenGLImporter::initInterop()
{
    if (m_interopDevice)
        return true;

    // Create our own D3D11 device for interop (Qt is running OpenGL, not D3D11)
    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        nullptr, 0,
        D3D11_SDK_VERSION,
        &m_d3d11Device,
        nullptr,
        nullptr);

    if (FAILED(hr) || !m_d3d11Device) {
        qWarning() << "DsSpoutOpenGLImporter: Failed to create D3D11 device, hr ="
                   << Qt::hex << hr;
        return false;
    }

    // Register the D3D11 device for GL interop
    m_interopDevice = m_wglDXOpenDeviceNV(m_d3d11Device.Get());
    if (!m_interopDevice) {
        qWarning() << "DsSpoutOpenGLImporter: wglDXOpenDeviceNV failed";
        m_d3d11Device.Reset();
        return false;
    }

    return true;
}

void DsSpoutOpenGLImporter::cleanupInterop()
{
    if (m_interopObject && m_interopDevice) {
        m_wglDXUnregisterObjectNV(m_interopDevice, m_interopObject);
        m_interopObject = nullptr;
    }

    if (m_glTexture) {
        auto ctx = QOpenGLContext::currentContext();
        if (ctx) {
            ctx->functions()->glDeleteTextures(1, &m_glTexture);
        }
        m_glTexture = 0;
    }

    if (m_interopDevice) {
        m_wglDXCloseDeviceNV(m_interopDevice);
        m_interopDevice = nullptr;
    }

    m_d3d11Device.Reset();
}

QSharedPointer<QRhiTexture> DsSpoutOpenGLImporter::importViaInterop(
    QRhi* rhi, HANDLE handle, const QSize& size, DXGI_FORMAT format)
{
    if (!initInterop())
        return {};

    // If handle or size changed, re-register
    if (handle != m_currentHandle || size != m_currentSize) {
        // Unregister old
        if (m_interopObject) {
            m_wglDXUnregisterObjectNV(m_interopDevice, m_interopObject);
            m_interopObject = nullptr;
        }
        if (m_glTexture) {
            auto ctx = QOpenGLContext::currentContext();
            if (ctx) ctx->functions()->glDeleteTextures(1, &m_glTexture);
            m_glTexture = 0;
        }
        m_rhiTexture.reset();

        // Open the Spout shared texture on our D3D11 device
        ID3D11Texture2D* sharedTex = nullptr;
        HRESULT hr = m_d3d11Device->OpenSharedResource(
            handle, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&sharedTex));

        if (FAILED(hr) || !sharedTex) {
            qWarning() << "DsSpoutOpenGLImporter: Failed to open shared resource, hr ="
                       << Qt::hex << hr;
            return {};
        }

        // Create GL texture
        auto ctx = QOpenGLContext::currentContext();
        if (!ctx) {
            sharedTex->Release();
            return {};
        }
        ctx->functions()->glGenTextures(1, &m_glTexture);

        // Register the D3D11 texture for GL interop
        m_interopObject = m_wglDXRegisterObjectNV(
            m_interopDevice,
            sharedTex,
            m_glTexture,
            GL_TEXTURE_2D,
            WGL_ACCESS_READ_ONLY_NV);

        sharedTex->Release();

        if (!m_interopObject) {
            qWarning() << "DsSpoutOpenGLImporter: wglDXRegisterObjectNV failed";
            ctx->functions()->glDeleteTextures(1, &m_glTexture);
            m_glTexture = 0;
            return {};
        }

        QRhiTexture::Format rhiFormat = dxgiToRhiFormat(format);
        if (rhiFormat == QRhiTexture::UnknownFormat)
            return {};

        QRhiTexture* rhiTex = rhi->newTexture(rhiFormat, size, 1, {});
        if (!rhiTex->createFrom({quint64(m_glTexture), 0})) {
            qWarning() << "DsSpoutOpenGLImporter: Failed to create QRhiTexture from GL texture";
            delete rhiTex;
            return {};
        }

        m_rhiTexture = QSharedPointer<QRhiTexture>(rhiTex);
        m_currentHandle = handle;
        m_currentSize = size;
    }

    // Lock for reading, unlock after use
    if (m_interopObject) {
        m_wglDXLockObjectsNV(m_interopDevice, 1, &m_interopObject);
        m_wglDXUnlockObjectsNV(m_interopDevice, 1, &m_interopObject);
    }

    return m_rhiTexture;
}

QSharedPointer<QRhiTexture> DsSpoutOpenGLImporter::importViaCpu(
    QRhi* rhi, const QSize& size, const QImage& image)
{
    if (image.isNull() || size.isEmpty())
        return {};

    // Create or recreate the texture if size changed
    if (!m_rhiTexture || m_currentSize != size) {
        m_rhiTexture.reset();
        QRhiTexture* rhiTex = rhi->newTexture(QRhiTexture::RGBA8, size, 1, {});
        if (!rhiTex->create()) {
            qWarning() << "DsSpoutOpenGLImporter: Failed to create QRhiTexture for CPU upload";
            delete rhiTex;
            return {};
        }
        m_rhiTexture = QSharedPointer<QRhiTexture>(rhiTex);
        m_currentSize = size;
    }

    // Upload via an offscreen frame so the texture data is available immediately
    QRhiCommandBuffer* cb = nullptr;
    if (rhi->beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess) {
        qWarning() << "DsSpoutOpenGLImporter: Failed to begin offscreen frame for CPU upload";
        return m_rhiTexture;
    }

    QRhiResourceUpdateBatch* batch = rhi->nextResourceUpdateBatch();
    QRhiTextureSubresourceUploadDescription subresDesc(image);
    QRhiTextureUploadDescription uploadDesc({0, 0, subresDesc});
    batch->uploadTexture(m_rhiTexture.get(), uploadDesc);
    cb->resourceUpdate(batch);

    rhi->endOffscreenFrame();

    return m_rhiTexture;
}

QSharedPointer<QRhiTexture> DsSpoutOpenGLImporter::import(
    QRhi* rhi,
    HANDLE handle,
    const QSize& size,
    DXGI_FORMAT format,
    const QImage& cpuFallback)
{
    if (!rhi || size.isEmpty())
        return {};

    if (probeInteropSupport() && handle) {
        return importViaInterop(rhi, handle, size, format);
    }

    // CPU fallback path
    return importViaCpu(rhi, size, cpuFallback);
}

void DsSpoutOpenGLImporter::releaseResources()
{
    m_rhiTexture.reset();
    cleanupInterop();
    m_currentHandle = nullptr;
    m_currentSize = QSize();
}
