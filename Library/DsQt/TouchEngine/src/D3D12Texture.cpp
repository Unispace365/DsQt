/* Shared Use License: This file is owned by Derivative Inc. (Derivative)
* and can only be used, and/or modified for use, in conjunction with
* Derivative's TouchDesigner software, and only if you are a licensee who has
* accepted Derivative's TouchDesigner license or assignment agreement
* (which also govern the use of this file). You may share or redistribute
* a modified version of this file provided the following conditions are met:
*
* 1. The shared file or redistribution must retain the information set out
* above and this list of conditions.
* 2. Derivative's name (Derivative Inc.) or its trademarks may not be used
* to endorse or promote products derived from this file without specific
* prior written permission from Derivative.
*/

#ifdef _WIN32

#include "D3D12Texture.h"
#include <TouchEngine/TED3D12.h>

D3D12Texture::D3D12Texture()
{
}

D3D12Texture::D3D12Texture(QRhi* rhi, const TouchObject<TED3DSharedTexture>& source, ID3D12Device* device)
    : mySource(source)
{
    if (!source || !device || !rhi)
        return;

    HANDLE sharedHandle = TED3DSharedTextureGetHandle(source);
    if (!sharedHandle)
        return;

    myWidth = static_cast<int>(TED3DSharedTextureGetWidth(source));
    myHeight = static_cast<int>(TED3DSharedTextureGetHeight(source));
    DXGI_FORMAT format = TED3DSharedTextureGetFormat(source);

    HRESULT hr = device->OpenSharedHandle(sharedHandle, IID_PPV_ARGS(&myResource));
    if (FAILED(hr) || !myResource)
    {
        myResource = nullptr;
        return;
    }

    QRhiTexture::Format rhiFormat = QRhiTexture::RGBA8;
    switch (format)
    {
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        rhiFormat = QRhiTexture::RGBA8;
        break;
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        rhiFormat = QRhiTexture::BGRA8;
        break;
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
        rhiFormat = QRhiTexture::RGBA16F;
        break;
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
        rhiFormat = QRhiTexture::RGBA32F;
        break;
    default:
        rhiFormat = QRhiTexture::RGBA8;
        break;
    }

    myRhiTexture.reset(rhi->newTexture(rhiFormat, QSize(myWidth, myHeight), 1,
        QRhiTexture::UsedAsTransferSource | QRhiTexture::RenderTarget));

    QRhiTexture::NativeTexture nativeTex;
    nativeTex.object = reinterpret_cast<quint64>(myResource.Get());
    nativeTex.layout = 0;

    if (!myRhiTexture->createFrom(nativeTex))
    {
        myRhiTexture.reset();
        myResource = nullptr;
    }
}

D3D12Texture::~D3D12Texture()
{
}

void D3D12Texture::release()
{
    myRhiTexture.reset();
    myResource.Reset();
    mySource = nullptr;
    myWidth = 0;
    myHeight = 0;
}

#endif // _WIN32
