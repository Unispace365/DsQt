#pragma once

#include "TouchEngineCore_p.h"

#include <QColor>
#include <QHash>
#include <QMetaObject>
#include <QPointer>
#include <QSet>
#include <QVector>
#include <QtQuick/QQuickRhiItem>

#include <memory>

QT_FORWARD_DECLARE_CLASS(QRhiBuffer)
QT_FORWARD_DECLARE_CLASS(QRhiGraphicsPipeline)
QT_FORWARD_DECLARE_CLASS(QRhi)
QT_FORWARD_DECLARE_CLASS(QRhiRenderPassDescriptor)
QT_FORWARD_DECLARE_CLASS(QRhiSampler)
QT_FORWARD_DECLARE_CLASS(QRhiShaderResourceBindings)
QT_FORWARD_DECLARE_CLASS(QRhiTexture)
QT_FORWARD_DECLARE_CLASS(QRhiTextureRenderTarget)
QT_FORWARD_DECLARE_CLASS(QQuickWindow)
QT_FORWARD_DECLARE_CLASS(QSGTexture)

namespace dsqt::touchengine {

class DsTouchEngineView;

namespace detail {

enum class TextureProviderStatus {
    Ready,
    SourceDestroyed,
    WrongWindow,
    NotTextureProvider,
    ProviderUnavailable,
    TextureUnavailable
};

struct TextureProviderSource
{
    QString link;
    QPointer<QSGTexture> texture;
    TextureProviderStatus status = TextureProviderStatus::SourceDestroyed;
    bool mirrorVertically = false;
};

struct TextureInputStaging;

class DsTouchEngineViewRenderer final : public QQuickRhiItemRenderer
{
public:
    DsTouchEngineViewRenderer();
    ~DsTouchEngineViewRenderer() override;

protected:
    void initialize(QRhiCommandBuffer *commandBuffer) override;
    void synchronize(QQuickRhiItem *item) override;
    void render(QRhiCommandBuffer *commandBuffer) override;

private:
    bool ensureStaticResources(QString *error);
    bool ensurePipeline(QRhiTexture *texture, QString *error);
    bool ensureInputStaging(TextureInputStaging *staging,
                            const TextureProviderSource &source,
                            QRhiTexture *sourceTexture,
                            const QSize &pixelSize,
                            const QRectF &sourceRect,
                            bool mirrorVertically,
                            QRhiTexture *targetTexture,
                            QRhiTextureRenderTarget *renderTarget,
                            QString *error);
    bool renderTextureInput(TextureInputStaging *staging,
                            QRhiTexture *targetTexture,
                            QRhiTextureRenderTarget *renderTarget,
                            QRhiCommandBuffer *commandBuffer,
                            QString *error);
    QVector<TextureInputSource> stageTextureInputs(QRhiCommandBuffer *commandBuffer);
    void pruneInputStaging(const QSet<QString> &activeLinks);
    void releaseInputStaging(const QString &link);
    void releaseInputStaging();
    void releasePipeline();
    void releaseResources();
    void connectAfterFrameEnd();
    void publishInitializationError(const QString &error);
    void publishInputError(const QString &link, const QString &error);

    QPointer<DsTouchEngineView> m_item;
    QPointer<QQuickWindow> m_window;
    std::shared_ptr<TouchEngineSharedState> m_shared;
    std::shared_ptr<TouchEngineCore> m_core;
    QVector<TextureProviderSource> m_textureInputs;
    QHash<QString, TextureInputStaging *> m_inputStaging;
    QSet<QSGTexture *> m_committedInputTextures;
    bool m_inputUniformUploadPending = true;
    QHash<QString, QString> m_inputErrors;
    QString m_initializationError;
    quint64 m_diagnosticProducerId = 0;

    QString m_outputLink;
    QColor m_clearColor = Qt::transparent;
    QMetaObject::Connection m_afterFrameConnection;

    QRhiBuffer *m_vertexBuffer = nullptr;
    QRhiBuffer *m_uniformBuffer = nullptr;
    QRhiSampler *m_sampler = nullptr;
    QRhiShaderResourceBindings *m_shaderResources = nullptr;
    QRhiGraphicsPipeline *m_pipeline = nullptr;
    QRhiTexture *m_boundTexture = nullptr;
    QRhi *m_currentRhi = nullptr;
    const QRhiRenderPassDescriptor *m_renderPassDescriptor = nullptr;
    bool m_staticUploadPending = true;
    bool m_verticesMirrored = false;
    bool m_textureInputRetryPending = false;
};

} // namespace detail
} // namespace dsqt::touchengine
