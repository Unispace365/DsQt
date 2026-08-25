#include "OpenGLTouchEngineBackend_p.h"

#include <TouchEngine/TED3D.h>
#include <TouchEngine/TEOpenGL.h>
#include <TouchEngine/TouchObject.h>

#include <QByteArray>
#include <QDebug>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QStringList>
#include <QThread>
#include <QtGui/qopenglcontext_platform.h>

#include <rhi/qrhi_platform.h>

#include <algorithm>
#include <cmath>
#include <deque>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace dsqt::touchengine::detail {

namespace {

    bool fail(QString* error, const QString& message) {
        if (error) *error = message;
        return false;
    }

    QString resultDescription(TEResult result) {
        const char* description = TEResultGetDescription(result);
        if (description && *description) return QString::fromUtf8(description);
        return QStringLiteral("TouchEngine error %1").arg(static_cast<int>(result));
    }

    bool completedLocallyOwnedOpenGLUnlock(TEResult result) noexcept {
        // TouchEngine-Windows runtimes through the current 2026 release execute
        // the internal OpenGL unlock and then return TEResultBadUsage. The
        // official sample consequently does not inspect Unlock's result. Keep
        // this compatibility exception narrowly scoped to an Unlock which this
        // backend calls after its own successful Lock; never use it to infer
        // ownership of an arbitrary TEOpenGLTexture.
        return result == TEResultSuccess || result == TEResultBadUsage;
    }

    class ExternalCommands final {
      public:
        explicit ExternalCommands(QRhiCommandBuffer* commandBuffer)
            : m_commandBuffer(commandBuffer) {
            if (m_commandBuffer) m_commandBuffer->beginExternal();
        }

        ~ExternalCommands() {
            if (m_commandBuffer) m_commandBuffer->endExternal();
        }

        ExternalCommands(const ExternalCommands&)            = delete;
        ExternalCommands& operator=(const ExternalCommands&) = delete;

      private:
        QRhiCommandBuffer* m_commandBuffer = nullptr;
    };

} // namespace

#ifdef Q_OS_WIN

namespace {

    constexpr std::size_t kMaximumInputTexturesPerLink = 6;

    struct TextureFormat {
        QRhiTexture::Format rhiFormat = QRhiTexture::UnknownFormat;
        QRhiTexture::Flags  rhiFlags;
        GLint               glInternalFormat = 0;

        explicit operator bool() const noexcept {
            return rhiFormat != QRhiTexture::UnknownFormat && glInternalFormat != 0;
        }
    };

    std::optional<TextureFormat> inputFormatFor(const QRhiTexture* texture) {
        if (!texture) return std::nullopt;

        TextureFormat result;
        result.rhiFlags = QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource;
        const bool srgb = texture->flags().testFlag(QRhiTexture::sRGB);

        switch (texture->format()) {
        case QRhiTexture::RGBA8:
        case QRhiTexture::BGRA8:
            result.rhiFormat = QRhiTexture::RGBA8;
            if (srgb) {
                result.rhiFlags |= QRhiTexture::sRGB;
                result.glInternalFormat = GL_SRGB8_ALPHA8;
            } else {
                result.glInternalFormat = GL_RGBA8;
            }
            break;
        case QRhiTexture::R8:
            result.rhiFormat        = QRhiTexture::R8;
            result.glInternalFormat = GL_R8;
            break;
        case QRhiTexture::RG8:
            result.rhiFormat        = QRhiTexture::RG8;
            result.glInternalFormat = GL_RG8;
            break;
        case QRhiTexture::R16:
            result.rhiFormat        = QRhiTexture::R16;
            result.glInternalFormat = GL_R16;
            break;
        case QRhiTexture::RG16:
            result.rhiFormat        = QRhiTexture::RG16;
            result.glInternalFormat = GL_RG16;
            break;
        case QRhiTexture::RGBA16F:
            result.rhiFormat        = QRhiTexture::RGBA16F;
            result.glInternalFormat = GL_RGBA16F;
            break;
        case QRhiTexture::RGBA32F:
            result.rhiFormat        = QRhiTexture::RGBA32F;
            result.glInternalFormat = GL_RGBA32F;
            break;
        case QRhiTexture::R16F:
            result.rhiFormat        = QRhiTexture::R16F;
            result.glInternalFormat = GL_R16F;
            break;
        case QRhiTexture::R32F:
            result.rhiFormat        = QRhiTexture::R32F;
            result.glInternalFormat = GL_R32F;
            break;
        case QRhiTexture::RGB10A2:
            result.rhiFormat        = QRhiTexture::RGB10A2;
            result.glInternalFormat = GL_RGB10_A2;
            break;
        default:
            return std::nullopt;
        }

        return result;
    }

    std::optional<TextureFormat> outputFormatFor(GLint internalFormat) {
        TextureFormat result;
        result.rhiFlags = QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource;

        switch (static_cast<GLenum>(internalFormat)) {
        case GL_RGBA8:
            result.rhiFormat        = QRhiTexture::RGBA8;
            result.glInternalFormat = GL_RGBA8;
            break;
#ifdef GL_BGRA8_EXT
        case GL_BGRA8_EXT:
            result.rhiFormat        = QRhiTexture::RGBA8;
            result.glInternalFormat = GL_RGBA8;
            break;
#endif
        case GL_SRGB8_ALPHA8:
            result.rhiFormat = QRhiTexture::RGBA8;
            result.rhiFlags |= QRhiTexture::sRGB;
            result.glInternalFormat = GL_SRGB8_ALPHA8;
            break;
        case GL_R8:
            result.rhiFormat        = QRhiTexture::R8;
            result.glInternalFormat = GL_R8;
            break;
        case GL_RG8:
            result.rhiFormat        = QRhiTexture::RG8;
            result.glInternalFormat = GL_RG8;
            break;
        case GL_R16:
            result.rhiFormat        = QRhiTexture::R16;
            result.glInternalFormat = GL_R16;
            break;
        case GL_RG16:
            result.rhiFormat        = QRhiTexture::RG16;
            result.glInternalFormat = GL_RG16;
            break;
        case GL_R16F:
            result.rhiFormat        = QRhiTexture::R16F;
            result.glInternalFormat = GL_R16F;
            break;
        case GL_R32F:
            result.rhiFormat        = QRhiTexture::R32F;
            result.glInternalFormat = GL_R32F;
            break;
        case GL_RG16F:
            result.rhiFormat        = QRhiTexture::RGBA16F;
            result.glInternalFormat = GL_RGBA16F;
            break;
        case GL_RG32F:
            result.rhiFormat        = QRhiTexture::RGBA32F;
            result.glInternalFormat = GL_RGBA32F;
            break;
        case GL_RGBA16:
            // QRhi has no four-channel 16-bit UNORM format. Float32 preserves
            // every 16-bit normalized value while remaining color-renderable.
            result.rhiFormat        = QRhiTexture::RGBA32F;
            result.glInternalFormat = GL_RGBA32F;
            break;
        case GL_RGBA16F:
            result.rhiFormat        = QRhiTexture::RGBA16F;
            result.glInternalFormat = GL_RGBA16F;
            break;
        case GL_RGBA32F:
            result.rhiFormat        = QRhiTexture::RGBA32F;
            result.glInternalFormat = GL_RGBA32F;
            break;
        case GL_R11F_G11F_B10F:
            result.rhiFormat        = QRhiTexture::RGBA16F;
            result.glInternalFormat = GL_RGBA16F;
            break;
        case GL_RGB10_A2:
            result.rhiFormat        = QRhiTexture::RGB10A2;
            result.glInternalFormat = GL_RGB10_A2;
            break;
        default:
            return std::nullopt;
        }

        return result;
    }

    bool isIdentityComponentMap(const TETextureComponentMap& map) {
        return map.r == TETextureComponentSourceRed && map.g == TETextureComponentSourceGreen &&
               map.b == TETextureComponentSourceBlue && map.a == TETextureComponentSourceAlpha;
    }

    struct BlitRect {
        int x0 = 0;
        int y0 = 0;
        int x1 = 0;
        int y1 = 0;
    };

    std::optional<BlitRect> sourceBlitRect(const TextureInputSource& source) {
        if (!source.texture) return std::nullopt;

        const QSize  nativeSize = source.texture->pixelSize();
        const QRectF rect       = source.normalizedSourceRect;
        if (nativeSize.width() <= 0 || nativeSize.height() <= 0 || !std::isfinite(rect.x()) ||
            !std::isfinite(rect.y()) || !std::isfinite(rect.width()) || !std::isfinite(rect.height()) ||
            rect.width() <= 0.0 || rect.height() <= 0.0) {
            return std::nullopt;
        }

        const double left   = std::clamp(rect.left(), 0.0, 1.0);
        const double top    = std::clamp(rect.top(), 0.0, 1.0);
        const double right  = std::clamp(rect.right(), 0.0, 1.0);
        const double bottom = std::clamp(rect.bottom(), 0.0, 1.0);
        if (right <= left || bottom <= top) return std::nullopt;

        BlitRect result;
        result.x0 = static_cast<int>(std::lround(left * nativeSize.width()));
        result.x1 = static_cast<int>(std::lround(right * nativeSize.width()));
        // QSGTexture sub-rects use top-left coordinates; framebuffer blits use
        // OpenGL's bottom-left coordinates.
        result.y0 = nativeSize.height() - static_cast<int>(std::lround(bottom * nativeSize.height()));
        result.y1 = nativeSize.height() - static_cast<int>(std::lround(top * nativeSize.height()));
        if (result.x1 <= result.x0 || result.y1 <= result.y0) return std::nullopt;
        return result;
    }

    struct InputCallbackEvent {
        quint64       slotId     = 0;
        quint64       generation = 0;
        TEObjectEvent event      = TEObjectEventBeginUse;
    };

    struct InputCallbackQueue {
        QMutex                         mutex;
        std::deque<InputCallbackEvent> events;
        bool                           accepting = true;
    };

    struct InputCallbackInfo {
        InputCallbackQueue* queue      = nullptr;
        quint64             slotId     = 0;
        quint64             generation = 0;
    };

    void inputTextureCallback(GLuint, TEObjectEvent event, void* opaque) {
        auto* info = static_cast<InputCallbackInfo*>(opaque);
        if (!info || !info->queue) return;

        // Copy the immutable/generation data before taking the queue lock. Once
        // the event is queued the render thread may recycle the callback record.
        InputCallbackQueue* queue      = info->queue;
        const quint64       slotId     = info->slotId;
        const quint64       generation = info->generation;
        QMutexLocker        lock(&queue->mutex);
        if (queue->accepting) {
            queue->events.push_back(InputCallbackEvent{
                slotId,
                generation,
                event,
            });
        }
    }

} // namespace

class OpenGLTouchEngineBackend::Impl {
  public:
    enum class InputState {
        Available,
        Pending,
        Published,
    };

    struct InputSlot {
        quint64                      id         = 0;
        quint64                      generation = 0;
        InputState                   state      = InputState::Available;
        QString                      link;
        std::unique_ptr<QRhiTexture> texture;
        QSize                        size;
        TextureFormat                format;
        InputCallbackInfo            callbackInfo;
    };

    struct OutputRecord {
        ~OutputRecord() {
            // Renderer SRBs keep a raw QRhiTexture pointer. Let QRhi retire the
            // resource after in-flight frames/SRBs instead of deleting it when a
            // cache entry is replaced, cleared, or abandoned after an unlock.
            if (texture) texture.release()->deleteLater();
        }

        std::unique_ptr<QRhiTexture> texture;
        QSize                        size;
        TextureFormat                format;
        bool                         mirrorVertically = false;
    };

    struct PendingOutputUnlock {
        QString                       link;
        TouchObject<TEOpenGLTexture>  source;
        std::shared_ptr<OutputRecord> output;
        bool                          publishOutput = false;
        bool                          mirrorVertically = false;
        QString                       copyError;
    };

    ~Impl() { releaseResources(); }

    bool checkRenderThread(QString* error) const {
        if (!renderThread || QThread::currentThread() == renderThread) return true;
        return fail(error, QStringLiteral("OpenGL TouchEngine work left the Qt Quick render thread"));
    }

    bool ensureCurrentContext(QString* error) {
        if (!qtContext || QOpenGLContext::currentContext() != qtContext) {
            return fail(error, QStringLiteral("Qt's QRhi OpenGL context is not current on the render thread"));
        }
        if (!nativeContext || wglGetCurrentContext() != nativeContext) {
            return fail(error, QStringLiteral("Qt's current WGL context does not match the QRhi context"));
        }

        HDC currentDc = wglGetCurrentDC();
        if (!currentDc) return fail(error, QStringLiteral("The QRhi WGL context has no current device context"));

        if (teContext && currentDc != nativeDc) {
            const TEResult result = TEOpenGLContextSetDC(teContext, currentDc);
            if (result != TEResultSuccess) {
                return fail(error, QStringLiteral("TEOpenGLContextSetDC failed: %1").arg(resultDescription(result)));
            }
        }
        nativeDc = currentDc;
        return true;
    }

    bool makeContextCurrent(QString* error) {
        if (!rhi || !rhi->makeThreadLocalNativeContextCurrent()) {
            return fail(error, QStringLiteral("Qt could not make the QRhi OpenGL context current"));
        }
        return ensureCurrentContext(error);
    }

    bool blit(GLuint source, GLenum sourceTarget, const BlitRect& sourceRect, GLuint destination,
              const QSize& destinationSize, QString* error) {
        if (!gl || !readFramebuffer || !drawFramebuffer)
            return fail(error, QStringLiteral("OpenGL copy resources are not initialized"));
        if (!source || !destination || destinationSize.width() <= 0 || destinationSize.height() <= 0) {
            return fail(error, QStringLiteral("Cannot copy an invalid OpenGL texture"));
        }
        if ((sourceTarget != GL_TEXTURE_2D && sourceTarget != GL_TEXTURE_RECTANGLE)) {
            return fail(error, QStringLiteral("TouchEngine supplied unsupported OpenGL texture target 0x%1")
                                   .arg(QString::number(sourceTarget, 16)));
        }

        GLint previousReadFramebuffer = 0;
        GLint previousDrawFramebuffer = 0;
        gl->glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousReadFramebuffer);
        gl->glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousDrawFramebuffer);

        while (gl->glGetError() != GL_NO_ERROR) {}

        gl->glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebuffer);
        gl->glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, sourceTarget, source, 0);
        const GLenum readStatus = gl->glCheckFramebufferStatus(GL_READ_FRAMEBUFFER);

        gl->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFramebuffer);
        gl->glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, destination, 0);
        const GLenum drawStatus = gl->glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);

        GLenum copyError = GL_NO_ERROR;
        if (readStatus == GL_FRAMEBUFFER_COMPLETE && drawStatus == GL_FRAMEBUFFER_COMPLETE) {
            gl->glBlitFramebuffer(sourceRect.x0, sourceRect.y0, sourceRect.x1, sourceRect.y1, 0, 0,
                                  destinationSize.width(), destinationSize.height(), GL_COLOR_BUFFER_BIT, GL_NEAREST);
            copyError = gl->glGetError();
        }

        gl->glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebuffer);
        gl->glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, sourceTarget, 0, 0);
        gl->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFramebuffer);
        gl->glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        gl->glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previousReadFramebuffer));
        gl->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(previousDrawFramebuffer));

        if (readStatus != GL_FRAMEBUFFER_COMPLETE) {
            return fail(error, QStringLiteral("The source OpenGL texture is not framebuffer-complete (0x%1)")
                                   .arg(QString::number(readStatus, 16)));
        }
        if (drawStatus != GL_FRAMEBUFFER_COMPLETE) {
            return fail(error, QStringLiteral("The destination OpenGL texture is not framebuffer-complete (0x%1)")
                                   .arg(QString::number(drawStatus, 16)));
        }
        if (copyError != GL_NO_ERROR) {
            return fail(
                error,
                QStringLiteral("OpenGL texture copy failed with error 0x%1").arg(QString::number(copyError, 16)));
        }
        return true;
    }

    void drainInputCallbacks() {
        std::deque<InputCallbackEvent> events;
        {
            QMutexLocker lock(&callbackQueue.mutex);
            events.swap(callbackQueue.events);
        }

        for (const InputCallbackEvent& event : events) {
            if (event.event != TEObjectEventRelease) continue;
            const auto it =
                std::find_if(inputSlots.cbegin(), inputSlots.cend(),
                             [&event](const std::unique_ptr<InputSlot>& slot) { return slot->id == event.slotId; });
            if (it != inputSlots.cend() && (*it)->generation == event.generation &&
                (*it)->state == InputState::Published) {
                (*it)->state = InputState::Available;
            }
        }
    }

    InputSlot* acquireInputSlot(const QString& link, QString* error) {
        drainInputCallbacks();
        std::size_t poolSize = 0;
        for (const auto& slot : inputSlots) {
            if (slot->link != link) continue;
            ++poolSize;
            if (slot->state == InputState::Available) return slot.get();
        }

        if (poolSize >= kMaximumInputTexturesPerLink) {
            fail(error,
                 QStringLiteral("TouchEngine has not released an OpenGL input texture for '%1'; "
                                "the bounded six-texture input pool is exhausted, so publication "
                                "will be retried")
                     .arg(link));
            return nullptr;
        }

        auto slot                 = std::make_unique<InputSlot>();
        slot->id                  = nextSlotId++;
        slot->link                = link;
        slot->callbackInfo.queue  = &callbackQueue;
        slot->callbackInfo.slotId = slot->id;
        InputSlot* result         = slot.get();
        inputSlots.push_back(std::move(slot));
        return result;
    }

    bool ensureInputTexture(InputSlot* slot, const QSize& size, const TextureFormat& format, QString* error) {
        if (slot->texture && slot->size == size && slot->format.rhiFormat == format.rhiFormat &&
            slot->format.rhiFlags == format.rhiFlags) {
            return true;
        }

        slot->texture.reset();
        slot->size   = {};
        slot->format = {};
        if (!rhi->isTextureFormatSupported(format.rhiFormat, format.rhiFlags)) {
            return fail(error, QStringLiteral("Qt's OpenGL backend cannot render to the input texture format"));
        }

        slot->texture.reset(rhi->newTexture(format.rhiFormat, size, 1, format.rhiFlags));
        if (!slot->texture || !slot->texture->create()) {
            slot->texture.reset();
            return fail(error, QStringLiteral("Qt could not allocate an OpenGL input staging texture"));
        }
        if (!slot->texture->nativeTexture().object) {
            slot->texture.reset();
            return fail(error, QStringLiteral("Qt did not expose the input staging texture's OpenGL name"));
        }

        slot->size   = size;
        slot->format = format;
        return true;
    }

    std::shared_ptr<OutputRecord> outputRecord(const QString& link, const QSize& size, const TextureFormat& format,
                                               QString* error) {
        const auto current = outputs.value(link);
        if (current && current->size == size && current->format.rhiFormat == format.rhiFormat &&
            current->format.rhiFlags == format.rhiFlags) {
            return current;
        }

        if (!rhi->isTextureFormatSupported(format.rhiFormat, format.rhiFlags)) {
            fail(error, QStringLiteral("Qt's OpenGL backend cannot render to the TouchEngine output format"));
            return {};
        }

        auto result = std::make_shared<OutputRecord>();
        result->texture.reset(rhi->newTexture(format.rhiFormat, size, 1, format.rhiFlags));
        if (!result->texture || !result->texture->create()) {
            fail(error, QStringLiteral("Qt could not allocate the OpenGL output texture"));
            return {};
        }
        if (!result->texture->nativeTexture().object) {
            fail(error, QStringLiteral("Qt did not expose the output texture's OpenGL name"));
            return {};
        }
        result->size   = size;
        result->format = format;
        return result;
    }

    bool outputSource(TETexture* texture, const QString& link, TouchObject<TEOpenGLTexture>* result,
                      QString* error) {
        const TETextureType type = TETextureGetType(texture);
        if (type == TETextureTypeOpenGL) {
            result->set(static_cast<TEOpenGLTexture*>(texture));
            return true;
        }
        if (type != TETextureTypeD3DShared) {
            return fail(error, QStringLiteral("Texture output %1 has unsupported TouchEngine type %2")
                                   .arg(link)
                                   .arg(static_cast<int>(type)));
        }

        const TEResult conversionResult = TEOpenGLContextGetTexture(
            teContext, static_cast<TED3DSharedTexture*>(texture), result->take());
        if (conversionResult != TEResultSuccess || !*result) {
            return fail(error, QStringLiteral("Could not open TouchEngine output %1 in OpenGL: %2")
                                   .arg(link, resultDescription(conversionResult)));
        }
        return true;
    }

    bool hasPendingOutputUnlock(const QString& link) const {
        return std::any_of(pendingOutputUnlocks.cbegin(), pendingOutputUnlocks.cend(),
                           [&link](const PendingOutputUnlock& pending) { return pending.link == link; });
    }

    bool retryPendingOutputUnlocks(QStringList* failures) {
        auto it = pendingOutputUnlocks.begin();
        while (it != pendingOutputUnlocks.end()) {
            const TEResult result = TEOpenGLTextureUnlock(it->source);
            if (!completedLocallyOwnedOpenGLUnlock(result)) {
                QString message = QStringLiteral("Could not unlock TouchEngine output %1: %2")
                                      .arg(it->link, resultDescription(result));
                if (!it->copyError.isEmpty()) message.prepend(it->copyError + QLatin1Char('\n'));
                if (failures) failures->push_back(std::move(message));
                ++it;
                continue;
            }

            if (it->publishOutput && it->output) {
                it->output->mirrorVertically = it->mirrorVertically;
                outputs.insert(it->link, std::move(it->output));
            }
            // Releasing the TE wrapper can release graphics-side resources, so keep
            // this operation inside the caller's current Qt QRhi OpenGL context.
            it->source.reset();
            it = pendingOutputUnlocks.erase(it);
        }
        return pendingOutputUnlocks.empty();
    }

    void releaseResources() {
        if (!rhi) return;

        Q_ASSERT(!renderThread || QThread::currentThread() == renderThread);
        const bool current =
            !renderThread || QThread::currentThread() == renderThread ? makeContextCurrent(nullptr) : false;

        {
            QMutexLocker lock(&callbackQueue.mutex);
            callbackQueue.accepting = false;
            callbackQueue.events.clear();
        }

        if (current && gl && !pendingOutputUnlocks.empty()) {
            gl->glFlush();
            for (PendingOutputUnlock& pending : pendingOutputUnlocks)
                pending.publishOutput = false;
            QStringList failures;
            retryPendingOutputUnlocks(&failures);
            for (const QString& failure : failures)
                qWarning().noquote() << "Dsqt.TouchEngine teardown:" << failure;
        }
        // Whether or not Unlock succeeded, drop retained TE wrappers before releasing
        // TEOpenGLContext. The normal path above does so with Qt's context current.
        pendingOutputUnlocks.clear();
        pendingInputs.clear();
        outputs.clear();
        inputSlots.clear();

        if (current && gl) {
            if (readFramebuffer) gl->glDeleteFramebuffers(1, &readFramebuffer);
            if (drawFramebuffer) gl->glDeleteFramebuffers(1, &drawFramebuffer);
            readFramebuffer = 0;
            drawFramebuffer = 0;
            teContext.reset();
            gl->glFlush();
        } else {
            // TouchEngineCore releases its TEInstance before the backend. This
            // fallback therefore only drops the context reference; normal Qt
            // Quick teardown always takes the current-context branch above.
            teContext.reset();
        }
        rhi       = nullptr;
        qtContext = nullptr;
        gl        = nullptr;
    }

    QRhi*                        rhi           = nullptr;
    QThread*                     renderThread  = nullptr;
    QOpenGLContext*              qtContext     = nullptr;
    QOpenGLExtraFunctions*       gl            = nullptr;
    HGLRC                        nativeContext = nullptr;
    HDC                          nativeDc      = nullptr;
    TouchObject<TEOpenGLContext> teContext;
    GLuint                       readFramebuffer = 0;
    GLuint                       drawFramebuffer = 0;
    QString                      rendererName;

    InputCallbackQueue                            callbackQueue;
    std::vector<std::unique_ptr<InputSlot>>       inputSlots;
    QHash<QString, InputSlot*>                    pendingInputs;
    std::vector<PendingOutputUnlock>              pendingOutputUnlocks;
    QHash<QString, std::shared_ptr<OutputRecord>> outputs;
    quint64                                       nextSlotId = 1;
};

OpenGLTouchEngineBackend::OpenGLTouchEngineBackend()
    : d(std::make_unique<Impl>()) {
}

OpenGLTouchEngineBackend::~OpenGLTouchEngineBackend() = default;

DsTouchEngineTypes::GraphicsApi OpenGLTouchEngineBackend::graphicsApi() const noexcept {
    return DsTouchEngineTypes::GraphicsApi::OpenGL;
}

bool OpenGLTouchEngineBackend::initialize(QRhi* rhi, QRhiCommandBuffer* commandBuffer, QString* error) {
    if (error) error->clear();
    if (!rhi || rhi->backend() != QRhi::OpenGLES2)
        return fail(error, QStringLiteral("The OpenGL backend requires Qt's OpenGL QRhi"));
    if (!commandBuffer) return fail(error, QStringLiteral("No QRhi command buffer was available for OpenGL setup"));
    if (d->rhi) return fail(error, QStringLiteral("The OpenGL TouchEngine backend is already initialized"));

    const auto* nativeHandles = static_cast<const QRhiGles2NativeHandles*>(rhi->nativeHandles());
    if (!nativeHandles || !nativeHandles->context) {
        return fail(error, QStringLiteral("Qt did not expose its QRhi QOpenGLContext"));
    }

    d->rhi          = rhi;
    d->renderThread = QThread::currentThread();
    d->qtContext    = nativeHandles->context;

    ExternalCommands external(commandBuffer);
    if (QOpenGLContext::currentContext() != d->qtContext) {
        return fail(error, QStringLiteral("Qt's QRhi OpenGL context did not become current for external commands"));
    }

    auto* wgl = d->qtContext->nativeInterface<QNativeInterface::QWGLContext>();
    if (!wgl || !(d->nativeContext = wgl->nativeContext())) {
        return fail(error, QStringLiteral("TouchEngine OpenGL interop requires Qt's WGL backend"));
    }
    if (wglGetCurrentContext() != d->nativeContext || !(d->nativeDc = wglGetCurrentDC())) {
        return fail(error, QStringLiteral("The QRhi WGL context is not current during OpenGL setup"));
    }

    d->gl = d->qtContext->extraFunctions();
    if (!d->gl) return fail(error, QStringLiteral("Qt could not initialize OpenGL functions"));
    d->gl->initializeOpenGLFunctions();

    const QSurfaceFormat format   = d->qtContext->format();
    const bool hasFramebufferBlit = format.majorVersion() >= 3 ||
                                    d->qtContext->hasExtension(QByteArrayLiteral("GL_EXT_framebuffer_blit")) ||
                                    d->qtContext->hasExtension(QByteArrayLiteral("GL_ANGLE_framebuffer_blit"));
    if (!hasFramebufferBlit) {
        return fail(error, QStringLiteral("The QRhi OpenGL context does not support framebuffer texture copies"));
    }

    d->gl->glGenFramebuffers(1, &d->readFramebuffer);
    d->gl->glGenFramebuffers(1, &d->drawFramebuffer);
    if (!d->readFramebuffer || !d->drawFramebuffer)
        return fail(error, QStringLiteral("Could not create OpenGL texture-copy framebuffers"));

    if (const GLubyte* renderer = d->gl->glGetString(GL_RENDERER))
        d->rendererName = QString::fromLatin1(reinterpret_cast<const char*>(renderer));

    TEOpenGLContext* context = nullptr;
    const TEResult   result  = TEOpenGLContextCreate(d->nativeDc, d->nativeContext, &context);
    if (result != TEResultSuccess || !context) {
        return fail(error, QStringLiteral("TEOpenGLContextCreate failed: %1").arg(resultDescription(result)));
    }
    d->teContext.take(context);
    return true;
}

TEGraphicsContext* OpenGLTouchEngineBackend::graphicsContext() const noexcept {
    return reinterpret_cast<TEGraphicsContext*>(d->teContext.get());
}

bool OpenGLTouchEngineBackend::configureInstance(TEInstance* instance, QString* error) {
    if (error) error->clear();
    if (!d->checkRenderThread(error)) return false;
    if (!instance || !d->teContext)
        return fail(error, QStringLiteral("Cannot configure OpenGL interop without an instance and context"));
    if (!d->makeContextCurrent(error)) return false;
    if (TEOpenGLContextSupportsTexturesForInstance(d->teContext, instance)) return true;

    const QString device = d->rendererName.isEmpty() ? QStringLiteral("unknown OpenGL renderer") : d->rendererName;
    return fail(error, QStringLiteral("TouchEngine cannot exchange OpenGL textures with %1").arg(device));
}

bool OpenGLTouchEngineBackend::prepareTextureInput(TEInstance* instance, const TextureInputSource& source,
                                                   QRhiCommandBuffer* commandBuffer, QString* error) {
    Q_UNUSED(instance)
    if (error) error->clear();
    if (!d->checkRenderThread(error)) return false;
    if (!d->rhi || !d->teContext || !commandBuffer)
        return fail(error, QStringLiteral("The OpenGL backend is not ready for texture input"));
    if (source.link.isEmpty() || !source.texture)
        return fail(error, QStringLiteral("Texture input requires a link and source texture"));
    if (source.texture->sampleCount() != 1 || source.texture->flags().testFlag(QRhiTexture::CubeMap) ||
        source.texture->flags().testFlag(QRhiTexture::ThreeDimensional) ||
        source.texture->flags().testFlag(QRhiTexture::TextureArray) ||
        source.texture->flags().testFlag(QRhiTexture::OneDimensional) ||
        source.texture->flags().testFlag(QRhiTexture::ExternalOES)) {
        return fail(error, QStringLiteral("Texture input %1 is not a single-sample 2D texture").arg(source.link));
    }

    const auto format = inputFormatFor(source.texture);
    if (!format) {
        return fail(error,
                    QStringLiteral("Texture input %1 uses a format TouchEngine OpenGL cannot import").arg(source.link));
    }
    const auto sourceRect = sourceBlitRect(source);
    if (!sourceRect) {
        return fail(error, QStringLiteral("Texture input %1 has an invalid source rectangle").arg(source.link));
    }

    QSize destinationSize = source.pixelSize;
    if (destinationSize.width() <= 0 || destinationSize.height() <= 0) {
        destinationSize = QSize(sourceRect->x1 - sourceRect->x0, sourceRect->y1 - sourceRect->y0);
    }
    if (destinationSize.width() <= 0 || destinationSize.height() <= 0) {
        return fail(error, QStringLiteral("Texture input %1 has no usable pixel size").arg(source.link));
    }

    Impl::InputSlot* slot = d->acquireInputSlot(source.link, error);
    if (!slot) return false;
    slot->state           = Impl::InputState::Pending;

    ExternalCommands external(commandBuffer);
    if (!d->ensureCurrentContext(error) || !d->ensureInputTexture(slot, destinationSize, *format, error)) {
        slot->state = Impl::InputState::Available;
        return false;
    }

    const QRhiTexture::NativeTexture sourceNative      = source.texture->nativeTexture();
    const QRhiTexture::NativeTexture destinationNative = slot->texture->nativeTexture();
    const GLenum                     sourceTarget =
        source.texture->flags().testFlag(QRhiTexture::TextureRectangleGL) ? GL_TEXTURE_RECTANGLE : GL_TEXTURE_2D;
    if (!d->blit(static_cast<GLuint>(sourceNative.object), sourceTarget, *sourceRect,
                 static_cast<GLuint>(destinationNative.object), destinationSize, error)) {
        slot->state = Impl::InputState::Available;
        return false;
    }

    if (Impl::InputSlot* previous = d->pendingInputs.value(source.link)) previous->state = Impl::InputState::Available;
    d->pendingInputs.insert(source.link, slot);
    return true;
}

bool OpenGLTouchEngineBackend::updateTextureOutput(TEInstance* instance, const QString& link, TETexture* texture,
                                                   QRhiCommandBuffer* commandBuffer, QString* error) {
    Q_UNUSED(instance)
    if (error) error->clear();
    if (!d->checkRenderThread(error)) return false;
    if (!d->rhi || !d->teContext || !commandBuffer)
        return fail(error, QStringLiteral("The OpenGL backend is not ready for texture output"));
    if (link.isEmpty() || !texture)
        return fail(error, QStringLiteral("Texture output requires a link and TouchEngine texture"));
    if (d->hasPendingOutputUnlock(link)) {
        return fail(error,
                    QStringLiteral("TouchEngine output %1 is still locked after a failed OpenGL unlock; "
                                   "the backend will retry after this Qt frame")
                        .arg(link));
    }
    if (!isIdentityComponentMap(TETextureGetComponentMap(texture))) {
        return fail(error,
                    QStringLiteral("Texture output %1 uses a component map OpenGL copying cannot preserve").arg(link));
    }

    ExternalCommands external(commandBuffer);
    if (!d->ensureCurrentContext(error)) return false;

    TouchObject<TEOpenGLTexture> source;
    if (!d->outputSource(texture, link, &source, error)) return false;

    const GLenum sourceTarget = TEOpenGLTextureGetTarget(source);
    if (sourceTarget != GL_TEXTURE_2D && sourceTarget != GL_TEXTURE_RECTANGLE) {
        source.reset();
        return fail(error, QStringLiteral("Texture output %1 has unsupported OpenGL target 0x%2")
                               .arg(link, QString::number(sourceTarget, 16)));
    }

    const QSize size(TEOpenGLTextureGetWidth(source), TEOpenGLTextureGetHeight(source));
    if (size.width() <= 0 || size.height() <= 0) {
        source.reset();
        return fail(error, QStringLiteral("Texture output %1 has an invalid pixel size").arg(link));
    }

    const auto format = outputFormatFor(TEOpenGLTextureGetInternalFormat(source));
    if (!format) {
        const GLint internalFormat = TEOpenGLTextureGetInternalFormat(source);
        source.reset();
        return fail(error, QStringLiteral("Texture output %1 has unsupported OpenGL format 0x%2")
                               .arg(link, QString::number(static_cast<quint32>(internalFormat), 16)));
    }

    std::shared_ptr<Impl::OutputRecord> output = d->outputRecord(link, size, *format, error);
    if (!output) {
        source.reset();
        return false;
    }

    const TEResult lockResult = TEOpenGLTextureLock(source);
    if (lockResult != TEResultSuccess) {
        source.reset();
        return fail(
            error, QStringLiteral("Could not lock TouchEngine output %1: %2").arg(link, resultDescription(lockResult)));
    }

    const BlitRect rect{0, 0, size.width(), size.height()};
    const bool     copied = d->blit(TEOpenGLTextureGetName(source), sourceTarget, rect,
                                    static_cast<GLuint>(output->texture->nativeTexture().object), size, error);
    const QString copyError = copied
                                  ? QString{}
                                  : (error && !error->isEmpty()
                                         ? *error
                                         : QStringLiteral("Could not copy TouchEngine output %1").arg(link));
    const bool mirrorVertically = TETextureGetOrigin(texture) == TETextureOriginBottomLeft;

    // Keep every source locked until all output copies have been recorded. afterFrameEnd()
    // flushes the batch once, unlocks every source, and only then publishes the stable Qt
    // caches. This removes one WGL/D3D interop flush per output at the cost of one frame of
    // initial output latency.
    d->pendingOutputUnlocks.push_back(Impl::PendingOutputUnlock{
        .link = link,
        .source = std::move(source),
        .output = std::move(output),
        .publishOutput = copied,
        .mirrorVertically = mirrorVertically,
        .copyError = copyError,
    });
    return copied;
}

TextureOutput OpenGLTouchEngineBackend::textureOutput(const QString& link) const {
    const auto output = d->outputs.value(link);
    if (!output || !output->texture) return {};
    return TextureOutput{
        .texture          = output->texture.get(),
        .pixelSize        = output->size,
        .mirrorVertically = output->mirrorVertically,
    };
}

void OpenGLTouchEngineBackend::clearTextureOutput(const QString& link) {
    Q_ASSERT(!d->renderThread || QThread::currentThread() == d->renderThread);
    for (Impl::PendingOutputUnlock& pending : d->pendingOutputUnlocks) {
        if (pending.link == link)
            pending.publishOutput = false;
    }
    d->outputs.remove(link);
}

void OpenGLTouchEngineBackend::clearTextureOutputs() {
    Q_ASSERT(!d->renderThread || QThread::currentThread() == d->renderThread);
    for (Impl::PendingOutputUnlock& pending : d->pendingOutputUnlocks)
        pending.publishOutput = false;
    d->outputs.clear();
}

bool OpenGLTouchEngineBackend::afterFrameEnd(TEInstance* instance, QString* error) {
    if (error) error->clear();
    if (!d->checkRenderThread(error)) return false;
    if (!instance || !d->rhi || !d->teContext)
        return fail(error, QStringLiteral("The OpenGL backend is not ready to publish texture inputs"));

    d->drainInputCallbacks();
    if (d->pendingInputs.isEmpty() && d->pendingOutputUnlocks.empty()) return true;
    if (!d->makeContextCurrent(error)) return false;

    d->gl->glFlush();
    QStringList failures;
    d->retryPendingOutputUnlocks(&failures);

    const bool publishInputs = !d->pendingInputs.isEmpty();
    if (publishInputs) {
        QHash<QString, Impl::InputSlot*> pending = std::exchange(d->pendingInputs, {});
        for (auto it = pending.cbegin(); it != pending.cend(); ++it) {
            Impl::InputSlot* slot = it.value();
            if (!slot || slot->state != Impl::InputState::Pending || !slot->texture) {
                failures.push_back(QStringLiteral("Texture input %1 lost its OpenGL staging texture").arg(it.key()));
                continue;
            }

            ++slot->generation;
            slot->callbackInfo.generation = slot->generation;
            slot->state                   = Impl::InputState::Published;

            TEOpenGLTexture* created = TEOpenGLTextureCreate(
                static_cast<GLuint>(slot->texture->nativeTexture().object), GL_TEXTURE_2D,
                slot->format.glInternalFormat, slot->size.width(), slot->size.height(), TETextureOriginBottomLeft,
                kTETextureComponentMapIdentity, &inputTextureCallback, &slot->callbackInfo);
            if (!created) {
                slot->state = Impl::InputState::Available;
                failures.push_back(
                    QStringLiteral("Could not wrap OpenGL texture input %1 for TouchEngine").arg(it.key()));
                continue;
            }

            TouchObject<TEOpenGLTexture> wrapped = TouchObject<TEOpenGLTexture>::make_take(created);
            const QByteArray identifier = it.key().toUtf8();
            const TEResult result = TEInstanceLinkSetTextureValue(
                instance, identifier.constData(), reinterpret_cast<TETexture*>(wrapped.get()),
                reinterpret_cast<TEGraphicsContext*>(d->teContext.get()));
            wrapped.reset();

            if (result != TEResultSuccess) {
                failures.push_back(
                    QStringLiteral("Could not publish texture input %1: %2")
                        .arg(it.key(), resultDescription(result)));
            }
        }
    }

    // Input wrapping and SetTextureValue may submit additional interop work. Output-only
    // frames have already been submitted by the single flush before the unlock batch.
    if (publishInputs)
        d->gl->glFlush();
    if (!failures.isEmpty()) return fail(error, failures.join(QLatin1Char('\n')));
    return true;
}

#else

class OpenGLTouchEngineBackend::Impl {};

OpenGLTouchEngineBackend::OpenGLTouchEngineBackend()
    : d(std::make_unique<Impl>()) {
}

OpenGLTouchEngineBackend::~OpenGLTouchEngineBackend() = default;

DsTouchEngineTypes::GraphicsApi OpenGLTouchEngineBackend::graphicsApi() const noexcept {
    return DsTouchEngineTypes::GraphicsApi::OpenGL;
}

bool OpenGLTouchEngineBackend::initialize(QRhi*, QRhiCommandBuffer*, QString* error) {
    return fail(error, QStringLiteral("TouchEngine OpenGL interop requires WGL on Windows"));
}

TEGraphicsContext* OpenGLTouchEngineBackend::graphicsContext() const noexcept {
    return nullptr;
}

bool OpenGLTouchEngineBackend::configureInstance(TEInstance*, QString* error) {
    return fail(error, QStringLiteral("TouchEngine OpenGL interop requires WGL on Windows"));
}

bool OpenGLTouchEngineBackend::prepareTextureInput(TEInstance*, const TextureInputSource&, QRhiCommandBuffer*,
                                                   QString* error) {
    return fail(error, QStringLiteral("TouchEngine OpenGL interop requires WGL on Windows"));
}

bool OpenGLTouchEngineBackend::updateTextureOutput(TEInstance*, const QString&, TETexture*, QRhiCommandBuffer*,
                                                   QString* error) {
    return fail(error, QStringLiteral("TouchEngine OpenGL interop requires WGL on Windows"));
}

TextureOutput OpenGLTouchEngineBackend::textureOutput(const QString&) const {
    return {};
}

void OpenGLTouchEngineBackend::clearTextureOutput(const QString&) {
}

void OpenGLTouchEngineBackend::clearTextureOutputs() {
}

bool OpenGLTouchEngineBackend::afterFrameEnd(TEInstance*, QString* error) {
    return fail(error, QStringLiteral("TouchEngine OpenGL interop requires WGL on Windows"));
}

#endif

} // namespace dsqt::touchengine::detail
