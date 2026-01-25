#include "dsTouchEngineMaterial.h"

// DsTouchEngineMaterial implementation
DsTouchEngineMaterial::DsTouchEngineMaterial()
{
    setFlag(Blending);
}

QSGMaterialType* DsTouchEngineMaterial::type() const
{
    static QSGMaterialType type;
    return &type;
}

QSGMaterialShader* DsTouchEngineMaterial::createShader(QSGRendererInterface::RenderMode renderMode) const
{
    Q_UNUSED(renderMode);
    return new DsTouchEngineMaterialShader();
}

int DsTouchEngineMaterial::compare(const QSGMaterial *other) const
{
    const DsTouchEngineMaterial* otherMaterial = static_cast<const DsTouchEngineMaterial*>(other);

    if (m_texture == otherMaterial->m_texture)
        return 0;

    return (m_texture < otherMaterial->m_texture) ? -1 : 1;
}

void DsTouchEngineMaterial::setTexture(QSGTexture* texture)
{
    m_texture = texture;
}

void DsTouchEngineMaterial::setFiltering(QSGTexture::Filtering filtering)
{
    m_filtering = filtering;
    if (m_texture) {
        m_texture->setFiltering(filtering);
    }
}

void DsTouchEngineMaterial::setMipmapFiltering(QSGTexture::Filtering filtering)
{
    m_mipmapFiltering = filtering;
    if (m_texture) {
        m_texture->setMipmapFiltering(filtering);
    }
}

// DsTouchEngineMaterialShader implementation
DsTouchEngineMaterialShader::DsTouchEngineMaterialShader()
{
    // Load pre-compiled .qsb shader files
    setShaderFileName(VertexStage, QStringLiteral(":/resources/shaders/texture.vert.qsb"));
    setShaderFileName(FragmentStage, QStringLiteral(":/resources/shaders/texture.frag.qsb"));
}

bool DsTouchEngineMaterialShader::updateUniformData(RenderState& state,
                                                  QSGMaterial* newMaterial,
                                                  QSGMaterial* oldMaterial)
{
    Q_UNUSED(newMaterial);
    Q_UNUSED(oldMaterial);

    bool changed = false;
    QByteArray* buf = state.uniformData();

    if (state.isMatrixDirty()) {
        const QMatrix4x4 matrix = state.combinedMatrix();
        memcpy(buf->data(), matrix.constData(), 64);
        changed = true;
    }

    if (state.isOpacityDirty()) {
        const float opacity = state.opacity();
        memcpy(buf->data() + 64, &opacity, 4);
        changed = true;
    }

    return changed;
}

void DsTouchEngineMaterialShader::updateSampledImage(RenderState& state,
                                                   int binding,
                                                   QSGTexture** texture,
                                                   QSGMaterial* newMaterial,
                                                   QSGMaterial* oldMaterial)
{
    Q_UNUSED(state);
    Q_UNUSED(oldMaterial);

    if (binding == 1) {
        DsTouchEngineMaterial* material = static_cast<DsTouchEngineMaterial*>(newMaterial);
        *texture = material->texture();

        if (*texture) {
            (*texture)->setFiltering(material->filtering());
            (*texture)->setMipmapFiltering(material->mipmapFiltering());
        }
    }
}
