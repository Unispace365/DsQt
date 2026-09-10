#pragma once

#include <QMetaType>
#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

namespace dsqt::touchengine {

class DsTouchEngineTypes : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(DsTouchEngine)
    QML_UNCREATABLE("DsTouchEngine provides enums for the TouchEngine module")

public:
    enum class State {
        Idle,
        WaitingForRenderer,
        Configuring,
        Loading,
        Ready,
        Unloading,
        Error
    };
    Q_ENUM(State)

    enum class GraphicsApi {
        Unknown,
        OpenGL,
        Direct3D11,
        Direct3D12,
        Vulkan,
        Unsupported
    };
    Q_ENUM(GraphicsApi)

    enum class TimeMode {
        External,
        Internal
    };
    Q_ENUM(TimeMode)

    enum class LinkScope {
        Input,
        Output
    };
    Q_ENUM(LinkScope)

    enum class LinkType {
        Unknown,
        Boolean,
        Integer,
        Double,
        String,
        Texture,
        FloatBuffer,
        Table
    };
    Q_ENUM(LinkType)

    static QString stateName(State state);
    static QString graphicsApiName(GraphicsApi api);
};

struct DsTouchEngineLinkInfo
{
    Q_GADGET
    Q_PROPERTY(QString identifier MEMBER identifier)
    Q_PROPERTY(QString name MEMBER name)
    Q_PROPERTY(QString label MEMBER label)
    Q_PROPERTY(dsqt::touchengine::DsTouchEngineTypes::LinkScope scope MEMBER scope)
    Q_PROPERTY(dsqt::touchengine::DsTouchEngineTypes::LinkType type MEMBER type)
    Q_PROPERTY(int count MEMBER count)

public:
    QString identifier;
    QString name;
    QString label;
    DsTouchEngineTypes::LinkScope scope = DsTouchEngineTypes::LinkScope::Input;
    DsTouchEngineTypes::LinkType type = DsTouchEngineTypes::LinkType::Unknown;
    int count = 0;

    QVariantMap toVariantMap() const;
};

} // namespace dsqt::touchengine

Q_DECLARE_METATYPE(dsqt::touchengine::DsTouchEngineTypes::State)
Q_DECLARE_METATYPE(dsqt::touchengine::DsTouchEngineTypes::GraphicsApi)
Q_DECLARE_METATYPE(dsqt::touchengine::DsTouchEngineTypes::TimeMode)
Q_DECLARE_METATYPE(dsqt::touchengine::DsTouchEngineTypes::LinkScope)
Q_DECLARE_METATYPE(dsqt::touchengine::DsTouchEngineTypes::LinkType)
Q_DECLARE_METATYPE(dsqt::touchengine::DsTouchEngineLinkInfo)
