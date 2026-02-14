#pragma once

#include "dsSpoutTextureImporter.h"

#include <d3d11.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

/**
 * DsSpoutOpenGLImporter - Import Spout's D3D11 KMT handle into an OpenGL RHI.
 *
 * Two paths:
 *  1. NVIDIA (WGL_NV_DX_interop2): zero-copy interop between our own D3D11
 *     device and OpenGL. The shared texture is registered and locked/unlocked
 *     per frame.
 *  2. CPU fallback (AMD/Intel): uses the QImage from DsSpoutReceiver::lastFrame()
 *     and uploads via QRhiResourceUpdateBatch.
 */
class DsSpoutOpenGLImporter : public DsSpoutTextureImporter
{
public:
    ~DsSpoutOpenGLImporter() override;

    QSharedPointer<QRhiTexture> import(
        QRhi* rhi,
        HANDLE handle,
        const QSize& size,
        DXGI_FORMAT format,
        const QImage& cpuFallback = {}) override;

    void releaseResources() override;

private:
    bool initInterop();
    void cleanupInterop();
    bool probeInteropSupport();

    QSharedPointer<QRhiTexture> importViaInterop(
        QRhi* rhi, HANDLE handle, const QSize& size, DXGI_FORMAT format);
    QSharedPointer<QRhiTexture> importViaCpu(
        QRhi* rhi, const QSize& size, const QImage& image);

    // WGL_NV_DX_interop state
    bool m_interopProbed   = false;
    bool m_interopSupported = false;

    ComPtr<ID3D11Device> m_d3d11Device;
    HANDLE m_interopDevice  = nullptr;    // wglDXOpenDeviceNV handle
    HANDLE m_interopObject  = nullptr;    // wglDXRegisterObjectNV handle
    unsigned int m_glTexture = 0;
    HANDLE m_currentHandle  = nullptr;
    QSize  m_currentSize;

    QSharedPointer<QRhiTexture> m_rhiTexture;

    // Function pointers for WGL_NV_DX_interop
    using PFN_wglDXOpenDeviceNV = HANDLE (WINAPI*)(void*);
    using PFN_wglDXCloseDeviceNV = BOOL (WINAPI*)(HANDLE);
    using PFN_wglDXRegisterObjectNV = HANDLE (WINAPI*)(HANDLE, void*, unsigned int, unsigned int, unsigned int);
    using PFN_wglDXUnregisterObjectNV = BOOL (WINAPI*)(HANDLE, HANDLE);
    using PFN_wglDXLockObjectsNV = BOOL (WINAPI*)(HANDLE, int, HANDLE*);
    using PFN_wglDXUnlockObjectsNV = BOOL (WINAPI*)(HANDLE, int, HANDLE*);

    PFN_wglDXOpenDeviceNV       m_wglDXOpenDeviceNV       = nullptr;
    PFN_wglDXCloseDeviceNV      m_wglDXCloseDeviceNV      = nullptr;
    PFN_wglDXRegisterObjectNV   m_wglDXRegisterObjectNV   = nullptr;
    PFN_wglDXUnregisterObjectNV m_wglDXUnregisterObjectNV = nullptr;
    PFN_wglDXLockObjectsNV      m_wglDXLockObjectsNV      = nullptr;
    PFN_wglDXUnlockObjectsNV    m_wglDXUnlockObjectsNV    = nullptr;
};
