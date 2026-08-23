#pragma once

#include "../TouchEngineRhiBackend_p.h"

#include <memory>

namespace dsqt::touchengine::detail {

class OpenGLTouchEngineBackend final : public TouchEngineRhiBackend {
  public:
    OpenGLTouchEngineBackend();
    ~OpenGLTouchEngineBackend() override;

    DsTouchEngineTypes::GraphicsApi graphicsApi() const noexcept override;
    bool                            initialize(QRhi* rhi, QRhiCommandBuffer* commandBuffer, QString* error) override;
    TEGraphicsContext*              graphicsContext() const noexcept override;
    bool                            configureInstance(TEInstance* instance, QString* error) override;
    bool prepareTextureInput(TEInstance* instance, const TextureInputSource& source, QRhiCommandBuffer* commandBuffer,
                             QString* error) override;
    bool updateTextureOutput(TEInstance* instance, const QString& link, TETexture* texture,
                             QRhiCommandBuffer* commandBuffer, QString* error) override;
    TextureOutput textureOutput(const QString& link) const override;
    void          clearTextureOutput(const QString& link) override;
    void          clearTextureOutputs() override;
    bool          afterFrameEnd(TEInstance* instance, QString* error) override;

  private:
    class Impl;
    std::unique_ptr<Impl> d;
};

} // namespace dsqt::touchengine::detail
