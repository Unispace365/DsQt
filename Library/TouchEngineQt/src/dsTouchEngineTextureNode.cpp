#include "dsTouchEngineTextureNode.h"
#include "dsTouchEngineMaterial.h"
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <rhi/qrhi.h>
DsTouchEngineTextureNode::DsTouchEngineTextureNode(QQuickWindow* window,QString instanceId,QString linkName)
    : m_window(window)
{

    //TODO: test if this can be deleted.
    if (m_window) {
        QSGRendererInterface* rif = m_window->rendererInterface();
        if (rif) {
            QRhi* rhi = static_cast<QRhi*>(
                rif->getResource(m_window, QSGRendererInterface::RhiResource));

            if (rhi) {
                //m_textureBridge = new RHITextureBridge(rhi, this->m_window);
            }
        }
    }

    // Create geometry (4 vertices for a textured quad)
    m_geometry = new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4);
    m_geometry->setDrawingMode(QSGGeometry::DrawTriangleStrip);
    setGeometry(m_geometry);

    // Create and set material
    m_material = new DsTouchEngineMaterial();
    setMaterial(m_material);

    // We own the geometry and material
    setFlag(QSGNode::OwnsGeometry);
    setFlag(QSGNode::OwnsMaterial);
    setFlag(QSGNode::UsePreprocess, false);
    setFlag(QSGNode::OwnedByParent, false);
}

DsTouchEngineTextureNode::~DsTouchEngineTextureNode()
{
    if (m_ownsTexture) {
        delete m_textureWrapper;
    }

    // geometry and material are deleted automatically due to OwnsGeometry/OwnsMaterial flags
}

void DsTouchEngineTextureNode::setTexture(QSharedPointer<QRhiTexture> rhiTex, const QSize& size)
{
    if (!rhiTex) {
        return;
    }

    //qDebug()<<"resource type:"<<rhiTex->resourceType();

    // Check if handle changed
    bool empty = false;
    if(!m_currentRhiTexture){
        empty=true;
    }
    if (empty || m_currentRhiTexture->nativeTexture().object != rhiTex->nativeTexture().object) {
        m_currentRhiTexture = rhiTex;

        if (rhiTex) {
            // Create wrapper
            if (m_ownsTexture) {
                delete m_textureWrapper;
            }
            m_textureWrapper = new RHITextureWrapper(rhiTex.get());

            // Set texture on material
            if (m_material) {
                m_material->setTexture(m_textureWrapper);
                m_material->setFiltering(m_filtering);
                m_material->setMipmapFiltering(m_mipmapFiltering);
            }

            // Set target rectangle (where to draw in scene coordinates)
            setRect(QRectF(0, 0, size.width(), size.height()));

            // Set source rectangle (what part of texture to use, normalized 0-1)
            setSourceRect(QRectF(0, 0, 1, 1));

            //markDirty(QSGNode::DirtyMaterial | QSGNode::DirtyGeometry);
        }
    }
}

// QSGImageNode interface implementation
void DsTouchEngineTextureNode::setRect(const QRectF &rect)
{
    if (m_rect != rect) {
        m_rect = rect;
        updateGeometry();
        markDirty(QSGNode::DirtyGeometry);
    }
}

QRectF DsTouchEngineTextureNode::rect() const
{
    return m_rect;
}

void DsTouchEngineTextureNode::setSourceRect(const QRectF &rect)
{
    if (m_sourceRect != rect) {
        m_sourceRect = rect;
        updateGeometry();
        markDirty(QSGNode::DirtyGeometry);
    }
}

QRectF DsTouchEngineTextureNode::sourceRect() const
{
    return m_sourceRect;
}

void DsTouchEngineTextureNode::setTexture(QSGTexture *texture)
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

QSGTexture* DsTouchEngineTextureNode::texture() const
{
    return m_textureWrapper;
}

void DsTouchEngineTextureNode::setFiltering(QSGTexture::Filtering filtering)
{
    if (m_filtering != filtering) {
        m_filtering = filtering;

        if (m_material) {
            m_material->setFiltering(filtering);
        }

        markDirty(QSGNode::DirtyMaterial);
    }
}

QSGTexture::Filtering DsTouchEngineTextureNode::filtering() const
{
    return m_filtering;
}

void DsTouchEngineTextureNode::setMipmapFiltering(QSGTexture::Filtering filtering)
{
    if (m_mipmapFiltering != filtering) {
        m_mipmapFiltering = filtering;

        if (m_material) {
            m_material->setMipmapFiltering(filtering);
        }

        markDirty(QSGNode::DirtyMaterial);
    }
}

QSGTexture::Filtering DsTouchEngineTextureNode::mipmapFiltering() const
{
    return m_mipmapFiltering;
}

void DsTouchEngineTextureNode::setTextureCoordinatesTransform(TextureCoordinatesTransformMode mode)
{
    if (m_transformMode != mode) {
        m_transformMode = mode;
        markDirty(QSGNode::DirtyGeometry);
    }
}

DsTouchEngineTextureNode::TextureCoordinatesTransformMode DsTouchEngineTextureNode::textureCoordinatesTransform() const
{
    return m_transformMode;
}

void DsTouchEngineTextureNode::setAnisotropyLevel(QSGTexture::AnisotropyLevel level)
{
    if (m_anisotropyLevel != level) {
        m_anisotropyLevel = level;
        markDirty(QSGNode::DirtyMaterial);
    }
}

QSGTexture::AnisotropyLevel DsTouchEngineTextureNode::anisotropyLevel() const
{
    return m_anisotropyLevel;
}

void DsTouchEngineTextureNode::setOwnsTexture(bool owns)
{
    m_ownsTexture = owns;
}

bool DsTouchEngineTextureNode::ownsTexture() const
{
    return m_ownsTexture;
}

// RHITextureWrapper implementation
DsTouchEngineTextureNode::RHITextureWrapper::RHITextureWrapper(QRhiTexture* rhiTexture)
    : m_rhiTexture(rhiTexture)
{
    if (rhiTexture) {
        m_size = rhiTexture->pixelSize();
    }
}

qint64 DsTouchEngineTextureNode::RHITextureWrapper::comparisonKey() const
{
    return qint64(m_rhiTexture);
}

QSize DsTouchEngineTextureNode::RHITextureWrapper::textureSize() const
{
    return m_size;
}

// Update geometry based on rect and sourceRect
void DsTouchEngineTextureNode::updateGeometry()
{
    if (!m_geometry) {
        return;
    }

    QSGGeometry::TexturedPoint2D* vertices = m_geometry->vertexDataAsTexturedPoint2D();

    // Target rectangle (where to draw)
    float x1 = m_rect.x();
    float y1 = m_rect.y();
    float x2 = m_rect.x() + m_rect.width();
    float y2 = m_rect.y() + m_rect.height();

    // Source rectangle (texture coordinates)
    float tx1 = m_sourceRect.x();
    float ty1 = m_sourceRect.y();
    float tx2 = m_sourceRect.x() + m_sourceRect.width();
    float ty2 = m_sourceRect.y() + m_sourceRect.height();

    // Apply texture coordinate transform if needed
    if (m_transformMode == MirrorHorizontally) {
        std::swap(tx1, tx2);
    } else if (m_transformMode == MirrorVertically) {
        std::swap(ty1, ty2);
    }

    // Set vertex positions and texture coordinates (triangle strip order)
    vertices[0].set(x1, y1, tx1, ty1); // Top-left
    vertices[1].set(x1, y2, tx1, ty2); // Bottom-left
    vertices[2].set(x2, y1, tx2, ty1); // Top-right
    vertices[3].set(x2, y2, tx2, ty2); // Bottom-right
}
