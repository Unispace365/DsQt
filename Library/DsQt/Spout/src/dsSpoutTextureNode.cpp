#include "dsSpoutTextureNode.h"
#include "dsSpoutMaterial.h"
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <rhi/qrhi.h>

DsSpoutTextureNode::DsSpoutTextureNode(QQuickWindow* window)
    : m_window(window)
{
    // Create geometry (4 vertices for a textured quad)
    m_geometry = new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4);
    m_geometry->setDrawingMode(QSGGeometry::DrawTriangleStrip);
    setGeometry(m_geometry);

    // Create and set material
    m_material = new DsSpoutMaterial();
    setMaterial(m_material);

    // We own the geometry and material
    setFlag(QSGNode::OwnsGeometry);
    setFlag(QSGNode::OwnsMaterial);
    setFlag(QSGNode::UsePreprocess, false);
    setFlag(QSGNode::OwnedByParent, false);
}

DsSpoutTextureNode::~DsSpoutTextureNode()
{
    m_currentRhiTexture.reset();
    if (m_ownsTexture) {
        delete m_textureWrapper;
    }
}

void DsSpoutTextureNode::setTexture(QSharedPointer<QRhiTexture> rhiTex, const QSize& size)
{
    if (!rhiTex) {
        return;
    }

    bool empty = !m_currentRhiTexture;
    if (empty || m_currentRhiTexture->nativeTexture().object != rhiTex->nativeTexture().object) {
        m_currentRhiTexture = rhiTex;

        if (m_ownsTexture) {
            delete m_textureWrapper;
        }
        m_textureWrapper = new RHITextureWrapper(rhiTex.get());

        if (m_material) {
            m_material->setTexture(m_textureWrapper);
            m_material->setFiltering(m_filtering);
            m_material->setMipmapFiltering(m_mipmapFiltering);
        }

        setRect(QRectF(0, 0, size.width(), size.height()));
        setSourceRect(QRectF(0, 0, 1, 1));
    }
}

void DsSpoutTextureNode::setRect(const QRectF &rect)
{
    if (m_rect != rect) {
        m_rect = rect;
        updateGeometry();
        markDirty(QSGNode::DirtyGeometry);
    }
}

QRectF DsSpoutTextureNode::rect() const
{
    return m_rect;
}

void DsSpoutTextureNode::setSourceRect(const QRectF &rect)
{
    if (m_sourceRect != rect) {
        m_sourceRect = rect;
        updateGeometry();
        markDirty(QSGNode::DirtyGeometry);
    }
}

QRectF DsSpoutTextureNode::sourceRect() const
{
    return m_sourceRect;
}

void DsSpoutTextureNode::setTexture(QSGTexture *texture)
{
    if (m_textureWrapper != texture) {
        if (m_ownsTexture) {
            delete m_textureWrapper;
        }
        m_textureWrapper = static_cast<RHITextureWrapper*>(texture);

        if (m_material) {
            m_material->setTexture(m_textureWrapper);
        }

        markDirty(QSGNode::DirtyMaterial);
    }
}

QSGTexture* DsSpoutTextureNode::texture() const
{
    return m_textureWrapper;
}

void DsSpoutTextureNode::setFiltering(QSGTexture::Filtering filtering)
{
    if (m_filtering != filtering) {
        m_filtering = filtering;

        if (m_material) {
            m_material->setFiltering(filtering);
        }

        markDirty(QSGNode::DirtyMaterial);
    }
}

QSGTexture::Filtering DsSpoutTextureNode::filtering() const
{
    return m_filtering;
}

void DsSpoutTextureNode::setMipmapFiltering(QSGTexture::Filtering filtering)
{
    if (m_mipmapFiltering != filtering) {
        m_mipmapFiltering = filtering;

        if (m_material) {
            m_material->setMipmapFiltering(filtering);
        }

        markDirty(QSGNode::DirtyMaterial);
    }
}

QSGTexture::Filtering DsSpoutTextureNode::mipmapFiltering() const
{
    return m_mipmapFiltering;
}

void DsSpoutTextureNode::setTextureCoordinatesTransform(TextureCoordinatesTransformMode mode)
{
    if (m_transformMode != mode) {
        m_transformMode = mode;
        updateGeometry();
        markDirty(QSGNode::DirtyGeometry);
    }
}

DsSpoutTextureNode::TextureCoordinatesTransformMode DsSpoutTextureNode::textureCoordinatesTransform() const
{
    return m_transformMode;
}

void DsSpoutTextureNode::setAnisotropyLevel(QSGTexture::AnisotropyLevel level)
{
    if (m_anisotropyLevel != level) {
        m_anisotropyLevel = level;
        markDirty(QSGNode::DirtyMaterial);
    }
}

QSGTexture::AnisotropyLevel DsSpoutTextureNode::anisotropyLevel() const
{
    return m_anisotropyLevel;
}

void DsSpoutTextureNode::setOwnsTexture(bool owns)
{
    m_ownsTexture = owns;
}

bool DsSpoutTextureNode::ownsTexture() const
{
    return m_ownsTexture;
}

// RHITextureWrapper implementation
DsSpoutTextureNode::RHITextureWrapper::RHITextureWrapper(QRhiTexture* rhiTexture)
    : m_rhiTexture(rhiTexture)
{
    if (rhiTexture) {
        m_size = rhiTexture->pixelSize();
    }
}

qint64 DsSpoutTextureNode::RHITextureWrapper::comparisonKey() const
{
    return qint64(m_rhiTexture);
}

QSize DsSpoutTextureNode::RHITextureWrapper::textureSize() const
{
    return m_size;
}

void DsSpoutTextureNode::updateGeometry()
{
    if (!m_geometry) {
        return;
    }

    QSGGeometry::TexturedPoint2D* vertices = m_geometry->vertexDataAsTexturedPoint2D();

    float x1 = m_rect.x();
    float y1 = m_rect.y();
    float x2 = m_rect.x() + m_rect.width();
    float y2 = m_rect.y() + m_rect.height();

    float tx1 = m_sourceRect.x();
    float ty1 = m_sourceRect.y();
    float tx2 = m_sourceRect.x() + m_sourceRect.width();
    float ty2 = m_sourceRect.y() + m_sourceRect.height();

    if (m_transformMode & MirrorHorizontally) {
        std::swap(tx1, tx2);
    }
    if (m_transformMode & MirrorVertically) {
        std::swap(ty1, ty2);
    }

    // Triangle strip order
    vertices[0].set(x1, y1, tx1, ty1); // Top-left
    vertices[1].set(x1, y2, tx1, ty2); // Bottom-left
    vertices[2].set(x2, y1, tx2, ty1); // Top-right
    vertices[3].set(x2, y2, tx2, ty2); // Bottom-right
}
