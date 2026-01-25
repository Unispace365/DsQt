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

#include "D3D12Renderer.h"
#include <TouchEngine/TouchEngine.h>
#include <TouchEngine/TED3D12.h>
#include <TouchEngine/TED3D.h>
#include <QtCore>
#include <rhi/qrhi.h>

D3D12Renderer::D3D12Renderer()
{
}

D3D12Renderer::~D3D12Renderer()
{
    stop();
}

bool
D3D12Renderer::setup(QQuickWindow* window)
{
    bool success = Renderer::setup(window);

    if (!success)
        return false;

    // Get the RHI from the window
    QRhi* rhi = window->rhi();
    if (!rhi)
    {
        qDebug() << "D3D12Renderer: No RHI available from window";
        return false;
    }

    if (rhi->backend() != QRhi::D3D12)
    {
        qDebug() << "D3D12Renderer: RHI backend is not D3D12";
        return false;
    }

    // Get native D3D12 handles from Qt's RHI - use the SAME device as Qt
    const QRhiD3D12NativeHandles* nativeHandles = static_cast<const QRhiD3D12NativeHandles*>(rhi->nativeHandles());
    if (!nativeHandles || !nativeHandles->dev)
    {
        qDebug() << "D3D12Renderer: Failed to get D3D12 native handles from RHI";
        return false;
    }

    // Use Qt's D3D12 device - this ensures textures are on the same device
    // nativeHandles->dev is void*, need to cast
    myDevice = static_cast<ID3D12Device*>(nativeHandles->dev);
    myCommandQueue = static_cast<ID3D12CommandQueue*>(nativeHandles->commandQueue);

    // Get device name for logging
    ComPtr<IDXGIDevice> dxgiDevice;
    if (SUCCEEDED(myDevice->QueryInterface(IID_PPV_ARGS(&dxgiDevice))))
    {
        ComPtr<IDXGIAdapter> adapter;
        if (SUCCEEDED(dxgiDevice->GetAdapter(&adapter)))
        {
            DXGI_ADAPTER_DESC desc;
            if (SUCCEEDED(adapter->GetDesc(&desc)))
            {
                myDeviceName = QString::fromWCharArray(desc.Description);
                qDebug() << "D3D12Renderer: Using Qt's D3D12 device:" << myDeviceName;
            }
        }
    }

    // Use the window's RHI directly
    myRhi = QSharedPointer<QRhi>(rhi, [](QRhi*) {});

    // Create TouchEngine D3D12 context using Qt's device
    TEResult result = TED3D12ContextCreate(myDevice.Get(), myContext.take());
    if (result != TEResultSuccess)
    {
        qDebug() << "D3D12Renderer: Failed to create TouchEngine D3D12 context";
        return false;
    }

    myIsSetupDone = true;
    return true;
}

bool
D3D12Renderer::configure(TEInstance* instance, QString& error)
{
    if (!myDevice)
    {
        error = "D3D12 device not initialized";
        return false;
    }

    int32_t count = 0;
    TEResult result = TEInstanceGetSupportedD3DHandleTypes(instance, nullptr, &count);
    // When querying count (nullptr for array), result may be TEResultInsufficientMemory (1)
    // which is expected. We just need count > 0 to proceed.
    if (count == 0)
    {
        error = "D3D12 is not supported. No supported handle types found.";
        error += "\nThe selected GPU is: ";
        error += myDeviceName;
        return false;
    }

    std::vector<TED3DHandleType> types(count);
    result = TEInstanceGetSupportedD3DHandleTypes(instance, types.data(), &count);
    if (result != TEResultSuccess && result != 1)  // 1 = TEResultInsufficientMemory when used correctly
    {
        error = "D3D12 is not supported. Failed to query handle types.";
        return false;
    }

    bool d3d12Supported = false;
    for (const auto& type : types)
    {
        if (type == TED3DHandleTypeD3D12ResourceNT)
        {
            d3d12Supported = true;
            break;
        }
    }

    if (!d3d12Supported)
    {
        error = "D3D12 NT handles are not supported by this TouchEngine instance.";
        error += "\nThe selected GPU is: ";
        error += myDeviceName;
        return false;
    }

    return true;
}

void
D3D12Renderer::resize(int width, int height)
{
    Renderer::resize(width, height);
}

void
D3D12Renderer::stop()
{
    myOutputImages.clear();
    myInputImages.clear();
    myContext = nullptr;

    // Don't release myDevice or myCommandQueue - they're owned by Qt's RHI
    myDevice = nullptr;
    myCommandQueue = nullptr;

    Renderer::stop();
}

bool
D3D12Renderer::render()
{
    return true;
}

QSharedPointer<QRhiTexture>
D3D12Renderer::getOutputRhiTexture(size_t index)
{
    if (index < myOutputImages.size())
    {
        return myOutputImages[index].getTexture().getRhiTexture();
    }
    return QSharedPointer<QRhiTexture>();
}

void
D3D12Renderer::clearInputImages()
{
    myInputImages.clear();
    Renderer::clearInputImages();
}

void
D3D12Renderer::addOutputImage()
{
    myOutputImages.emplace_back();
    myOutputImages.back().setup();
    Renderer::addOutputImage();
}

bool
D3D12Renderer::updateOutputImage(const TouchObject<TEInstance>& instance, size_t index, const std::string& identifier)
{
    bool success = false;

    TouchObject<TETexture> texture;
    TEResult result = TEInstanceLinkGetTextureValue(instance, identifier.c_str(), TELinkValueCurrent, texture.take());

    if (result == TEResultSuccess)
    {
        setOutputImage(index, texture);

        if (texture && TETextureGetType(texture) == TETextureTypeD3DShared)
        {
            TED3DSharedTexture* sharedTexture = static_cast<TED3DSharedTexture*>(texture.get());
            TED3DHandleType handleType = TED3DSharedTextureGetHandleType(sharedTexture);

            if (handleType == TED3DHandleTypeD3D12ResourceNT)
            {
                TouchObject<TED3DSharedTexture> d3dTexture;
                d3dTexture.set(sharedTexture);

                D3D12Texture tex(myWindow->rhi(), d3dTexture, myDevice.Get());
                if (tex.isValid())
                {
                    myOutputImages.at(index).update(std::move(tex));
                    success = true;
                }
            }
        }
    }

    if (!success)
    {
        myOutputImages.at(index).update(D3D12Texture());
        setOutputImage(index, nullptr);
    }

    return success;
}

void
D3D12Renderer::clearOutputImages()
{
    Renderer::clearOutputImages();
}

void
D3D12Renderer::releaseTextures()
{
    // Release all D3D12 texture resources before the device is destroyed
    for (auto& image : myOutputImages)
    {
        image.release();
    }
    for (auto& image : myInputImages)
    {
        image.release();
    }
}

#endif // _WIN32
