#include "D3D12TouchEngineBackend_p.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#include <TouchEngine/TED3D.h>
#include <TouchEngine/TED3D12.h>
#include <TouchEngine/TouchObject.h>

#include <QtCore/QByteArray>
#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QPoint>
#include <QtCore/QStringList>
#include <QtCore/QtMath>
#include <QtGui/rhi/qrhi_platform.h>

#include <d3d12.h>
#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <limits>
#include <list>
#include <memory>
#include <utility>
#include <vector>

namespace dsqt::touchengine::detail {
namespace {

using Microsoft::WRL::ComPtr;

constexpr qsizetype kMaximumInputTexturesPerLink = 6;
constexpr std::size_t kMaximumOutputImports = 32;
constexpr std::size_t kMaximumFenceImports = 16;

template<typename T>
QString enumValues(const std::vector<T> &values)
{
    QStringList result;
    result.reserve(static_cast<qsizetype>(values.size()));
    for (const T value : values)
        result.push_back(QString::number(static_cast<int>(value)));
    return result.join(QLatin1Char(','));
}

void assignError(QString *error, const QString &message)
{
    if (error)
        *error = message;
}

void appendError(QString *error, const QString &message)
{
    if (!error)
        return;
    if (!error->isEmpty())
        error->append(QLatin1Char('\n'));
    error->append(message);
}

QString teError(const char *operation, TEResult result)
{
    return QStringLiteral("%1 failed (TouchEngine result %2)")
        .arg(QString::fromLatin1(operation))
        .arg(static_cast<int>(result));
}

QString hresultError(const char *operation, HRESULT result)
{
    return QStringLiteral("%1 failed (HRESULT 0x%2)")
        .arg(QString::fromLatin1(operation))
        .arg(static_cast<qulonglong>(static_cast<quint32>(result)), 8, 16, QLatin1Char('0'));
}

struct TextureFormat
{
    QRhiTexture::Format rhi = QRhiTexture::UnknownFormat;
    QRhiTexture::Flags flags;
    DXGI_FORMAT dxgi = DXGI_FORMAT_UNKNOWN;

    explicit operator bool() const noexcept
    {
        return rhi != QRhiTexture::UnknownFormat && dxgi != DXGI_FORMAT_UNKNOWN;
    }
};

TextureFormat textureFormat(QRhiTexture::Format format, bool srgb)
{
    switch (format) {
    case QRhiTexture::RGBA8:
        return { format, srgb ? QRhiTexture::Flags(QRhiTexture::sRGB) : QRhiTexture::Flags(),
                 srgb ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM };
    case QRhiTexture::BGRA8:
        return { format, srgb ? QRhiTexture::Flags(QRhiTexture::sRGB) : QRhiTexture::Flags(),
                 srgb ? DXGI_FORMAT_B8G8R8A8_UNORM_SRGB : DXGI_FORMAT_B8G8R8A8_UNORM };
    case QRhiTexture::R8:
        return { format, {}, DXGI_FORMAT_R8_UNORM };
    case QRhiTexture::RG8:
        return { format, {}, DXGI_FORMAT_R8G8_UNORM };
    case QRhiTexture::R16:
        return { format, {}, DXGI_FORMAT_R16_UNORM };
    case QRhiTexture::RG16:
        return { format, {}, DXGI_FORMAT_R16G16_UNORM };
    case QRhiTexture::RGBA16F:
        return { format, {}, DXGI_FORMAT_R16G16B16A16_FLOAT };
    case QRhiTexture::RGBA32F:
        return { format, {}, DXGI_FORMAT_R32G32B32A32_FLOAT };
    case QRhiTexture::R16F:
        return { format, {}, DXGI_FORMAT_R16_FLOAT };
    case QRhiTexture::R32F:
        return { format, {}, DXGI_FORMAT_R32_FLOAT };
    case QRhiTexture::RGB10A2:
        return { format, {}, DXGI_FORMAT_R10G10B10A2_UNORM };
    case QRhiTexture::R8SI:
        return { format, {}, DXGI_FORMAT_R8_SINT };
    case QRhiTexture::R32SI:
        return { format, {}, DXGI_FORMAT_R32_SINT };
    case QRhiTexture::RG32SI:
        return { format, {}, DXGI_FORMAT_R32G32_SINT };
    case QRhiTexture::RGBA32SI:
        return { format, {}, DXGI_FORMAT_R32G32B32A32_SINT };
    case QRhiTexture::R8UI:
        return { format, {}, DXGI_FORMAT_R8_UINT };
    case QRhiTexture::R32UI:
        return { format, {}, DXGI_FORMAT_R32_UINT };
    case QRhiTexture::RG32UI:
        return { format, {}, DXGI_FORMAT_R32G32_UINT };
    case QRhiTexture::RGBA32UI:
        return { format, {}, DXGI_FORMAT_R32G32B32A32_UINT };
    default:
        return {};
    }
}

TextureFormat textureFormat(DXGI_FORMAT format)
{
    switch (format) {
    case DXGI_FORMAT_R8G8B8A8_UNORM:
        return { QRhiTexture::RGBA8, {}, format };
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        return { QRhiTexture::RGBA8, QRhiTexture::sRGB, format };
    case DXGI_FORMAT_B8G8R8A8_UNORM:
        return { QRhiTexture::BGRA8, {}, format };
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        return { QRhiTexture::BGRA8, QRhiTexture::sRGB, format };
    case DXGI_FORMAT_R8_UNORM:
        return { QRhiTexture::R8, {}, format };
    case DXGI_FORMAT_R8G8_UNORM:
        return { QRhiTexture::RG8, {}, format };
    case DXGI_FORMAT_R16_UNORM:
        return { QRhiTexture::R16, {}, format };
    case DXGI_FORMAT_R16G16_UNORM:
        return { QRhiTexture::RG16, {}, format };
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
        return { QRhiTexture::RGBA16F, {}, format };
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
        return { QRhiTexture::RGBA32F, {}, format };
    case DXGI_FORMAT_R16_FLOAT:
        return { QRhiTexture::R16F, {}, format };
    case DXGI_FORMAT_R32_FLOAT:
        return { QRhiTexture::R32F, {}, format };
    case DXGI_FORMAT_R10G10B10A2_UNORM:
        return { QRhiTexture::RGB10A2, {}, format };
    case DXGI_FORMAT_R8_SINT:
        return { QRhiTexture::R8SI, {}, format };
    case DXGI_FORMAT_R32_SINT:
        return { QRhiTexture::R32SI, {}, format };
    case DXGI_FORMAT_R32G32_SINT:
        return { QRhiTexture::RG32SI, {}, format };
    case DXGI_FORMAT_R32G32B32A32_SINT:
        return { QRhiTexture::RGBA32SI, {}, format };
    case DXGI_FORMAT_R8_UINT:
        return { QRhiTexture::R8UI, {}, format };
    case DXGI_FORMAT_R32_UINT:
        return { QRhiTexture::R32UI, {}, format };
    case DXGI_FORMAT_R32G32_UINT:
        return { QRhiTexture::RG32UI, {}, format };
    case DXGI_FORMAT_R32G32B32A32_UINT:
        return { QRhiTexture::RGBA32UI, {}, format };
    default:
        return {};
    }
}

bool sourceRectangle(const TextureInputSource &source, QRect *rectangle, QString *error)
{
    if (!source.texture || !rectangle) {
        assignError(error, QStringLiteral("The texture input source is null"));
        return false;
    }
    const QSize textureSize = source.texture->pixelSize();
    const QRectF normalized = source.normalizedSourceRect;
    if (textureSize.isEmpty() || normalized.width() <= 0.0 || normalized.height() <= 0.0
        || normalized.left() < 0.0 || normalized.top() < 0.0
        || normalized.right() > 1.0 || normalized.bottom() > 1.0) {
        assignError(error, QStringLiteral("The normalized texture source rectangle is invalid"));
        return false;
    }
    const int left = qBound(0, qFloor(normalized.left() * textureSize.width()), textureSize.width());
    const int top = qBound(0, qFloor(normalized.top() * textureSize.height()), textureSize.height());
    const int right = qBound(left, qCeil(normalized.right() * textureSize.width()), textureSize.width());
    const int bottom = qBound(top, qCeil(normalized.bottom() * textureSize.height()), textureSize.height());
    *rectangle = QRect(QPoint(left, top), QPoint(right - 1, bottom - 1));
    if (rectangle->isEmpty()) {
        assignError(error, QStringLiteral("The texture input source rectangle is empty"));
        return false;
    }
    if (!source.pixelSize.isEmpty() && source.pixelSize != rectangle->size()) {
        assignError(error,
                    QStringLiteral("D3D12 texture input cannot scale from %1x%2 to %3x%4")
                        .arg(rectangle->width()).arg(rectangle->height())
                        .arg(source.pixelSize.width()).arg(source.pixelSize.height()));
        return false;
    }
    return true;
}

template<typename T>
bool contains(const std::vector<T> &values, T value)
{
    return std::find(values.cbegin(), values.cend(), value) != values.cend();
}

template<typename T, typename Function>
bool queryCapabilities(Function function, TEInstance *instance, std::vector<T> *values,
                       const char *name, QString *error)
{
    int32_t count = 0;
    TEResult result = function(instance, nullptr, &count);
    if (result != TEResultInsufficientMemory && result != TEResultSuccess) {
        assignError(error, teError(name, result));
        return false;
    }
    if (count < 0) {
        assignError(error, QStringLiteral("%1 returned an invalid capability count")
                               .arg(QString::fromLatin1(name)));
        return false;
    }
    values->assign(static_cast<size_t>(count), T{});
    if (count == 0)
        return true;
    result = function(instance, values->data(), &count);
    if (result != TEResultSuccess) {
        assignError(error, teError(name, result));
        return false;
    }
    values->resize(static_cast<size_t>(count));
    return true;
}

struct InputUseToken
{
    std::atomic_uint references { 1 };
    std::atomic_uint64_t endUseSerial { 0 };
};

void retainToken(InputUseToken *token)
{
    token->references.fetch_add(1, std::memory_order_relaxed);
}

void releaseToken(InputUseToken *token)
{
    if (token && token->references.fetch_sub(1, std::memory_order_acq_rel) == 1)
        delete token;
}

void inputTextureCallback(HANDLE, TEObjectEvent event, void *info)
{
    auto *token = static_cast<InputUseToken *>(info);
    if (event == TEObjectEventEndUse)
        token->endUseSerial.fetch_add(1, std::memory_order_release);
    else if (event == TEObjectEventRelease)
        releaseToken(token);
}

void releaseLater(std::unique_ptr<QRhiTexture> &texture)
{
    if (texture)
        texture.release()->deleteLater();
}

} // namespace

class D3D12TouchEngineBackend::Impl
{
public:
    struct InputSlot
    {
        ~InputSlot()
        {
            teTexture.reset();
            releaseToken(token);
        }

        QString link;
        QSize size;
        TextureFormat format;
        ComPtr<ID3D12Resource> nativeTexture;
        std::unique_ptr<QRhiTexture> rhiTexture;
        TouchObject<TED3DSharedTexture> teTexture;
        InputUseToken *token = nullptr;
        quint64 publishedEndUseSerial = 0;
        bool awaitingEndUse = false;
        bool pendingPublication = false;
    };

    struct OutputCache
    {
        std::unique_ptr<QRhiTexture> texture;
        QSize size;
        TextureFormat format;
        bool mirrorVertically = false;
    };

    struct ImportedOutput
    {
        TouchObject<TETexture> teTexture;
        const TETexture *sourceObject = nullptr;
        HANDLE sourceHandle = nullptr;
        ComPtr<ID3D12Resource> nativeTexture;
        std::unique_ptr<QRhiTexture> importedTexture;
        QSize size;
        TextureFormat format;
        quint64 lastUseSerial = 0;
    };

    struct ImportedFence
    {
        TouchObject<TESemaphore> teSemaphore;
        HANDLE sourceHandle = nullptr;
        ComPtr<ID3D12Fence> nativeFence;
        quint64 lastUseSerial = 0;
    };

    struct PendingOutput
    {
        std::shared_ptr<ImportedOutput> imported;
        std::shared_ptr<ImportedFence> sourceFence;
    };

    struct RetiredOutput
    {
        PendingOutput output;
        quint64 fenceValue = 0;
    };

    ~Impl()
    {
        clearTextureOutputs();
        // ImportedOutput destroys importedTexture before its native COM object (member
        // destruction is reverse declaration order). createFrom() is non-owning, so the
        // cache and pending/retired shared references keep both alive through GPU use.
    }

    bool initialize(QRhi *newRhi, QString *error)
    {
        if (!newRhi || newRhi->backend() != QRhi::D3D12) {
            assignError(error, QStringLiteral("D3D12 backend requires a D3D12 QRhi"));
            return false;
        }
        const auto *handles = static_cast<const QRhiD3D12NativeHandles *>(newRhi->nativeHandles());
        if (!handles || !handles->dev || !handles->commandQueue) {
            assignError(error, QStringLiteral("Qt did not expose its D3D12 device and direct command queue"));
            return false;
        }
        rhi = newRhi;
        device = static_cast<ID3D12Device *>(handles->dev);
        commandQueue = static_cast<ID3D12CommandQueue *>(handles->commandQueue);

        TED3D12Context *created = nullptr;
        const TEResult contextResult = TED3D12ContextCreate(device, &created);
        if (contextResult != TEResultSuccess || !created) {
            assignError(error, teError("TED3D12ContextCreate", contextResult));
            return false;
        }
        context.take(created);

        HRESULT result = device->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&hostFence));
        if (FAILED(result)) {
            assignError(error, hresultError("ID3D12Device::CreateFence", result));
            return false;
        }
        HANDLE handle = nullptr;
        result = device->CreateSharedHandle(hostFence.Get(), nullptr, GENERIC_ALL, nullptr, &handle);
        if (FAILED(result)) {
            assignError(error, hresultError("ID3D12Device::CreateSharedHandle(fence)", result));
            return false;
        }
        hostTeFence.take(TED3DSharedFenceCreate(handle, nullptr, nullptr));
        CloseHandle(handle);
        if (!hostTeFence) {
            assignError(error, QStringLiteral("TED3DSharedFenceCreate returned null"));
            return false;
        }
        return true;
    }

    bool configureInstance(TEInstance *instance, QString *error)
    {
        if (!instance || !context) {
            assignError(error, QStringLiteral("D3D12 backend is not initialized"));
            return false;
        }
        // A reloaded TEInstance owns a different texture/fence pool. Drop reusable
        // references from the previous instance so it can release that pool. Any record
        // still covered by a host-fence retirement remains alive through RetiredOutput.
        outputImports.clear();
        fenceImports.clear();
        importUseSerial = 0;
        std::vector<TETextureType> textureTypes;
        std::vector<TED3DHandleType> handleTypes;
        std::vector<DXGI_FORMAT> formats;
        std::vector<TESemaphoreType> semaphoreTypes;
        if (!queryCapabilities<TETextureType>(TEInstanceGetSupportedTextureTypes, instance,
                                               &textureTypes, "TEInstanceGetSupportedTextureTypes", error)
            || !queryCapabilities<TED3DHandleType>(TEInstanceGetSupportedD3DHandleTypes, instance,
                                                    &handleTypes, "TEInstanceGetSupportedD3DHandleTypes", error)
            || !queryCapabilities<DXGI_FORMAT>(TEInstanceGetSupportedD3DFormats, instance,
                                                &formats, "TEInstanceGetSupportedD3DFormats", error)
            || !queryCapabilities<TESemaphoreType>(TEInstanceGetSupportedSemaphoreTypes, instance,
                                                    &semaphoreTypes, "TEInstanceGetSupportedSemaphoreTypes", error)) {
            return false;
        }
        if (!contains(textureTypes, TETextureTypeD3DShared)
            || !contains(handleTypes, TED3DHandleTypeD3D12ResourceNT)
            || !contains(semaphoreTypes, TESemaphoreTypeD3DFence)) {
            assignError(error,
                        QStringLiteral("TouchEngine did not negotiate D3D12 NT-handle textures and fences "
                                       "(textureTypes=[%1], handleTypes=[%2], semaphoreTypes=[%3])")
                            .arg(enumValues(textureTypes),
                                 enumValues(handleTypes),
                                 enumValues(semaphoreTypes)));
            return false;
        }
        if (!TEInstanceDoesTextureOwnershipTransfer(instance)) {
            assignError(error, QStringLiteral("TouchEngine did not negotiate texture ownership transfer for D3D12"));
            return false;
        }
        supportedFormats = std::move(formats);
        configured = true;
        return true;
    }

    bool transitionToShaderResource(QRhiCommandBuffer *commandBuffer, QRhiTexture *rhiTexture,
                                    ID3D12Resource *resource, D3D12_RESOURCE_STATES before,
                                    QString *error)
    {
        if (before == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) {
            rhiTexture->setNativeLayout(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            return true;
        }
        commandBuffer->beginComputePass(nullptr, QRhiCommandBuffer::ExternalContent);
        commandBuffer->beginExternal();
        const auto *handles = static_cast<const QRhiD3D12CommandBufferNativeHandles *>(
            commandBuffer->nativeHandles());
        auto *commandList = handles
            ? static_cast<ID3D12GraphicsCommandList1 *>(handles->commandList) : nullptr;
        if (!commandList) {
            commandBuffer->endExternal();
            commandBuffer->endComputePass();
            assignError(error, QStringLiteral("Qt did not expose the active D3D12 command list"));
            return false;
        }
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = resource;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = before;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        commandList->ResourceBarrier(1, &barrier);
        commandBuffer->endExternal();
        rhiTexture->setNativeLayout(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        commandBuffer->endComputePass();
        return true;
    }

    std::shared_ptr<InputSlot> acquireInputSlot(const QString &link, const QSize &size,
                                                const TextureFormat &format, QString *error)
    {
        QList<std::shared_ptr<InputSlot>> &pool = inputSlots[link];
        for (const auto &slot : pool) {
            if (slot->awaitingEndUse
                && slot->token->endUseSerial.load(std::memory_order_acquire) > slot->publishedEndUseSerial) {
                slot->awaitingEndUse = false;
            }
            if (!slot->awaitingEndUse && !slot->pendingPublication
                && slot->size == size && slot->format.dxgi == format.dxgi)
                return slot;
        }
        if (pool.size() >= kMaximumInputTexturesPerLink) {
            assignError(error,
                        QStringLiteral("TouchEngine has not released a D3D12 input texture for '%1'; "
                                       "the bounded input pool is exhausted").arg(link));
            return {};
        }

        auto slot = std::make_shared<InputSlot>();
        slot->link = link;
        slot->size = size;
        slot->format = format;

        D3D12_HEAP_PROPERTIES heap = {};
        heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        heap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heap.CreationNodeMask = 1;
        heap.VisibleNodeMask = 1;
        D3D12_RESOURCE_DESC description = {};
        description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        description.Width = static_cast<UINT64>(size.width());
        description.Height = static_cast<UINT>(size.height());
        description.DepthOrArraySize = 1;
        description.MipLevels = 1;
        description.Format = format.dxgi;
        description.SampleDesc.Count = 1;
        description.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        description.Flags = D3D12_RESOURCE_FLAG_NONE;
        HRESULT result = device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_SHARED,
                                                          &description,
                                                          D3D12_RESOURCE_STATE_COPY_DEST,
                                                          nullptr,
                                                          IID_PPV_ARGS(&slot->nativeTexture));
        if (FAILED(result)) {
            assignError(error, hresultError("ID3D12Device::CreateCommittedResource", result));
            return {};
        }
        HANDLE handle = nullptr;
        result = device->CreateSharedHandle(slot->nativeTexture.Get(), nullptr, GENERIC_ALL,
                                            nullptr, &handle);
        if (FAILED(result)) {
            assignError(error, hresultError("ID3D12Device::CreateSharedHandle(texture)", result));
            return {};
        }
        slot->token = new InputUseToken;
        TED3DSharedTexture *teTexture = TED3DSharedTextureCreate(
            handle, TED3DHandleTypeD3D12ResourceNT, format.dxgi,
            static_cast<quint64>(size.width()), static_cast<quint32>(size.height()),
            TETextureOriginTopLeft, kTETextureComponentMapIdentity,
            inputTextureCallback, slot->token);
        CloseHandle(handle); // TouchEngine duplicates successful NT handles.
        if (!teTexture) {
            assignError(error, QStringLiteral("TED3DSharedTextureCreate returned null"));
            return {};
        }
        retainToken(slot->token); // Held until TEObjectEventRelease.
        slot->teTexture.take(teTexture);

        slot->rhiTexture.reset(rhi->newTexture(format.rhi, size, 1,
                                               format.flags | QRhiTexture::UsedAsTransferSource));
        const QRhiTexture::NativeTexture native {
            static_cast<quint64>(reinterpret_cast<quintptr>(slot->nativeTexture.Get())),
            D3D12_RESOURCE_STATE_COPY_DEST
        };
        if (!slot->rhiTexture || !slot->rhiTexture->createFrom(native)) {
            assignError(error, QStringLiteral("QRhi could not import the D3D12 input staging texture"));
            return {};
        }
        pool.append(slot);
        return slot;
    }

    bool prepareTextureInput(TEInstance *instance, const TextureInputSource &source,
                             QRhiCommandBuffer *commandBuffer, QString *error)
    {
        if (!configured || !instance || !commandBuffer || !source.texture || source.link.isEmpty()) {
            assignError(error, QStringLiteral("D3D12 texture input has invalid or unconfigured arguments"));
            return false;
        }
        if (source.texture->sampleCount() != 1) {
            assignError(error, QStringLiteral("Multisampled D3D12 texture inputs must be resolved before sharing"));
            return false;
        }
        if (!source.texture->flags().testFlag(QRhiTexture::UsedAsTransferSource)) {
            assignError(error,
                        QStringLiteral("The Qt texture for '%1' was not created with UsedAsTransferSource")
                            .arg(source.link));
            return false;
        }
        QRect rectangle;
        if (!sourceRectangle(source, &rectangle, error))
            return false;
        const TextureFormat format = textureFormat(source.texture->format(),
                                                   source.texture->flags().testFlag(QRhiTexture::sRGB));
        if (!format) {
            assignError(error, QStringLiteral("The Qt texture format for '%1' is not supported by D3D12 interop")
                                   .arg(source.link));
            return false;
        }
        if (!supportedFormats.empty() && !contains(supportedFormats, format.dxgi)) {
            assignError(error, QStringLiteral("TouchEngine did not negotiate DXGI format %1 for '%2'")
                                   .arg(static_cast<int>(format.dxgi)).arg(source.link));
            return false;
        }
        const auto slot = acquireInputSlot(source.link, rectangle.size(), format, error);
        if (!slot)
            return false;

        QRhiTextureCopyDescription copy;
        copy.setSourceTopLeft(rectangle.topLeft());
        copy.setPixelSize(rectangle.size());
        QRhiResourceUpdateBatch *updates = rhi->nextResourceUpdateBatch();
        updates->copyTexture(slot->rhiTexture.get(), source.texture, copy);
        commandBuffer->resourceUpdate(updates);
        slot->rhiTexture->setNativeLayout(D3D12_RESOURCE_STATE_COPY_DEST);
        if (!transitionToShaderResource(commandBuffer, slot->rhiTexture.get(),
                                        slot->nativeTexture.Get(),
                                        D3D12_RESOURCE_STATE_COPY_DEST, error)) {
            return false;
        }
        slot->pendingPublication = true;
        pendingInputs.push_back(slot);
        return true;
    }

    template<typename T>
    static void pruneImports(std::vector<std::shared_ptr<T>> &imports, std::size_t maximum)
    {
        while (imports.size() > maximum) {
            auto oldest = imports.end();
            for (auto it = imports.begin(); it != imports.end(); ++it) {
                if (it->use_count() != 1)
                    continue;
                if (oldest == imports.end()
                    || (*it)->lastUseSerial < (*oldest)->lastUseSerial) {
                    oldest = it;
                }
            }
            if (oldest == imports.end())
                break;
            imports.erase(oldest);
        }
    }

    std::shared_ptr<ImportedOutput> importOutput(TED3DSharedTexture *shared,
                                                 const QString &link,
                                                 QString *error)
    {
        auto *sourceObject = reinterpret_cast<TETexture *>(shared);
        const HANDLE sourceHandle = TED3DSharedTextureGetHandle(shared);
        if (!sourceHandle) {
            assignError(error,
                        QStringLiteral("TouchEngine output '%1' has no D3D12 shared handle")
                            .arg(link));
            return {};
        }

        for (const auto &imported : outputImports) {
            if (imported->sourceObject == sourceObject
                && imported->sourceHandle == sourceHandle) {
                imported->lastUseSerial = ++importUseSerial;
                return imported;
            }
        }

        auto imported = std::make_shared<ImportedOutput>();
        imported->sourceObject = sourceObject;
        imported->sourceHandle = sourceHandle;
        imported->teTexture.set(sourceObject);
        HRESULT result = device->OpenSharedHandle(sourceHandle,
                                                  IID_PPV_ARGS(&imported->nativeTexture));
        if (FAILED(result)) {
            assignError(error, hresultError("ID3D12Device::OpenSharedHandle(texture)", result));
            return {};
        }

        const D3D12_RESOURCE_DESC description = imported->nativeTexture->GetDesc();
        if (description.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || !description.Width || !description.Height || description.SampleDesc.Count != 1
            || description.DepthOrArraySize != 1) {
            assignError(error,
                        QStringLiteral("TouchEngine output '%1' has an unsupported D3D12 shape")
                            .arg(link));
            return {};
        }

        DXGI_FORMAT viewFormat = TED3DSharedTextureGetFormat(shared);
        if (viewFormat == DXGI_FORMAT_UNKNOWN)
            viewFormat = description.Format;
        imported->format = textureFormat(viewFormat);
        if (!imported->format) {
            assignError(error,
                        QStringLiteral("TouchEngine output '%1' has unsupported DXGI format %2")
                            .arg(link).arg(static_cast<int>(viewFormat)));
            return {};
        }
        if (description.Width > static_cast<UINT64>(std::numeric_limits<int>::max())
            || description.Height > static_cast<UINT>(std::numeric_limits<int>::max())) {
            assignError(error,
                        QStringLiteral("TouchEngine output '%1' is too large for QRhi").arg(link));
            return {};
        }
        imported->size = QSize(static_cast<int>(description.Width),
                               static_cast<int>(description.Height));
        imported->importedTexture.reset(
            rhi->newTexture(imported->format.rhi, imported->size, 1,
                            imported->format.flags | QRhiTexture::UsedAsTransferSource));
        const QRhiTexture::NativeTexture native {
            static_cast<quint64>(reinterpret_cast<quintptr>(imported->nativeTexture.Get())),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        };
        if (!imported->importedTexture || !imported->importedTexture->createFrom(native)) {
            assignError(error,
                        QStringLiteral("QRhi could not import TouchEngine output '%1'").arg(link));
            return {};
        }

        imported->lastUseSerial = ++importUseSerial;
        outputImports.push_back(imported);
        pruneImports(outputImports, kMaximumOutputImports);
        return imported;
    }

    std::shared_ptr<ImportedFence> importFence(TESemaphore *semaphore, QString *error)
    {
        auto *sharedFence = static_cast<TED3DSharedFence *>(semaphore);
        const HANDLE sourceHandle = TED3DSharedFenceGetHandle(sharedFence);
        if (!sourceHandle) {
            assignError(error, QStringLiteral("TouchEngine returned a D3D fence without a handle"));
            return {};
        }

        for (const auto &imported : fenceImports) {
            if (imported->sourceHandle == sourceHandle) {
                imported->lastUseSerial = ++importUseSerial;
                return imported;
            }
        }

        auto imported = std::make_shared<ImportedFence>();
        imported->sourceHandle = sourceHandle;
        imported->teSemaphore.set(semaphore);
        const HRESULT result = device->OpenSharedHandle(sourceHandle,
                                                        IID_PPV_ARGS(&imported->nativeFence));
        if (FAILED(result)) {
            assignError(error, hresultError("ID3D12Device::OpenSharedHandle(fence)", result));
            return {};
        }
        imported->lastUseSerial = ++importUseSerial;
        fenceImports.push_back(imported);
        pruneImports(fenceImports, kMaximumFenceImports);
        return imported;
    }

    std::shared_ptr<OutputCache> ensureOutputCache(const QString &link, const QSize &size,
                                                   const TextureFormat &format, QString *error)
    {
        auto cache = outputCaches.value(link);
        if (cache && cache->texture && cache->size == size
            && cache->format.dxgi == format.dxgi)
            return cache;
        if (!cache) {
            cache = std::make_shared<OutputCache>();
            outputCaches.insert(link, cache);
        } else {
            releaseLater(cache->texture);
        }
        cache->size = size;
        cache->format = format;
        cache->texture.reset(rhi->newTexture(format.rhi, size, 1,
                                             format.flags | QRhiTexture::UsedAsTransferSource));
        if (!cache->texture || !cache->texture->create()) {
            cache->texture.reset();
            assignError(error, QStringLiteral("QRhi could not create the D3D12 output cache for '%1'").arg(link));
            return {};
        }
        return cache;
    }

    bool updateTextureOutput(TEInstance *instance, const QString &link, TETexture *texture,
                             QRhiCommandBuffer *commandBuffer, QString *error)
    {
        if (!configured || !instance || !commandBuffer || link.isEmpty()) {
            assignError(error, QStringLiteral("D3D12 texture output has invalid or unconfigured arguments"));
            return false;
        }
        if (!texture) {
            clearTextureOutput(link);
            return true;
        }
        if (TETextureGetType(texture) != TETextureTypeD3DShared) {
            assignError(error, QStringLiteral("TouchEngine output '%1' is not a D3D shared texture").arg(link));
            return false;
        }
        auto *shared = static_cast<TED3DSharedTexture *>(texture);
        if (TED3DSharedTextureGetHandleType(shared) != TED3DHandleTypeD3D12ResourceNT) {
            assignError(error, QStringLiteral("TouchEngine output '%1' is not a D3D12 NT-handle texture").arg(link));
            return false;
        }

        PendingOutput pending;
        pending.imported = importOutput(shared, link, error);
        if (!pending.imported)
            return false;

        const auto cache = ensureOutputCache(link,
                                             pending.imported->size,
                                             pending.imported->format,
                                             error);
        if (!cache)
            return false;
        if (TEInstanceHasTextureTransfer(instance, texture)) {
            TouchObject<TESemaphore> semaphore;
            quint64 waitValue = 0;
            const TEResult transferResult = TEInstanceGetTextureTransfer(
                instance, texture, semaphore.take(), &waitValue);
            if (transferResult != TEResultSuccess) {
                assignError(error, teError("TEInstanceGetTextureTransfer", transferResult));
                return false;
            }
            if (!semaphore || TESemaphoreGetType(semaphore) != TESemaphoreTypeD3DFence) {
                assignError(error, QStringLiteral("TouchEngine output '%1' did not provide a D3D fence").arg(link));
                return false;
            }
            pending.sourceFence = importFence(semaphore.get(), error);
            if (!pending.sourceFence)
                return false;
            const HRESULT result = commandQueue->Wait(pending.sourceFence->nativeFence.Get(),
                                                      waitValue);
            if (FAILED(result)) {
                assignError(error, hresultError("ID3D12CommandQueue::Wait", result));
                return false;
            }
        }

        pendingOutputs.push_back(std::move(pending));
        PendingOutput &queued = pendingOutputs.back();
        QRhiTextureCopyDescription copy;
        copy.setPixelSize(queued.imported->size);
        QRhiResourceUpdateBatch *updates = rhi->nextResourceUpdateBatch();
        updates->copyTexture(cache->texture.get(), queued.imported->importedTexture.get(), copy);
        commandBuffer->resourceUpdate(updates);
        queued.imported->importedTexture->setNativeLayout(D3D12_RESOURCE_STATE_COPY_SOURCE);
        if (!transitionToShaderResource(commandBuffer,
                                        queued.imported->importedTexture.get(),
                                        queued.imported->nativeTexture.Get(),
                                        D3D12_RESOURCE_STATE_COPY_SOURCE, error)) {
            return false; // afterFrameEnd still safely returns ownership.
        }
        cache->mirrorVertically = TETextureGetOrigin(texture) == TETextureOriginBottomLeft;
        return true;
    }

    TextureOutput textureOutput(const QString &link) const
    {
        const auto cache = outputCaches.value(link);
        if (!cache || !cache->texture)
            return {};
        return { cache->texture.get(), cache->size, cache->mirrorVertically };
    }

    void clearTextureOutput(const QString &link)
    {
        const auto cache = outputCaches.take(link);
        if (cache)
            releaseLater(cache->texture);
    }

    void clearTextureOutputs()
    {
        for (const auto &cache : std::as_const(outputCaches)) {
            if (cache)
                releaseLater(cache->texture);
        }
        outputCaches.clear();
    }

    void retireCompletedOutputs()
    {
        if (!hostFence || retiredOutputs.empty())
            return;
        const quint64 completed = hostFence->GetCompletedValue();
        if (completed == std::numeric_limits<quint64>::max())
            return; // The device was removed. Its teardown will release these records.

        auto it = retiredOutputs.begin();
        while (it != retiredOutputs.end()) {
            if (it->fenceValue <= completed)
                it = retiredOutputs.erase(it);
            else
                ++it;
        }
        pruneImports(outputImports, kMaximumOutputImports);
        pruneImports(fenceImports, kMaximumFenceImports);
    }

    bool afterFrameEnd(TEInstance *instance, QString *error)
    {
        if (!instance) {
            assignError(error, QStringLiteral("Cannot finalize D3D12 transfers without a TEInstance"));
            return false;
        }
        // Poll only; this never stalls the render thread. Erasing a record destroys the
        // QRhi wrapper first, then the opened resource/fence objects, after GPU completion.
        retireCompletedOutputs();
        if (pendingOutputs.empty() && pendingInputs.empty() && retryInputTransfers.empty())
            return true;

        const quint64 fenceValue = nextFenceValue++;
        const HRESULT signalResult = commandQueue->Signal(hostFence.Get(), fenceValue);
        if (FAILED(signalResult)) {
            assignError(error, hresultError("ID3D12CommandQueue::Signal", signalResult));
            // No fence value covers the submitted Qt copies yet. Keep the complete
            // imported-resource records and input publications for a later retry.
            return false;
        }

        bool success = true;
        std::vector<PendingOutput> nextPendingOutputs;
        nextPendingOutputs.reserve(pendingOutputs.size());
        for (PendingOutput &pending : pendingOutputs) {
            const TEResult result = TEInstanceAddTextureTransfer(
                instance, pending.imported->teTexture,
                static_cast<TESemaphore *>(hostTeFence.get()),
                fenceValue);
            if (result != TEResultSuccess) {
                appendError(error, teError("TEInstanceAddTextureTransfer", result));
                nextPendingOutputs.push_back(std::move(pending));
                success = false;
                continue;
            }
            retiredOutputs.push_back({ std::move(pending), fenceValue });
        }
        pendingOutputs = std::move(nextPendingOutputs);

        std::vector<std::shared_ptr<InputSlot>> nextInputRetries;
        auto addInputTransfer = [&](const std::shared_ptr<InputSlot> &slot) {
            const TEResult result = TEInstanceAddTextureTransfer(
                instance, reinterpret_cast<TETexture *>(slot->teTexture.get()),
                static_cast<TESemaphore *>(hostTeFence.get()), fenceValue);
            if (result != TEResultSuccess) {
                appendError(error, teError("TEInstanceAddTextureTransfer(input)", result));
                nextInputRetries.push_back(slot);
                success = false;
            }
        };
        for (const auto &slot : retryInputTransfers)
            addInputTransfer(slot);
        retryInputTransfers.clear();

        for (const auto &slot : pendingInputs) {
            slot->publishedEndUseSerial = slot->token->endUseSerial.load(std::memory_order_acquire);
            slot->awaitingEndUse = true;
            slot->pendingPublication = false;
            const QByteArray identifier = slot->link.toUtf8();
            TEResult result = TEInstanceLinkSetTextureValue(
                instance, identifier.constData(), reinterpret_cast<TETexture *>(slot->teTexture.get()),
                reinterpret_cast<TEGraphicsContext *>(context.get()));
            if (result == TEResultSuccess)
                addInputTransfer(slot);
            else {
                slot->awaitingEndUse = false;
                appendError(error, teError("publishing D3D12 texture input", result));
                success = false;
            }
        }
        pendingInputs.clear();
        retryInputTransfers = std::move(nextInputRetries);
        return success;
    }

    QRhi *rhi = nullptr;
    ID3D12Device *device = nullptr;
    ID3D12CommandQueue *commandQueue = nullptr;
    ComPtr<ID3D12Fence> hostFence;
    TouchObject<TED3DSharedFence> hostTeFence;
    TouchObject<TED3D12Context> context;
    quint64 nextFenceValue = 1;
    bool configured = false;
    std::vector<DXGI_FORMAT> supportedFormats;
    QHash<QString, QList<std::shared_ptr<InputSlot>>> inputSlots;
    std::vector<std::shared_ptr<InputSlot>> pendingInputs;
    QHash<QString, std::shared_ptr<OutputCache>> outputCaches;
    std::vector<std::shared_ptr<ImportedOutput>> outputImports;
    std::vector<std::shared_ptr<ImportedFence>> fenceImports;
    std::vector<PendingOutput> pendingOutputs;
    std::list<RetiredOutput> retiredOutputs;
    std::vector<std::shared_ptr<InputSlot>> retryInputTransfers;
    quint64 importUseSerial = 0;
};

D3D12TouchEngineBackend::D3D12TouchEngineBackend()
    : d(std::make_unique<Impl>())
{
}

D3D12TouchEngineBackend::~D3D12TouchEngineBackend() = default;

DsTouchEngineTypes::GraphicsApi D3D12TouchEngineBackend::graphicsApi() const noexcept
{
    return DsTouchEngineTypes::GraphicsApi::Direct3D12;
}

bool D3D12TouchEngineBackend::initialize(QRhi *rhi, QRhiCommandBuffer *commandBuffer, QString *error)
{
    Q_UNUSED(commandBuffer);
    return d->initialize(rhi, error);
}

TEGraphicsContext *D3D12TouchEngineBackend::graphicsContext() const noexcept
{
    return reinterpret_cast<TEGraphicsContext *>(d->context.get());
}

bool D3D12TouchEngineBackend::configureInstance(TEInstance *instance, QString *error)
{
    return d->configureInstance(instance, error);
}

bool D3D12TouchEngineBackend::prepareTextureInput(TEInstance *instance,
                                                  const TextureInputSource &source,
                                                  QRhiCommandBuffer *commandBuffer,
                                                  QString *error)
{
    return d->prepareTextureInput(instance, source, commandBuffer, error);
}

bool D3D12TouchEngineBackend::updateTextureOutput(TEInstance *instance,
                                                  const QString &link,
                                                  TETexture *texture,
                                                  QRhiCommandBuffer *commandBuffer,
                                                  QString *error)
{
    return d->updateTextureOutput(instance, link, texture, commandBuffer, error);
}

TextureOutput D3D12TouchEngineBackend::textureOutput(const QString &link) const
{
    return d->textureOutput(link);
}

void D3D12TouchEngineBackend::clearTextureOutput(const QString &link)
{
    d->clearTextureOutput(link);
}

void D3D12TouchEngineBackend::clearTextureOutputs()
{
    d->clearTextureOutputs();
}

bool D3D12TouchEngineBackend::afterFrameEnd(TEInstance *instance, QString *error)
{
    return d->afterFrameEnd(instance, error);
}

} // namespace dsqt::touchengine::detail
