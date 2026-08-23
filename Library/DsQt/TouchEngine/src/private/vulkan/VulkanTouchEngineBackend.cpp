#include <QtCore/qglobal.h>

#if defined(Q_OS_WIN) && __has_include(<vulkan/vulkan.h>)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  ifndef VK_USE_PLATFORM_WIN32_KHR
#    define VK_USE_PLATFORM_WIN32_KHR
#  endif
#  include <windows.h>
#  include <vulkan/vulkan.h>
#  include <vulkan/vulkan_win32.h>
#  define DSQT_TOUCHENGINE_VULKAN_BACKEND 1
#endif

#include "VulkanTouchEngineBackend_p.h"

#if defined(DSQT_TOUCHENGINE_VULKAN_BACKEND)

#include <QDebug>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QStringList>
#include <QVector>
#include <QVulkanInstance>

#include <rhi/qrhi_platform.h>

#include <TouchEngine/TEResult.h>
#include <TouchEngine/TESemaphore.h>
#include <TouchEngine/TETexture.h>
#include <TouchEngine/TEVulkan.h>
#include <TouchEngine/TouchObject.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace dsqt::touchengine::detail {
namespace {

constexpr VkExternalMemoryHandleTypeFlagBits kMemoryHandleType =
    VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
constexpr VkExternalSemaphoreHandleTypeFlagBits kSemaphoreHandleType =
    VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;
constexpr std::size_t kMaximumInputPoolSize = 4;

bool isOpaqueWin32MemoryHandle(VkExternalMemoryHandleTypeFlagBits handleType)
{
    return handleType == VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT
        || handleType == VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT;
}

bool fail(QString *error, QString message)
{
    if (error)
        *error = std::move(message);
    return false;
}

enum class TextureAcquireResult
{
    Acquired,
    NotReady,
    Error,
};

QString teResultMessage(const char *operation, TEResult result)
{
    const char *description = TEResultGetDescription(result);
    return QStringLiteral("%1 failed: %2 (TEResult %3)")
        .arg(QString::fromLatin1(operation),
             description ? QString::fromUtf8(description) : QStringLiteral("unknown error"),
             QString::number(static_cast<int>(result)));
}

QString vkResultMessage(const char *operation, VkResult result)
{
    return QStringLiteral("%1 failed (VkResult %2)")
        .arg(QString::fromLatin1(operation), QString::number(static_cast<int>(result)));
}

bool isIdentityComponentMapping(const VkComponentMapping &mapping)
{
    const auto identityOr = [](VkComponentSwizzle value, VkComponentSwizzle explicitValue) {
        return value == VK_COMPONENT_SWIZZLE_IDENTITY || value == explicitValue;
    };
    return identityOr(mapping.r, VK_COMPONENT_SWIZZLE_R)
        && identityOr(mapping.g, VK_COMPONENT_SWIZZLE_G)
        && identityOr(mapping.b, VK_COMPONENT_SWIZZLE_B)
        && identityOr(mapping.a, VK_COMPONENT_SWIZZLE_A);
}

bool rhiToVkFormat(QRhiTexture *texture, VkFormat *vkFormat)
{
    if (!texture || !vkFormat)
        return false;

    const bool srgb = texture->flags().testFlag(QRhiTexture::sRGB);
    switch (texture->format()) {
    case QRhiTexture::RGBA8:
        *vkFormat = srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
        return true;
    case QRhiTexture::BGRA8:
        *vkFormat = srgb ? VK_FORMAT_B8G8R8A8_SRGB : VK_FORMAT_B8G8R8A8_UNORM;
        return true;
    case QRhiTexture::R8:
        *vkFormat = VK_FORMAT_R8_UNORM;
        return true;
    case QRhiTexture::RG8:
        *vkFormat = VK_FORMAT_R8G8_UNORM;
        return true;
    case QRhiTexture::R16:
        *vkFormat = VK_FORMAT_R16_UNORM;
        return true;
    case QRhiTexture::RG16:
        *vkFormat = VK_FORMAT_R16G16_UNORM;
        return true;
    case QRhiTexture::R16F:
        *vkFormat = VK_FORMAT_R16_SFLOAT;
        return true;
    case QRhiTexture::R32F:
        *vkFormat = VK_FORMAT_R32_SFLOAT;
        return true;
    case QRhiTexture::RGBA16F:
        *vkFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
        return true;
    case QRhiTexture::RGBA32F:
        *vkFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
        return true;
    case QRhiTexture::RGB10A2:
        *vkFormat = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
        return true;
    default:
        return false;
    }
}

bool vkToRhiFormat(VkFormat vkFormat,
                   QRhiTexture::Format *rhiFormat,
                   QRhiTexture::Flags *flags)
{
    if (!rhiFormat || !flags)
        return false;
    *flags = {};

    switch (vkFormat) {
    case VK_FORMAT_R8G8B8A8_SRGB:
        *flags |= QRhiTexture::sRGB;
        Q_FALLTHROUGH();
    case VK_FORMAT_R8G8B8A8_UNORM:
        *rhiFormat = QRhiTexture::RGBA8;
        return true;
    case VK_FORMAT_B8G8R8A8_SRGB:
        *flags |= QRhiTexture::sRGB;
        Q_FALLTHROUGH();
    case VK_FORMAT_B8G8R8A8_UNORM:
        *rhiFormat = QRhiTexture::BGRA8;
        return true;
    case VK_FORMAT_R8_UNORM:
        *rhiFormat = QRhiTexture::R8;
        return true;
    case VK_FORMAT_R8G8_UNORM:
        *rhiFormat = QRhiTexture::RG8;
        return true;
    case VK_FORMAT_R16_UNORM:
        *rhiFormat = QRhiTexture::R16;
        return true;
    case VK_FORMAT_R16G16_UNORM:
        *rhiFormat = QRhiTexture::RG16;
        return true;
    case VK_FORMAT_R16_SFLOAT:
        *rhiFormat = QRhiTexture::R16F;
        return true;
    case VK_FORMAT_R32_SFLOAT:
        *rhiFormat = QRhiTexture::R32F;
        return true;
    case VK_FORMAT_R16G16B16A16_SFLOAT:
        *rhiFormat = QRhiTexture::RGBA16F;
        return true;
    case VK_FORMAT_R32G32B32A32_SFLOAT:
        *rhiFormat = QRhiTexture::RGBA32F;
        return true;
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
        *rhiFormat = QRhiTexture::RGB10A2;
        return true;
    default:
        return false;
    }
}

class QueueSignalRegistry
{
public:
    static void add(QRhi *rhi, VkSemaphore semaphore)
    {
        QMutexLocker lock(&mutex());
        auto &entries = registryEntries()[rhi];
        entries.push_back(semaphore);

        QRhiVulkanQueueSubmitParams params = {};
        params.signalSemaphoreCount = uint32_t(entries.size());
        params.signalSemaphores = entries.data();
        rhi->setQueueSubmitParams(&params);
    }

    static void remove(QRhi *rhi, const std::vector<VkSemaphore> &owned)
    {
        if (!rhi || owned.empty())
            return;

        QMutexLocker lock(&mutex());
        auto it = registryEntries().find(rhi);
        if (it == registryEntries().end())
            return;

        auto &entries = it.value();
        for (VkSemaphore semaphore : owned)
            entries.removeAll(semaphore);
        if (entries.isEmpty())
            registryEntries().erase(it);
    }

private:
    static QMutex &mutex()
    {
        static QMutex value;
        return value;
    }

    static QHash<QRhi *, QVector<VkSemaphore>> &registryEntries()
    {
        static QHash<QRhi *, QVector<VkSemaphore>> value;
        return value;
    }
};

struct InputUseState
{
    std::atomic_bool available{true};
    std::atomic_bool needsAcquire{false};
    std::atomic_bool released{false};
    std::atomic_int callbacks{0};
};

struct OutputReleaseState
{
    // One reference belongs to the render-thread cache entry and one to the callback
    // installed on the TETexture. The callback reference is released by the final
    // TEObjectEventRelease, allowing backend teardown without a dangling info pointer.
    std::atomic_uint references{2};
    std::atomic_uint callbacks{0};
    std::atomic_bool released{false};
    quint64 generation = 0;
};

std::atomic<quint64> nextOutputGeneration{1};

void releaseOutputState(OutputReleaseState *state)
{
    if (state && state->references.fetch_sub(1, std::memory_order_acq_rel) == 1)
        delete state;
}

void outputTextureCallback(HANDLE, TEObjectEvent event, void *info)
{
    auto *state = static_cast<OutputReleaseState *>(info);
    if (!state)
        return;

    state->callbacks.fetch_add(1, std::memory_order_acq_rel);
    if (event == TEObjectEventRelease)
        state->released.store(true, std::memory_order_release);
    state->callbacks.fetch_sub(1, std::memory_order_acq_rel);

    // Release is the final callback for this object; relinquish the callback-owned ref
    // only after all callback accesses are complete.
    if (event == TEObjectEventRelease)
        releaseOutputState(state);
}

void inputTextureCallback(HANDLE, TEObjectEvent event, void *info)
{
    auto *state = static_cast<InputUseState *>(info);
    if (!state)
        return;

    state->callbacks.fetch_add(1, std::memory_order_acq_rel);
    switch (event) {
    case TEObjectEventBeginUse:
        state->available.store(false, std::memory_order_release);
        break;
    case TEObjectEventEndUse:
        state->needsAcquire.store(true, std::memory_order_release);
        state->available.store(true, std::memory_order_release);
        break;
    case TEObjectEventRelease:
        state->available.store(false, std::memory_order_release);
        state->released.store(true, std::memory_order_release);
        break;
    }
    state->callbacks.fetch_sub(1, std::memory_order_acq_rel);
}

} // namespace

struct VulkanTouchEngineBackend::Impl
{
    struct Dispatch
    {
        PFN_vkGetDeviceProcAddr getDeviceProcAddr = nullptr;
        PFN_vkGetPhysicalDeviceProperties2 getPhysicalDeviceProperties2 = nullptr;
        PFN_vkGetPhysicalDeviceImageFormatProperties2 getPhysicalDeviceImageFormatProperties2 = nullptr;
        PFN_vkGetMemoryWin32HandleKHR getMemoryWin32Handle = nullptr;
        PFN_vkGetMemoryWin32HandlePropertiesKHR getMemoryWin32HandleProperties = nullptr;
        PFN_vkGetSemaphoreWin32HandleKHR getSemaphoreWin32Handle = nullptr;
        PFN_vkImportSemaphoreWin32HandleKHR importSemaphoreWin32Handle = nullptr;
    } vk;

    struct NativeImage
    {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        std::unique_ptr<QRhiTexture> wrapper;
        QSize size;
        VkFormat vkFormat = VK_FORMAT_UNDEFINED;
        QRhiTexture::Format rhiFormat = QRhiTexture::UnknownFormat;
        QRhiTexture::Flags rhiFlags;
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    };

    struct InputEntry
    {
        NativeImage native;
        TouchObject<TEVulkanTexture> texture;
        InputUseState use;
        bool everPublished = false;
    };

    struct InputLink
    {
        QString link;
        std::vector<std::unique_ptr<InputEntry>> entries;
        InputEntry *currentPublished = nullptr;
    };

    struct ImportedOutput
    {
        NativeImage native;
        HANDLE sourceHandle = nullptr;
        const TEVulkanTexture *sourceObject = nullptr;
        quint64 instanceGeneration = 0;
        VkExternalMemoryHandleTypeFlagBits handleType = kMemoryHandleType;
        OutputReleaseState *lifetime = nullptr;
        QString link;
        int lastUsedFrameSlot = -1;
        bool purgeRequested = false;
        bool nativeRetirementPending = false;
    };

    struct RetiredImportNative
    {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        ImportedOutput *owner = nullptr;
    };

    struct OutputRecord
    {
        QString link;
        std::unique_ptr<QRhiTexture> cache;
        QSize size;
        QRhiTexture::Format format = QRhiTexture::UnknownFormat;
        QRhiTexture::Flags flags;
        bool mirrorVertically = false;
    };

    struct HostSignal
    {
        VkSemaphore native = VK_NULL_HANDLE;
        TouchObject<TEVulkanSemaphore> textureTransfer;
        int frameSlot = -1;
    };

    struct PendingInput
    {
        QString link;
        InputEntry *entry = nullptr;
        HostSignal signal;
        VkImageLayout oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageLayout newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        bool transferRegistered = false;
    };

    struct PendingOutput
    {
        QString link;
        TouchObject<TETexture> texture;
        HostSignal signal;
        ImportedOutput *imported = nullptr;
        VkImageLayout oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageLayout newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        bool releaseRecorded = false;
        QString recoveryError;
    };

    struct FrameSlot
    {
        VkCommandPool commandPool = VK_NULL_HANDLE;
        std::vector<VkCommandBuffer> commandBuffers;
        std::vector<VkSemaphore> retiredSemaphores;
        std::vector<std::unique_ptr<QRhiTexture>> retiredRhiTextures;
        std::vector<RetiredImportNative> importsAwaitingNativeDestroy;
        std::size_t nextCommandBuffer = 0;
        bool active = false;
    };

    QRhi *rhi = nullptr;
    QVulkanInstance *qtInstance = nullptr;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    uint32_t queueFamily = 0;
    TouchObject<TEVulkanContext> context;
    TEInstance *configuredInstance = nullptr;

    bool initialized = false;
    bool configured = false;
    bool ownershipTransfer = false;
    VkImageLayout inputReleaseLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    std::vector<VkFormat> supportedFormats;

    std::vector<FrameSlot> frameSlots;
    std::vector<InputLink> inputLinks;
    std::vector<std::unique_ptr<InputEntry>> retiredInputs;
    std::vector<std::unique_ptr<ImportedOutput>> importedOutputs;
    std::vector<OutputRecord> outputs;
    std::vector<PendingInput> pendingInputs;
    std::vector<PendingOutput> pendingOutputs;
    std::vector<VkSemaphore> frameSignals;
    quint64 instanceGeneration = 1;

    ~Impl() { shutdown(); }

    template<typename T>
    T loadDeviceFunction(const char *name) const
    {
        return reinterpret_cast<T>(vk.getDeviceProcAddr(device, name));
    }

    bool loadDispatch(QString *error)
    {
        vk.getDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
            qtInstance->getInstanceProcAddr("vkGetDeviceProcAddr"));
        vk.getPhysicalDeviceProperties2 = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(
            qtInstance->getInstanceProcAddr("vkGetPhysicalDeviceProperties2"));
        if (!vk.getPhysicalDeviceProperties2) {
            vk.getPhysicalDeviceProperties2 = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(
                qtInstance->getInstanceProcAddr("vkGetPhysicalDeviceProperties2KHR"));
        }
        vk.getPhysicalDeviceImageFormatProperties2 =
            reinterpret_cast<PFN_vkGetPhysicalDeviceImageFormatProperties2>(
                qtInstance->getInstanceProcAddr("vkGetPhysicalDeviceImageFormatProperties2"));
        if (!vk.getPhysicalDeviceImageFormatProperties2) {
            vk.getPhysicalDeviceImageFormatProperties2 =
                reinterpret_cast<PFN_vkGetPhysicalDeviceImageFormatProperties2>(
                    qtInstance->getInstanceProcAddr("vkGetPhysicalDeviceImageFormatProperties2KHR"));
        }

        if (!vk.getDeviceProcAddr || !vk.getPhysicalDeviceProperties2
            || !vk.getPhysicalDeviceImageFormatProperties2) {
            return fail(error,
                        QStringLiteral("The Qt Vulkan instance does not expose the Vulkan 1.1 "
                                       "physical-device query functions required by TouchEngine"));
        }

        vk.getMemoryWin32Handle =
            loadDeviceFunction<PFN_vkGetMemoryWin32HandleKHR>("vkGetMemoryWin32HandleKHR");
        vk.getMemoryWin32HandleProperties =
            loadDeviceFunction<PFN_vkGetMemoryWin32HandlePropertiesKHR>(
                "vkGetMemoryWin32HandlePropertiesKHR");
        vk.getSemaphoreWin32Handle =
            loadDeviceFunction<PFN_vkGetSemaphoreWin32HandleKHR>("vkGetSemaphoreWin32HandleKHR");
        vk.importSemaphoreWin32Handle =
            loadDeviceFunction<PFN_vkImportSemaphoreWin32HandleKHR>(
                "vkImportSemaphoreWin32HandleKHR");

        if (!vk.getMemoryWin32Handle || !vk.getMemoryWin32HandleProperties
            || !vk.getSemaphoreWin32Handle || !vk.importSemaphoreWin32Handle) {
            return fail(error,
                        QStringLiteral("Qt's Vulkan device was created without the external-memory/"
                                       "external-semaphore Win32 commands required by TouchEngine. "
                                       "Before the QQuickWindow scene graph is initialized, call "
                                       "QQuickGraphicsConfiguration::setDeviceExtensions() with "
                                       "VK_KHR_external_memory, VK_KHR_external_memory_win32, "
                                       "VK_KHR_external_semaphore, and "
                                       "VK_KHR_external_semaphore_win32."));
        }
        return true;
    }

    bool verifySemaphoreExport(QString *error)
    {
        VkExportSemaphoreCreateInfo exportInfo = {};
        exportInfo.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO;
        exportInfo.handleTypes = kSemaphoreHandleType;

        VkSemaphoreCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        createInfo.pNext = &exportInfo;

        VkSemaphore semaphore = VK_NULL_HANDLE;
        VkResult result = vkCreateSemaphore(device, &createInfo, nullptr, &semaphore);
        if (result != VK_SUCCESS) {
            return fail(error,
                        vkResultMessage("Vulkan external semaphore probe", result)
                            + QStringLiteral(". Configure the required device extensions before "
                                             "the QQuickWindow scene graph is initialized."));
        }

        VkSemaphoreGetWin32HandleInfoKHR handleInfo = {};
        handleInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR;
        handleInfo.semaphore = semaphore;
        handleInfo.handleType = kSemaphoreHandleType;
        HANDLE handle = nullptr;
        result = vk.getSemaphoreWin32Handle(device, &handleInfo, &handle);
        if (handle)
            CloseHandle(handle);
        vkDestroySemaphore(device, semaphore, nullptr);
        if (result != VK_SUCCESS) {
            return fail(error,
                        vkResultMessage("vkGetSemaphoreWin32HandleKHR probe", result)
                            + QStringLiteral(". Configure VK_KHR_external_semaphore and "
                                             "VK_KHR_external_semaphore_win32 through "
                                             "QQuickGraphicsConfiguration before window creation."));
        }
        return true;
    }

    uint32_t memoryType(uint32_t allowed, VkMemoryPropertyFlags preferred) const
    {
        VkPhysicalDeviceMemoryProperties properties = {};
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &properties);
        for (uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
            if ((allowed & (uint32_t(1) << i))
                && (properties.memoryTypes[i].propertyFlags & preferred) == preferred) {
                return i;
            }
        }
        for (uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
            if (allowed & (uint32_t(1) << i))
                return i;
        }
        return UINT32_MAX;
    }

    void discardNative(NativeImage *native)
    {
        native->wrapper.reset();
        if (native->image)
            vkDestroyImage(device, native->image, nullptr);
        if (native->memory)
            vkFreeMemory(device, native->memory, nullptr);
        native->image = VK_NULL_HANDLE;
        native->memory = VK_NULL_HANDLE;
    }

    void releaseImportedState(ImportedOutput *imported)
    {
        if (!imported || !imported->lifetime)
            return;
        releaseOutputState(imported->lifetime);
        imported->lifetime = nullptr;
    }

    bool importHasPendingUse(const ImportedOutput *imported) const
    {
        return std::any_of(pendingOutputs.begin(), pendingOutputs.end(),
                           [imported](const PendingOutput &pending) {
                               return pending.imported == imported;
                           });
    }

    static bool importWasReleased(const ImportedOutput *imported)
    {
        return imported->lifetime
            && imported->lifetime->released.load(std::memory_order_acquire);
    }

    static bool importHasNative(const ImportedOutput *imported)
    {
        return imported->native.wrapper || imported->native.image || imported->native.memory;
    }

    void finishRetiredImports(FrameSlot *slot)
    {
        for (const RetiredImportNative &retired : slot->importsAwaitingNativeDestroy) {
            if (retired.image)
                vkDestroyImage(device, retired.image, nullptr);
            if (retired.memory)
                vkFreeMemory(device, retired.memory, nullptr);
            if (retired.owner)
                retired.owner->nativeRetirementPending = false;
        }
        slot->importsAwaitingNativeDestroy.clear();
    }

    void stageReleasedImports(int slotIndex, FrameSlot *slot)
    {
        auto it = importedOutputs.begin();
        while (it != importedOutputs.end()) {
            ImportedOutput *imported = it->get();
            const bool teReleased = importWasReleased(imported);
            const bool gpuSlotSafe = imported->lastUsedFrameSlot < 0
                || imported->lastUsedFrameSlot == slotIndex;
            if ((teReleased || imported->purgeRequested) && gpuSlotSafe
                && !importHasPendingUse(imported) && !imported->nativeRetirementPending) {
                if (!importHasNative(imported)) {
                    imported->purgeRequested = false;
                    if (teReleased) {
                        releaseImportedState(imported);
                        it = importedOutputs.erase(it);
                    } else {
                        ++it;
                    }
                    continue;
                }

                // Destroying the QRhi wrapper queues its VkImageView for Qt's deferred
                // release. Retire only the raw native handles; the lightweight cache entry
                // and callback state remain discoverable until TEObjectEventRelease so a
                // later publication of this same live TETexture cannot replace its callback.
                imported->native.wrapper.reset();
                slot->importsAwaitingNativeDestroy.push_back(RetiredImportNative{
                    imported->native.image, imported->native.memory, imported});
                imported->native.image = VK_NULL_HANDLE;
                imported->native.memory = VK_NULL_HANDLE;
                imported->native.layout = VK_IMAGE_LAYOUT_UNDEFINED;
                imported->lastUsedFrameSlot = -1;
                imported->purgeRequested = false;
                imported->nativeRetirementPending = true;
                ++it;
            } else {
                ++it;
            }
        }
    }

    void requestImportPurge(const QString &link)
    {
        for (const auto &imported : importedOutputs) {
            if (link.isEmpty() || imported->link == link)
                imported->purgeRequested = true;
        }
    }

    bool purgeRequestedImportsOutsideFrame(QString *error)
    {
        if (!rhi || rhi->isRecordingFrame())
            return true;

        const auto canPurge = [this](const std::unique_ptr<ImportedOutput> &imported) {
            return (importWasReleased(imported.get()) || imported->purgeRequested
                    || imported->nativeRetirementPending)
                && !importHasPendingUse(imported.get());
        };

        const bool hasStagedImports = std::any_of(
            frameSlots.begin(), frameSlots.end(), [](const FrameSlot &slot) {
                return !slot.importsAwaitingNativeDestroy.empty();
            });
        const bool hasPurgeableImports = std::any_of(importedOutputs.begin(),
                                                     importedOutputs.end(), canPurge);
        if (!hasStagedImports && !hasPurgeableImports)
            return true;

        // First drain every prior submission. This also completes any QRhi image-view
        // releases queued when an import was staged on a frame slot.
        if (rhi->finish() != QRhi::FrameOpSuccess) {
            return fail(error,
                        QStringLiteral("QRhi could not quiesce Vulkan output imports for purge"));
        }
        bool releasedWrapper = false;
        for (auto &slot : frameSlots) {
            finishRetiredImports(&slot);
            if (!slot.retiredRhiTextures.empty()) {
                slot.retiredRhiTextures.clear();
                releasedWrapper = true;
            }
        }

        for (auto &imported : importedOutputs) {
            if (canPurge(imported) && !imported->nativeRetirementPending
                && imported->native.wrapper) {
                imported->native.wrapper.reset();
                imported->nativeRetirementPending = true;
                releasedWrapper = true;
            }
        }

        // QRhi owns the VkImageView made by createFrom(). Drain that deferred release
        // before destroying the VkImage and imported VkDeviceMemory underneath it.
        if (releasedWrapper && rhi->finish() != QRhi::FrameOpSuccess) {
            return fail(error,
                        QStringLiteral("QRhi could not retire Vulkan output import views"));
        }

        auto it = importedOutputs.begin();
        while (it != importedOutputs.end()) {
            ImportedOutput *imported = it->get();
            if (canPurge(*it) && imported->nativeRetirementPending
                && !imported->native.wrapper) {
                discardNative(&(*it)->native);
                imported->nativeRetirementPending = false;
                imported->lastUsedFrameSlot = -1;
                imported->purgeRequested = false;
            }

            if (importWasReleased(imported) && !imported->nativeRetirementPending
                && !importHasNative(imported)) {
                releaseImportedState(imported);
                it = importedOutputs.erase(it);
            } else {
                if (!imported->nativeRetirementPending && !importHasNative(imported))
                    imported->purgeRequested = false;
                ++it;
            }
        }
        return true;
    }

    bool supportsExternalImage(VkFormat format,
                               VkImageUsageFlags usage,
                               VkExternalMemoryHandleTypeFlagBits handleType,
                               VkExternalMemoryFeatureFlags required,
                               QString *error,
                               VkExternalMemoryFeatureFlags *availableFeatures = nullptr) const
    {
        VkPhysicalDeviceExternalImageFormatInfo externalInfo = {};
        externalInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO;
        externalInfo.handleType = handleType;

        VkPhysicalDeviceImageFormatInfo2 formatInfo = {};
        formatInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
        formatInfo.pNext = &externalInfo;
        formatInfo.format = format;
        formatInfo.type = VK_IMAGE_TYPE_2D;
        formatInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        formatInfo.usage = usage;

        VkExternalImageFormatProperties externalProperties = {};
        externalProperties.sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES;
        VkImageFormatProperties2 properties = {};
        properties.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
        properties.pNext = &externalProperties;

        const VkResult result = vk.getPhysicalDeviceImageFormatProperties2(
            physicalDevice, &formatInfo, &properties);
        if (result != VK_SUCCESS)
            return fail(error, vkResultMessage("Vulkan external image capability query", result));

        const auto features = externalProperties.externalMemoryProperties.externalMemoryFeatures;
        if ((features & required) != required) {
            return fail(error,
                        QStringLiteral("The Qt Vulkan physical device cannot %1 format %2 with "
                                       "the requested Win32 external-memory handle type")
                            .arg(required & VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT
                                     ? QStringLiteral("export") : QStringLiteral("import"),
                                 QString::number(static_cast<int>(format))));
        }
        if (availableFeatures)
            *availableFeatures = features;
        return true;
    }

    FrameSlot *beginFrameSlot(QString *error)
    {
        const int slotIndex = rhi ? rhi->currentFrameSlot() : -1;
        if (slotIndex < 0) {
            fail(error, QStringLiteral("Vulkan texture exchange requires an active QRhi frame"));
            return nullptr;
        }

        if (frameSlots.size() <= std::size_t(slotIndex))
            frameSlots.resize(std::size_t(slotIndex) + 1);
        FrameSlot &slot = frameSlots[std::size_t(slotIndex)];

        if (!slot.commandPool) {
            VkCommandPoolCreateInfo poolInfo = {};
            poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
            poolInfo.queueFamilyIndex = queueFamily;
            const VkResult result = vkCreateCommandPool(device, &poolInfo, nullptr,
                                                        &slot.commandPool);
            if (result != VK_SUCCESS) {
                fail(error, vkResultMessage("vkCreateCommandPool", result));
                return nullptr;
            }
        }

        if (!slot.active) {
            // Qt has waited this frame slot's fence before exposing it again. QRhi-only
            // cache wrappers can now be deleted, and imported native objects whose
            // wrappers were deleted on the previous visit can finally be destroyed.
            slot.retiredRhiTextures.clear();
            finishRetiredImports(&slot);
            for (VkSemaphore semaphore : slot.retiredSemaphores)
                vkDestroySemaphore(device, semaphore, nullptr);
            slot.retiredSemaphores.clear();

            const VkResult result = vkResetCommandPool(device, slot.commandPool, 0);
            if (result != VK_SUCCESS) {
                fail(error, vkResultMessage("vkResetCommandPool", result));
                return nullptr;
            }
            slot.nextCommandBuffer = 0;
            slot.active = true;
            stageReleasedImports(slotIndex, &slot);
        }
        return &slot;
    }

    bool retireRhiTexture(std::unique_ptr<QRhiTexture> *texture, QString *error)
    {
        if (!texture || !*texture)
            return true;

        if (rhi->isRecordingFrame()) {
            FrameSlot *slot = beginFrameSlot(error);
            if (!slot)
                return false;
            slot->retiredRhiTextures.push_back(std::move(*texture));
            return true;
        }

        // Clear/unload outside a frame can synchronously drain QRhi. Delete the wrapper
        // first so finish() also processes the native resource release it enqueues.
        texture->reset();
        if (rhi->finish() != QRhi::FrameOpSuccess) {
            return fail(error,
                        QStringLiteral("QRhi could not retire a Vulkan output cache texture"));
        }
        return true;
    }

    VkCommandBuffer nextNativeCommandBuffer(FrameSlot *slot, QString *error)
    {
        if (slot->nextCommandBuffer == slot->commandBuffers.size()) {
            VkCommandBufferAllocateInfo allocateInfo = {};
            allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocateInfo.commandPool = slot->commandPool;
            allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocateInfo.commandBufferCount = 1;
            VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
            const VkResult result = vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer);
            if (result != VK_SUCCESS) {
                fail(error, vkResultMessage("vkAllocateCommandBuffers", result));
                return VK_NULL_HANDLE;
            }
            slot->commandBuffers.push_back(commandBuffer);
        }
        return slot->commandBuffers[slot->nextCommandBuffer++];
    }

    bool importSemaphore(TEVulkanSemaphore *teSemaphore,
                         VkSemaphore *semaphore,
                         QString *error)
    {
        const VkSemaphoreType type = TEVulkanSemaphoreGetType(teSemaphore);
        VkSemaphoreTypeCreateInfo typeInfo = {};
        typeInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        typeInfo.semaphoreType = type;
        typeInfo.initialValue = 0;

        VkSemaphoreCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        createInfo.pNext = type == VK_SEMAPHORE_TYPE_TIMELINE ? &typeInfo : nullptr;
        VkResult result = vkCreateSemaphore(device, &createInfo, nullptr, semaphore);
        if (result != VK_SUCCESS) {
            const QString suffix = type == VK_SEMAPHORE_TYPE_TIMELINE
                ? QStringLiteral(". TouchEngine returned a timeline semaphore, but Qt's Vulkan "
                                 "device did not enable timeline-semaphore support")
                : QString();
            return fail(error, vkResultMessage("vkCreateSemaphore for TouchEngine import", result)
                                   + suffix);
        }

        VkImportSemaphoreWin32HandleInfoKHR importInfo = {};
        importInfo.sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR;
        importInfo.semaphore = *semaphore;
        importInfo.handleType = TEVulkanSemaphoreGetHandleType(teSemaphore);
        importInfo.handle = TEVulkanSemaphoreGetHandle(teSemaphore);
        result = vk.importSemaphoreWin32Handle(device, &importInfo);
        if (result != VK_SUCCESS) {
            vkDestroySemaphore(device, *semaphore, nullptr);
            *semaphore = VK_NULL_HANDLE;
            return fail(error, vkResultMessage("vkImportSemaphoreWin32HandleKHR", result));
        }
        return true;
    }

    TextureAcquireResult submitAcquire(TEVulkanSemaphore *teSemaphore,
                                       uint64_t waitValue,
                                       NativeImage *native,
                                       VkImageLayout oldLayout,
                                       VkImageLayout newLayout,
                                       VkAccessFlags destinationAccess,
                                       VkPipelineStageFlags destinationStage,
                                       bool transferQueueOwnership,
                                       QString *error)
    {
        VkSemaphore importedSemaphore = VK_NULL_HANDLE;
        if (!importSemaphore(teSemaphore, &importedSemaphore, error))
            return TextureAcquireResult::Error;

        FrameSlot *slot = beginFrameSlot(error);
        if (!slot) {
            vkDestroySemaphore(device, importedSemaphore, nullptr);
            return TextureAcquireResult::Error;
        }
        VkCommandBuffer commandBuffer = nextNativeCommandBuffer(slot, error);
        if (!commandBuffer) {
            vkDestroySemaphore(device, importedSemaphore, nullptr);
            return TextureAcquireResult::Error;
        }

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VkResult result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
        if (result != VK_SUCCESS) {
            vkDestroySemaphore(device, importedSemaphore, nullptr);
            fail(error, vkResultMessage("vkBeginCommandBuffer", result));
            return TextureAcquireResult::Error;
        }

        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = destinationAccess;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        const bool useExternalQueueFamily = ownershipTransfer && transferQueueOwnership;
        barrier.srcQueueFamilyIndex = useExternalQueueFamily ? VK_QUEUE_FAMILY_EXTERNAL
                                                             : VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = useExternalQueueFamily ? queueFamily
                                                             : VK_QUEUE_FAMILY_IGNORED;
        barrier.image = native->image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(commandBuffer,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             destinationStage,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);

        result = vkEndCommandBuffer(commandBuffer);
        if (result != VK_SUCCESS) {
            vkDestroySemaphore(device, importedSemaphore, nullptr);
            fail(error, vkResultMessage("vkEndCommandBuffer", result));
            return TextureAcquireResult::Error;
        }

        // The imported semaphore orders the entire acquire submission, including
        // the layout/ownership barrier itself, after TouchEngine's release.
        const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        const VkSemaphoreType semaphoreType = TEVulkanSemaphoreGetType(teSemaphore);
        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &importedSemaphore;
        submitInfo.pWaitDstStageMask = &waitStage;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        if (semaphoreType == VK_SEMAPHORE_TYPE_BINARY) {
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = &importedSemaphore;
        }

        VkTimelineSemaphoreSubmitInfo timelineInfo = {};
        if (semaphoreType == VK_SEMAPHORE_TYPE_TIMELINE) {
            timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
            timelineInfo.waitSemaphoreValueCount = 1;
            timelineInfo.pWaitSemaphoreValues = &waitValue;
            submitInfo.pNext = &timelineInfo;
        }

        result = vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
        if (result != VK_SUCCESS) {
            vkDestroySemaphore(device, importedSemaphore, nullptr);
            fail(error, vkResultMessage("vkQueueSubmit for TouchEngine acquire", result));
            return TextureAcquireResult::Error;
        }

        slot->retiredSemaphores.push_back(importedSemaphore);
        native->layout = newLayout;
        native->wrapper->setNativeLayout(int(newLayout));
        return TextureAcquireResult::Acquired;
    }

    TextureAcquireResult acquireFromTouchEngine(TEInstance *instance,
                                                TETexture *texture,
                                                NativeImage *native,
                                                QString *error)
    {
        if (!TEInstanceHasVulkanTextureTransfer(instance, texture)) {
            // A value may be visible before its ownership transfer is pending. Without
            // TouchEngine's semaphore and layouts Qt cannot safely touch the image, but
            // this is normal asynchronous back-pressure rather than a session error.
            if (error)
                error->clear();
            return TextureAcquireResult::NotReady;
        }

        VkImageLayout oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageLayout newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        uint64_t waitValue = 0;
        TouchObject<TESemaphore> transferSemaphore;
        const TEResult teResult = TEInstanceGetVulkanTextureTransfer(
            instance, texture, &oldLayout, &newLayout, transferSemaphore.take(), &waitValue);
        if (teResult != TEResultSuccess) {
            fail(error, teResultMessage("TEInstanceGetVulkanTextureTransfer", teResult));
            return TextureAcquireResult::Error;
        }
        if (!transferSemaphore || TESemaphoreGetType(transferSemaphore) != TESemaphoreTypeVulkan) {
            fail(error,
                 QStringLiteral("TouchEngine returned a non-Vulkan semaphore for a Vulkan "
                                "texture transfer"));
            return TextureAcquireResult::Error;
        }

        return submitAcquire(reinterpret_cast<TEVulkanSemaphore *>(transferSemaphore.get()),
                             waitValue,
                             native,
                             oldLayout,
                             newLayout,
                             VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
                             VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                             true,
                             error);
    }

    TextureAcquireResult acquireDiscardedInputFromTouchEngine(TEInstance *instance,
                                                               TETexture *texture,
                                                               NativeImage *native,
                                                               QString *error)
    {
        if (!TEInstanceHasTextureTransfer(instance, texture)) {
            // TouchEngine may discard a transfer it never consumed. EndUse still proves
            // that the image is no longer in use, and its previous contents are irrelevant
            // because the next host operation overwrites the complete image.
            native->layout = VK_IMAGE_LAYOUT_UNDEFINED;
            native->wrapper->setNativeLayout(int(VK_IMAGE_LAYOUT_UNDEFINED));
            return TextureAcquireResult::Acquired;
        }

        uint64_t waitValue = 0;
        TouchObject<TESemaphore> transferSemaphore;
        const TEResult teResult = TEInstanceGetTextureTransfer(
            instance, texture, transferSemaphore.take(), &waitValue);
        if (teResult == TEResultNoMatchingEntity) {
            // A disposable input transfer may be discarded after HasTextureTransfer()
            // and before GetTextureTransfer(). EndUse still makes the image safe to
            // overwrite; no previous contents or layout need to be preserved.
            native->layout = VK_IMAGE_LAYOUT_UNDEFINED;
            native->wrapper->setNativeLayout(int(VK_IMAGE_LAYOUT_UNDEFINED));
            return TextureAcquireResult::Acquired;
        }
        if (teResult != TEResultSuccess) {
            fail(error, teResultMessage("TEInstanceGetTextureTransfer", teResult));
            return TextureAcquireResult::Error;
        }
        if (!transferSemaphore || TESemaphoreGetType(transferSemaphore) != TESemaphoreTypeVulkan) {
            fail(error,
                 QStringLiteral("TouchEngine returned a non-Vulkan semaphore for a returned "
                                "Vulkan input texture"));
            return TextureAcquireResult::Error;
        }

        // The previous input contents are deliberately discarded before Qt overwrites the
        // pool image. TouchEngine's generic transfer supplies synchronization but no layouts;
        // wait for its release and reinitialize from UNDEFINED without a queue-family acquire.
        return submitAcquire(reinterpret_cast<TEVulkanSemaphore *>(transferSemaphore.get()),
                             waitValue,
                             native,
                             VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             VK_ACCESS_TRANSFER_WRITE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             false,
                             error);
    }

    bool recordReleaseBarrier(NativeImage *native,
                              VkImageLayout oldLayout,
                              VkImageLayout newLayout,
                              VkAccessFlags sourceAccess,
                              VkPipelineStageFlags sourceStage,
                              QRhiCommandBuffer *commandBuffer,
                              QString *error)
    {
        commandBuffer->beginExternal();
        const auto *handles = static_cast<const QRhiVulkanCommandBufferNativeHandles *>(
            commandBuffer->nativeHandles());
        if (!handles || !handles->commandBuffer) {
            commandBuffer->endExternal();
            return fail(error,
                        QStringLiteral("Qt did not expose its Vulkan command buffer after "
                                       "QRhiCommandBuffer::beginExternal()"));
        }

        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask = sourceAccess;
        barrier.dstAccessMask = 0;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = ownershipTransfer ? queueFamily : VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = ownershipTransfer ? VK_QUEUE_FAMILY_EXTERNAL
                                                       : VK_QUEUE_FAMILY_IGNORED;
        barrier.image = native->image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(handles->commandBuffer,
                             sourceStage,
                             VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);
        commandBuffer->endExternal();

        native->layout = newLayout;
        native->wrapper->setNativeLayout(int(newLayout));
        return true;
    }

    bool createHostSignal(HostSignal *signal, QString *error)
    {
        // Even frames with no TE->Qt acquire need a slot-owned retirement list. The
        // semaphore is still referenced by Qt's submit after afterFrameEnd() returns.
        if (!beginFrameSlot(error))
            return false;
        signal->frameSlot = rhi->currentFrameSlot();

        VkExportSemaphoreCreateInfo exportInfo = {};
        exportInfo.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO;
        exportInfo.handleTypes = kSemaphoreHandleType;
        VkSemaphoreCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        createInfo.pNext = &exportInfo;

        VkResult result = vkCreateSemaphore(device, &createInfo, nullptr, &signal->native);
        if (result != VK_SUCCESS)
            return fail(error, vkResultMessage("vkCreateSemaphore", result));

        VkSemaphoreGetWin32HandleInfoKHR handleInfo = {};
        handleInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR;
        handleInfo.semaphore = signal->native;
        handleInfo.handleType = kSemaphoreHandleType;
        HANDLE handle = nullptr;
        result = vk.getSemaphoreWin32Handle(device, &handleInfo, &handle);
        if (result != VK_SUCCESS) {
            vkDestroySemaphore(device, signal->native, nullptr);
            signal->native = VK_NULL_HANDLE;
            return fail(error, vkResultMessage("vkGetSemaphoreWin32HandleKHR", result));
        }

        signal->textureTransfer.take(TEVulkanSemaphoreCreate(VK_SEMAPHORE_TYPE_BINARY,
                                                             handle,
                                                             kSemaphoreHandleType,
                                                             nullptr,
                                                             nullptr));
        CloseHandle(handle);
        if (!signal->textureTransfer) {
            vkDestroySemaphore(device, signal->native, nullptr);
            signal->native = VK_NULL_HANDLE;
            return fail(error, QStringLiteral("TEVulkanSemaphoreCreate returned null"));
        }
        return true;
    }

    void attachSignal(VkSemaphore semaphore)
    {
        frameSignals.push_back(semaphore);
        QueueSignalRegistry::add(rhi, semaphore);
    }

    void retireSignal(HostSignal *signal)
    {
        if (!signal->native)
            return;
        if (signal->frameSlot >= 0
            && frameSlots.size() > std::size_t(signal->frameSlot)) {
            frameSlots[std::size_t(signal->frameSlot)].retiredSemaphores.push_back(signal->native);
        } else {
            // This only occurs during a failed/non-frame path, where no queue submission can
            // still reference the semaphore.
            vkDestroySemaphore(device, signal->native, nullptr);
        }
        signal->native = VK_NULL_HANDLE;
        signal->textureTransfer.reset();
    }

    bool allocateExportedInput(InputEntry *entry,
                               VkFormat format,
                               QRhiTexture::Format rhiFormat,
                               QRhiTexture::Flags rhiFlags,
                               const QSize &size,
                               QString *error)
    {
        const VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
            | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        VkExternalMemoryFeatureFlags externalFeatures = 0;
        if (!supportsExternalImage(format, usage, kMemoryHandleType,
                                   VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT, error,
                                   &externalFeatures)) {
            return false;
        }

        VkExternalMemoryImageCreateInfo externalInfo = {};
        externalInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
        externalInfo.handleTypes = kMemoryHandleType;
        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.pNext = &externalInfo;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = format;
        imageInfo.extent = {uint32_t(size.width()), uint32_t(size.height()), 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = usage;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkResult result = vkCreateImage(device, &imageInfo, nullptr, &entry->native.image);
        if (result != VK_SUCCESS)
            return fail(error, vkResultMessage("vkCreateImage for TouchEngine input", result));

        VkMemoryRequirements requirements = {};
        vkGetImageMemoryRequirements(device, entry->native.image, &requirements);
        const uint32_t typeIndex = memoryType(requirements.memoryTypeBits,
                                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (typeIndex == UINT32_MAX)
            return fail(error, QStringLiteral("No compatible Vulkan memory type for shared input"));

        VkExportMemoryAllocateInfo exportInfo = {};
        exportInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
        exportInfo.handleTypes = kMemoryHandleType;
        VkMemoryDedicatedAllocateInfo dedicatedInfo = {};
        dedicatedInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
        dedicatedInfo.pNext = &exportInfo;
        dedicatedInfo.image = entry->native.image;
        VkMemoryAllocateInfo allocateInfo = {};
        allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocateInfo.pNext = externalFeatures & VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT
            ? static_cast<const void *>(&dedicatedInfo)
            : static_cast<const void *>(&exportInfo);
        allocateInfo.allocationSize = requirements.size;
        allocateInfo.memoryTypeIndex = typeIndex;
        result = vkAllocateMemory(device, &allocateInfo, nullptr, &entry->native.memory);
        if (result != VK_SUCCESS)
            return fail(error, vkResultMessage("vkAllocateMemory for TouchEngine input", result));
        result = vkBindImageMemory(device, entry->native.image, entry->native.memory, 0);
        if (result != VK_SUCCESS)
            return fail(error, vkResultMessage("vkBindImageMemory for TouchEngine input", result));

        entry->native.wrapper.reset(rhi->newTexture(rhiFormat, size, 1, rhiFlags));
        const QRhiTexture::NativeTexture nativeTexture = {
            quint64(entry->native.image), int(VK_IMAGE_LAYOUT_UNDEFINED)};
        if (!entry->native.wrapper || !entry->native.wrapper->createFrom(nativeTexture)) {
            return fail(error,
                        QStringLiteral("QRhiTexture::createFrom failed for exported Vulkan input"));
        }

        VkMemoryGetWin32HandleInfoKHR handleInfo = {};
        handleInfo.sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR;
        handleInfo.memory = entry->native.memory;
        handleInfo.handleType = kMemoryHandleType;
        HANDLE handle = nullptr;
        result = vk.getMemoryWin32Handle(device, &handleInfo, &handle);
        if (result != VK_SUCCESS)
            return fail(error, vkResultMessage("vkGetMemoryWin32HandleKHR", result));

        entry->texture.take(TEVulkanTextureCreate(handle,
                                                  kMemoryHandleType,
                                                  format,
                                                  size.width(),
                                                  size.height(),
                                                  TETextureOriginTopLeft,
                                                  kTEVkComponentMappingIdentity,
                                                  inputTextureCallback,
                                                  &entry->use));
        CloseHandle(handle);
        if (!entry->texture)
            return fail(error, QStringLiteral("TEVulkanTextureCreate returned null"));

        entry->native.size = size;
        entry->native.vkFormat = format;
        entry->native.rhiFormat = rhiFormat;
        entry->native.rhiFlags = rhiFlags;
        entry->native.layout = VK_IMAGE_LAYOUT_UNDEFINED;
        return true;
    }

    ImportedOutput *importOutput(TEVulkanTexture *source,
                                 const QString &link,
                                 QRhiTexture::Format rhiFormat,
                                 QRhiTexture::Flags rhiFlags,
                                 QString *error)
    {
        const HANDLE handle = TEVulkanTextureGetHandle(source);
        const VkFormat format = TEVulkanTextureGetFormat(source);
        const QSize size(TEVulkanTextureGetWidth(source), TEVulkanTextureGetHeight(source));
        const auto handleType = TEVulkanTextureGetHandleType(source);
        ImportedOutput *reusable = nullptr;
        for (const auto &entry : importedOutputs) {
            const bool released = importWasReleased(entry.get());
            if (!released && entry->instanceGeneration == instanceGeneration
                && entry->sourceObject == source && entry->sourceHandle == handle
                && entry->native.vkFormat == format && entry->native.size == size
                && entry->handleType == handleType) {
                entry->link = link;
                entry->purgeRequested = false;
                if (entry->nativeRetirementPending) {
                    // Normal frame-slot back-pressure: Core keeps this output
                    // in its retry set. Do not publish a session error for a
                    // resource that will become available on a later frame.
                    if (error)
                        error->clear();
                    return nullptr;
                }
                if (entry->native.wrapper)
                    return entry.get();
                reusable = entry.get();
                break;
            }
        }

        const VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT
            | VK_IMAGE_USAGE_SAMPLED_BIT;
        if (!supportsExternalImage(format, usage, handleType,
                                   VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT, error)) {
            return nullptr;
        }

        std::unique_ptr<ImportedOutput> newImported;
        ImportedOutput *imported = reusable;
        if (!imported) {
            newImported = std::make_unique<ImportedOutput>();
            imported = newImported.get();
            imported->sourceHandle = handle;
            imported->sourceObject = source;
            imported->instanceGeneration = instanceGeneration;
            imported->handleType = handleType;
        }
        imported->link = link;
        imported->native.size = size;
        imported->native.vkFormat = format;
        imported->native.rhiFormat = rhiFormat;
        imported->native.rhiFlags = rhiFlags;

        VkExternalMemoryImageCreateInfo externalInfo = {};
        externalInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
        externalInfo.handleTypes = handleType;
        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.pNext = &externalInfo;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = format;
        imageInfo.extent = {uint32_t(size.width()), uint32_t(size.height()), 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = usage;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        // Vulkan requires UNDEFINED here. The live shared layout comes from the TE transfer;
        // it must never be discarded with an UNDEFINED -> ... transition.
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkResult result = vkCreateImage(device, &imageInfo, nullptr, &imported->native.image);
        if (result != VK_SUCCESS) {
            fail(error, vkResultMessage("vkCreateImage for TouchEngine output import", result));
            return nullptr;
        }

        VkMemoryRequirements requirements = {};
        vkGetImageMemoryRequirements(device, imported->native.image, &requirements);
        uint32_t compatibleMemoryTypes = requirements.memoryTypeBits;
        if (!isOpaqueWin32MemoryHandle(handleType)) {
            VkMemoryWin32HandlePropertiesKHR handleProperties = {};
            handleProperties.sType = VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR;
            result = vk.getMemoryWin32HandleProperties(device, handleType, handle,
                                                        &handleProperties);
            if (result != VK_SUCCESS) {
                fail(error, vkResultMessage("vkGetMemoryWin32HandlePropertiesKHR", result));
                discardNative(&imported->native);
                return nullptr;
            }
            compatibleMemoryTypes &= handleProperties.memoryTypeBits;
        }

        // vkGetMemoryWin32HandlePropertiesKHR is explicitly invalid for opaque handles
        // (VUID-vkGetMemoryWin32HandlePropertiesKHR-handleType-00666). TouchEngine's
        // opaque Vulkan textures originate on the physical device selected by the UUID/LUID
        // passed to TEVulkanContextCreate. Recreating the same external image on that device
        // therefore supplies the compatible memory-type mask for the import.
        const uint32_t typeIndex = memoryType(compatibleMemoryTypes,
                                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (typeIndex == UINT32_MAX) {
            fail(error, QStringLiteral("No compatible memory type for TouchEngine Vulkan output"));
            discardNative(&imported->native);
            return nullptr;
        }

        VkImportMemoryWin32HandleInfoKHR importInfo = {};
        importInfo.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR;
        importInfo.handleType = handleType;
        importInfo.handle = handle; // TouchEngine owns this handle; never CloseHandle it.
        VkMemoryDedicatedAllocateInfo dedicatedInfo = {};
        dedicatedInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
        dedicatedInfo.pNext = &importInfo;
        dedicatedInfo.image = imported->native.image;
        VkMemoryAllocateInfo allocateInfo = {};
        allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocateInfo.pNext = &dedicatedInfo;
        allocateInfo.allocationSize = requirements.size;
        allocateInfo.memoryTypeIndex = typeIndex;
        result = vkAllocateMemory(device, &allocateInfo, nullptr, &imported->native.memory);
        if (result != VK_SUCCESS) {
            fail(error, vkResultMessage("vkAllocateMemory for TouchEngine output import", result));
            discardNative(&imported->native);
            return nullptr;
        }
        result = vkBindImageMemory(device, imported->native.image, imported->native.memory, 0);
        if (result != VK_SUCCESS) {
            fail(error, vkResultMessage("vkBindImageMemory for TouchEngine output import", result));
            discardNative(&imported->native);
            return nullptr;
        }

        imported->native.wrapper.reset(
            rhi->newTexture(rhiFormat, size, 1,
                            rhiFlags | QRhiTexture::UsedAsTransferSource));
        const QRhiTexture::NativeTexture nativeTexture = {
            quint64(imported->native.image), int(VK_IMAGE_LAYOUT_UNDEFINED)};
        if (!imported->native.wrapper
            || !imported->native.wrapper->createFrom(nativeTexture)) {
            fail(error,
                 QStringLiteral("QRhiTexture::createFrom failed for imported TouchEngine output"));
            discardNative(&imported->native);
            return nullptr;
        }

        if (newImported) {
            auto *lifetime = new OutputReleaseState;
            lifetime->generation = nextOutputGeneration.fetch_add(1, std::memory_order_relaxed);
            const TEResult callbackResult = TEVulkanTextureSetCallback(
                source, outputTextureCallback, lifetime);
            if (callbackResult != TEResultSuccess) {
                fail(error, teResultMessage("TEVulkanTextureSetCallback", callbackResult));
                // No callback was installed, so release both initially-created references.
                releaseOutputState(lifetime);
                releaseOutputState(lifetime);
                discardNative(&imported->native);
                return nullptr;
            }
            imported->lifetime = lifetime;
            importedOutputs.push_back(std::move(newImported));
        }
        return imported;
    }

    InputLink *inputLink(const QString &link)
    {
        auto it = std::find_if(inputLinks.begin(), inputLinks.end(),
                               [&link](const InputLink &candidate) {
                                   return candidate.link == link;
                               });
        if (it != inputLinks.end())
            return &*it;
        inputLinks.push_back(InputLink{link, {}});
        return &inputLinks.back();
    }

    void collectRetiredInputs()
    {
        const auto firstLive = std::remove_if(
            retiredInputs.begin(), retiredInputs.end(), [](const auto &entry) {
                return entry->use.released.load(std::memory_order_acquire)
                    && entry->use.callbacks.load(std::memory_order_acquire) == 0;
            });
        retiredInputs.erase(firstLive, retiredInputs.end());
    }

    OutputRecord *outputRecord(const QString &link)
    {
        auto it = std::find_if(outputs.begin(), outputs.end(),
                               [&link](const OutputRecord &candidate) {
                                   return candidate.link == link;
                               });
        if (it != outputs.end())
            return &*it;
        outputs.push_back(OutputRecord{});
        outputs.back().link = link;
        return &outputs.back();
    }

    bool ensureOutputCache(OutputRecord *record,
                           QRhiTexture::Format format,
                           QRhiTexture::Flags flags,
                           const QSize &size,
                           QString *error)
    {
        if (record->cache && record->format == format && record->flags == flags
            && record->size == size) {
            return true;
        }

        std::unique_ptr<QRhiTexture> replacement(
            rhi->newTexture(format, size, 1, flags | QRhiTexture::UsedAsTransferSource));
        if (!replacement || !replacement->create())
            return fail(error, QStringLiteral("Failed to create Qt-owned Vulkan output cache"));

        if (record->cache && !retireRhiTexture(&record->cache, error))
            return false;
        record->cache = std::move(replacement);
        record->format = format;
        record->flags = flags;
        record->size = size;
        return true;
    }

    void destroyNative(NativeImage *native)
    {
        if (native->image)
            vkDestroyImage(device, native->image, nullptr);
        if (native->memory)
            vkFreeMemory(device, native->memory, nullptr);
        native->image = VK_NULL_HANDLE;
        native->memory = VK_NULL_HANDLE;
    }

    bool retireInputNative(NativeImage *native, QString *error)
    {
        if (!native || (!native->wrapper && !native->image && !native->memory))
            return true;
        if (!rhi || !device)
            return fail(error, QStringLiteral("Cannot retire Vulkan input without a live QRhi"));

        if (rhi->isRecordingFrame()) {
            FrameSlot *slot = beginFrameSlot(error);
            if (!slot)
                return false;

            // Releasing createFrom() queues Qt's VkImageView for deferred
            // destruction. Keep the allocation alive for one complete reuse
            // of this frame slot before destroying the VkImage underneath it.
            native->wrapper.reset();
            slot->importsAwaitingNativeDestroy.push_back(
                RetiredImportNative{native->image, native->memory, nullptr});
            native->image = VK_NULL_HANDLE;
            native->memory = VK_NULL_HANDLE;
            native->layout = VK_IMAGE_LAYOUT_UNDEFINED;
            return true;
        }

        native->wrapper.reset();
        if (rhi->finish() != QRhi::FrameOpSuccess) {
            return fail(error,
                        QStringLiteral("QRhi could not retire a Vulkan input texture"));
        }
        destroyNative(native);
        native->layout = VK_IMAGE_LAYOUT_UNDEFINED;
        return true;
    }

    bool resetInstance(QString *error)
    {
        if (error)
            error->clear();

        // These values are negotiated for one TEInstance and must never be
        // consulted while its replacement is configuring.
        configured = false;
        configuredInstance = nullptr;
        supportedFormats.clear();
        ownershipTransfer = false;
        inputReleaseLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (++instanceGeneration == 0)
            instanceGeneration = 1;

        QueueSignalRegistry::remove(rhi, frameSignals);
        frameSignals.clear();

        // The previous instance has already been released, so no TouchEngine
        // consumer can still wait on these publication records. The Vulkan
        // semaphores remain frame-slot retired because an earlier Qt submit may
        // still reference them.
        for (auto &pending : pendingInputs)
            retireSignal(&pending.signal);
        pendingInputs.clear();
        for (auto &pending : pendingOutputs) {
            retireSignal(&pending.signal);
            pending.texture.reset();
        }
        pendingOutputs.clear();
        requestImportPurge(QString{});

        // Detach every callback before deleting its embedded InputUseState.
        // TEInstance release above is the callback fence; detaching also makes
        // releasing our final texture reference independent of the entry.
        for (auto &link : inputLinks) {
            for (auto &entry : link.entries) {
                if (!entry->texture)
                    continue;
                const TEResult result = TEVulkanTextureSetCallback(entry->texture,
                                                                   nullptr,
                                                                   nullptr);
                if (result != TEResultSuccess) {
                    return fail(error,
                                teResultMessage("TEVulkanTextureSetCallback(reset)", result));
                }
            }
        }

        for (auto &link : inputLinks)
            for (auto &entry : link.entries)
                entry->texture.reset();

        bool retiredAllInputs = true;
        QStringList retirementFailures;
        for (auto &link : inputLinks) {
            for (auto &entry : link.entries) {
                QString retirementError;
                if (!retireInputNative(&entry->native, &retirementError)) {
                    retiredAllInputs = false;
                    retirementFailures.push_back(retirementError);
                }
            }
        }
        if (!retiredAllInputs)
            return fail(error, retirementFailures.join(QLatin1Char('\n')));
        inputLinks.clear();
        // The previous TEInstance release is the callback fence. Retired input
        // entries contain no live native resources and can now be discarded even
        // if a broken runtime omitted their final Release notification.
        retiredInputs.clear();

        // Old output imports are callback-tombstoned until their final Release
        // and frame-slot retirement. They are marked purge-only, so a new
        // TEInstance can never reuse them even if allocator addresses match.
        if (rhi && rhi->isRecordingFrame()) {
            QString slotError;
            if (FrameSlot *slot = beginFrameSlot(&slotError))
                stageReleasedImports(rhi->currentFrameSlot(), slot);
            else
                return fail(error, slotError);
        } else {
            QString purgeError;
            if (!purgeRequestedImportsOutsideFrame(&purgeError))
                return fail(error, purgeError);
        }
        return true;
    }

    void shutdown()
    {
        if (!device)
            return;

        QueueSignalRegistry::remove(rhi, frameSignals);
        std::vector<VkSemaphore> pendingNativeSemaphores;
        for (auto &pending : pendingInputs) {
            if (pending.signal.native)
                pendingNativeSemaphores.push_back(pending.signal.native);
            pending.signal.native = VK_NULL_HANDLE;
        }
        for (auto &pending : pendingOutputs) {
            if (pending.signal.native)
                pendingNativeSemaphores.push_back(pending.signal.native);
            pending.signal.native = VK_NULL_HANDLE;
        }

        for (auto &link : inputLinks) {
            for (auto &entry : link.entries) {
                if (entry->texture)
                    TEVulkanTextureSetCallback(entry->texture, nullptr, nullptr);
                entry->native.wrapper.reset();
            }
        }
        for (auto &imported : importedOutputs) {
            imported->native.wrapper.reset();
        }
        for (auto &record : outputs)
            record.cache.reset();
        for (auto &slot : frameSlots)
            slot.retiredRhiTextures.clear();

        const bool canFinish = rhi && !rhi->isRecordingFrame();
        const bool gpuComplete = canFinish && rhi->finish() == QRhi::FrameOpSuccess;
        if (gpuComplete) {
            for (VkSemaphore semaphore : pendingNativeSemaphores)
                vkDestroySemaphore(device, semaphore, nullptr);
            for (auto &link : inputLinks)
                for (auto &entry : link.entries)
                    destroyNative(&entry->native);
            for (auto &imported : importedOutputs)
                destroyNative(&imported->native);
            for (auto &slot : frameSlots) {
                for (const RetiredImportNative &retired : slot.importsAwaitingNativeDestroy) {
                    if (retired.image)
                        vkDestroyImage(device, retired.image, nullptr);
                    if (retired.memory)
                        vkFreeMemory(device, retired.memory, nullptr);
                }
                for (VkSemaphore semaphore : slot.retiredSemaphores)
                    vkDestroySemaphore(device, semaphore, nullptr);
                if (slot.commandPool)
                    vkDestroyCommandPool(device, slot.commandPool, nullptr);
            }
        } else {
            // Destruction can happen during scene-graph invalidation. Invoking Vulkan destroys
            // while Qt may still have queued image-view/command-buffer releases is a crash risk;
            // the device owner will reclaim these handles when the QRhi device is destroyed.
            qWarning() << "Dsqt.TouchEngine Vulkan backend could not quiesce QRhi during teardown;"
                          " native interop handles are intentionally left to device destruction";
        }

        // Keep TE's wrappers alive until all Qt submissions and imported image views have
        // retired. This prevents either process from dropping the shared allocation early.
        for (auto &pending : pendingInputs)
            pending.signal.textureTransfer.reset();
        for (auto &pending : pendingOutputs) {
            pending.signal.textureTransfer.reset();
            pending.texture.reset();
        }
        for (auto &link : inputLinks)
            for (auto &entry : link.entries)
                entry->texture.reset();
        retiredInputs.clear();
        for (auto &imported : importedOutputs)
            releaseImportedState(imported.get());

        context.reset();
        device = VK_NULL_HANDLE;
        physicalDevice = VK_NULL_HANDLE;
        queue = VK_NULL_HANDLE;
        rhi = nullptr;
    }
};

VulkanTouchEngineBackend::VulkanTouchEngineBackend()
    : m_impl(std::make_unique<Impl>())
{
}

VulkanTouchEngineBackend::~VulkanTouchEngineBackend() = default;

DsTouchEngineTypes::GraphicsApi VulkanTouchEngineBackend::graphicsApi() const noexcept
{
    return DsTouchEngineTypes::GraphicsApi::Vulkan;
}

bool VulkanTouchEngineBackend::initialize(QRhi *rhi,
                                          QRhiCommandBuffer *commandBuffer,
                                          QString *error)
{
    if (error)
        error->clear();
    if (!rhi || rhi->backend() != QRhi::Vulkan)
        return fail(error, QStringLiteral("Vulkan backend requires a Vulkan QRhi"));
    if (!commandBuffer)
        return fail(error, QStringLiteral("Vulkan backend requires an active QRhi command buffer"));

    const auto *native = static_cast<const QRhiVulkanNativeHandles *>(rhi->nativeHandles());
    if (!native || !native->physDev || !native->dev || !native->gfxQueue || !native->inst) {
        return fail(error, QStringLiteral("Qt did not expose complete Vulkan QRhi native handles"));
    }

    m_impl->rhi = rhi;
    m_impl->physicalDevice = native->physDev;
    m_impl->device = native->dev;
    m_impl->queue = native->gfxQueue;
    m_impl->queueFamily = native->gfxQueueFamilyIdx;
    m_impl->qtInstance = native->inst;
    if (!m_impl->loadDispatch(error) || !m_impl->verifySemaphoreExport(error))
        return false;

    VkPhysicalDeviceIDProperties identity = {};
    identity.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
    VkPhysicalDeviceProperties2 properties = {};
    properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties.pNext = &identity;
    m_impl->vk.getPhysicalDeviceProperties2(m_impl->physicalDevice, &properties);

    TEVulkanContext *context = nullptr;
    const TEResult result = TEVulkanContextCreate(identity.deviceUUID,
                                                   identity.driverUUID,
                                                   identity.deviceLUID,
                                                   identity.deviceLUIDValid == VK_TRUE,
                                                   TETextureOriginTopLeft,
                                                   &context);
    if (result != TEResultSuccess)
        return fail(error, teResultMessage("TEVulkanContextCreate", result));
    m_impl->context.take(context);
    m_impl->initialized = true;
    return true;
}

TEGraphicsContext *VulkanTouchEngineBackend::graphicsContext() const noexcept
{
    return reinterpret_cast<TEGraphicsContext *>(m_impl->context.get());
}

bool VulkanTouchEngineBackend::resetInstance(QString *error)
{
    return m_impl->resetInstance(error);
}

bool VulkanTouchEngineBackend::configureInstance(TEInstance *instance, QString *error)
{
    if (error)
        error->clear();
    if (!m_impl->initialized || !instance)
        return fail(error, QStringLiteral("Vulkan backend is not initialized"));

    int32_t count = 0;
    TEResult result = TEInstanceGetSupportedVkFormats(instance, nullptr, &count);
    if (result != TEResultSuccess && result != TEResultInsufficientMemory)
        return fail(error, teResultMessage("TEInstanceGetSupportedVkFormats", result));
    if (count <= 0)
        return fail(error, QStringLiteral("TouchEngine reported no supported Vulkan formats"));

    m_impl->supportedFormats.resize(std::size_t(count));
    result = TEInstanceGetSupportedVkFormats(instance, m_impl->supportedFormats.data(), &count);
    if (result != TEResultSuccess)
        return fail(error, teResultMessage("TEInstanceGetSupportedVkFormats", result));
    m_impl->supportedFormats.resize(std::size_t(count));

    // Outputs are copied into Qt-owned textures, so TRANSFER_SRC is the precise layout
    // consumed by this backend.
    result = TEInstanceSetVulkanOutputAcquireImageLayout(instance,
                                                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    if (result != TEResultSuccess) {
        return fail(error,
                    teResultMessage("TEInstanceSetVulkanOutputAcquireImageLayout", result));
    }

    m_impl->ownershipTransfer = TEInstanceDoesVulkanTextureOwnershipTransfer(instance);
    m_impl->inputReleaseLayout = TEInstanceGetVulkanInputReleaseImageLayout(instance);
    m_impl->configuredInstance = instance;
    m_impl->configured = true;
    return true;
}

bool VulkanTouchEngineBackend::prepareTextureInput(TEInstance *instance,
                                                   const TextureInputSource &source,
                                                   QRhiCommandBuffer *commandBuffer,
                                                   QString *error)
{
    if (error)
        error->clear();
    if (!m_impl->configured || instance != m_impl->configuredInstance)
        return fail(error, QStringLiteral("TouchEngine Vulkan instance is not configured"));
    if (!commandBuffer || !source.texture || source.link.isEmpty())
        return fail(error, QStringLiteral("Texture input requires a link, texture, and command buffer"));
    if (!source.texture->flags().testFlag(QRhiTexture::UsedAsTransferSource)) {
        return fail(error,
                    QStringLiteral("The Qt input texture for '%1' was not created with "
                                   "QRhiTexture::UsedAsTransferSource")
                        .arg(source.link));
    }
    const auto duplicate = std::find_if(m_impl->pendingInputs.begin(), m_impl->pendingInputs.end(),
                                        [&source](const Impl::PendingInput &pending) {
                                            return pending.link == source.link;
                                        });
    if (duplicate != m_impl->pendingInputs.end())
        return fail(error, QStringLiteral("Texture input '%1' was prepared twice in one frame")
                               .arg(source.link));

    const QSize sourceSize = source.pixelSize.isValid() ? source.pixelSize
                                                        : source.texture->pixelSize();
    QRectF normalized = source.normalizedSourceRect.normalized()
                            .intersected(QRectF(0.0, 0.0, 1.0, 1.0));
    if (!sourceSize.isValid() || normalized.isEmpty())
        return fail(error, QStringLiteral("Texture input '%1' has an empty source rectangle")
                               .arg(source.link));
    const int left = std::clamp(qRound(normalized.left() * sourceSize.width()),
                                0, sourceSize.width());
    const int top = std::clamp(qRound(normalized.top() * sourceSize.height()),
                               0, sourceSize.height());
    const int right = std::clamp(qRound(normalized.right() * sourceSize.width()),
                                 left, sourceSize.width());
    const int bottom = std::clamp(qRound(normalized.bottom() * sourceSize.height()),
                                  top, sourceSize.height());
    const QSize copySize(right - left, bottom - top);
    if (!copySize.isValid())
        return fail(error, QStringLiteral("Texture input '%1' has a zero-sized copy region")
                               .arg(source.link));

    VkFormat vkFormat = VK_FORMAT_UNDEFINED;
    if (!rhiToVkFormat(source.texture, &vkFormat)) {
        return fail(error, QStringLiteral("Qt texture format %1 is not supported for Vulkan input")
                               .arg(int(source.texture->format())));
    }
    if (std::find(m_impl->supportedFormats.begin(), m_impl->supportedFormats.end(), vkFormat)
        == m_impl->supportedFormats.end()) {
        return fail(error, QStringLiteral("TouchEngine does not support Vulkan format %1 for '%2'")
                               .arg(int(vkFormat)).arg(source.link));
    }

    m_impl->collectRetiredInputs();
    Impl::InputLink *link = m_impl->inputLink(source.link);
    const auto retireReservedEntry = [&](Impl::InputEntry *candidate) {
        if (!candidate)
            return false;

        if (candidate->use.needsAcquire.load(std::memory_order_acquire)) {
            QString acquireError;
            const TextureAcquireResult acquireResult =
                m_impl->acquireDiscardedInputFromTouchEngine(
                    instance,
                    reinterpret_cast<TETexture *>(candidate->texture.get()),
                    &candidate->native,
                    &acquireError);
            if (acquireResult == TextureAcquireResult::Acquired) {
                candidate->use.needsAcquire.store(false, std::memory_order_release);
            } else {
                if (acquireResult == TextureAcquireResult::NotReady) {
                    candidate->use.available.store(true, std::memory_order_release);
                } else {
                    qWarning().noquote()
                        << "Dsqt.TouchEngine: could not reclaim an old Vulkan input pool entry:"
                        << acquireError;
                }
                return false;
            }
        }

        QString retirementError;
        if (!m_impl->retireInputNative(&candidate->native, &retirementError)) {
            qWarning().noquote()
                << "Dsqt.TouchEngine: could not retire an old Vulkan input pool entry:"
                << retirementError;
            return false;
        }

        const auto oldEntry = std::find_if(
            link->entries.begin(), link->entries.end(), [candidate](const auto &entry) {
                return entry.get() == candidate;
            });
        if (oldEntry == link->entries.end())
            return false;

        m_impl->retiredInputs.push_back(std::move(*oldEntry));
        link->entries.erase(oldEntry);
        // The TE object may remain retained by the current link until a
        // replacement LinkSet. Its callback state remains in retiredInputs
        // until the final Release event.
        m_impl->retiredInputs.back()->texture.reset();
        return true;
    };

    Impl::InputEntry *entry = nullptr;
    for (const auto &candidate : link->entries) {
        if (candidate->native.size != copySize || candidate->native.vkFormat != vkFormat)
            continue;
        bool expected = true;
        if (!candidate->use.available.compare_exchange_strong(expected, false,
                                                               std::memory_order_acq_rel)) {
            continue;
        }

        if (candidate->use.needsAcquire.load(std::memory_order_acquire)) {
            const TextureAcquireResult acquireResult =
                m_impl->acquireDiscardedInputFromTouchEngine(
                instance,
                reinterpret_cast<TETexture *>(candidate->texture.get()),
                &candidate->native,
                error);
            if (acquireResult == TextureAcquireResult::Error) {
                // GetVulkanTextureTransfer may already have consumed the only pending
                // transfer before a later import or queue operation failed. Quarantine
                // this entry until instance reset instead of risking unsynchronized use.
                return false;
            }
            if (acquireResult == TextureAcquireResult::NotReady) {
                // Do not touch an image whose return transfer is not pending. Another
                // pool entry may still accept this frame; otherwise the previous link
                // value remains valid and a later engine frame retries this entry.
                candidate->use.available.store(true, std::memory_order_release);
                continue;
            }
            candidate->use.needsAcquire.store(false, std::memory_order_release);
        }

        entry = candidate.get();
        break;
    }

    if (!entry && link->entries.size() > kMaximumInputPoolSize) {
        // A prior replacement may have temporarily overflowed the active pool
        // when its reclaim failed. Shrink back to the bound before considering
        // another unseen descriptor; never evict the currently published value
        // unless a replacement has already been recorded below.
        for (const auto &candidate : link->entries) {
            if (candidate.get() == link->currentPublished
                || (candidate->native.size == copySize
                    && candidate->native.vkFormat == vkFormat)) {
                continue;
            }
            bool expected = true;
            if (!candidate->use.available.compare_exchange_strong(
                    expected, false, std::memory_order_acq_rel)) {
                continue;
            }
            if (retireReservedEntry(candidate.get()))
                break;
        }
        if (link->entries.size() > kMaximumInputPoolSize)
            return true;
    }

    Impl::InputEntry *evictionCandidate = nullptr;
    const bool needsReplacementAtCap = !entry
        && link->entries.size() == kMaximumInputPoolSize;
    const bool needsOverflowCleanup = entry
        && link->entries.size() > kMaximumInputPoolSize;
    if (needsReplacementAtCap || needsOverflowCleanup) {
        for (const auto &candidate : link->entries) {
            if (candidate.get() == entry
                || (candidate->native.size == copySize
                    && candidate->native.vkFormat == vkFormat)) {
                continue;
            }
            bool expected = true;
            if (!candidate->use.available.compare_exchange_strong(
                    expected, false, std::memory_order_acq_rel)) {
                continue;
            }
            evictionCandidate = candidate.get();
            break;
        }
    }

    if (!entry
        && (link->entries.size() < kMaximumInputPoolSize || evictionCandidate)) {
        auto candidate = std::make_unique<Impl::InputEntry>();
        candidate->use.available.store(false, std::memory_order_release);
        if (!m_impl->allocateExportedInput(candidate.get(), vkFormat,
                                           source.texture->format(),
                                           source.texture->flags() & QRhiTexture::sRGB,
                                           copySize, error)) {
            if (evictionCandidate)
                evictionCandidate->use.available.store(true, std::memory_order_release);
            m_impl->discardNative(&candidate->native);
            return false;
        }
        entry = candidate.get();
        link->entries.push_back(std::move(candidate));
    }
    if (!entry) {
        // GPU/engine back-pressure is expected. Keep the last published input instead of
        // blocking either render thread or TouchEngine.
        return true;
    }

    Impl::HostSignal signal;
    if (!m_impl->createHostSignal(&signal, error)) {
        entry->use.available.store(true, std::memory_order_release);
        if (evictionCandidate)
            evictionCandidate->use.available.store(true, std::memory_order_release);
        return false;
    }

    QRhiTextureCopyDescription copy;
    copy.setSourceTopLeft(QPoint(left, top));
    copy.setPixelSize(copySize);
    QRhiResourceUpdateBatch *updates = m_impl->rhi->nextResourceUpdateBatch();
    updates->copyTexture(entry->native.wrapper.get(), source.texture, copy);
    commandBuffer->resourceUpdate(updates);

    VkImageLayout oldLayout = VkImageLayout(entry->native.wrapper->nativeTexture().layout);
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED)
        oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    if (!m_impl->recordReleaseBarrier(&entry->native,
                                      oldLayout,
                                      m_impl->inputReleaseLayout,
                                      VK_ACCESS_TRANSFER_WRITE_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      commandBuffer,
                                      error)) {
        vkDestroySemaphore(m_impl->device, signal.native, nullptr);
        signal.native = VK_NULL_HANDLE;
        // No ownership transfer or semaphore publication reached TouchEngine. Qt still
        // owns this pool image, so it is safe to retry it on a later frame.
        entry->use.available.store(true, std::memory_order_release);
        if (evictionCandidate)
            evictionCandidate->use.available.store(true, std::memory_order_release);
        return false;
    }

    m_impl->attachSignal(signal.native);
    m_impl->pendingInputs.push_back(Impl::PendingInput{
        source.link, entry, std::move(signal), oldLayout, m_impl->inputReleaseLayout, false});

    // The replacement is now fully recorded and a failed publication will gate
    // StartFrame. It is safe to retire an incompatible EndUse-complete NT-handle
    // entry without risking a frame that falls back to the reclaimed texture.
    retireReservedEntry(evictionCandidate);
    return true;
}

bool VulkanTouchEngineBackend::updateTextureOutput(TEInstance *instance,
                                                   const QString &link,
                                                   TETexture *texture,
                                                   QRhiCommandBuffer *commandBuffer,
                                                   QString *error)
{
    if (error)
        error->clear();
    if (!m_impl->configured || instance != m_impl->configuredInstance)
        return fail(error, QStringLiteral("TouchEngine Vulkan instance is not configured"));
    if (!texture) {
        clearTextureOutput(link);
        return true;
    }
    if (!commandBuffer || link.isEmpty())
        return fail(error, QStringLiteral("Texture output requires a link and command buffer"));
    if (TETextureGetType(texture) != TETextureTypeVulkan)
        return fail(error, QStringLiteral("TouchEngine output '%1' is not a Vulkan texture")
                               .arg(link));
    auto duplicate = std::find_if(m_impl->pendingOutputs.begin(), m_impl->pendingOutputs.end(),
                                  [&link](const Impl::PendingOutput &pending) {
                                      return pending.link == link;
                                  });
    if (duplicate != m_impl->pendingOutputs.end()) {
        if (duplicate->releaseRecorded) {
            return fail(error,
                        QStringLiteral("Texture output '%1' already has a pending Vulkan "
                                       "ownership return")
                            .arg(link));
        }
        if (!duplicate->imported || !duplicate->imported->native.wrapper
            || !duplicate->signal.native) {
            return fail(error,
                        QStringLiteral("Texture output '%1' has incomplete Vulkan recovery state")
                            .arg(link));
        }

        if (!m_impl->beginFrameSlot(error)) {
            duplicate->recoveryError = error ? *error
                                             : QStringLiteral("Failed to prepare Vulkan recovery frame");
            return false;
        }
        VkImageLayout recoveryOldLayout = VkImageLayout(
            duplicate->imported->native.wrapper->nativeTexture().layout);
        if (recoveryOldLayout == VK_IMAGE_LAYOUT_UNDEFINED)
            recoveryOldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        if (!m_impl->recordReleaseBarrier(&duplicate->imported->native,
                                          recoveryOldLayout,
                                          duplicate->newLayout,
                                          VK_ACCESS_TRANSFER_READ_BIT,
                                          VK_PIPELINE_STAGE_TRANSFER_BIT,
                                          commandBuffer,
                                          error)) {
            duplicate->recoveryError = error
                ? *error : QStringLiteral("Failed to record Vulkan output recovery barrier");
            return false;
        }

        duplicate->oldLayout = recoveryOldLayout;
        duplicate->signal.frameSlot = m_impl->rhi->currentFrameSlot();
        duplicate->imported->lastUsedFrameSlot = duplicate->signal.frameSlot;
        duplicate->releaseRecorded = true;
        duplicate->recoveryError.clear();
        m_impl->attachSignal(duplicate->signal.native);
        return true;
    }

    auto *vulkanTexture = reinterpret_cast<TEVulkanTexture *>(texture);
    if (!isIdentityComponentMapping(TEVulkanTextureGetVkComponentMapping(vulkanTexture))) {
        return fail(error,
                    QStringLiteral("TouchEngine output '%1' uses a Vulkan component swizzle. "
                                   "Qt's QRhi native-texture wrapper creates an identity image "
                                   "view, so this mapping cannot be represented safely.")
                        .arg(link));
    }

    QRhiTexture::Format rhiFormat = QRhiTexture::UnknownFormat;
    QRhiTexture::Flags rhiFlags;
    const VkFormat vkFormat = TEVulkanTextureGetFormat(vulkanTexture);
    if (!vkToRhiFormat(vkFormat, &rhiFormat, &rhiFlags)) {
        return fail(error, QStringLiteral("TouchEngine Vulkan output format %1 is not supported")
                               .arg(int(vkFormat)));
    }
    const QSize size(TEVulkanTextureGetWidth(vulkanTexture),
                     TEVulkanTextureGetHeight(vulkanTexture));
    if (!size.isValid())
        return fail(error, QStringLiteral("TouchEngine output '%1' has an invalid size").arg(link));

    if (!TEInstanceHasVulkanTextureTransfer(instance, texture)) {
        // The output value is valid, but TouchEngine has not yet supplied the
        // semaphore and layouts required for safe Vulkan access. Core retains
        // interest and retries this output while the previous cache stays live.
        return false;
    }

    // Reap this slot's deferred native imports before cache lookup. A live TETexture
    // can be presented again after a clear; preserving and reusing its callback state
    // avoids replacing a callback whose Release event has not happened yet.
    if (!m_impl->beginFrameSlot(error))
        return false;

    Impl::ImportedOutput *imported = m_impl->importOutput(vulkanTexture,
                                                          link,
                                                          rhiFormat,
                                                          rhiFlags,
                                                          error);
    if (!imported)
        return false;

    // A link can publish a new TE texture before the old object's final Release callback.
    // The old native import is no longer useful, but remains alive until its last Qt frame
    // slot is known complete (and until any pending ownership return has succeeded).
    for (const auto &candidate : m_impl->importedOutputs) {
        if (candidate.get() != imported && candidate->link == link)
            candidate->purgeRequested = true;
    }

    Impl::OutputRecord *record = m_impl->outputRecord(link);
    if (!m_impl->ensureOutputCache(record, rhiFormat, rhiFlags, size, error))
        return false;

    Impl::HostSignal signal;
    if (!m_impl->createHostSignal(&signal, error))
        return false;
    imported->lastUsedFrameSlot = signal.frameSlot;
    const TextureAcquireResult acquireResult = m_impl->acquireFromTouchEngine(
        instance, texture, &imported->native, error);
    if (acquireResult != TextureAcquireResult::Acquired) {
        vkDestroySemaphore(m_impl->device, signal.native, nullptr);
        signal.native = VK_NULL_HANDLE;
        if (acquireResult == TextureAcquireResult::NotReady && error)
            error->clear();
        return false;
    }

    // From this point Qt owns the TE image. Install recovery state before recording any
    // further QRhi/native commands so every later failure gates StartFrame and retains the
    // texture plus unsignaled return semaphore until a safe return or teardown.
    Impl::PendingOutput pending;
    pending.link = link;
    pending.texture.set(texture);
    pending.signal = std::move(signal);
    pending.imported = imported;
    pending.newLayout = m_impl->inputReleaseLayout;
    pending.recoveryError = QStringLiteral("Vulkan output ownership was acquired but has not "
                                           "yet been released back to TouchEngine");
    m_impl->pendingOutputs.push_back(std::move(pending));
    Impl::PendingOutput &recovery = m_impl->pendingOutputs.back();

    QRhiResourceUpdateBatch *updates = m_impl->rhi->nextResourceUpdateBatch();
    updates->copyTexture(record->cache.get(), imported->native.wrapper.get());
    commandBuffer->resourceUpdate(updates);

    VkImageLayout oldLayout = VkImageLayout(imported->native.wrapper->nativeTexture().layout);
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED)
        oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    recovery.oldLayout = oldLayout;
    if (!m_impl->recordReleaseBarrier(&imported->native,
                                      oldLayout,
                                      m_impl->inputReleaseLayout,
                                      VK_ACCESS_TRANSFER_READ_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      commandBuffer,
                                      error)) {
        recovery.recoveryError = error
            ? *error : QStringLiteral("Failed to record Vulkan output release barrier");
        return false;
    }

    record->mirrorVertically = TETextureGetOrigin(texture) == TETextureOriginBottomLeft;
    recovery.releaseRecorded = true;
    recovery.recoveryError.clear();
    m_impl->attachSignal(recovery.signal.native);
    return true;
}

TextureOutput VulkanTouchEngineBackend::textureOutput(const QString &link) const
{
    const auto it = std::find_if(m_impl->outputs.begin(), m_impl->outputs.end(),
                                 [&link](const Impl::OutputRecord &record) {
                                     return record.link == link;
                                 });
    if (it == m_impl->outputs.end() || !it->cache)
        return {};
    return TextureOutput{it->cache.get(), it->size, it->mirrorVertically};
}

void VulkanTouchEngineBackend::clearTextureOutput(const QString &link)
{
    m_impl->requestImportPurge(link);

    auto it = std::find_if(m_impl->outputs.begin(), m_impl->outputs.end(),
                           [&link](const Impl::OutputRecord &record) {
                               return record.link == link;
                           });
    if (it != m_impl->outputs.end()) {
        QString retirementError;
        const bool retired = m_impl->retireRhiTexture(&it->cache, &retirementError);
        if (retired || !it->cache)
            m_impl->outputs.erase(it);
        if (!retired)
            qWarning().noquote() << "Dsqt.TouchEngine:" << retirementError;
    }

    QString purgeError;
    if (!m_impl->purgeRequestedImportsOutsideFrame(&purgeError))
        qWarning().noquote() << "Dsqt.TouchEngine:" << purgeError;
}

void VulkanTouchEngineBackend::clearTextureOutputs()
{
    m_impl->requestImportPurge(QString{});

    auto it = m_impl->outputs.begin();
    while (it != m_impl->outputs.end()) {
        QString retirementError;
        const bool retired = m_impl->retireRhiTexture(&it->cache, &retirementError);
        if (retired || !it->cache) {
            it = m_impl->outputs.erase(it);
        } else {
            qWarning().noquote() << "Dsqt.TouchEngine:" << retirementError;
            ++it;
        }
    }

    QString purgeError;
    if (!m_impl->purgeRequestedImportsOutsideFrame(&purgeError))
        qWarning().noquote() << "Dsqt.TouchEngine:" << purgeError;
}

bool VulkanTouchEngineBackend::afterFrameEnd(TEInstance *instance, QString *error)
{
    if (error)
        error->clear();

    // Instance recreation can occur during a Qt frame. There is nothing to
    // publish until InstanceReady configures the replacement; treating that
    // quiet frame as a transfer failure would incorrectly gate its load.
    if (m_impl->pendingInputs.empty() && m_impl->pendingOutputs.empty()) {
        QueueSignalRegistry::remove(m_impl->rhi, m_impl->frameSignals);
        m_impl->frameSignals.clear();
        for (auto &slot : m_impl->frameSlots)
            slot.active = false;
        return true;
    }
    if (!m_impl->configured || instance != m_impl->configuredInstance)
        return fail(error, QStringLiteral("TouchEngine Vulkan instance is not configured"));

    QStringList failures;
    auto inputIt = m_impl->pendingInputs.begin();
    while (inputIt != m_impl->pendingInputs.end()) {
        auto &pending = *inputIt;
        const QByteArray identifier = pending.link.toUtf8();
        TEResult result = TEResultSuccess;
        const char *operation = "Vulkan texture input transfer";
        if (!pending.transferRegistered) {
            operation = "Vulkan texture input ownership transfer";
            result = TEInstanceAddVulkanTextureTransfer(
                instance,
                reinterpret_cast<TETexture *>(pending.entry->texture.get()),
                pending.oldLayout,
                pending.newLayout,
                reinterpret_cast<TESemaphore *>(pending.signal.textureTransfer.get()),
                0);
            if (result == TEResultSuccess)
                pending.transferRegistered = true;
        }
        if (result == TEResultSuccess) {
            operation = "Vulkan texture input link publication";
            result = TEInstanceLinkSetTextureValue(
                instance,
                identifier.constData(),
                reinterpret_cast<TETexture *>(pending.entry->texture.get()),
                nullptr);
        }
        if (result == TEResultSuccess) {
            pending.entry->everPublished = true;
            const auto link = std::find_if(
                m_impl->inputLinks.begin(), m_impl->inputLinks.end(),
                [&pending](const Impl::InputLink &candidate) {
                    return candidate.link == pending.link;
                });
            if (link != m_impl->inputLinks.end())
                link->currentPublished = pending.entry;
            m_impl->retireSignal(&pending.signal);
            inputIt = m_impl->pendingInputs.erase(inputIt);
        } else {
            // The image has already been released to the external queue family. Do not make
            // it available to Qt again without a matching TE transfer. In particular, retain
            // the signaled native semaphore and its TE wrapper: retrying the API operation
            // does not require (and must not perform) a second binary-semaphore signal.
            pending.entry->use.available.store(false, std::memory_order_release);
            failures.push_back(teResultMessage(operation, result));
            ++inputIt;
        }
    }

    auto outputIt = m_impl->pendingOutputs.begin();
    while (outputIt != m_impl->pendingOutputs.end()) {
        auto &pending = *outputIt;
        if (!pending.releaseRecorded) {
            failures.push_back(pending.recoveryError.isEmpty()
                                   ? QStringLiteral("Vulkan output '%1' is still owned by Qt and "
                                                    "has no recorded return barrier")
                                         .arg(pending.link)
                                   : pending.recoveryError);
            ++outputIt;
            continue;
        }
        const TEResult result = TEInstanceAddVulkanTextureTransfer(
            instance,
            pending.texture,
            pending.oldLayout,
            pending.newLayout,
            reinterpret_cast<TESemaphore *>(pending.signal.textureTransfer.get()),
            0);
        if (result == TEResultSuccess) {
            m_impl->retireSignal(&pending.signal);
            outputIt = m_impl->pendingOutputs.erase(outputIt);
        } else {
            failures.push_back(teResultMessage("Vulkan texture output return", result));
            ++outputIt;
        }
    }

    // These registrations belong only to the Qt submit that just ended. Failed TE API
    // publication keeps the semaphore objects above, but must not enqueue a second signal
    // of the already-signaled binary semaphore on a later Qt frame.
    QueueSignalRegistry::remove(m_impl->rhi, m_impl->frameSignals);
    m_impl->frameSignals.clear();
    for (auto &slot : m_impl->frameSlots)
        slot.active = false;

    if (!failures.isEmpty())
        return fail(error, failures.join(QLatin1Char('\n')));
    return true;
}

} // namespace dsqt::touchengine::detail

#else

namespace dsqt::touchengine::detail {

struct VulkanTouchEngineBackend::Impl
{
};

VulkanTouchEngineBackend::VulkanTouchEngineBackend()
    : m_impl(std::make_unique<Impl>())
{
}
VulkanTouchEngineBackend::~VulkanTouchEngineBackend() = default;
DsTouchEngineTypes::GraphicsApi VulkanTouchEngineBackend::graphicsApi() const noexcept
{
    return DsTouchEngineTypes::GraphicsApi::Vulkan;
}
bool VulkanTouchEngineBackend::initialize(QRhi *, QRhiCommandBuffer *, QString *error)
{
    if (error)
        *error = QStringLiteral("Vulkan TouchEngine support is unavailable in this build");
    return false;
}
TEGraphicsContext *VulkanTouchEngineBackend::graphicsContext() const noexcept { return nullptr; }
bool VulkanTouchEngineBackend::resetInstance(QString *error)
{
    if (error)
        error->clear();
    return true;
}
bool VulkanTouchEngineBackend::configureInstance(TEInstance *, QString *) { return false; }
bool VulkanTouchEngineBackend::prepareTextureInput(TEInstance *, const TextureInputSource &,
                                                   QRhiCommandBuffer *, QString *) { return false; }
bool VulkanTouchEngineBackend::updateTextureOutput(TEInstance *, const QString &, TETexture *,
                                                   QRhiCommandBuffer *, QString *) { return false; }
TextureOutput VulkanTouchEngineBackend::textureOutput(const QString &) const { return {}; }
void VulkanTouchEngineBackend::clearTextureOutput(const QString &) { }
void VulkanTouchEngineBackend::clearTextureOutputs() { }
bool VulkanTouchEngineBackend::afterFrameEnd(TEInstance *, QString *) { return false; }

} // namespace dsqt::touchengine::detail

#endif
