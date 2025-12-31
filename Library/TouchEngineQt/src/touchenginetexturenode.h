#pragma once

#include <QSGImageNode>
#include <QSGTexture>
#include <QSGGeometry>
#include <QQuickWindow>

class RHITextureBridge;
class TouchEngineMaterial;

/**
 * TouchEngineTextureNode - Scene graph node for rendering TouchEngine textures
 * Uses Qt RHI for cross-platform texture rendering
 */
class TouchEngineTextureNode : public QSGImageNode
{
public:
    explicit TouchEngineTextureNode(QQuickWindow* window,QString instanceId,QString linkName);
    ~TouchEngineTextureNode() override;

    void setTexture(QSharedPointer<QRhiTexture> rhiTex, const QSize& size);

    // QSGImageNode interface
    void setRect(const QRectF &rect) override;
    QRectF rect() const override;

    void setSourceRect(const QRectF &rect) override;
    QRectF sourceRect() const override;

    void setTexture(QSGTexture *texture) override;
    QSGTexture* texture() const override;

    void setFiltering(QSGTexture::Filtering filtering) override;
    QSGTexture::Filtering filtering() const override;

    void setMipmapFiltering(QSGTexture::Filtering filtering) override;
    QSGTexture::Filtering mipmapFiltering() const override;

    void setTextureCoordinatesTransform(TextureCoordinatesTransformMode mode) override;
    TextureCoordinatesTransformMode textureCoordinatesTransform() const override;

    void setAnisotropyLevel(QSGTexture::AnisotropyLevel level) override;
    QSGTexture::AnisotropyLevel anisotropyLevel() const override;

    void setOwnsTexture(bool owns) override;
    bool ownsTexture() const override;

private:
    class RHITextureWrapper : public QSGTexture {
    public:
        explicit RHITextureWrapper(QRhiTexture* rhiTexture);
        ~RHITextureWrapper() override = default;

        qint64 comparisonKey() const override;
        QSize textureSize() const override;
        bool hasAlphaChannel() const override { return true; }
        bool hasMipmaps() const override { return false; }

        QRhiTexture* rhiTexture() const override { return m_rhiTexture; }

    private:
        QRhiTexture* m_rhiTexture = nullptr;
        QSize m_size;
    };

    QQuickWindow* m_window = nullptr;
    RHITextureWrapper* m_textureWrapper = nullptr;
    QSharedPointer<QRhiTexture> m_currentRhiTexture = nullptr;
    void* m_currentHandle = nullptr;

    // Geometry and material
    QSGGeometry* m_geometry = nullptr;
    TouchEngineMaterial* m_material = nullptr;

    // QSGImageNode state
    QRectF m_rect;
    QRectF m_sourceRect;
    QSGTexture::Filtering m_filtering = QSGTexture::Linear;
    QSGTexture::Filtering m_mipmapFiltering = QSGTexture::None;
    TextureCoordinatesTransformMode m_transformMode = NoTransform;
    QSGTexture::AnisotropyLevel m_anisotropyLevel = QSGTexture::AnisotropyNone;
    bool m_ownsTexture = false;

    void updateGeometry();
};
