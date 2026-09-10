#pragma once

#include "../TouchEngineRhiBackend_p.h"

#include <QSize>
#include <QString>

#include <memory>

class QOpenGLExtraFunctions;

namespace dsqt::touchengine::detail {

class D3D11NvInteropBridge final
{
public:
    struct AcquiredOutput
    {
        quint32 textureName = 0;
        QSize pixelSize;
        QRhiTexture::Format format = QRhiTexture::UnknownFormat;
        QRhiTexture::Flags flags;
        bool mirrorVertically = false;
    };

    D3D11NvInteropBridge();
    ~D3D11NvInteropBridge();

    bool initialize(QRhi *rhi, QOpenGLExtraFunctions *gl,
                    void *nativeDeviceContext, QString *error);
    TEGraphicsContext *graphicsContext() const noexcept;
    QString adapterName() const;

    bool configureInstance(TEInstance *instance, QString *error);
    bool resetInstance(QString *error);

    bool prepareTextureInput(TEInstance *instance,
                             const TextureInputSource &source,
                             QRhiCommandBuffer *commandBuffer,
                             QString *error);
    bool acquireTextureOutput(TEInstance *instance,
                              const QString &link,
                              TETexture *texture,
                              QRhiCommandBuffer *commandBuffer,
                              AcquiredOutput *output,
                              QString *error);
    bool copyAcquiredOutput(const AcquiredOutput &output,
                            quint32 destinationTextureName,
                            QString *error);
    bool afterFrameEnd(TEInstance *instance, QString *error);

    void releaseResources();

private:
    class Impl;
    std::unique_ptr<Impl> d;
};

} // namespace dsqt::touchengine::detail
