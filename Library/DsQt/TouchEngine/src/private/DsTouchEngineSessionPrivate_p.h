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

    bool updateStatistics(qint64 cpuMemoryBytes,
                          qint64 gpuMemoryBytes,
                          qint64 cpuFrameTimeNs,
                          qint64 gpuFrameTimeNs,
                          qint64 frames,
                          qint64 framesDropped)
    {
        if (statisticsCpuMemoryBytes == cpuMemoryBytes
            && statisticsGpuMemoryBytes == gpuMemoryBytes
            && statisticsCpuFrameTimeNs == cpuFrameTimeNs
            && statisticsGpuFrameTimeNs == gpuFrameTimeNs
            && statisticsFrames == frames
            && statisticsFramesDropped == framesDropped) {
            return false;
        }

        statisticsCpuMemoryBytes = cpuMemoryBytes;
        statisticsGpuMemoryBytes = gpuMemoryBytes;
        statisticsCpuFrameTimeNs = cpuFrameTimeNs;
        statisticsGpuFrameTimeNs = gpuFrameTimeNs;
        statisticsFrames = frames;
        statisticsFramesDropped = framesDropped;
        return true;
    }

    bool resetStatistics()
    {
        return updateStatistics(0, 0, 0, -1, 0, -1);
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
    qint64 statisticsCpuMemoryBytes = 0;
    qint64 statisticsGpuMemoryBytes = 0;
    qint64 statisticsCpuFrameTimeNs = 0;
    qint64 statisticsGpuFrameTimeNs = -1;
    qint64 statisticsFrames = 0;
    qint64 statisticsFramesDropped = -1;
    QString touchDesignerVersion = QStringLiteral("unknown");

    QHash<QString, QPointer<QQuickItem>> textureInputs;
    QVector<QPointer<DsTouchEngineView>> views;

    std::shared_ptr<TouchEngineSharedState> shared;
    QTimer eventPump;
};

} // namespace detail
} // namespace dsqt::touchengine
