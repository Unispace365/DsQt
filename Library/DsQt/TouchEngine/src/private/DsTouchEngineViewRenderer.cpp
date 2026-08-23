#include "DsTouchEngineViewRenderer_p.h"

#include "DsTouchEngineSessionPrivate_p.h"

#include <Dsqt/TouchEngine/DsTouchEngineSession.h>
#include <Dsqt/TouchEngine/DsTouchEngineView.h>

#include <QFile>
#include <QMatrix4x4>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSGTexture>
#include <QSGTextureProvider>
#include <QStringList>
#include <rhi/qshader.h>

#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <utility>

namespace dsqt::touchengine::detail {

struct TextureInputStaging
{
    QSGTexture *sourceTexture = nullptr;
    QRhiTexture *sourceRhiTexture = nullptr;
    QRhiTexture *texture = nullptr;
    QRhiTextureRenderTarget *renderTarget = nullptr;
    QRhiRenderPassDescriptor *renderPassDescriptor = nullptr;
    QRhiBuffer *vertexBuffer = nullptr;
    QRhiShaderResourceBindings *shaderResources = nullptr;
    QRhiGraphicsPipeline *pipeline = nullptr;
    QSize pixelSize;
    QRectF sourceRect;
    bool mirrorVertically = false;
    bool vertexUploadPending = true;
};

namespace {

constexpr int VertexStride = 6 * sizeof(float);
std::atomic<quint64> nextRendererDiagnosticProducerId{1};

std::array<float, 24> vertices(const QRectF &sourceRect, bool mirrorVertically)
{
    const float left = static_cast<float>(sourceRect.left());
    const float right = static_cast<float>(sourceRect.right());
    const float top = static_cast<float>(mirrorVertically
                                             ? sourceRect.bottom()
                                             : sourceRect.top());
    const float bottom = static_cast<float>(mirrorVertically
                                                ? sourceRect.top()
                                                : sourceRect.bottom());
    return {
        -1.0f, -1.0f, 0.0f, 1.0f, left,  bottom,
         1.0f, -1.0f, 0.0f, 1.0f, right, bottom,
        -1.0f,  1.0f, 0.0f, 1.0f, left,  top,
         1.0f,  1.0f, 0.0f, 1.0f, right, top,
    };
}

std::array<float, 24> vertices(bool mirrorVertically)
{
    return vertices(QRectF(0.0, 0.0, 1.0, 1.0), mirrorVertically);
}

template<typename Texture>
bool isYInverted(const Texture *texture)
{
    // Some QSG texture implementations/Qt versions expose this orientation
    // bit, while current Qt keeps presentation mirroring on the provider
    // item. Keep the staging path compatible with both shapes of the API.
    if constexpr (requires(const Texture &candidate) { candidate.isYInverted(); })
        return texture && texture->isYInverted();
    return false;
}

QSize stagingPixelSize(QSGTexture *texture,
                       QRhiTexture *sourceTexture,
                       const QRectF &sourceRect)
{
    if (!texture || !sourceTexture)
        return {};

    // For an atlas, rhiTexture()->pixelSize() is the backing allocation while
    // normalizedTextureSubRect() identifies the provider's image. Derive the
    // target extent from both so this stays correct even on Qt versions or
    // custom QSGTexture implementations whose textureSize() reports the
    // backing atlas rather than the logical subtexture.
    if (texture->isAtlasTexture()) {
        const QSize backingSize = sourceTexture->pixelSize();
        const QSize subtextureSize(
            qRound(sourceRect.width() * backingSize.width()),
            qRound(sourceRect.height() * backingSize.height()));
        return subtextureSize.isEmpty() ? QSize{} : subtextureSize;
    }
    const QSize textureSize = texture->textureSize();
    return textureSize.isEmpty() ? QSize{} : textureSize;
}

void releaseInputBindings(TextureInputStaging *staging)
{
    if (!staging)
        return;
    delete staging->pipeline;
    staging->pipeline = nullptr;
    delete staging->shaderResources;
    staging->shaderResources = nullptr;
    staging->sourceRhiTexture = nullptr;
    staging->sourceTexture = nullptr;
}

void releaseInputResources(TextureInputStaging *staging)
{
    if (!staging)
        return;
    releaseInputBindings(staging);
    delete staging->renderTarget;
    staging->renderTarget = nullptr;
    delete staging->renderPassDescriptor;
    staging->renderPassDescriptor = nullptr;
    delete staging->texture;
    staging->texture = nullptr;
    delete staging->vertexBuffer;
    staging->vertexBuffer = nullptr;
    staging->pixelSize = {};
    staging->sourceRect = {};
    staging->mirrorVertically = false;
    staging->vertexUploadPending = true;
}

QShader loadShader(const QString &resourcePath)
{
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QShader::fromSerialized(file.readAll());
}

} // namespace

DsTouchEngineViewRenderer::DsTouchEngineViewRenderer()
    : m_diagnosticProducerId(
          nextRendererDiagnosticProducerId.fetch_add(1, std::memory_order_relaxed))
{
}

DsTouchEngineViewRenderer::~DsTouchEngineViewRenderer()
{
    QObject::disconnect(m_afterFrameConnection);
    releaseResources();
    m_core.reset();
}

void DsTouchEngineViewRenderer::initialize(QRhiCommandBuffer *)
{
    if (m_currentRhi != rhi()) {
        QObject::disconnect(m_afterFrameConnection);
        m_afterFrameConnection = {};
        releaseResources();
        m_core.reset();
        m_renderPassDescriptor = nullptr;
        m_currentRhi = rhi();
    }

    // A QQuickRhiItem can recreate its render target after resize or device
    // loss. Static buffers survive; the pipeline's render-pass compatibility
    // does not.
    if (m_renderPassDescriptor != renderTarget()->renderPassDescriptor()) {
        m_renderPassDescriptor = renderTarget()->renderPassDescriptor();
        releasePipeline();
    }
}

void DsTouchEngineViewRenderer::synchronize(QQuickRhiItem *item)
{
    auto *view = static_cast<DsTouchEngineView *>(item);
    m_item = view;
    m_outputLink = view->outputLink();
    m_clearColor = view->clearColor();

    DsTouchEngineSession *session = view->session();
    const auto nextShared = session ? session->d->shared : std::shared_ptr<TouchEngineSharedState>{};
    if (m_shared != nextShared) {
        QObject::disconnect(m_afterFrameConnection);
        m_afterFrameConnection = {};
        publishInitializationError({});
        // The SRB stores a raw pointer to the core-owned output texture. Drop
        // it before the core/backend so a replacement allocation cannot reuse
        // the same address and trigger the texture-pointer cache fast path.
        releasePipeline();
        releaseInputStaging();
        m_core.reset();
        m_shared = nextShared;
    }

    if (m_window != view->window()) {
        QObject::disconnect(m_afterFrameConnection);
        m_afterFrameConnection = {};
        releaseInputStaging();
        m_window = view->window();
        if (m_core)
            connectAfterFrameEnd();
    }

    m_textureInputs.clear();
    if (!session)
        return;

    m_textureInputs.reserve(session->d->textureInputs.size());
    for (auto it = session->d->textureInputs.cbegin(); it != session->d->textureInputs.cend(); ++it) {
        TextureProviderSource source;
        source.link = it.key();

        QQuickItem *sourceItem = it.value();
        if (!sourceItem) {
            source.status = TextureProviderStatus::SourceDestroyed;
        } else if (sourceItem->window() != view->window()) {
            source.status = TextureProviderStatus::WrongWindow;
        } else if (!sourceItem->isTextureProvider()) {
            source.status = TextureProviderStatus::NotTextureProvider;
        } else if (QSGTextureProvider *provider = sourceItem->textureProvider()) {
            source.texture = provider->texture();
            source.status = source.texture
                ? TextureProviderStatus::Ready
                : TextureProviderStatus::TextureUnavailable;
        } else {
            source.status = TextureProviderStatus::ProviderUnavailable;
        }

        // QQuickRhiItem and image-like providers expose their presentation
        // orientation on the item instead of QSGTexture in current Qt.
        // Capture it while the GUI thread is blocked by synchronize().
        if (sourceItem) {
            const QVariant mirror = sourceItem->property("mirrorVertically");
            source.mirrorVertically = mirror.isValid() && mirror.toBool();
        }
        m_textureInputs.push_back(source);
    }
}

void DsTouchEngineViewRenderer::render(QRhiCommandBuffer *commandBuffer)
{
    bool initializationReady = true;
    if (m_shared && !m_core) {
        QString error;
        m_core = TouchEngineCore::acquire(m_shared, rhi(), commandBuffer, &error);
        if (!m_core) {
            publishInitializationError(error);
            initializationReady = false;
        } else {
            connectAfterFrameEnd();
        }
    }

    QString error;
    if (!ensureStaticResources(&error)) {
        publishInitializationError(error);
        return;
    }

    TextureOutput output;
    if (m_core) {
        // QSG texture providers are not guaranteed to expose transfer-source
        // textures (and may expose an atlas sub-rectangle). Normalize them
        // through a Qt-owned render target before the backend sees them.
        const QVector<TextureInputSource> stagedInputs = stageTextureInputs(commandBuffer);
        m_core->beginQtFrame(commandBuffer, stagedInputs);
        output = m_core->textureOutput(m_outputLink);
    }

    if (!output && m_boundTexture)
        releasePipeline();

    if (output && !ensurePipeline(output.texture, &error)) {
        publishInitializationError(error);
        initializationReady = false;
        output = {};
    }

    if (initializationReady)
        publishInitializationError({});

    QRhiResourceUpdateBatch *updates = rhi()->nextResourceUpdateBatch();
    if (m_staticUploadPending) {
        const auto data = vertices(output.mirrorVertically);
        updates->updateDynamicBuffer(m_vertexBuffer, 0,
                                     static_cast<quint32>(data.size() * sizeof(float)), data.data());
        m_verticesMirrored = output.mirrorVertically;
        m_staticUploadPending = false;
    } else if (output && m_verticesMirrored != output.mirrorVertically) {
        const auto data = vertices(output.mirrorVertically);
        updates->updateDynamicBuffer(m_vertexBuffer, 0,
                                     static_cast<quint32>(data.size() * sizeof(float)), data.data());
        m_verticesMirrored = output.mirrorVertically;
    }

    commandBuffer->beginPass(renderTarget(), m_clearColor, {1.0f, 0}, updates);
    if (output && m_pipeline && m_shaderResources) {
        commandBuffer->setGraphicsPipeline(m_pipeline);
        commandBuffer->setShaderResources(m_shaderResources);
        const QRhiCommandBuffer::VertexInput binding(m_vertexBuffer, 0);
        commandBuffer->setVertexInput(0, 1, &binding);
        commandBuffer->setViewport(QRhiViewport(0, 0,
                                                static_cast<float>(renderTarget()->pixelSize().width()),
                                                static_cast<float>(renderTarget()->pixelSize().height())));
        commandBuffer->draw(4);
    }
    commandBuffer->endPass();

    if ((m_core && m_core->renderLoopNeeded()) || m_textureInputRetryPending)
        update();
}

bool DsTouchEngineViewRenderer::ensureStaticResources(QString *error)
{
    if (m_vertexBuffer)
        return true;

    m_vertexBuffer = rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer,
                                      24 * sizeof(float));
    m_uniformBuffer = rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 80);
    m_sampler = rhi()->newSampler(QRhiSampler::Linear, QRhiSampler::Linear,
                                  QRhiSampler::None,
                                  QRhiSampler::ClampToEdge,
                                  QRhiSampler::ClampToEdge);
    if (!m_vertexBuffer->create() || !m_uniformBuffer->create() || !m_sampler->create()) {
        if (error)
            *error = QStringLiteral("Could not create QRhi resources for DsTouchEngineView");
        releaseResources();
        return false;
    }
    m_staticUploadPending = true;
    return true;
}

bool DsTouchEngineViewRenderer::ensurePipeline(QRhiTexture *texture, QString *error)
{
    if (m_pipeline && m_shaderResources && m_boundTexture == texture)
        return true;

    delete m_shaderResources;
    m_shaderResources = rhi()->newShaderResourceBindings();
    m_shaderResources->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            m_uniformBuffer),
        QRhiShaderResourceBinding::sampledTexture(
            1, QRhiShaderResourceBinding::FragmentStage, texture, m_sampler),
    });
    if (!m_shaderResources->create()) {
        if (error)
            *error = QStringLiteral("Could not create TouchEngine shader bindings");
        delete m_shaderResources;
        m_shaderResources = nullptr;
        return false;
    }

    if (!m_pipeline) {
        const QShader vertexShader = loadShader(QStringLiteral(":/dsqt/touchengine/shaders/texture.vert.qsb"));
        const QShader fragmentShader = loadShader(QStringLiteral(":/dsqt/touchengine/shaders/texture.frag.qsb"));
        if (!vertexShader.isValid() || !fragmentShader.isValid()) {
            if (error)
                *error = QStringLiteral("TouchEngine view shaders are missing or invalid");
            return false;
        }

        m_pipeline = rhi()->newGraphicsPipeline();
        m_pipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);
        m_pipeline->setSampleCount(renderTarget()->sampleCount());
        m_pipeline->setShaderStages({
            {QRhiShaderStage::Vertex, vertexShader},
            {QRhiShaderStage::Fragment, fragmentShader},
        });
        QRhiVertexInputLayout layout;
        layout.setBindings({QRhiVertexInputBinding(VertexStride)});
        layout.setAttributes({
            QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float4, 0),
            QRhiVertexInputAttribute(0, 1, QRhiVertexInputAttribute::Float2, 4 * sizeof(float)),
        });
        m_pipeline->setVertexInputLayout(layout);
        m_pipeline->setShaderResourceBindings(m_shaderResources);
        m_pipeline->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
        if (!m_pipeline->create()) {
            if (error)
                *error = QStringLiteral("Could not create the TouchEngine view graphics pipeline");
            delete m_pipeline;
            m_pipeline = nullptr;
            return false;
        }
    }

    m_boundTexture = texture;
    return true;
}

bool DsTouchEngineViewRenderer::ensureInputStaging(TextureInputStaging *staging,
                                                   const TextureProviderSource &source,
                                                   QRhiTexture *sourceTexture,
                                                   const QSize &pixelSize,
                                                   const QRectF &sourceRect,
                                                   bool mirrorVertically,
                                                   QString *error)
{
    if (!staging || !source.texture || !sourceTexture || pixelSize.isEmpty()) {
        if (error)
            *error = QStringLiteral("The QML texture provider is not ready");
        return false;
    }

    const int textureSizeMax = rhi()->resourceLimit(QRhi::TextureSizeMax);
    if (pixelSize.width() > textureSizeMax || pixelSize.height() > textureSizeMax) {
        if (error) {
            *error = QStringLiteral("The texture size %1x%2 exceeds the RHI limit of %3")
                         .arg(pixelSize.width())
                         .arg(pixelSize.height())
                         .arg(textureSizeMax);
        }
        return false;
    }

    constexpr QRhiTexture::Flags stagingFlags =
        QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource;
    if (!rhi()->isTextureFormatSupported(QRhiTexture::RGBA8, stagingFlags)) {
        if (error)
            *error = QStringLiteral("The active RHI cannot create an RGBA8 transfer-source render target");
        return false;
    }

    if (staging->pixelSize != pixelSize) {
        releaseInputResources(staging);

        staging->texture = rhi()->newTexture(QRhiTexture::RGBA8, pixelSize, 1, stagingFlags);
        if (!staging->texture || !staging->texture->create()) {
            if (error)
                *error = QStringLiteral("Could not create the RGBA8 input staging texture");
            releaseInputResources(staging);
            return false;
        }
        staging->texture->setName(QStringLiteral("DsQt TouchEngine input %1").arg(source.link).toUtf8());

        const QRhiTextureRenderTargetDescription targetDescription(
            QRhiColorAttachment(staging->texture));
        staging->renderTarget = rhi()->newTextureRenderTarget(targetDescription);
        if (!staging->renderTarget) {
            if (error)
                *error = QStringLiteral("Could not allocate the input staging render target");
            releaseInputResources(staging);
            return false;
        }
        staging->renderPassDescriptor =
            staging->renderTarget->newCompatibleRenderPassDescriptor();
        if (!staging->renderPassDescriptor) {
            if (error)
                *error = QStringLiteral("Could not create the input staging render-pass descriptor");
            releaseInputResources(staging);
            return false;
        }
        staging->renderTarget->setRenderPassDescriptor(staging->renderPassDescriptor);
        if (!staging->renderTarget->create()) {
            if (error)
                *error = QStringLiteral("Could not create the input staging render target");
            releaseInputResources(staging);
            return false;
        }

        staging->vertexBuffer = rhi()->newBuffer(QRhiBuffer::Dynamic,
                                                  QRhiBuffer::VertexBuffer,
                                                  24 * sizeof(float));
        if (!staging->vertexBuffer || !staging->vertexBuffer->create()) {
            if (error)
                *error = QStringLiteral("Could not create the input staging vertex buffer");
            releaseInputResources(staging);
            return false;
        }
        staging->pixelSize = pixelSize;
        staging->vertexUploadPending = true;
    }

    const bool sourceChanged = staging->sourceTexture != source.texture.data()
        || staging->sourceRhiTexture != sourceTexture;
    if (sourceChanged)
        releaseInputBindings(staging);

    if (!staging->shaderResources) {
        staging->sourceTexture = source.texture.data();
        staging->sourceRhiTexture = sourceTexture;
        staging->shaderResources = rhi()->newShaderResourceBindings();
        staging->shaderResources->setBindings({
            QRhiShaderResourceBinding::uniformBuffer(
                0,
                QRhiShaderResourceBinding::VertexStage
                    | QRhiShaderResourceBinding::FragmentStage,
                m_uniformBuffer),
            QRhiShaderResourceBinding::sampledTexture(
                1, QRhiShaderResourceBinding::FragmentStage, sourceTexture, m_sampler),
        });
        if (!staging->shaderResources->create()) {
            if (error)
                *error = QStringLiteral("Could not create the input staging shader bindings");
            releaseInputBindings(staging);
            return false;
        }

        const QShader vertexShader =
            loadShader(QStringLiteral(":/dsqt/touchengine/shaders/texture.vert.qsb"));
        const QShader fragmentShader =
            loadShader(QStringLiteral(":/dsqt/touchengine/shaders/texture.frag.qsb"));
        if (!vertexShader.isValid() || !fragmentShader.isValid()) {
            if (error)
                *error = QStringLiteral("TouchEngine texture staging shaders are missing or invalid");
            releaseInputBindings(staging);
            return false;
        }

        staging->pipeline = rhi()->newGraphicsPipeline();
        staging->pipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);
        staging->pipeline->setSampleCount(1);
        staging->pipeline->setShaderStages({
            {QRhiShaderStage::Vertex, vertexShader},
            {QRhiShaderStage::Fragment, fragmentShader},
        });
        QRhiVertexInputLayout layout;
        layout.setBindings({QRhiVertexInputBinding(VertexStride)});
        layout.setAttributes({
            QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float4, 0),
            QRhiVertexInputAttribute(0, 1, QRhiVertexInputAttribute::Float2,
                                     4 * sizeof(float)),
        });
        staging->pipeline->setVertexInputLayout(layout);
        staging->pipeline->setShaderResourceBindings(staging->shaderResources);
        staging->pipeline->setRenderPassDescriptor(staging->renderPassDescriptor);
        if (!staging->pipeline->create()) {
            if (error)
                *error = QStringLiteral("Could not create the input staging graphics pipeline");
            releaseInputBindings(staging);
            return false;
        }
    }

    if (staging->sourceRect != sourceRect
        || staging->mirrorVertically != mirrorVertically) {
        staging->sourceRect = sourceRect;
        staging->mirrorVertically = mirrorVertically;
        staging->vertexUploadPending = true;
    }
    return true;
}

QVector<TextureInputSource> DsTouchEngineViewRenderer::stageTextureInputs(
    QRhiCommandBuffer *commandBuffer)
{
    m_textureInputRetryPending = false;

    QVector<TextureInputSource> stagedInputs;
    stagedInputs.reserve(m_textureInputs.size());

    QSet<QString> currentLinks;
    QSet<QSGTexture *> committedTextures;
    QVector<TextureInputStaging *> renderPasses;
    renderPasses.reserve(m_textureInputs.size());

    QRhiResourceUpdateBatch *updates = rhi()->nextResourceUpdateBatch();

    struct alignas(16) UniformData {
        float matrix[16];
        float opacity;
        float padding[3];
    } uniforms{};
    const QMatrix4x4 matrix = rhi()->clipSpaceCorrMatrix();
    std::memcpy(uniforms.matrix, matrix.constData(), sizeof(uniforms.matrix));
    uniforms.opacity = 1.0f;
    updates->updateDynamicBuffer(m_uniformBuffer, 0, sizeof(uniforms), &uniforms);

    const QRectF unitRect(0.0, 0.0, 1.0, 1.0);
    for (const TextureProviderSource &source : std::as_const(m_textureInputs)) {
        if (source.link.isEmpty())
            continue;
        currentLinks.insert(source.link);

        TextureInputSource staged;
        staged.link = source.link;
        const TextureProviderStatus status =
            source.status == TextureProviderStatus::Ready && !source.texture
            ? TextureProviderStatus::TextureUnavailable
            : source.status;
        if (status != TextureProviderStatus::Ready) {
            releaseInputStaging(source.link);

            QString error;
            switch (status) {
            case TextureProviderStatus::SourceDestroyed:
                error = QStringLiteral("The source item was destroyed; pass a live QQuickItem to setTextureInput(), or call clearTextureInput()");
                break;
            case TextureProviderStatus::WrongWindow:
                error = QStringLiteral("The source item belongs to a different QQuickWindow; the source and DsTouchEngineView must share a window");
                break;
            case TextureProviderStatus::NotTextureProvider:
                error = QStringLiteral("The source item is not a scene-graph texture provider; use an Image, QQuickRhiItem, ShaderEffectSource, or another item whose isTextureProvider() returns true");
                break;
            case TextureProviderStatus::ProviderUnavailable:
                error = QStringLiteral("The source item's texture provider is temporarily unavailable; rendering will retry automatically");
                m_textureInputRetryPending = true;
                break;
            case TextureProviderStatus::TextureUnavailable:
                error = QStringLiteral("The source texture is not ready yet; rendering will retry automatically");
                m_textureInputRetryPending = true;
                break;
            case TextureProviderStatus::Ready:
                break;
            }
            publishInputError(source.link, error);
            stagedInputs.push_back(staged);
            continue;
        }

        if (!committedTextures.contains(source.texture.data())) {
            if (auto *dynamicTexture = qobject_cast<QSGDynamicTexture *>(source.texture.data()))
                dynamicTexture->updateTexture();
            source.texture->commitTextureOperations(rhi(), updates);
            committedTextures.insert(source.texture.data());
        }

        QRhiTexture *sourceTexture = source.texture->rhiTexture();
        const QRectF providedRect = source.texture->normalizedTextureSubRect();
        const bool finiteRect = std::isfinite(providedRect.x())
            && std::isfinite(providedRect.y())
            && std::isfinite(providedRect.width())
            && std::isfinite(providedRect.height());
        const QRectF sourceRect = finiteRect ? providedRect.intersected(unitRect) : QRectF{};
        const bool rectUsable = finiteRect
            && providedRect.width() > 0.0
            && providedRect.height() > 0.0
            && !sourceRect.isEmpty();
        const QSize pixelSize = stagingPixelSize(source.texture.data(), sourceTexture, sourceRect);
        const bool mirrorVertically = source.mirrorVertically
            != isYInverted(source.texture.data());

        QString error;
        bool retryable = false;
        if (!sourceTexture || pixelSize.isEmpty() || !rectUsable) {
            error = QStringLiteral("The QML texture provider has no usable texture or source rectangle");
            // A missing native texture or extent is a normal scene-graph
            // creation/replacement state. Invalid numeric/source-rectangle
            // data is a hard provider contract failure and must not spin the
            // render loop indefinitely.
            retryable = !sourceTexture || (pixelSize.isEmpty() && rectUsable);
        }

        TextureInputStaging *staging = nullptr;
        if (error.isEmpty()) {
            staging = m_inputStaging.value(source.link);
            if (!staging) {
                staging = new TextureInputStaging;
                m_inputStaging.insert(source.link, staging);
            }
            if (!ensureInputStaging(staging, source, sourceTexture, pixelSize,
                                    sourceRect, mirrorVertically, &error)) {
                releaseInputStaging(source.link);
                staging = nullptr;
            }
        } else {
            releaseInputStaging(source.link);
        }

        if (!staging) {
            publishInputError(source.link, error);
            if (retryable)
                m_textureInputRetryPending = true;
            stagedInputs.push_back(staged);
            continue;
        }

        publishInputError(source.link, {});
        if (staging->vertexUploadPending) {
            const auto data = vertices(staging->sourceRect, staging->mirrorVertically);
            updates->updateDynamicBuffer(staging->vertexBuffer, 0,
                                         static_cast<quint32>(data.size() * sizeof(float)),
                                         data.data());
            staging->vertexUploadPending = false;
        }

        staged.texture = staging->texture;
        staged.pixelSize = staging->pixelSize;
        staged.normalizedSourceRect = unitRect;
        stagedInputs.push_back(staged);
        renderPasses.push_back(staging);
    }

    pruneInputStaging(currentLinks);
    const QStringList diagnosedLinks = m_inputErrors.keys();
    for (const QString &link : diagnosedLinks) {
        if (!currentLinks.contains(link))
            publishInputError(link, {});
    }

    // This batch contains provider uploads, input-quad vertices, and the
    // shared transform. Submit it before any staging pass samples a provider.
    commandBuffer->resourceUpdate(updates);
    for (TextureInputStaging *staging : std::as_const(renderPasses)) {
        commandBuffer->beginPass(staging->renderTarget, Qt::transparent, {1.0f, 0});
        commandBuffer->setGraphicsPipeline(staging->pipeline);
        commandBuffer->setShaderResources(staging->shaderResources);
        const QRhiCommandBuffer::VertexInput binding(staging->vertexBuffer, 0);
        commandBuffer->setVertexInput(0, 1, &binding);
        commandBuffer->setViewport(QRhiViewport(
            0, 0,
            static_cast<float>(staging->pixelSize.width()),
            static_cast<float>(staging->pixelSize.height())));
        commandBuffer->draw(4);
        commandBuffer->endPass();
    }

    return stagedInputs;
}

void DsTouchEngineViewRenderer::pruneInputStaging(const QSet<QString> &activeLinks)
{
    for (auto it = m_inputStaging.begin(); it != m_inputStaging.end();) {
        if (activeLinks.contains(it.key())) {
            ++it;
            continue;
        }
        releaseInputResources(it.value());
        delete it.value();
        it = m_inputStaging.erase(it);
    }
}

void DsTouchEngineViewRenderer::releaseInputStaging(const QString &link)
{
    const auto it = m_inputStaging.find(link);
    if (it == m_inputStaging.end())
        return;
    releaseInputResources(it.value());
    delete it.value();
    m_inputStaging.erase(it);
}

void DsTouchEngineViewRenderer::releaseInputStaging()
{
    for (TextureInputStaging *staging : std::as_const(m_inputStaging)) {
        releaseInputResources(staging);
        delete staging;
    }
    m_inputStaging.clear();
    const QStringList diagnosedLinks = m_inputErrors.keys();
    for (const QString &link : diagnosedLinks)
        publishInputError(link, {});
    m_textureInputRetryPending = false;
}

void DsTouchEngineViewRenderer::releasePipeline()
{
    delete m_pipeline;
    m_pipeline = nullptr;
    delete m_shaderResources;
    m_shaderResources = nullptr;
    m_boundTexture = nullptr;
}

void DsTouchEngineViewRenderer::releaseResources()
{
    publishInitializationError({});
    releasePipeline();
    // Input pipelines and SRBs refer to the static sampler/uniform buffer;
    // render targets refer to their staging textures. Drop them in dependency
    // order before releasing any of those referenced resources.
    releaseInputStaging();
    delete m_sampler;
    m_sampler = nullptr;
    delete m_uniformBuffer;
    m_uniformBuffer = nullptr;
    delete m_vertexBuffer;
    m_vertexBuffer = nullptr;
}

void DsTouchEngineViewRenderer::connectAfterFrameEnd()
{
    if (m_afterFrameConnection || !m_window || !m_item || !m_core)
        return;

    const std::weak_ptr<TouchEngineCore> weakCore = m_core;
    m_afterFrameConnection = QObject::connect(
        m_window, &QQuickWindow::afterFrameEnd, m_item,
        [this, weakCore] {
            if (auto core = weakCore.lock()) {
                core->afterFrameEnd();
                if (core->recoveryRequired()) {
                    // Configure/Unload cannot safely run while the old backend
                    // still owns an unresolved transfer. Quiesce Qt's queue,
                    // drop every descriptor that can reference core-owned
                    // textures, and let the next render acquire a fresh core.
                    // Its durable desired-state snapshot already contains the
                    // user's latest load or unload request.
                    QObject::disconnect(m_afterFrameConnection);
                    m_afterFrameConnection = {};
                    if (rhi())
                        rhi()->finish();
                    releasePipeline();
                    releaseInputStaging();
                    m_core.reset();
                    update();
                    return;
                }
                // Transfer failures are only known after submission. A paused
                // session may not otherwise have scheduled another frame in
                // render(), so explicitly keep retry processing alive.
                if (core->renderLoopNeeded())
                    update();
            }
        },
        Qt::DirectConnection);
}

void DsTouchEngineViewRenderer::publishInitializationError(const QString &error)
{
    if (!m_shared || m_initializationError == error)
        return;
    m_initializationError = error;

    Event message;
    message.kind = EventKind::Error;
    message.diagnosticKey = rendererInitializationDiagnosticKey(m_diagnosticProducerId);
    message.message = error;
    m_shared->pushEvent(std::move(message));
}

void DsTouchEngineViewRenderer::publishInputError(const QString &link, const QString &error)
{
    if (!m_shared || m_inputErrors.value(link) == error)
        return;

    if (error.isEmpty())
        m_inputErrors.remove(link);
    else
        m_inputErrors.insert(link, error);

    Event message;
    message.kind = EventKind::Error;
    message.diagnosticKey = textureSourceDiagnosticKey(m_diagnosticProducerId, link);
    message.message = error.isEmpty()
        ? QString()
        : QStringLiteral("Could not stage texture input '%1': %2").arg(link, error);
    m_shared->pushEvent(std::move(message));
}

} // namespace dsqt::touchengine::detail
