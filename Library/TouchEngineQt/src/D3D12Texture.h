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

#pragma once

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <memory>
#include <functional>
#include <d3d12.h>
#include <wrl/client.h>
#include <TouchEngine/TouchObject.h>
#include <TouchEngine/TED3D.h>
#include <rhi/qrhi.h>

using Microsoft::WRL::ComPtr;

class D3D12Texture
{
public:
    D3D12Texture();
    D3D12Texture(QRhi* rhi, const TouchObject<TED3DSharedTexture>& texture, ID3D12Device* device);
    D3D12Texture(const D3D12Texture& o) = default;
    D3D12Texture(D3D12Texture&& o) noexcept = default;
    D3D12Texture& operator=(const D3D12Texture& o) = default;
    D3D12Texture& operator=(D3D12Texture&& o) noexcept = default;
    ~D3D12Texture();

    ID3D12Resource* getResource() const { return myResource.Get(); }

    constexpr const TouchObject<TED3DSharedTexture>&
    getSource() const
    {
        return mySource;
    }

    QSharedPointer<QRhiTexture> getRhiTexture() const { return myRhiTexture; }

    int getWidth() const { return myWidth; }
    int getHeight() const { return myHeight; }
    bool isValid() const { return myResource != nullptr; }

    void release();

private:
    TouchObject<TED3DSharedTexture> mySource;
    ComPtr<ID3D12Resource> myResource;
    QSharedPointer<QRhiTexture> myRhiTexture;
    int myWidth = 0;
    int myHeight = 0;
};

#endif // _WIN32
