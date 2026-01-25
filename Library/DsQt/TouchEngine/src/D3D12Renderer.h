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

#include "Renderer.h"
#include "D3D12Image.h"
#include <vector>
#include <rhi/qrhi.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

class D3D12Renderer : public Renderer
{
public:
    D3D12Renderer();
    virtual ~D3D12Renderer();

    virtual TEGraphicsContext*
    getTEContext() const override
    {
        return reinterpret_cast<TEGraphicsContext*>(myContext.get());
    }

    virtual bool setup(QQuickWindow* window) override;
    virtual bool configure(TEInstance* instance, QString& error) override;
    virtual void resize(int width, int height) override;
    virtual void stop() override;
    virtual bool render();
    virtual void clearInputImages() override;
    virtual void addOutputImage() override;
    virtual bool updateOutputImage(const TouchObject<TEInstance>& instance, size_t index, const std::string& identifier) override;
    virtual void clearOutputImages() override;
    virtual void releaseTextures() override;

    virtual QSharedPointer<QRhiTexture> getOutputRhiTexture(size_t index) override;

    ID3D12Device* getDevice() const { return myDevice.Get(); }
    const QString& getDeviceName() const { return myDeviceName; }

private:
    // These are borrowed from Qt's RHI - we don't own them
    ComPtr<ID3D12Device> myDevice;
    ComPtr<ID3D12CommandQueue> myCommandQueue;

    TouchObject<TED3D12Context> myContext;
    std::vector<D3D12Image> myInputImages;
    std::vector<D3D12Image> myOutputImages;
    QString myDeviceName;
    bool myIsSetupDone = false;
};

#endif // _WIN32
