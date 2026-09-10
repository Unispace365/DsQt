#include <Dsqt/TouchEngine/DsTouchEngineTypes.h>

namespace dsqt::touchengine {

QString DsTouchEngineTypes::stateName(State state)
{
    switch (state) {
    case State::Idle: return QStringLiteral("idle");
    case State::WaitingForRenderer: return QStringLiteral("waitingForRenderer");
    case State::Configuring: return QStringLiteral("configuring");
    case State::Loading: return QStringLiteral("loading");
    case State::Ready: return QStringLiteral("ready");
    case State::Unloading: return QStringLiteral("unloading");
    case State::Error: return QStringLiteral("error");
    }
    return QStringLiteral("unknown");
}

QString DsTouchEngineTypes::graphicsApiName(GraphicsApi api)
{
    switch (api) {
    case GraphicsApi::Unknown: return QStringLiteral("unknown");
    case GraphicsApi::OpenGL: return QStringLiteral("opengl");
    case GraphicsApi::Direct3D11: return QStringLiteral("d3d11");
    case GraphicsApi::Direct3D12: return QStringLiteral("d3d12");
    case GraphicsApi::Vulkan: return QStringLiteral("vulkan");
    case GraphicsApi::Unsupported: return QStringLiteral("unsupported");
    }
    return QStringLiteral("unknown");
}

QVariantMap DsTouchEngineLinkInfo::toVariantMap() const
{
    return {
        {QStringLiteral("identifier"), identifier},
        {QStringLiteral("name"), name},
        {QStringLiteral("label"), label},
        {QStringLiteral("scope"), QVariant::fromValue(scope)},
        {QStringLiteral("type"), QVariant::fromValue(type)},
        {QStringLiteral("count"), count},
    };
}

} // namespace dsqt::touchengine

