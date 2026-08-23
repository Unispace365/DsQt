#pragma once

#include <Dsqt/TouchEngine/DsTouchEngineTypes.h>

#include <QRectF>
#include <QSize>
#include <QString>

#include <rhi/qrhi.h>

#include <TouchEngine/TouchEngine.h>

#include <memory>

namespace dsqt::touchengine::detail {

struct TextureInputSource
{
    QString link;
    QRhiTexture *texture = nullptr;
    QSize pixelSize;
    QRectF normalizedSourceRect = QRectF(0.0, 0.0, 1.0, 1.0);
};

struct TextureOutput
{
    QRhiTexture *texture = nullptr;
    QSize pixelSize;
    bool mirrorVertically = false;

    explicit operator bool() const noexcept { return texture != nullptr; }
};

class TouchEngineRhiBackend
{
public:
    virtual ~TouchEngineRhiBackend() = default;

    virtual DsTouchEngineTypes::GraphicsApi graphicsApi() const noexcept = 0;

    // All methods run on the Qt Quick render thread. initialize() is called
    // with a valid QRhi and command buffer before the TEInstance is configured.
    virtual bool initialize(QRhi *rhi, QRhiCommandBuffer *commandBuffer, QString *error) = 0;
    virtual TEGraphicsContext *graphicsContext() const noexcept = 0;

    // Called after TEEventInstanceReady and before TEInstanceLoad(). Backends
    // query the instance's negotiated texture/semaphore capabilities here.
    virtual bool configureInstance(TEInstance *instance, QString *error) = 0;

    // Called immediately after the previous TEInstance has been released. The
    // release is the callback lifetime fence, so backends can now discard any
    // transfers, callbacks, and capability state that belonged to that
    // instance. QRhi and the backend graphics context remain valid.
    virtual bool resetInstance(QString *error)
    {
        if (error)
            error->clear();
        return true;
    }

    // Copies the current Qt Quick source into backend-owned exportable storage.
    // Publication to TouchEngine happens from afterFrameEnd(), after Qt has
    // submitted the command buffer containing the copy.
    virtual bool prepareTextureInput(TEInstance *instance,
                                     const TextureInputSource &source,
                                     QRhiCommandBuffer *commandBuffer,
                                     QString *error) = 0;

    // Acquires, copies, and schedules return of a TE output texture. The cache
    // returned by textureOutput() is Qt-owned and remains valid across frames.
    virtual bool updateTextureOutput(TEInstance *instance,
                                     const QString &link,
                                     TETexture *texture,
                                     QRhiCommandBuffer *commandBuffer,
                                     QString *error) = 0;
    virtual TextureOutput textureOutput(const QString &link) const = 0;
    virtual void clearTextureOutput(const QString &link) = 0;
    virtual void clearTextureOutputs() = 0;

    // QQuickWindow::afterFrameEnd, direct on the render thread. GPU signals,
    // TE texture transfers, and input link publication are finalized here.
    virtual bool afterFrameEnd(TEInstance *instance, QString *error) = 0;
};

std::unique_ptr<TouchEngineRhiBackend> createTouchEngineRhiBackend(
    QRhi *rhi,
    DsTouchEngineTypes::GraphicsApi *api,
    QString *error);

} // namespace dsqt::touchengine::detail
