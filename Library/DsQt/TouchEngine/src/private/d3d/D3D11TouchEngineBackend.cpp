#include "D3D11TouchEngineBackend_p.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#include <TouchEngine/TED3D.h>
#include <TouchEngine/TED3D11.h>
#include <TouchEngine/TouchObject.h>

#include <QtCore/QByteArray>
#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QPoint>
#include <QtCore/QtMath>
#include <QtGui/rhi/qrhi_platform.h>

#include <d3d11_4.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

namespace dsqt::touchengine::detail {
namespace {

using Microsoft::WRL::ComPtr;

constexpr DWORD kKeyedMutexTimeoutMs = 16;
constexpr qsizetype kMaximumInputTexturesPerLink = 6;

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
    std::atomic_uint64_t releaseSerial { 0 };
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

void inputTextureCallback(ID3D11Texture2D *, TEObjectEvent event, void *info)
{
    auto *token = static_cast<InputUseToken *>(info);
    if (event == TEObjectEventRelease) {
        token->releaseSerial.fetch_add(1, std::memory_order_release);
        releaseToken(token);
    }
}

void releaseLater(std::unique_ptr<QRhiTexture> &texture)
{
    if (texture)
        texture.release()->deleteLater();
}

} // namespace

class D3D11TouchEngineBackend::Impl
{
public:
    struct InputSlot
    {
        ~InputSlot()
        {
            renderTarget.reset();
            renderPassDescriptor.reset();
            releaseLater(rhiTexture);
            releaseToken(token);
        }

        QString link;
        QSize size;
        TextureFormat format;
        ComPtr<ID3D11Texture2D> nativeTexture;
        std::unique_ptr<QRhiTexture> rhiTexture;
        std::unique_ptr<QRhiTextureRenderTarget> renderTarget;
        std::unique_ptr<QRhiRenderPassDescriptor> renderPassDescriptor;
        InputUseToken *token = nullptr;
        quint64 publishedReleaseSerial = 0;
        bool awaitingRelease = false;
        bool pendingPublication = false;
    };

    struct OutputCache
    {
        std::unique_ptr<QRhiTexture> texture;
        QSize size;
        TextureFormat format;
        bool mirrorVertically = false;
    };

    enum class ReturnKind { None, KeyedMutex, Fence };

    struct PendingOutput
    {
        TouchObject<TETexture> teTexture;
        TouchObject<TED3D11Texture> d3dTexture;
        std::unique_ptr<QRhiTexture> importedTexture;
        ComPtr<IDXGIKeyedMutex> keyedMutex;
        ReturnKind returnKind = ReturnKind::None;
        quint64 releaseValue = 0;
    };

    struct RetryReturn
    {
        TouchObject<TETexture> teTexture;
        ReturnKind returnKind = ReturnKind::None;
        quint64 releaseValue = 0;
    };

    ~Impl()
    {
        clearTextureOutputs();
        for (PendingOutput &pending : pendingOutputs)
            releaseLater(pending.importedTexture);
    }

    bool initialize(QRhi *newRhi, QString *error)
    {
        if (!newRhi || newRhi->backend() != QRhi::D3D11) {
            assignError(error, QStringLiteral("D3D11 backend requires a D3D11 QRhi"));
            return false;
        }
        const auto *handles = static_cast<const QRhiD3D11NativeHandles *>(newRhi->nativeHandles());
        if (!handles || !handles->dev || !handles->context) {
            assignError(error, QStringLiteral("Qt did not expose its D3D11 device and immediate context"));
            return false;
        }

        rhi = newRhi;
        device = static_cast<ID3D11Device *>(handles->dev);
        deviceContext = static_cast<ID3D11DeviceContext *>(handles->context);

        TED3D11Context *created = nullptr;
        const TEResult result = TED3D11ContextCreate(device, &created);
        if (result != TEResultSuccess || !created) {
            assignError(error, teError("TED3D11ContextCreate", result));
            return false;
        }
        context.take(created);
        createHostFence(); // Optional: keyed mutex output remains fully supported without D3D11.4.
        return true;
    }

    void createHostFence()
    {
        ComPtr<ID3D11Device5> device5;
        if (FAILED(device->QueryInterface(IID_PPV_ARGS(&device5)))
            || FAILED(deviceContext->QueryInterface(IID_PPV_ARGS(&context4))))
            return;
        if (FAILED(device5->CreateFence(0, D3D11_FENCE_FLAG_SHARED, IID_PPV_ARGS(&hostFence))))
            return;

        HANDLE handle = nullptr;
        if (FAILED(hostFence->CreateSharedHandle(nullptr, GENERIC_ALL, nullptr, &handle))) {
            hostFence.Reset();
            return;
        }
        hostTeFence.take(TED3DSharedFenceCreate(handle, nullptr, nullptr));
        CloseHandle(handle);
        if (!hostTeFence)
            hostFence.Reset();
    }

    bool configureInstance(TEInstance *instance, QString *error)
    {
        if (!instance || !context) {
            assignError(error, QStringLiteral("D3D11 backend is not initialized"));
            return false;
        }
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
            || (!contains(handleTypes, TED3DHandleTypeD3D11Global)
                && !contains(handleTypes, TED3DHandleTypeD3D11NT))) {
            assignError(error, QStringLiteral("TouchEngine did not negotiate D3D11 shared textures"));
            return false;
        }
        if (!TEInstanceDoesTextureOwnershipTransfer(instance)) {
            assignError(error, QStringLiteral("TouchEngine did not negotiate texture ownership transfer for D3D11"));
            return false;
        }
        supportedFormats = std::move(formats);
        supportsFenceTransfers = contains(semaphoreTypes, TESemaphoreTypeD3DFence);
        releaseKeyedMutexToZero = TEInstanceRequiresKeyedMutexReleaseToZero(instance);
        configured = true;
        return true;
    }

    std::shared_ptr<InputSlot> acquireInputSlot(const QString &link, const QSize &size,
                                                const TextureFormat &format, bool *backpressured,
                                                QString *error)
    {
        if (backpressured)
            *backpressured = false;
        QList<std::shared_ptr<InputSlot>> &pool = inputSlots[link];
        qsizetype reclaimableIndex = -1;
        for (qsizetype index = 0; index < pool.size(); ++index) {
            const auto &slot = pool[index];
            if (slot->awaitingRelease
                && slot->token->releaseSerial.load(std::memory_order_acquire)
                    > slot->publishedReleaseSerial) {
                slot->awaitingRelease = false;
            }
            if (!slot->awaitingRelease && !slot->pendingPublication) {
                if (slot->size == size && slot->format.dxgi == format.dxgi)
                    return slot;
                if (reclaimableIndex < 0)
                    reclaimableIndex = index;
            }
        }
        if (pool.size() >= kMaximumInputTexturesPerLink && reclaimableIndex >= 0) {
            auto reclaimed = pool.takeAt(reclaimableIndex);
            releaseLater(reclaimed->rhiTexture);
        }
        if (pool.size() >= kMaximumInputTexturesPerLink) {
            // Keep the previously published texture bound until TouchEngine returns a slot.
            // Treat this as bounded GPU backpressure, not as a fatal session error.
            if (backpressured)
                *backpressured = true;
            return {};
        }

        auto slot = std::make_shared<InputSlot>();
        slot->link = link;
        slot->size = size;
        slot->format = format;

        D3D11_TEXTURE2D_DESC description = {};
        description.Width = static_cast<UINT>(size.width());
        description.Height = static_cast<UINT>(size.height());
        description.MipLevels = 1;
        description.ArraySize = 1;
        description.Format = format.dxgi;
        description.SampleDesc.Count = 1;
        description.Usage = D3D11_USAGE_DEFAULT;
        description.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        const HRESULT createResult = device->CreateTexture2D(&description, nullptr, &slot->nativeTexture);
        if (FAILED(createResult)) {
            assignError(error, hresultError("ID3D11Device::CreateTexture2D", createResult));
            return {};
        }

        slot->rhiTexture.reset(rhi->newTexture(
            format.rhi, size, 1,
            format.flags | QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));
        const QRhiTexture::NativeTexture native {
            static_cast<quint64>(reinterpret_cast<quintptr>(slot->nativeTexture.Get())), 0
        };
        if (!slot->rhiTexture || !slot->rhiTexture->createFrom(native)) {
            assignError(error, QStringLiteral("QRhi could not import the D3D11 input staging texture"));
            return {};
        }
        const QRhiTextureRenderTargetDescription targetDescription(
            QRhiColorAttachment(slot->rhiTexture.get()));
        slot->renderTarget.reset(rhi->newTextureRenderTarget(targetDescription));
        if (!slot->renderTarget) {
            assignError(error, QStringLiteral("QRhi could not create the D3D11 input render target"));
            return {};
        }
        slot->renderPassDescriptor.reset(
            slot->renderTarget->newCompatibleRenderPassDescriptor());
        if (!slot->renderPassDescriptor) {
            assignError(error, QStringLiteral("QRhi could not create the D3D11 input render pass"));
            return {};
        }
        slot->renderTarget->setRenderPassDescriptor(slot->renderPassDescriptor.get());
        if (!slot->renderTarget->create()) {
            assignError(error, QStringLiteral("QRhi could not initialize the D3D11 input render target"));
            return {};
        }
        slot->token = new InputUseToken;
        pool.append(slot);
        return slot;
    }

    bool prepareTextureInput(TEInstance *instance, const TextureInputSource &source,
                             QRhiCommandBuffer *commandBuffer, QString *error)
    {
        if (!configured || !instance || !commandBuffer || !source.render || source.link.isEmpty()) {
            assignError(error, QStringLiteral("D3D11 texture input has invalid or unconfigured arguments"));
            return false;
        }
        if (!source.pixelSize.isValid()) {
            assignError(error, QStringLiteral("D3D11 texture input '%1' has no usable size")
                                   .arg(source.link));
            return false;
        }
        const TextureFormat format = textureFormat(
            source.format, source.flags.testFlag(QRhiTexture::sRGB));
        if (!format) {
            assignError(error, QStringLiteral("The Qt texture format for '%1' is not supported by D3D11 interop")
                                   .arg(source.link));
            return false;
        }
        if (!supportedFormats.empty() && !contains(supportedFormats, format.dxgi)) {
            assignError(error, QStringLiteral("TouchEngine did not negotiate DXGI format %1 for '%2'")
                                   .arg(static_cast<int>(format.dxgi)).arg(source.link));
            return false;
        }

        bool backpressured = false;
        const auto slot = acquireInputSlot(source.link, source.pixelSize, format,
                                           &backpressured, error);
        if (!slot)
            return backpressured;
        if (!source.render(slot->rhiTexture.get(), slot->renderTarget.get(), commandBuffer, error))
            return false;
        slot->pendingPublication = true;
        pendingInputs.push_back(slot);
        return true;
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
            assignError(error, QStringLiteral("QRhi could not create the D3D11 output cache for '%1'").arg(link));
            return {};
        }
        return cache;
    }

    bool updateTextureOutput(TEInstance *instance, const QString &link, TETexture *texture,
                             QRhiCommandBuffer *commandBuffer, QString *error)
    {
        if (!configured || !instance || !commandBuffer || link.isEmpty()) {
            assignError(error, QStringLiteral("D3D11 texture output has invalid or unconfigured arguments"));
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
        TouchObject<TED3D11Texture> d3dTexture;
        const TEResult getResult = TED3D11ContextGetTexture(context, shared, d3dTexture.take());
        if (getResult != TEResultSuccess || !d3dTexture) {
            assignError(error, teError("TED3D11ContextGetTexture", getResult));
            return false;
        }
        ID3D11Texture2D *native = TED3D11TextureGetTexture(d3dTexture);
        if (!native) {
            assignError(error, QStringLiteral("TED3D11TextureGetTexture returned null"));
            return false;
        }
        D3D11_TEXTURE2D_DESC description = {};
        native->GetDesc(&description);
        if (!description.Width || !description.Height || description.SampleDesc.Count != 1
            || description.ArraySize != 1) {
            assignError(error, QStringLiteral("TouchEngine output '%1' has an unsupported D3D11 shape").arg(link));
            return false;
        }
        DXGI_FORMAT viewFormat = TED3DSharedTextureGetFormat(shared);
        if (viewFormat == DXGI_FORMAT_UNKNOWN)
            viewFormat = description.Format;
        const TextureFormat format = textureFormat(viewFormat);
        if (!format) {
            assignError(error, QStringLiteral("TouchEngine output '%1' has unsupported DXGI format %2")
                                   .arg(link).arg(static_cast<int>(viewFormat)));
            return false;
        }
        const QSize size(static_cast<int>(description.Width), static_cast<int>(description.Height));
        const auto cache = ensureOutputCache(link, size, format, error);
        if (!cache)
            return false;

        PendingOutput pending;
        pending.teTexture.set(texture);
        pending.d3dTexture = std::move(d3dTexture);
        pending.importedTexture.reset(rhi->newTexture(format.rhi, size, 1,
                                                      format.flags | QRhiTexture::UsedAsTransferSource));
        const QRhiTexture::NativeTexture nativeHandle {
            static_cast<quint64>(reinterpret_cast<quintptr>(native)), 0
        };
        if (!pending.importedTexture || !pending.importedTexture->createFrom(nativeHandle)) {
            assignError(error, QStringLiteral("QRhi could not import TouchEngine output '%1'").arg(link));
            return false;
        }

        if (TEInstanceHasTextureTransfer(instance, texture)) {
            TouchObject<TESemaphore> semaphore;
            quint64 waitValue = 0;
            const TEResult transferResult = TEInstanceGetTextureTransfer(
                instance, texture, semaphore.take(), &waitValue);
            if (transferResult != TEResultSuccess) {
                assignError(error, teError("TEInstanceGetTextureTransfer", transferResult));
                return false;
            }

            if (!semaphore) {
                const HRESULT queryResult = native->QueryInterface(IID_PPV_ARGS(&pending.keyedMutex));
                if (FAILED(queryResult)) {
                    assignError(error, hresultError("ID3D11Texture2D::QueryInterface(IDXGIKeyedMutex)", queryResult));
                    return false;
                }
                const HRESULT acquireResult = pending.keyedMutex->AcquireSync(
                    waitValue, kKeyedMutexTimeoutMs);
                if (acquireResult != S_OK) {
                    assignError(error,
                                acquireResult == WAIT_TIMEOUT || acquireResult == DXGI_ERROR_WAIT_TIMEOUT
                                    ? QStringLiteral("Timed out after %1 ms acquiring TouchEngine output '%2'")
                                          .arg(kKeyedMutexTimeoutMs).arg(link)
                                    : hresultError("IDXGIKeyedMutex::AcquireSync", acquireResult));
                    return false;
                }
                pending.returnKind = ReturnKind::KeyedMutex;
                pending.releaseValue = releaseKeyedMutexToZero
                    ? 0
                    : (waitValue == std::numeric_limits<quint64>::max() ? 0 : waitValue + 1);
            } else {
                if (TESemaphoreGetType(semaphore) != TESemaphoreTypeD3DFence
                    || !supportsFenceTransfers || !context4 || !hostFence || !hostTeFence) {
                    assignError(error, QStringLiteral("TouchEngine output '%1' requires an unavailable D3D11 fence transfer")
                                           .arg(link));
                    return false;
                }
                auto *sharedFence = static_cast<TED3DSharedFence *>(semaphore.get());
                ComPtr<ID3D11Fence> sourceFence;
                ComPtr<ID3D11Device5> device5;
                HRESULT fenceResult = device->QueryInterface(IID_PPV_ARGS(&device5));
                if (SUCCEEDED(fenceResult))
                    fenceResult = device5->OpenSharedFence(TED3DSharedFenceGetHandle(sharedFence),
                                                            IID_PPV_ARGS(&sourceFence));
                if (SUCCEEDED(fenceResult))
                    fenceResult = context4->Wait(sourceFence.Get(), waitValue);
                if (FAILED(fenceResult)) {
                    assignError(error, hresultError("D3D11 fence wait", fenceResult));
                    return false;
                }
                pending.returnKind = ReturnKind::Fence;
            }
        } else if (supportsFenceTransfers && context4 && hostFence && hostTeFence) {
            // Some outputs (notably a direct input-to-output pass-through) are
            // already available to the host and therefore have no incoming
            // transfer. Signal our fence after the Qt copy before returning the
            // resource to TouchEngine.
            pending.returnKind = ReturnKind::Fence;
        } else {
            const HRESULT queryResult = native->QueryInterface(IID_PPV_ARGS(&pending.keyedMutex));
            if (FAILED(queryResult)) {
                assignError(error,
                            QStringLiteral("TouchEngine output '%1' has no pending transfer and no usable host fence or keyed mutex")
                                .arg(link));
                return false;
            }
            const quint64 acquireValue = 0;
            const HRESULT acquireResult = pending.keyedMutex->AcquireSync(
                acquireValue, kKeyedMutexTimeoutMs);
            if (acquireResult != S_OK) {
                assignError(error,
                            acquireResult == WAIT_TIMEOUT || acquireResult == DXGI_ERROR_WAIT_TIMEOUT
                                ? QStringLiteral("Timed out after %1 ms acquiring transfer-less TouchEngine output '%2'")
                                      .arg(kKeyedMutexTimeoutMs).arg(link)
                                : hresultError("IDXGIKeyedMutex::AcquireSync", acquireResult));
                return false;
            }
            pending.returnKind = ReturnKind::KeyedMutex;
            pending.releaseValue = releaseKeyedMutexToZero ? 0 : acquireValue + 1;
        }

        QRhiTextureCopyDescription copy;
        copy.setPixelSize(size);
        QRhiResourceUpdateBatch *updates = rhi->nextResourceUpdateBatch();
        updates->copyTexture(cache->texture.get(), pending.importedTexture.get(), copy);
        commandBuffer->resourceUpdate(updates);
        cache->mirrorVertically = TETextureGetOrigin(texture) == TETextureOriginBottomLeft;
        pendingOutputs.push_back(std::move(pending));
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

    bool afterFrameEnd(TEInstance *instance, QString *error)
    {
        if (!instance) {
            assignError(error, QStringLiteral("Cannot finalize D3D11 transfers without a TEInstance"));
            return false;
        }
        bool success = true;
        bool needsFenceSignal = false;
        for (const PendingOutput &pending : pendingOutputs)
            needsFenceSignal = needsFenceSignal || pending.returnKind == ReturnKind::Fence;
        for (const RetryReturn &retry : retryReturns)
            needsFenceSignal = needsFenceSignal || retry.returnKind == ReturnKind::Fence;

        quint64 fenceValue = 0;
        bool fenceSignaled = !needsFenceSignal;
        if (needsFenceSignal) {
            fenceValue = nextFenceValue++;
            const HRESULT signalResult = context4->Signal(hostFence.Get(), fenceValue);
            if (FAILED(signalResult)) {
                appendError(error, hresultError("ID3D11DeviceContext4::Signal", signalResult));
                success = false;
            } else {
                fenceSignaled = true;
            }
        }

        std::vector<RetryReturn> nextRetries;
        auto addTransfer = [&](TouchObject<TETexture> &texture, ReturnKind kind, quint64 keyedValue) {
            if (!texture || kind == ReturnKind::None)
                return;
            if (kind == ReturnKind::Fence && !fenceSignaled) {
                nextRetries.push_back({ texture, kind, keyedValue });
                return;
            }
            TESemaphore *semaphore = kind == ReturnKind::Fence
                ? static_cast<TESemaphore *>(hostTeFence.get()) : nullptr;
            const quint64 value = kind == ReturnKind::Fence ? fenceValue : keyedValue;
            const TEResult result = TEInstanceAddTextureTransfer(instance, texture, semaphore, value);
            if (result != TEResultSuccess) {
                appendError(error, teError("TEInstanceAddTextureTransfer", result));
                nextRetries.push_back({ texture, kind, keyedValue });
                success = false;
            }
        };

        for (RetryReturn &retry : retryReturns)
            addTransfer(retry.teTexture, retry.returnKind, retry.releaseValue);
        retryReturns.clear();

        std::vector<PendingOutput> nextPendingOutputs;
        nextPendingOutputs.reserve(pendingOutputs.size());
        for (PendingOutput &pending : pendingOutputs) {
            // A failed fence signal means the submitted Qt copy is not yet represented by
            // a fence value that TouchEngine can wait on. Keep every object involved in
            // the import alive and retry the complete return on the next frame end.
            if (pending.returnKind == ReturnKind::Fence && !fenceSignaled) {
                nextPendingOutputs.push_back(std::move(pending));
                continue;
            }

            bool released = true;
            if (pending.returnKind == ReturnKind::KeyedMutex) {
                const HRESULT releaseResult = pending.keyedMutex->ReleaseSync(pending.releaseValue);
                if (FAILED(releaseResult)) {
                    appendError(error, hresultError("IDXGIKeyedMutex::ReleaseSync", releaseResult));
                    released = false;
                    success = false;
                }
            }
            if (!released) {
                // ReleaseSync did not transfer ownership. Dropping the D3D wrapper here
                // could let the resource disappear while TouchEngine still waits for it.
                // The retry vector is bounded by the number of acquired outputs.
                nextPendingOutputs.push_back(std::move(pending));
                continue;
            }

            addTransfer(pending.teTexture, pending.returnKind, pending.releaseValue);
            releaseLater(pending.importedTexture);
        }
        pendingOutputs = std::move(nextPendingOutputs);
        retryReturns = std::move(nextRetries);

        for (const auto &slot : pendingInputs) {
            slot->pendingPublication = false;
            slot->publishedReleaseSerial = slot->token->releaseSerial.load(std::memory_order_acquire);
            slot->awaitingRelease = true;

            TouchObject<TED3D11Texture> texture;
            TED3D11Texture *created = TED3D11TextureCreate(slot->nativeTexture.Get(),
                                                           TETextureOriginTopLeft,
                                                           kTETextureComponentMapIdentity,
                                                           inputTextureCallback,
                                                           slot->token);
            if (!created) {
                slot->awaitingRelease = false;
                appendError(error, QStringLiteral("TED3D11TextureCreate returned null"));
                success = false;
                continue;
            }
            retainToken(slot->token); // Released by the transient wrapper's Release callback.
            texture.take(created);

            const QByteArray identifier = slot->link.toUtf8();
            const TEResult result = TEInstanceLinkSetTextureValue(
                instance, identifier.constData(), reinterpret_cast<TETexture *>(texture.get()),
                reinterpret_cast<TEGraphicsContext *>(context.get()));
            if (result != TEResultSuccess) {
                slot->awaitingRelease = false;
                appendError(error, teError("TEInstanceLinkSetTextureValue", result));
                success = false;
            }
        }
        pendingInputs.clear();
        return success;
    }

    QRhi *rhi = nullptr;
    ID3D11Device *device = nullptr;
    ID3D11DeviceContext *deviceContext = nullptr;
    ComPtr<ID3D11DeviceContext4> context4;
    ComPtr<ID3D11Fence> hostFence;
    TouchObject<TED3DSharedFence> hostTeFence;
    TouchObject<TED3D11Context> context;
    quint64 nextFenceValue = 1;
    bool configured = false;
    bool supportsFenceTransfers = false;
    bool releaseKeyedMutexToZero = false;
    std::vector<DXGI_FORMAT> supportedFormats;
    QHash<QString, QList<std::shared_ptr<InputSlot>>> inputSlots;
    std::vector<std::shared_ptr<InputSlot>> pendingInputs;
    QHash<QString, std::shared_ptr<OutputCache>> outputCaches;
    std::vector<PendingOutput> pendingOutputs;
    std::vector<RetryReturn> retryReturns;
};

D3D11TouchEngineBackend::D3D11TouchEngineBackend()
    : d(std::make_unique<Impl>())
{
}

D3D11TouchEngineBackend::~D3D11TouchEngineBackend() = default;

DsTouchEngineTypes::GraphicsApi D3D11TouchEngineBackend::graphicsApi() const noexcept
{
    return DsTouchEngineTypes::GraphicsApi::Direct3D11;
}

bool D3D11TouchEngineBackend::initialize(QRhi *rhi, QRhiCommandBuffer *commandBuffer, QString *error)
{
    Q_UNUSED(commandBuffer);
    return d->initialize(rhi, error);
}

TEGraphicsContext *D3D11TouchEngineBackend::graphicsContext() const noexcept
{
    return reinterpret_cast<TEGraphicsContext *>(d->context.get());
}

bool D3D11TouchEngineBackend::configureInstance(TEInstance *instance, QString *error)
{
    return d->configureInstance(instance, error);
}

bool D3D11TouchEngineBackend::prepareTextureInput(TEInstance *instance,
                                                  const TextureInputSource &source,
                                                  QRhiCommandBuffer *commandBuffer,
                                                  QString *error)
{
    return d->prepareTextureInput(instance, source, commandBuffer, error);
}

bool D3D11TouchEngineBackend::updateTextureOutput(TEInstance *instance,
                                                  const QString &link,
                                                  TETexture *texture,
                                                  QRhiCommandBuffer *commandBuffer,
                                                  QString *error)
{
    return d->updateTextureOutput(instance, link, texture, commandBuffer, error);
}

TextureOutput D3D11TouchEngineBackend::textureOutput(const QString &link) const
{
    return d->textureOutput(link);
}

void D3D11TouchEngineBackend::clearTextureOutput(const QString &link)
{
    d->clearTextureOutput(link);
}

void D3D11TouchEngineBackend::clearTextureOutputs()
{
    d->clearTextureOutputs();
}

bool D3D11TouchEngineBackend::afterFrameEnd(TEInstance *instance, QString *error)
{
    return d->afterFrameEnd(instance, error);
}

} // namespace dsqt::touchengine::detail
