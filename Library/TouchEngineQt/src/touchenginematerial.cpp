#include "touchenginematerial.h"

// TouchEngineMaterial implementation
TouchEngineMaterial::TouchEngineMaterial()
{
    setFlag(Blending);
}

QSGMaterialType* TouchEngineMaterial::type() const
{
    static QSGMaterialType type;
    return &type;
}

QSGMaterialShader* TouchEngineMaterial::createShader(QSGRendererInterface::RenderMode renderMode) const
{
    Q_UNUSED(renderMode);
    return new TouchEngineMaterialShader();
}

int TouchEngineMaterial::compare(const QSGMaterial *other) const
{
    const TouchEngineMaterial* otherMaterial = static_cast<const TouchEngineMaterial*>(other);

    if (m_texture == otherMaterial->m_texture)
        return 0;

    return (m_texture < otherMaterial->m_texture) ? -1 : 1;
}

void TouchEngineMaterial::setTexture(QSGTexture* texture)
{
    m_texture = texture;
}

void TouchEngineMaterial::setFiltering(QSGTexture::Filtering filtering)
{
    m_filtering = filtering;
    if (m_texture) {
        m_texture->setFiltering(filtering);
    }
}

void TouchEngineMaterial::setMipmapFiltering(QSGTexture::Filtering filtering)
{
    m_mipmapFiltering = filtering;
    if (m_texture) {
        m_texture->setMipmapFiltering(filtering);
    }
}

// TouchEngineMaterialShader implementation
TouchEngineMaterialShader::TouchEngineMaterialShader()
{
    // Load pre-compiled .qsb shader files
    setShaderFileName(VertexStage, QStringLiteral(":/resources/shaders/texture.vert.qsb"));
    setShaderFileName(FragmentStage, QStringLiteral(":/resources/shaders/texture.frag.qsb"));
}

bool TouchEngineMaterialShader::updateUniformData(RenderState& state,
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

void TouchEngineMaterialShader::updateSampledImage(RenderState& state,
                                                   int binding,
                                                   QSGTexture** texture,
                                                   QSGMaterial* newMaterial,
                                                   QSGMaterial* oldMaterial)
{
    Q_UNUSED(state);
    Q_UNUSED(oldMaterial);

    if (binding == 1) {
        TouchEngineMaterial* material = static_cast<TouchEngineMaterial*>(newMaterial);
        *texture = material->texture();

        if (*texture) {
            (*texture)->setFiltering(material->filtering());
            (*texture)->setMipmapFiltering(material->mipmapFiltering());
        }
    }
}
