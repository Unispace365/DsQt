#include "dsSpoutMaterial.h"

// DsSpoutMaterial implementation
DsSpoutMaterial::DsSpoutMaterial()
{
    setFlag(Blending);
}

QSGMaterialType* DsSpoutMaterial::type() const
{
    static QSGMaterialType type;
    return &type;
}

QSGMaterialShader* DsSpoutMaterial::createShader(QSGRendererInterface::RenderMode renderMode) const
{
    Q_UNUSED(renderMode);
    return new DsSpoutMaterialShader();
}

int DsSpoutMaterial::compare(const QSGMaterial *other) const
{
    const DsSpoutMaterial* otherMaterial = static_cast<const DsSpoutMaterial*>(other);

    if (m_texture == otherMaterial->m_texture)
        return 0;

    return (m_texture < otherMaterial->m_texture) ? -1 : 1;
}

void DsSpoutMaterial::setTexture(QSGTexture* texture)
{
    m_texture = texture;
}

void DsSpoutMaterial::setFiltering(QSGTexture::Filtering filtering)
{
    m_filtering = filtering;
    if (m_texture) {
        m_texture->setFiltering(filtering);
    }
}

void DsSpoutMaterial::setMipmapFiltering(QSGTexture::Filtering filtering)
{
    m_mipmapFiltering = filtering;
    if (m_texture) {
        m_texture->setMipmapFiltering(filtering);
    }
}

// DsSpoutMaterialShader implementation
DsSpoutMaterialShader::DsSpoutMaterialShader()
{
    // Ensure shader resources are initialized (needed for static library linking)
    Q_INIT_RESOURCE(dsqt_spout_shaders);

    setShaderFileName(VertexStage,
                      QStringLiteral(":/resources/shaders/texture.vert.qsb"));
    setShaderFileName(FragmentStage,
                      QStringLiteral(":/resources/shaders/texture.frag.qsb"));
}

bool DsSpoutMaterialShader::updateUniformData(RenderState& state,
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

void DsSpoutMaterialShader::updateSampledImage(RenderState& state,
                                                int binding,
                                                QSGTexture** texture,
                                                QSGMaterial* newMaterial,
                                                QSGMaterial* oldMaterial)
{
    Q_UNUSED(state);
    Q_UNUSED(oldMaterial);

    if (binding == 1) {
        DsSpoutMaterial* material = static_cast<DsSpoutMaterial*>(newMaterial);
        *texture = material->texture();

        if (*texture) {
            (*texture)->setFiltering(material->filtering());
            (*texture)->setMipmapFiltering(material->mipmapFiltering());
        }
    }
}
