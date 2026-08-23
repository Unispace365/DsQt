#pragma once

#include "TouchEngineSharedState_p.h"

#include <QHash>
#include <QMap>
#include <QPointer>
#include <QStringList>
#include <QTimer>
#include <QVector>

#include <utility>

QT_FORWARD_DECLARE_CLASS(QQuickItem)

namespace dsqt::touchengine {

class DsTouchEngineView;

namespace detail {

struct TextureInputBinding
{
    QString link;
    QPointer<QQuickItem> sourceItem;
};

class DsTouchEngineSessionPrivate final
{
public:
    DsTouchEngineSessionPrivate()
        : shared(std::make_shared<TouchEngineSharedState>())
    {
        eventPump.setInterval(8);
        eventPump.setTimerType(Qt::PreciseTimer);
    }

    bool updateDiagnostic(QString key, const QString &message)
    {
        if (key.isEmpty())
            key = coreDiagnosticKey();

        if (message.isEmpty())
            diagnostics.remove(key);
        else
            diagnostics.insert(std::move(key), message);

        const QString combined = QStringList(diagnostics.values()).join(u'\n');
        if (errorString == combined)
            return false;
        errorString = combined;
        return true;
    }

    QString componentPath;
    QString preferredEnginePath;
    double frameRate = 60.0;
    DsTouchEngineTypes::TimeMode timeMode = DsTouchEngineTypes::TimeMode::External;
    bool running = true;

    DsTouchEngineTypes::State state = DsTouchEngineTypes::State::Idle;
    DsTouchEngineTypes::GraphicsApi graphicsApi = DsTouchEngineTypes::GraphicsApi::Unknown;
    QString errorString;
    QMap<QString, QString> diagnostics;
    QVariantList links;
    QHash<QString, QVariant> outputValues;
    quint64 frameCount = 0;

    QHash<QString, QPointer<QQuickItem>> textureInputs;
    QVector<QPointer<DsTouchEngineView>> views;

    std::shared_ptr<TouchEngineSharedState> shared;
    QTimer eventPump;
};

} // namespace detail
} // namespace dsqt::touchengine
