#include "D3D11NvInteropBridge_p.h"

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

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QOpenGLExtraFunctions>
#include <QStringList>

#include <d3d11_4.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

namespace dsqt::touchengine::detail {
namespace {

using Microsoft::WRL::ComPtr;

constexpr DWORD kKeyedMutexTimeoutMs = 16;
constexpr qsizetype kMaximumInputTexturesPerLink = 6;
constexpr qsizetype kMaximumOutputSlotsPerLink = 3;
constexpr GLenum kWglAccessReadOnlyNv = 0x0000;
constexpr GLenum kWglAccessReadWriteNv = 0x0001;

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
        .arg(static_cast<qulonglong>(static_cast<quint32>(result)), 8, 16,
             QLatin1Char('0'));
}

QString win32Error(const char *operation)
{
    return QStringLiteral("%1 failed (Win32 error %2)")
        .arg(QString::fromLatin1(operation))
        .arg(GetLastError());
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
        return {format,
                srgb ? QRhiTexture::Flags(QRhiTexture::sRGB) : QRhiTexture::Flags(),
                srgb ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
                     : DXGI_FORMAT_R8G8B8A8_UNORM};
    case QRhiTexture::BGRA8:
        return {format,
                srgb ? QRhiTexture::Flags(QRhiTexture::sRGB) : QRhiTexture::Flags(),
                srgb ? DXGI_FORMAT_B8G8R8A8_UNORM_SRGB
                     : DXGI_FORMAT_B8G8R8A8_UNORM};
    case QRhiTexture::R8:
        return {format, {}, DXGI_FORMAT_R8_UNORM};
    case QRhiTexture::RG8:
        return {format, {}, DXGI_FORMAT_R8G8_UNORM};
    case QRhiTexture::R16:
        return {format, {}, DXGI_FORMAT_R16_UNORM};
    case QRhiTexture::RG16:
        return {format, {}, DXGI_FORMAT_R16G16_UNORM};
    case QRhiTexture::RGBA16F:
        return {format, {}, DXGI_FORMAT_R16G16B16A16_FLOAT};
    case QRhiTexture::RGBA32F:
        return {format, {}, DXGI_FORMAT_R32G32B32A32_FLOAT};
    case QRhiTexture::R16F:
        return {format, {}, DXGI_FORMAT_R16_FLOAT};
    case QRhiTexture::R32F:
        return {format, {}, DXGI_FORMAT_R32_FLOAT};
    case QRhiTexture::RGB10A2:
        return {format, {}, DXGI_FORMAT_R10G10B10A2_UNORM};
    default:
        return {};
    }
}

TextureFormat textureFormat(DXGI_FORMAT format)
{
    switch (format) {
    case DXGI_FORMAT_R8G8B8A8_UNORM:
        return {QRhiTexture::RGBA8, {}, format};
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        return {QRhiTexture::RGBA8, QRhiTexture::sRGB, format};
    case DXGI_FORMAT_B8G8R8A8_UNORM:
        return {QRhiTexture::BGRA8, {}, format};
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        return {QRhiTexture::BGRA8, QRhiTexture::sRGB, format};
    case DXGI_FORMAT_R8_UNORM:
        return {QRhiTexture::R8, {}, format};
    case DXGI_FORMAT_R8G8_UNORM:
        return {QRhiTexture::RG8, {}, format};
    case DXGI_FORMAT_R16_UNORM:
        return {QRhiTexture::R16, {}, format};
    case DXGI_FORMAT_R16G16_UNORM:
        return {QRhiTexture::RG16, {}, format};
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
        return {QRhiTexture::RGBA16F, {}, format};
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
        return {QRhiTexture::RGBA32F, {}, format};
    case DXGI_FORMAT_R16_FLOAT:
        return {QRhiTexture::R16F, {}, format};
    case DXGI_FORMAT_R32_FLOAT:
        return {QRhiTexture::R32F, {}, format};
    case DXGI_FORMAT_R10G10B10A2_UNORM:
        return {QRhiTexture::RGB10A2, {}, format};
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
bool queryCapabilities(Function function, TEInstance *instance,
                       std::vector<T> *values, const char *name, QString *error)
{
    int32_t count = 0;
    TEResult result = function(instance, nullptr, &count);
    if (result != TEResultInsufficientMemory && result != TEResultSuccess) {
        assignError(error, teError(name, result));
        return false;
    }
    if (count < 0) {
        assignError(error,
                    QStringLiteral("%1 returned an invalid capability count")
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

bool isValidWglProcAddress(PROC address) noexcept
{
    const auto value = reinterpret_cast<quintptr>(address);
    return address && value != 1 && value != 2 && value != 3
        && value != static_cast<quintptr>(-1);
}

template<typename Function>
Function resolveWglFunction(const char *name) noexcept
{
    const PROC address = wglGetProcAddress(name);
    return isValidWglProcAddress(address)
        ? reinterpret_cast<Function>(address) : nullptr;
}

struct InputUseToken
{
    std::atomic_uint references {1};
    std::atomic_uint64_t releaseSerial {0};
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

class ExternalCommands final
{
public:
    explicit ExternalCommands(QRhiCommandBuffer *commandBuffer)
        : m_commandBuffer(commandBuffer)
    {
        if (m_commandBuffer)
            m_commandBuffer->beginExternal();
    }

    ~ExternalCommands()
    {
        if (m_commandBuffer)
            m_commandBuffer->endExternal();
    }

    ExternalCommands(const ExternalCommands &) = delete;
    ExternalCommands &operator=(const ExternalCommands &) = delete;

private:
    QRhiCommandBuffer *m_commandBuffer = nullptr;
};

} // namespace

class D3D11NvInteropBridge::Impl
{
public:
    using OpenDevice = HANDLE (WINAPI *)(void *);
    using CloseDevice = BOOL (WINAPI *)(HANDLE);
    using RegisterObject = HANDLE (WINAPI *)(HANDLE, void *, GLuint, GLenum, GLenum);
    using UnregisterObject = BOOL (WINAPI *)(HANDLE, HANDLE);
    using LockObjects = BOOL (WINAPI *)(HANDLE, GLint, HANDLE *);
    using UnlockObjects = BOOL (WINAPI *)(HANDLE, GLint, HANDLE *);

    struct Registration
    {
        ComPtr<ID3D11Texture2D> texture;
        GLuint glName = 0;
        HANDLE object = nullptr;
        QSize size;
        TextureFormat format;
        quint64 lastUseSerial = 0;
        bool locked = false;
    };

    struct InputSlot
    {
        ~InputSlot() { releaseToken(token); }

        QString link;
        std::shared_ptr<Registration> registration;
        std::unique_ptr<QRhiTexture> rhiTexture;
        std::unique_ptr<QRhiTextureRenderTarget> renderTarget;
        std::unique_ptr<QRhiRenderPassDescriptor> renderPassDescriptor;
        InputUseToken *token = nullptr;
        quint64 publishedReleaseSerial = 0;
        bool awaitingRelease = false;
        bool pendingPublication = false;
        bool publishAfterUnlock = false;
    };

    struct OutputSlot
    {
        std::shared_ptr<Registration> registration;
        ComPtr<ID3D11Query> completionQuery;
        quint64 readyFenceValue = 0;
        quint64 copySerial = 0;
        bool pendingCopy = false;
        bool queryPending = false;
        bool consumeAfterUnlock = false;
        bool mirrorVertically = false;
    };

    enum class ReturnKind { None, KeyedMutex, Fence };

    struct PendingOutput
    {
        QString link;
        TouchObject<TETexture> teTexture;
        TouchObject<TED3D11Texture> d3dTexture;
        std::shared_ptr<OutputSlot> destination;
        ComPtr<IDXGIKeyedMutex> keyedMutex;
        ReturnKind returnKind = ReturnKind::None;
        quint64 releaseValue = 0;
    };

    struct RetryReturn
    {
        TouchObject<TETexture> teTexture;
        ReturnKind returnKind = ReturnKind::None;
        quint64 value = 0;
    };

    bool initialize(QRhi *newRhi, QOpenGLExtraFunctions *newGl,
                    HDC nativeDeviceContext, QString *error)
    {
        if (!newRhi || newRhi->backend() != QRhi::OpenGLES2 || !newGl
            || !nativeDeviceContext || !wglGetCurrentContext()
            || wglGetCurrentDC() != nativeDeviceContext) {
            assignError(error,
                        QStringLiteral("The D3D11/NV bridge requires an initialized WGL QRhi"));
            return false;
        }

        rhi = newRhi;
        gl = newGl;
        gl->initializeOpenGLFunctions();
        gl->glGenFramebuffers(1, &readFramebuffer);
        gl->glGenFramebuffers(1, &drawFramebuffer);
        if (!readFramebuffer || !drawFramebuffer) {
            assignError(error,
                        QStringLiteral("OpenGL could not create NV interop copy resources"));
            return false;
        }

        openDevice = resolveWglFunction<OpenDevice>("wglDXOpenDeviceNV");
        closeDevice = resolveWglFunction<CloseDevice>("wglDXCloseDeviceNV");
        registerObject = resolveWglFunction<RegisterObject>("wglDXRegisterObjectNV");
        unregisterObject = resolveWglFunction<UnregisterObject>("wglDXUnregisterObjectNV");
        lockObjects = resolveWglFunction<LockObjects>("wglDXLockObjectsNV");
        unlockObjects = resolveWglFunction<UnlockObjects>("wglDXUnlockObjectsNV");
        if (!openDevice || !closeDevice || !registerObject || !unregisterObject
            || !lockObjects || !unlockObjects) {
            assignError(error,
                        QStringLiteral("One or more WGL_NV_DX_interop2 entry points are unavailable"));
            return false;
        }

        ComPtr<IDXGIFactory1> factory;
        HRESULT result = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
        if (FAILED(result) || !factory) {
            assignError(error, hresultError("CreateDXGIFactory1", result));
            return false;
        }

        QStringList attemptedAdapters;
        for (UINT index = 0;; ++index) {
            ComPtr<IDXGIAdapter1> adapter;
            result = factory->EnumAdapters1(index, &adapter);
            if (result == DXGI_ERROR_NOT_FOUND)
                break;
            if (FAILED(result) || !adapter)
                continue;

            DXGI_ADAPTER_DESC1 description {};
            if (FAILED(adapter->GetDesc1(&description))
                || (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
                continue;
            }
            const QString name = QString::fromWCharArray(description.Description).trimmed();
            attemptedAdapters.push_back(name);

            ComPtr<ID3D11Device> candidateDevice;
            ComPtr<ID3D11DeviceContext> candidateContext;
            result = D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
                                       D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0,
                                       D3D11_SDK_VERSION, &candidateDevice, nullptr,
                                       &candidateContext);
            if (FAILED(result) || !candidateDevice || !candidateContext)
                continue;

            HANDLE candidateInterop = openDevice(candidateDevice.Get());
            if (!candidateInterop)
                continue;

            device = std::move(candidateDevice);
            deviceContext = std::move(candidateContext);
            interopDevice = candidateInterop;
            adapterName = name;
            break;
        }

        if (!device || !deviceContext || !interopDevice) {
            assignError(error,
                        attemptedAdapters.isEmpty()
                            ? QStringLiteral("No hardware D3D11 adapter was available")
                            : QStringLiteral("No D3D11 adapter could interoperate with the current WGL context; tried: %1")
                                  .arg(attemptedAdapters.join(QStringLiteral(", "))));
            return false;
        }

        TED3D11Context *created = nullptr;
        const TEResult teResult = TED3D11ContextCreate(device.Get(), &created);
        if (teResult != TEResultSuccess || !created) {
            assignError(error, teError("TED3D11ContextCreate", teResult));
            releaseResources();
            return false;
        }

        context.take(created);
        createHostFence();
        return true;
    }

    void createHostFence()
    {
        ComPtr<ID3D11Device5> device5;
        if (FAILED(device.As(&device5))
            || FAILED(deviceContext.As(&context4))) {
            return;
        }
        if (FAILED(device5->CreateFence(0, D3D11_FENCE_FLAG_SHARED,
                                        IID_PPV_ARGS(&hostFence)))) {
            return;
        }

        HANDLE handle = nullptr;
        if (FAILED(hostFence->CreateSharedHandle(nullptr, GENERIC_ALL, nullptr,
                                                 &handle))) {
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
            assignError(error, QStringLiteral("The D3D11/NV bridge is not initialized"));
            return false;
        }

        std::vector<TETextureType> textureTypes;
        std::vector<TED3DHandleType> handleTypes;
        std::vector<DXGI_FORMAT> formats;
        std::vector<TESemaphoreType> semaphoreTypes;
        if (!queryCapabilities<TETextureType>(
                TEInstanceGetSupportedTextureTypes, instance, &textureTypes,
                "TEInstanceGetSupportedTextureTypes", error)
            || !queryCapabilities<TED3DHandleType>(
                TEInstanceGetSupportedD3DHandleTypes, instance, &handleTypes,
                "TEInstanceGetSupportedD3DHandleTypes", error)
            || !queryCapabilities<DXGI_FORMAT>(
                TEInstanceGetSupportedD3DFormats, instance, &formats,
                "TEInstanceGetSupportedD3DFormats", error)
            || !queryCapabilities<TESemaphoreType>(
                TEInstanceGetSupportedSemaphoreTypes, instance, &semaphoreTypes,
                "TEInstanceGetSupportedSemaphoreTypes", error)) {
            return false;
        }
        if (!contains(textureTypes, TETextureTypeD3DShared)
            || (!contains(handleTypes, TED3DHandleTypeD3D11Global)
                && !contains(handleTypes, TED3DHandleTypeD3D11NT))) {
            assignError(error,
                        QStringLiteral("TouchEngine did not negotiate D3D11 shared textures"));
            return false;
        }
        if (!TEInstanceDoesTextureOwnershipTransfer(instance)) {
            assignError(error,
                        QStringLiteral("TouchEngine did not negotiate D3D11 texture ownership transfer"));
            return false;
        }

        supportedFormats = std::move(formats);
        supportsFenceTransfers = contains(semaphoreTypes, TESemaphoreTypeD3DFence);
        releaseKeyedMutexToZero = TEInstanceRequiresKeyedMutexReleaseToZero(instance);
        configured = true;
        return true;
    }

    std::shared_ptr<Registration> createRegistration(ID3D11Texture2D *texture,
                                                     const QSize &size,
                                                     const TextureFormat &format,
                                                     GLenum access,
                                                     QString *error)
    {
        if (!texture || !gl || !interopDevice) {
            assignError(error, QStringLiteral("Cannot register an invalid D3D11 texture"));
            return {};
        }

        auto registration = std::make_shared<Registration>();
        registration->texture = texture;
        registration->size = size;
        registration->format = format;
        registration->lastUseSerial = ++useSerial;
        gl->glGenTextures(1, &registration->glName);
        if (!registration->glName) {
            assignError(error, QStringLiteral("OpenGL could not allocate an NV interop texture name"));
            return {};
        }

        registration->object = registerObject(
            interopDevice, texture, registration->glName, GL_TEXTURE_2D, access);
        if (!registration->object) {
            assignError(error, win32Error("wglDXRegisterObjectNV"));
            gl->glDeleteTextures(1, &registration->glName);
            registration->glName = 0;
            return {};
        }
        return registration;
    }

    void destroyRegistration(const std::shared_ptr<Registration> &registration,
                             QString *error = nullptr)
    {
        if (!registration)
            return;
        if (registration->locked && registration->object) {
            HANDLE object = registration->object;
            if (!unlockObjects(interopDevice, 1, &object)) {
                appendError(error, win32Error("wglDXUnlockObjectsNV(teardown)"));
            } else {
                registration->locked = false;
            }
        }
        if (registration->object && interopDevice) {
            if (!unregisterObject(interopDevice, registration->object))
                appendError(error, win32Error("wglDXUnregisterObjectNV"));
            registration->object = nullptr;
        }
        if (registration->glName && gl) {
            gl->glDeleteTextures(1, &registration->glName);
            registration->glName = 0;
        }
        registration->texture.Reset();
    }

    bool lockRegistration(const std::shared_ptr<Registration> &registration,
                          QString *error)
    {
        if (!registration || !registration->object || registration->locked) {
            assignError(error, QStringLiteral("The NV interop texture is invalid or already locked"));
            return false;
        }
        HANDLE object = registration->object;
        if (!lockObjects(interopDevice, 1, &object)) {
            assignError(error, win32Error("wglDXLockObjectsNV"));
            return false;
        }
        registration->locked = true;
        registration->lastUseSerial = ++useSerial;
        return true;
    }

    bool unlockRegistration(const std::shared_ptr<Registration> &registration,
                            QString *error)
    {
        if (!registration || !registration->locked)
            return true;
        HANDLE object = registration->object;
        if (!unlockObjects(interopDevice, 1, &object)) {
            appendError(error, win32Error("wglDXUnlockObjectsNV"));
            return false;
        }
        registration->locked = false;
        return true;
    }

    bool outputCopyComplete(const std::shared_ptr<OutputSlot> &slot) const
    {
        if (!slot || slot->pendingCopy || !slot->queryPending)
            return false;
        if (slot->readyFenceValue && hostFence)
            return hostFence->GetCompletedValue() >= slot->readyFenceValue;
        if (!slot->completionQuery)
            return false;

        BOOL complete = FALSE;
        return deviceContext->GetData(slot->completionQuery.Get(), &complete,
                                      sizeof(complete),
                                      D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK
            && complete;
    }

    std::shared_ptr<OutputSlot> acquireOutputSlot(
        const QString &link, const QSize &size, const TextureFormat &format,
        QString *error)
    {
        QList<std::shared_ptr<OutputSlot>> &pool = outputSlots[link];
        qsizetype reclaimableIndex = -1;
        for (qsizetype index = 0; index < pool.size(); ++index) {
            const auto &slot = pool[index];
            if (!slot || slot->pendingCopy || slot->queryPending
                || (slot->registration && slot->registration->locked)) {
                continue;
            }
            if (slot->registration && slot->registration->size == size
                && slot->registration->format.dxgi == format.dxgi) {
                return slot;
            }
            if (reclaimableIndex < 0)
                reclaimableIndex = index;
        }

        if (pool.size() >= kMaximumOutputSlotsPerLink && reclaimableIndex >= 0) {
            const auto reclaimed = pool.takeAt(reclaimableIndex);
            destroyRegistration(reclaimed ? reclaimed->registration : nullptr,
                                error);
        }
        if (pool.size() >= kMaximumOutputSlotsPerLink)
            return {};

        D3D11_TEXTURE2D_DESC description {};
        description.Width = static_cast<UINT>(size.width());
        description.Height = static_cast<UINT>(size.height());
        description.MipLevels = 1;
        description.ArraySize = 1;
        description.Format = format.dxgi;
        description.SampleDesc.Count = 1;
        description.Usage = D3D11_USAGE_DEFAULT;
        description.BindFlags = D3D11_BIND_SHADER_RESOURCE
            | D3D11_BIND_RENDER_TARGET;

        ComPtr<ID3D11Texture2D> texture;
        const HRESULT createResult = device->CreateTexture2D(
            &description, nullptr, &texture);
        if (FAILED(createResult)) {
            assignError(error,
                        hresultError("ID3D11Device::CreateTexture2D(output staging)",
                                     createResult));
            return {};
        }

        auto slot = std::make_shared<OutputSlot>();
        slot->registration = createRegistration(
            texture.Get(), size, format, kWglAccessReadOnlyNv, error);
        if (!slot->registration)
            return {};

        D3D11_QUERY_DESC queryDescription {};
        queryDescription.Query = D3D11_QUERY_EVENT;
        const HRESULT queryResult = device->CreateQuery(
            &queryDescription, &slot->completionQuery);
        if (FAILED(queryResult)) {
            assignError(error,
                        hresultError("ID3D11Device::CreateQuery(output staging)",
                                     queryResult));
            destroyRegistration(slot->registration, error);
            return {};
        }

        pool.push_back(slot);
        return slot;
    }

    std::shared_ptr<OutputSlot> newestReadyOutput(const QString &link)
    {
        auto found = outputSlots.find(link);
        if (found == outputSlots.end())
            return {};

        std::shared_ptr<OutputSlot> newest;
        for (const auto &slot : std::as_const(found.value())) {
            if (!outputCopyComplete(slot))
                continue;
            if (!newest || slot->copySerial > newest->copySerial)
                newest = slot;
        }

        // When Qt missed more than one completed staging copy, publish only the
        // newest one and recycle older completed slots to keep latency bounded.
        for (const auto &slot : std::as_const(found.value())) {
            if (slot != newest && outputCopyComplete(slot)) {
                slot->readyFenceValue = 0;
                slot->queryPending = false;
            }
        }
        return newest;
    }

    std::shared_ptr<InputSlot> acquireInputSlot(const QString &link,
                                                const QSize &size,
                                                const TextureFormat &format,
                                                QRhiCommandBuffer *commandBuffer,
                                                bool *backpressured,
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
                if (slot->registration && slot->registration->size == size
                    && slot->registration->format.dxgi == format.dxgi) {
                    return slot;
                }
                if (reclaimableIndex < 0)
                    reclaimableIndex = index;
            }
        }

        if (pool.size() >= kMaximumInputTexturesPerLink && reclaimableIndex >= 0) {
            auto reclaimed = pool.takeAt(reclaimableIndex);
            reclaimed->renderTarget.reset();
            reclaimed->renderPassDescriptor.reset();
            reclaimed->rhiTexture.reset();
            destroyRegistration(reclaimed->registration, error);
        }
        if (pool.size() >= kMaximumInputTexturesPerLink) {
            if (backpressured)
                *backpressured = true;
            return {};
        }

        auto slot = std::make_shared<InputSlot>();
        slot->link = link;
        slot->token = new InputUseToken;

        D3D11_TEXTURE2D_DESC description {};
        description.Width = static_cast<UINT>(size.width());
        description.Height = static_cast<UINT>(size.height());
        description.MipLevels = 1;
        description.ArraySize = 1;
        description.Format = format.dxgi;
        description.SampleDesc.Count = 1;
        description.Usage = D3D11_USAGE_DEFAULT;
        description.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        ComPtr<ID3D11Texture2D> nativeTexture;
        HRESULT result = device->CreateTexture2D(&description, nullptr, &nativeTexture);
        if (FAILED(result)) {
            assignError(error, hresultError("ID3D11Device::CreateTexture2D", result));
            return {};
        }

        slot->registration = createRegistration(nativeTexture.Get(), size, format,
                                                kWglAccessReadWriteNv, error);
        if (!slot->registration)
            return {};
        // QRhi validates the framebuffer during render-target creation. The
        // registered GL texture is only a usable image while GL owns the NV
        // interop object, so take that ownership for the complete import setup.
        if (!lockRegistration(slot->registration, error)) {
            destroyRegistration(slot->registration, error);
            return {};
        }

        slot->rhiTexture.reset(rhi->newTexture(
            format.rhi, size, 1,
            format.flags | QRhiTexture::RenderTarget
                | QRhiTexture::UsedAsTransferSource));
        const QRhiTexture::NativeTexture native {
            static_cast<quint64>(slot->registration->glName), 0
        };
        if (!slot->rhiTexture || !slot->rhiTexture->createFrom(native)) {
            assignError(error,
                        QStringLiteral("QRhi could not import the NV interop input texture"));
            slot->rhiTexture.reset();
            destroyRegistration(slot->registration, error);
            return {};
        }

        const QRhiTextureRenderTargetDescription targetDescription(
            QRhiColorAttachment(slot->rhiTexture.get()));
        slot->renderTarget.reset(rhi->newTextureRenderTarget(targetDescription));
        if (!slot->renderTarget) {
            assignError(error,
                        QStringLiteral("QRhi could not create the NV interop input render target"));
            destroyRegistration(slot->registration, error);
            return {};
        }
        slot->renderPassDescriptor.reset(
            slot->renderTarget->newCompatibleRenderPassDescriptor());
        if (!slot->renderPassDescriptor) {
            assignError(error,
                        QStringLiteral("QRhi could not create the NV interop input render pass"));
            destroyRegistration(slot->registration, error);
            return {};
        }
        slot->renderTarget->setRenderPassDescriptor(
            slot->renderPassDescriptor.get());
        if (!slot->renderTarget->create()) {
            assignError(error,
                        QStringLiteral("QRhi could not initialize the NV interop input render target"));
            destroyRegistration(slot->registration, error);
            return {};
        }
        if (!unlockRegistration(slot->registration, error)) {
            destroyRegistration(slot->registration, error);
            return {};
        }

        Q_UNUSED(commandBuffer);
        pool.append(slot);
        return slot;
    }

    bool prepareTextureInput(TEInstance *instance,
                             const TextureInputSource &source,
                             QRhiCommandBuffer *commandBuffer,
                             QString *error)
    {
        if (!configured || !instance || !commandBuffer || !source.render
            || source.link.isEmpty()) {
            assignError(error,
                        QStringLiteral("D3D11/NV texture input has invalid or unconfigured arguments"));
            return false;
        }
        if (!source.pixelSize.isValid()) {
            assignError(error,
                        QStringLiteral("D3D11/NV texture input '%1' has no usable size")
                            .arg(source.link));
            return false;
        }
        const TextureFormat format = textureFormat(
            source.format, source.flags.testFlag(QRhiTexture::sRGB));
        if (!format || !rhi->isTextureFormatSupported(
                           format.rhi,
                           format.flags | QRhiTexture::RenderTarget
                               | QRhiTexture::UsedAsTransferSource)) {
            assignError(error,
                        QStringLiteral("The Qt texture format for '%1' is not supported by D3D11/NV interop")
                            .arg(source.link));
            return false;
        }
        if (!supportedFormats.empty() && !contains(supportedFormats, format.dxgi)) {
            assignError(error,
                        QStringLiteral("TouchEngine did not negotiate DXGI format %1 for '%2'")
                            .arg(static_cast<int>(format.dxgi))
                            .arg(source.link));
            return false;
        }

        bool backpressured = false;
        std::shared_ptr<InputSlot> slot;
        {
            ExternalCommands external(commandBuffer);
            slot = acquireInputSlot(source.link, source.pixelSize, format,
                                    commandBuffer, &backpressured, error);
            if (!slot)
                return backpressured;
            if (!lockRegistration(slot->registration, error))
                return false;
            slot->pendingPublication = true;
            slot->publishAfterUnlock = false;
            pendingInputs.push_back(slot);
        }

        if (!source.render(slot->rhiTexture.get(), slot->renderTarget.get(),
                           commandBuffer, error)) {
            return false;
        }
        slot->publishAfterUnlock = true;
        return true;
    }

    bool acquireOutputOwnership(TEInstance *instance, TETexture *texture,
                                ID3D11Texture2D *native,
                                PendingOutput *pending, QString *error)
    {
        if (TEInstanceHasTextureTransfer(instance, texture)) {
            TouchObject<TESemaphore> semaphore;
            quint64 waitValue = 0;
            const TEResult transferResult = TEInstanceGetTextureTransfer(
                instance, texture, semaphore.take(), &waitValue);
            if (transferResult != TEResultSuccess) {
                assignError(error,
                            teError("TEInstanceGetTextureTransfer", transferResult));
                return false;
            }

            if (!semaphore) {
                const HRESULT queryResult = native->QueryInterface(
                    IID_PPV_ARGS(&pending->keyedMutex));
                if (FAILED(queryResult)) {
                    assignError(error,
                                hresultError("ID3D11Texture2D::QueryInterface(IDXGIKeyedMutex)",
                                             queryResult));
                    return false;
                }
                const HRESULT acquireResult = pending->keyedMutex->AcquireSync(
                    waitValue, kKeyedMutexTimeoutMs);
                if (acquireResult != S_OK) {
                    assignError(
                        error,
                        acquireResult == WAIT_TIMEOUT
                                || acquireResult == DXGI_ERROR_WAIT_TIMEOUT
                            ? QStringLiteral("Timed out after %1 ms acquiring TouchEngine output '%2'")
                                  .arg(kKeyedMutexTimeoutMs)
                                  .arg(pending->link)
                            : hresultError("IDXGIKeyedMutex::AcquireSync",
                                           acquireResult));
                    return false;
                }
                pending->returnKind = ReturnKind::KeyedMutex;
                pending->releaseValue = releaseKeyedMutexToZero
                    ? 0
                    : (waitValue == std::numeric_limits<quint64>::max()
                           ? 0 : waitValue + 1);
                return true;
            }

            if (TESemaphoreGetType(semaphore) != TESemaphoreTypeD3DFence
                || !supportsFenceTransfers || !context4 || !hostFence
                || !hostTeFence) {
                assignError(error,
                            QStringLiteral("TouchEngine output '%1' requires an unavailable D3D11 fence transfer")
                                .arg(pending->link));
                return false;
            }
            auto *sharedFence = static_cast<TED3DSharedFence *>(semaphore.get());
            ComPtr<ID3D11Fence> sourceFence;
            ComPtr<ID3D11Device5> device5;
            HRESULT fenceResult = device.As(&device5);
            if (SUCCEEDED(fenceResult)) {
                fenceResult = device5->OpenSharedFence(
                    TED3DSharedFenceGetHandle(sharedFence),
                    IID_PPV_ARGS(&sourceFence));
            }
            if (SUCCEEDED(fenceResult))
                fenceResult = context4->Wait(sourceFence.Get(), waitValue);
            if (FAILED(fenceResult)) {
                assignError(error, hresultError("D3D11 fence wait", fenceResult));
                return false;
            }
            pending->returnKind = ReturnKind::Fence;
            return true;
        }

        if (supportsFenceTransfers && context4 && hostFence && hostTeFence) {
            pending->returnKind = ReturnKind::Fence;
            return true;
        }

        const HRESULT queryResult = native->QueryInterface(
            IID_PPV_ARGS(&pending->keyedMutex));
        if (FAILED(queryResult)) {
            assignError(error,
                        QStringLiteral("TouchEngine output '%1' has no pending transfer and no usable host fence or keyed mutex")
                            .arg(pending->link));
            return false;
        }
        const quint64 acquireValue = 0;
        const HRESULT acquireResult = pending->keyedMutex->AcquireSync(
            acquireValue, kKeyedMutexTimeoutMs);
        if (acquireResult != S_OK) {
            assignError(error,
                        acquireResult == WAIT_TIMEOUT
                                || acquireResult == DXGI_ERROR_WAIT_TIMEOUT
                            ? QStringLiteral("Timed out after %1 ms acquiring transfer-less TouchEngine output '%2'")
                                  .arg(kKeyedMutexTimeoutMs)
                                  .arg(pending->link)
                            : hresultError("IDXGIKeyedMutex::AcquireSync",
                                           acquireResult));
            return false;
        }
        pending->returnKind = ReturnKind::KeyedMutex;
        pending->releaseValue = releaseKeyedMutexToZero ? 0 : acquireValue + 1;
        return true;
    }

    bool acquireTextureOutput(TEInstance *instance, const QString &link,
                              TETexture *texture,
                              QRhiCommandBuffer *commandBuffer,
                              D3D11NvInteropBridge::AcquiredOutput *output,
                              QString *error)
    {
        if (!configured || !instance || !texture || !output || link.isEmpty()) {
            assignError(error,
                        QStringLiteral("D3D11/NV texture output has invalid or unconfigured arguments"));
            return false;
        }
        if (TETextureGetType(texture) != TETextureTypeD3DShared) {
            assignError(error,
                        QStringLiteral("TouchEngine output '%1' is not a D3D shared texture")
                            .arg(link));
            return false;
        }
        const bool linkPending = std::any_of(
            pendingOutputs.cbegin(), pendingOutputs.cend(),
            [&link](const PendingOutput &pending) { return pending.link == link; });
        if (linkPending)
            return false;

        auto *shared = static_cast<TED3DSharedTexture *>(texture);
        TouchObject<TED3D11Texture> d3dTexture;
        const TEResult getResult = TED3D11ContextGetTexture(
            context, shared, d3dTexture.take());
        if (getResult != TEResultSuccess || !d3dTexture) {
            assignError(error, teError("TED3D11ContextGetTexture", getResult));
            return false;
        }
        ID3D11Texture2D *native = TED3D11TextureGetTexture(d3dTexture);
        if (!native) {
            assignError(error,
                        QStringLiteral("TED3D11TextureGetTexture returned null"));
            return false;
        }

        D3D11_TEXTURE2D_DESC description {};
        native->GetDesc(&description);
        if (!description.Width || !description.Height
            || description.SampleDesc.Count != 1 || description.ArraySize != 1
            || description.Usage != D3D11_USAGE_DEFAULT) {
            assignError(error,
                        QStringLiteral("TouchEngine output '%1' has an unsupported D3D11 shape or usage")
                            .arg(link));
            return false;
        }
        DXGI_FORMAT viewFormat = TED3DSharedTextureGetFormat(shared);
        if (viewFormat == DXGI_FORMAT_UNKNOWN)
            viewFormat = description.Format;
        const TextureFormat format = textureFormat(viewFormat);
        if (!format) {
            assignError(error,
                        QStringLiteral("TouchEngine output '%1' has unsupported DXGI format %2")
                            .arg(link)
                            .arg(static_cast<int>(viewFormat)));
            return false;
        }
        if (!supportedFormats.empty() && !contains(supportedFormats, viewFormat)) {
            assignError(error,
                        QStringLiteral("TouchEngine output '%1' uses unnegotiated DXGI format %2")
                            .arg(link)
                            .arg(static_cast<int>(viewFormat)));
            return false;
        }

        const QSize size(static_cast<int>(description.Width),
                         static_cast<int>(description.Height));
        Q_UNUSED(commandBuffer);
        const auto destination = acquireOutputSlot(link, size, format, error);
        if (!destination)
            return false;

        // Lock only a staging texture whose D3D copy was submitted on an older
        // Qt frame. Do this before queueing work for the current TouchEngine
        // output so wglDXLockObjectsNV cannot serialize on that newer work.
        const auto ready = newestReadyOutput(link);
        if (ready) {
            if (!lockRegistration(ready->registration, error))
                return false;
            ready->consumeAfterUnlock = true;
            output->textureName = ready->registration->glName;
            output->pixelSize = ready->registration->size;
            output->format = ready->registration->format.rhi;
            output->flags = ready->registration->format.flags;
            output->mirrorVertically = ready->mirrorVertically;
        }

        PendingOutput pending;
        pending.link = link;
        pending.teTexture.set(texture);
        pending.d3dTexture = std::move(d3dTexture);
        pending.destination = destination;
        if (!acquireOutputOwnership(instance, texture, native, &pending, error))
            return false;

        deviceContext->CopyResource(destination->registration->texture.Get(), native);
        deviceContext->End(destination->completionQuery.Get());
        destination->pendingCopy = true;
        destination->copySerial = ++useSerial;
        destination->mirrorVertically =
            TETextureGetOrigin(texture) == TETextureOriginBottomLeft;
        pendingOutputs.push_back(std::move(pending));
        return true;
    }

    bool copyAcquiredOutput(
        const D3D11NvInteropBridge::AcquiredOutput &output,
        GLuint destinationTextureName, QString *error)
    {
        if (!output.textureName || !destinationTextureName
            || !output.pixelSize.isValid()) {
            assignError(error,
                        QStringLiteral("Cannot copy an invalid NV interop output"));
            return false;
        }

        std::shared_ptr<OutputSlot> sourceSlot;
        for (auto pool = outputSlots.cbegin(); pool != outputSlots.cend(); ++pool) {
            const auto found = std::find_if(
                pool.value().cbegin(), pool.value().cend(),
                [&output](const std::shared_ptr<OutputSlot> &slot) {
                    return slot && slot->registration
                        && slot->registration->glName == output.textureName;
                });
            if (found != pool.value().cend()) {
                sourceSlot = *found;
                break;
            }
        }
        if (!sourceSlot || !sourceSlot->registration
            || !sourceSlot->registration->locked) {
            assignError(error,
                        QStringLiteral("The acquired NV interop output is no longer locked"));
            return false;
        }
        while (gl->glGetError() != GL_NO_ERROR) {}
        gl->glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebuffer);
        gl->glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_2D, output.textureName, 0);
        const GLenum readStatus = gl->glCheckFramebufferStatus(GL_READ_FRAMEBUFFER);
        gl->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFramebuffer);
        gl->glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_2D, destinationTextureName, 0);
        const GLenum drawStatus = gl->glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);

        GLenum copyError = GL_NO_ERROR;
        if (readStatus == GL_FRAMEBUFFER_COMPLETE
            && drawStatus == GL_FRAMEBUFFER_COMPLETE) {
            gl->glBlitFramebuffer(
                0, 0, output.pixelSize.width(), output.pixelSize.height(),
                0, 0, output.pixelSize.width(), output.pixelSize.height(),
                GL_COLOR_BUFFER_BIT, GL_NEAREST);
            copyError = gl->glGetError();
            gl->glFinish();
        }

        gl->glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebuffer);
        gl->glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_2D, 0, 0);
        gl->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFramebuffer);
        gl->glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_2D, 0, 0);
        gl->glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        gl->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

        HANDLE object = sourceSlot->registration->object;
        const bool unlocked = unlockObjects(interopDevice, 1, &object);
        if (unlocked) {
            sourceSlot->registration->locked = false;
            sourceSlot->readyFenceValue = 0;
            sourceSlot->queryPending = false;
            sourceSlot->consumeAfterUnlock = false;
        }
        if (readStatus != GL_FRAMEBUFFER_COMPLETE) {
            assignError(error,
                        QStringLiteral("The staged NV interop source is not framebuffer-complete (0x%1)")
                            .arg(QString::number(readStatus, 16)));
            return false;
        }
        if (drawStatus != GL_FRAMEBUFFER_COMPLETE) {
            assignError(error,
                        QStringLiteral("The OpenGL output cache is not framebuffer-complete (0x%1)")
                            .arg(QString::number(drawStatus, 16)));
            return false;
        }
        if (copyError != GL_NO_ERROR) {
            assignError(error,
                        QStringLiteral("The NV interop copy failed with OpenGL error 0x%1")
                            .arg(QString::number(copyError, 16)));
            return false;
        }
        if (!unlocked) {
            assignError(error, win32Error("wglDXUnlockObjectsNV(output copy)"));
            return false;
        }
        return true;
    }

    bool afterFrameEnd(TEInstance *instance, QString *error)
    {
        if (!instance) {
            assignError(error,
                        QStringLiteral("Cannot finalize D3D11/NV transfers without a TEInstance"));
            return false;
        }

        bool success = true;
        bool hasLockedOutput = false;
        for (auto pool = outputSlots.cbegin(); pool != outputSlots.cend(); ++pool) {
            for (const auto &slot : pool.value()) {
                hasLockedOutput = hasLockedOutput
                    || (slot && slot->registration
                        && slot->registration->locked);
            }
        }
        if ((!pendingInputs.empty() || hasLockedOutput) && gl)
            gl->glFlush();

        for (auto pool = outputSlots.begin(); pool != outputSlots.end(); ++pool) {
            for (const auto &slot : pool.value()) {
                if (!slot || !slot->registration
                    || !slot->registration->locked) {
                    continue;
                }
                if (!unlockRegistration(slot->registration, error)) {
                    success = false;
                    continue;
                }
                if (slot->consumeAfterUnlock) {
                    slot->readyFenceValue = 0;
                    slot->queryPending = false;
                    slot->consumeAfterUnlock = false;
                }
            }
        }

        std::vector<std::shared_ptr<InputSlot>> nextInputs;
        for (const auto &slot : pendingInputs) {
            if (!unlockRegistration(slot->registration, error)) {
                nextInputs.push_back(slot);
                success = false;
                continue;
            }
            slot->pendingPublication = false;
            if (!slot->publishAfterUnlock)
                continue;

            slot->publishedReleaseSerial =
                slot->token->releaseSerial.load(std::memory_order_acquire);
            slot->awaitingRelease = true;
            TouchObject<TED3D11Texture> texture;
            TED3D11Texture *created = TED3D11TextureCreate(
                slot->registration->texture.Get(), TETextureOriginBottomLeft,
                kTETextureComponentMapIdentity, inputTextureCallback, slot->token);
            if (!created) {
                slot->awaitingRelease = false;
                appendError(error,
                            QStringLiteral("TED3D11TextureCreate returned null"));
                success = false;
                continue;
            }
            retainToken(slot->token);
            texture.take(created);

            const QByteArray identifier = slot->link.toUtf8();
            const TEResult result = TEInstanceLinkSetTextureValue(
                instance, identifier.constData(),
                reinterpret_cast<TETexture *>(texture.get()),
                reinterpret_cast<TEGraphicsContext *>(context.get()));
            if (result != TEResultSuccess) {
                slot->awaitingRelease = false;
                appendError(error,
                            teError("TEInstanceLinkSetTextureValue", result));
                success = false;
            }
        }
        pendingInputs = std::move(nextInputs);

        std::vector<PendingOutput> nextPendingOutputs;
        const bool needsFenceSignal = !pendingOutputs.empty()
            && context4 && hostFence;
        quint64 fenceValue = 0;
        bool fenceSignaled = !needsFenceSignal;
        if (needsFenceSignal) {
            fenceValue = nextFenceValue++;
            const HRESULT signalResult = context4->Signal(hostFence.Get(), fenceValue);
            if (FAILED(signalResult)) {
                appendError(error,
                            hresultError("ID3D11DeviceContext4::Signal",
                                         signalResult));
                success = false;
            } else {
                fenceSignaled = true;
            }
        } else if (!pendingOutputs.empty()) {
            // Completion queries are polled without an implicit flush. Ensure
            // keyed-mutex-only devices submit their staging copies as well.
            deviceContext->Flush();
        }

        auto addTransfer = [&](TouchObject<TETexture> &texture,
                               ReturnKind kind, quint64 value) {
            if (!texture || kind == ReturnKind::None)
                return true;
            TESemaphore *semaphore = kind == ReturnKind::Fence
                ? static_cast<TESemaphore *>(hostTeFence.get()) : nullptr;
            const TEResult result = TEInstanceAddTextureTransfer(
                instance, texture, semaphore, value);
            if (result != TEResultSuccess) {
                appendError(error,
                            teError("TEInstanceAddTextureTransfer", result));
                return false;
            }
            return true;
        };

        std::vector<RetryReturn> nextRetries;
        for (RetryReturn &retry : retryReturns) {
            if (!addTransfer(retry.teTexture, retry.returnKind, retry.value)) {
                nextRetries.push_back(std::move(retry));
                success = false;
            }
        }
        retryReturns.clear();

        for (PendingOutput &pending : pendingOutputs) {
            if (pending.destination) {
                pending.destination->pendingCopy = false;
                pending.destination->queryPending = true;
                pending.destination->readyFenceValue = fenceSignaled
                    && needsFenceSignal ? fenceValue : 0;
            }
            if (pending.returnKind == ReturnKind::Fence && !fenceSignaled) {
                nextPendingOutputs.push_back(std::move(pending));
                success = false;
                continue;
            }

            bool released = true;
            quint64 transferValue = pending.releaseValue;
            if (pending.returnKind == ReturnKind::KeyedMutex) {
                const HRESULT releaseResult = pending.keyedMutex->ReleaseSync(
                    pending.releaseValue);
                if (FAILED(releaseResult)) {
                    appendError(error,
                                hresultError("IDXGIKeyedMutex::ReleaseSync",
                                             releaseResult));
                    released = false;
                    success = false;
                }
            } else if (pending.returnKind == ReturnKind::Fence) {
                transferValue = fenceValue;
            }
            if (!released) {
                nextPendingOutputs.push_back(std::move(pending));
                continue;
            }
            if (!addTransfer(pending.teTexture, pending.returnKind,
                             transferValue)) {
                nextRetries.push_back({pending.teTexture, pending.returnKind,
                                       transferValue});
                success = false;
            }
        }

        pendingOutputs = std::move(nextPendingOutputs);
        retryReturns = std::move(nextRetries);
        return success && pendingOutputs.empty() && pendingInputs.empty()
            && retryReturns.empty();
    }

    bool resetInstance(QString *error)
    {
        if (gl)
            gl->glFlush();
        for (const auto &slot : pendingInputs)
            unlockRegistration(slot ? slot->registration : nullptr, error);
        pendingInputs.clear();
        pendingOutputs.clear();
        retryReturns.clear();

        for (auto &pool : inputSlots) {
            for (const auto &slot : pool) {
                if (!slot)
                    continue;
                slot->renderTarget.reset();
                slot->renderPassDescriptor.reset();
                slot->rhiTexture.reset();
                destroyRegistration(slot->registration, error);
            }
        }
        inputSlots.clear();
        for (auto pool = outputSlots.begin(); pool != outputSlots.end(); ++pool) {
            for (const auto &slot : pool.value())
                destroyRegistration(slot ? slot->registration : nullptr, error);
        }
        outputSlots.clear();

        supportedFormats.clear();
        configured = false;
        supportsFenceTransfers = false;
        releaseKeyedMutexToZero = false;
        return !error || error->isEmpty();
    }

    void releaseResources()
    {
        QString ignored;
        if (rhi)
            rhi->makeThreadLocalNativeContextCurrent();
        resetInstance(&ignored);
        context.reset();
        hostTeFence.reset();
        hostFence.Reset();
        context4.Reset();
        if (gl && readFramebuffer)
            gl->glDeleteFramebuffers(1, &readFramebuffer);
        if (gl && drawFramebuffer)
            gl->glDeleteFramebuffers(1, &drawFramebuffer);
        readFramebuffer = 0;
        drawFramebuffer = 0;
        if (interopDevice && closeDevice)
            closeDevice(interopDevice);
        interopDevice = nullptr;
        deviceContext.Reset();
        device.Reset();
        rhi = nullptr;
        gl = nullptr;
    }

    QRhi *rhi = nullptr;
    QOpenGLExtraFunctions *gl = nullptr;
    GLuint readFramebuffer = 0;
    GLuint drawFramebuffer = 0;
    OpenDevice openDevice = nullptr;
    CloseDevice closeDevice = nullptr;
    RegisterObject registerObject = nullptr;
    UnregisterObject unregisterObject = nullptr;
    LockObjects lockObjects = nullptr;
    UnlockObjects unlockObjects = nullptr;
    HANDLE interopDevice = nullptr;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> deviceContext;
    ComPtr<ID3D11DeviceContext4> context4;
    ComPtr<ID3D11Fence> hostFence;
    TouchObject<TED3DSharedFence> hostTeFence;
    TouchObject<TED3D11Context> context;
    QString adapterName;
    quint64 nextFenceValue = 1;
    quint64 useSerial = 0;
    bool configured = false;
    bool supportsFenceTransfers = false;
    bool releaseKeyedMutexToZero = false;
    std::vector<DXGI_FORMAT> supportedFormats;
    QHash<QString, QList<std::shared_ptr<InputSlot>>> inputSlots;
    std::vector<std::shared_ptr<InputSlot>> pendingInputs;
    QHash<QString, QList<std::shared_ptr<OutputSlot>>> outputSlots;
    std::vector<PendingOutput> pendingOutputs;
    std::vector<RetryReturn> retryReturns;
};

D3D11NvInteropBridge::D3D11NvInteropBridge()
    : d(std::make_unique<Impl>())
{
}

D3D11NvInteropBridge::~D3D11NvInteropBridge()
{
    d->releaseResources();
}

bool D3D11NvInteropBridge::initialize(QRhi *rhi, QOpenGLExtraFunctions *gl,
                                     void *nativeDeviceContext, QString *error)
{
    if (error)
        error->clear();
    return d->initialize(rhi, gl, static_cast<HDC>(nativeDeviceContext), error);
}

TEGraphicsContext *D3D11NvInteropBridge::graphicsContext() const noexcept
{
    return reinterpret_cast<TEGraphicsContext *>(d->context.get());
}

QString D3D11NvInteropBridge::adapterName() const
{
    return d->adapterName;
}

bool D3D11NvInteropBridge::configureInstance(TEInstance *instance, QString *error)
{
    if (error)
        error->clear();
    return d->configureInstance(instance, error);
}

bool D3D11NvInteropBridge::resetInstance(QString *error)
{
    if (error)
        error->clear();
    return d->resetInstance(error);
}

bool D3D11NvInteropBridge::prepareTextureInput(
    TEInstance *instance, const TextureInputSource &source,
    QRhiCommandBuffer *commandBuffer, QString *error)
{
    if (error)
        error->clear();
    return d->prepareTextureInput(instance, source, commandBuffer, error);
}

bool D3D11NvInteropBridge::acquireTextureOutput(
    TEInstance *instance, const QString &link, TETexture *texture,
    QRhiCommandBuffer *commandBuffer, AcquiredOutput *output, QString *error)
{
    if (error)
        error->clear();
    if (output)
        *output = {};
    return d->acquireTextureOutput(instance, link, texture, commandBuffer,
                                   output, error);
}

bool D3D11NvInteropBridge::copyAcquiredOutput(
    const AcquiredOutput &output, quint32 destinationTextureName,
    QString *error)
{
    if (error)
        error->clear();
    return d->copyAcquiredOutput(output, destinationTextureName, error);
}

bool D3D11NvInteropBridge::afterFrameEnd(TEInstance *instance, QString *error)
{
    if (error)
        error->clear();
    return d->afterFrameEnd(instance, error);
}

void D3D11NvInteropBridge::releaseResources()
{
    d->releaseResources();
}

} // namespace dsqt::touchengine::detail
