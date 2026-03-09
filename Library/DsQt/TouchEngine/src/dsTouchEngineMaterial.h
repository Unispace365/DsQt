#pragma once

#include <QSGMaterial>
#include <QSGMaterialShader>
#include <QSGTexture>

/**
 * Material for rendering TouchEngine textures with RHI
 */
class DsTouchEngineMaterial : public QSGMaterial
{
public:
    DsTouchEngineMaterial();

    QSGMaterialType* type() const override;
    QSGMaterialShader* createShader(QSGRendererInterface::RenderMode renderMode) const override;
    int compare(const QSGMaterial *other) const override;

    void setTexture(QSGTexture* texture);
    QSGTexture* texture() const { return m_texture; }

    void setFiltering(QSGTexture::Filtering filtering);
    QSGTexture::Filtering filtering() const { return m_filtering; }

    void setMipmapFiltering(QSGTexture::Filtering filtering);
    QSGTexture::Filtering mipmapFiltering() const { return m_mipmapFiltering; }

private:
    QSGTexture* m_texture = nullptr;
    QSGTexture::Filtering m_filtering = QSGTexture::Linear;
    QSGTexture::Filtering m_mipmapFiltering = QSGTexture::None;
};

/**
 * RHI-based shader for TouchEngine material
 */
class DsTouchEngineMaterialShader : public QSGMaterialShader
{
public:
    DsTouchEngineMaterialShader();

    bool updateUniformData(RenderState& state,
                           QSGMaterial* newMaterial,
                           QSGMaterial* oldMaterial) override;

    void updateSampledImage(RenderState& state,
                            int binding,
                            QSGTexture** texture,
                            QSGMaterial* newMaterial,
                            QSGMaterial* oldMaterial) override;
};
