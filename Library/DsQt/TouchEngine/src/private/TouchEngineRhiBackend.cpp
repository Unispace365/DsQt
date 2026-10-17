#include "TouchEngineRhiBackend_p.h"

#include "d3d/D3D11TouchEngineBackend_p.h"
#include "d3d/D3D12TouchEngineBackend_p.h"
#include "opengl/OpenGLTouchEngineBackend_p.h"
#include "vulkan/VulkanTouchEngineBackend_p.h"

namespace dsqt::touchengine::detail {

std::unique_ptr<TouchEngineRhiBackend> createTouchEngineRhiBackend(
    QRhi *rhi,
    DsTouchEngineTypes::GraphicsApi *api,
    QString *error)
{
    if (api)
        *api = DsTouchEngineTypes::GraphicsApi::Unknown;

    if (!rhi) {
        if (error)
            *error = QStringLiteral("Qt did not provide a QRhi");
        return {};
    }

    std::unique_ptr<TouchEngineRhiBackend> result;
    switch (rhi->backend()) {
    case QRhi::OpenGLES2:
        result = std::make_unique<OpenGLTouchEngineBackend>();
        break;
    case QRhi::D3D11:
#ifdef Q_OS_WIN
        result = std::make_unique<D3D11TouchEngineBackend>();
#endif
        break;
    case QRhi::D3D12:
#ifdef Q_OS_WIN
        result = std::make_unique<D3D12TouchEngineBackend>();
#endif
        break;
    case QRhi::Vulkan:
#ifdef Q_OS_WIN
        result = std::make_unique<VulkanTouchEngineBackend>();
#endif
        break;
    case QRhi::Metal:
        if (error)
            *error = QStringLiteral("Metal is intentionally not supported by Dsqt.TouchEngine");
        break;
    case QRhi::Null:
        if (error)
            *error = QStringLiteral("The null QRhi backend cannot exchange TouchEngine textures");
        break;
    }

    if (!result && error && error->isEmpty())
        *error = QStringLiteral("This Qt graphics backend is not supported on the current platform");
    if (result && api)
        *api = result->graphicsApi();
    return result;
}

} // namespace dsqt::touchengine::detail

