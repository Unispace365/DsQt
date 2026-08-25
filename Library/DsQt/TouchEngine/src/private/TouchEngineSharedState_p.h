#pragma once

#include <Dsqt/TouchEngine/DsTouchEngineTypes.h>

#include <QHash>
#include <QMutex>
#include <QString>
#include <QVariant>
#include <QVector>

#include <atomic>
#include <algorithm>
#include <deque>
#include <memory>
#include <utility>

QT_FORWARD_DECLARE_CLASS(QRhi)

namespace dsqt::touchengine::detail {

class TouchEngineCore;

enum class CommandKind {
    Load,
    Unload,
    SetRunning,
    RequestFrame,
    SetInput,
    ClearInput
};

struct Command
{
    CommandKind kind = CommandKind::RequestFrame;
    QString componentPath;
    QString preferredEnginePath;
    QString link;
    QVariant value;
    double frameRate = 60.0;
    DsTouchEngineTypes::TimeMode timeMode = DsTouchEngineTypes::TimeMode::External;
    bool flag = false;
    quint64 serial = 0;
};

struct DesiredState
{
    bool wantsLoad = false;
    Command loadCommand;
    bool running = true;
    QHash<QString, QVariant> inputValues;
    quint64 latestDurableSerial = 0;
};

struct PendingInputCommand
{
    Command command;
    bool timeDependent = false;
    quint64 lastAcceptedInstanceToken = 0;
};

enum class EventKind {
    State,
    GraphicsApi,
    ConfiguredEngine,
    Error,
    Links,
    OutputValue,
    FrameFinished,
    Statistics
};

struct Event
{
    EventKind kind = EventKind::State;
    DsTouchEngineTypes::State state = DsTouchEngineTypes::State::Idle;
    DsTouchEngineTypes::GraphicsApi graphicsApi = DsTouchEngineTypes::GraphicsApi::Unknown;
    QString configuredEnginePath;
    QString message;
    QString diagnosticKey;
    QString link;
    QVariant value;
    QVariantList links;
    quint64 frameNumber = 0;
    qint64 statisticsCpuMemoryBytes = 0;
    qint64 statisticsGpuMemoryBytes = 0;
    qint64 statisticsCpuFrameTimeNs = 0;
    qint64 statisticsGpuFrameTimeNs = -1;
    qint64 statisticsFrames = 0;
    qint64 statisticsFramesDropped = -1;
};

inline QString coreDiagnosticKey()
{
    return QStringLiteral("core");
}

inline QString rendererInitializationDiagnosticKey(quint64 producerId)
{
    return QStringLiteral("renderer/%1/initialization").arg(producerId);
}

inline QString sessionLoadDiagnosticKey()
{
    return QStringLiteral("session.load");
}

inline QString textureSourceDiagnosticKey(quint64 producerId, const QString &link)
{
    return QStringLiteral("renderer/%1/texture-source/").arg(producerId) + link;
}

inline QString inputDiagnosticKey(const QString &link)
{
    return QStringLiteral("core.input/") + link;
}

inline QString inputQueueDiagnosticKey(const QString &link)
{
    return QStringLiteral("session.input-queue/") + link;
}

inline QString texturePrepareDiagnosticKey(const QString &link)
{
    return QStringLiteral("core.texture-prepare/") + link;
}

inline QString textureClearDiagnosticKey(const QString &link)
{
    return QStringLiteral("core.texture-clear/") + link;
}

inline QString linkEnumerationDiagnosticKey()
{
    return QStringLiteral("core.link-enumeration");
}

inline QString scalarOutputDiagnosticKey(const QString &link)
{
    return QStringLiteral("core.scalar-output/") + link;
}

inline QString textureOutputDiagnosticKey(const QString &link)
{
    return QStringLiteral("core.texture-output/") + link;
}

inline QString frameStartDiagnosticKey()
{
    return QStringLiteral("core.frame-start");
}

inline QString frameFinishDiagnosticKey()
{
    return QStringLiteral("core.frame-finish");
}

inline QString eventProcessingDiagnosticKey()
{
    return QStringLiteral("core.event-processing");
}

inline QString transferDiagnosticKey()
{
    return QStringLiteral("core.texture-transfer");
}

class TouchEngineSharedState final
{
public:
    static constexpr qsizetype MaxPendingTimeDependentBuffersPerLink = 64;

    void pushCommand(Command command)
    {
        QMutexLocker lock(&commandMutex);
        commands.push_back(std::move(command));
    }

    std::deque<Command> takeCommands()
    {
        QMutexLocker lock(&commandMutex);
        std::deque<Command> result;
        result.swap(commands);
        return result;
    }

    bool hasCommands()
    {
        QMutexLocker lock(&commandMutex);
        return !commands.empty();
    }

    bool hasLifecycleCommands()
    {
        QMutexLocker lock(&commandMutex);
        return std::any_of(commands.cbegin(), commands.cend(), [](const Command &command) {
            return command.kind == CommandKind::Load || command.kind == CommandKind::Unload;
        });
    }

    void rememberLoad(const Command &command)
    {
        QMutexLocker lock(&desiredMutex);
        desired.wantsLoad = true;
        desired.loadCommand = command;
        desired.latestDurableSerial = std::max(desired.latestDurableSerial, command.serial);
    }

    void rememberUnload(quint64 serial)
    {
        QMutexLocker lock(&desiredMutex);
        desired.wantsLoad = false;
        desired.latestDurableSerial = std::max(desired.latestDurableSerial, serial);
    }

    void rememberRunning(bool running, quint64 serial)
    {
        QMutexLocker lock(&desiredMutex);
        desired.running = running;
        desired.latestDurableSerial = std::max(desired.latestDurableSerial, serial);
    }

    void rememberInput(const QString &link, const QVariant &value, bool clear, quint64 serial)
    {
        QMutexLocker lock(&desiredMutex);
        if (clear)
            desired.inputValues.remove(link);
        else
            desired.inputValues.insert(link, value);
        desired.latestDurableSerial = std::max(desired.latestDurableSerial, serial);
    }

    DesiredState desiredSnapshot()
    {
        QMutexLocker lock(&desiredMutex);
        return desired;
    }

    void pushInputCommand(Command command, bool timeDependent)
    {
        QMutexLocker lock(&inputMutex);
        auto &pending = inputCommands[command.link];
        if (!timeDependent) {
            // A static value or clear replaces every input write that has not
            // reached TouchEngine yet. This is both the SDK's queue-reset
            // behavior and the public last-writer contract.
            pending.clear();
            setInputQueueDiagnosticLocked(command.link, {});
        } else {
            const qsizetype timedCount = std::count_if(
                pending.cbegin(), pending.cend(), [](const PendingInputCommand &entry) {
                    return entry.timeDependent;
                });
            if (timedCount >= MaxPendingTimeDependentBuffersPerLink) {
                const auto oldest = std::find_if(
                    pending.begin(), pending.end(), [](const PendingInputCommand &entry) {
                        return entry.timeDependent;
                    });
                if (oldest != pending.end())
                    pending.erase(oldest);
                setInputQueueDiagnosticLocked(
                    command.link,
                    QStringLiteral("Time-dependent input '%1' exceeded %2 pending buffers; the oldest buffer was dropped")
                        .arg(command.link)
                        .arg(MaxPendingTimeDependentBuffersPerLink));
            }
        }
        pending.push_back(PendingInputCommand{std::move(command), timeDependent});
    }

    // A texture binding is itself a later write. Cancel a clear which has not
    // reached TouchEngine; a clear enqueued after this call remains ordered
    // after the binding and therefore still wins.
    void rememberTextureBinding(const QString &link)
    {
        QMutexLocker lock(&inputMutex);
        auto pending = inputCommands.find(link);
        if (pending != inputCommands.end()) {
            std::erase_if(*pending, [](const PendingInputCommand &entry) {
                return entry.command.kind == CommandKind::ClearInput;
            });
            if (pending->empty()) {
                inputCommands.erase(pending);
                setInputQueueDiagnosticLocked(link, {});
            }
        }
        // A scalar fallback from an older binding must not be replayed over a
        // newly configured texture after Core recreation.
        durableInputValues.remove(link);
    }

    QVector<PendingInputCommand> pendingInputSnapshot()
    {
        QMutexLocker lock(&inputMutex);
        QVector<PendingInputCommand> result;
        for (const auto &perLink : std::as_const(inputCommands)) {
            for (const PendingInputCommand &entry : perLink)
                result.push_back(entry);
        }
        std::sort(result.begin(), result.end(), [](const auto &left, const auto &right) {
            return left.command.serial < right.command.serial;
        });
        return result;
    }

    bool pendingInputSnapshotIfNoCommands(QVector<PendingInputCommand> *result)
    {
        if (!result)
            return false;

        // This lock order is the lifecycle/input ordering fence. A Load or
        // Unload that linearizes first prevents input application to the old
        // instance; input commands already visible here precede a lifecycle
        // command that has not yet acquired commandMutex.
        QMutexLocker commandLock(&commandMutex);
        if (!commands.empty())
            return false;
        QMutexLocker inputLock(&inputMutex);

        result->clear();
        for (const auto &perLink : std::as_const(inputCommands)) {
            for (const PendingInputCommand &entry : perLink)
                result->push_back(entry);
        }
        std::sort(result->begin(), result->end(), [](const auto &left, const auto &right) {
            return left.command.serial < right.command.serial;
        });
        return true;
    }

    bool isInputCommandPending(const QString &link, quint64 serial)
    {
        QMutexLocker lock(&inputMutex);
        const auto pending = inputCommands.constFind(link);
        if (pending == inputCommands.cend())
            return false;
        return std::any_of(pending->cbegin(), pending->cend(), [serial](const auto &entry) {
            return entry.command.serial == serial;
        });
    }

    void consumeInputCommand(const QString &link, quint64 serial, bool applied)
    {
        QMutexLocker lock(&inputMutex);
        auto pending = inputCommands.find(link);
        if (pending == inputCommands.end())
            return;
        const auto entry = std::find_if(pending->begin(), pending->end(), [serial](const auto &candidate) {
            return candidate.command.serial == serial;
        });
        if (entry == pending->end())
            return;

        if (applied && !entry->timeDependent) {
            if (entry->command.kind == CommandKind::ClearInput)
                durableInputValues.remove(link);
            else
                durableInputValues.insert(link, entry->command.value);
        }
        pending->erase(entry);
        if (pending->empty()) {
            inputCommands.erase(pending);
            setInputQueueDiagnosticLocked(link, {});
        } else {
            const qsizetype timedCount = std::count_if(
                pending->cbegin(), pending->cend(), [](const PendingInputCommand &candidate) {
                    return candidate.timeDependent;
                });
            if (timedCount < MaxPendingTimeDependentBuffersPerLink)
                setInputQueueDiagnosticLocked(link, {});
        }
    }

    void markInputCommandAccepted(const QString &link, quint64 serial,
                                  quint64 instanceToken)
    {
        QMutexLocker lock(&inputMutex);
        auto pending = inputCommands.find(link);
        if (pending == inputCommands.end())
            return;
        const auto entry = std::find_if(pending->begin(), pending->end(), [serial](const auto &candidate) {
            return candidate.command.serial == serial;
        });
        if (entry != pending->end() && entry->timeDependent) {
            entry->lastAcceptedInstanceToken = instanceToken;
            const bool allAccepted = std::all_of(
                pending->cbegin(), pending->cend(), [instanceToken](const auto &candidate) {
                    return candidate.timeDependent
                        && candidate.lastAcceptedInstanceToken == instanceToken;
                });
            if (allAccepted)
                setInputQueueDiagnosticLocked(link, {});
        }
    }

    void discardInputCommands(const QString &link)
    {
        QMutexLocker lock(&inputMutex);
        inputCommands.remove(link);
        setInputQueueDiagnosticLocked(link, {});
    }

    QHash<QString, QVariant> durableInputSnapshot()
    {
        QMutexLocker lock(&inputMutex);
        return durableInputValues;
    }

    bool hasPendingInputCommandsForInstance(quint64 instanceToken)
    {
        QMutexLocker lock(&inputMutex);
        for (const auto &perLink : std::as_const(inputCommands)) {
            if (std::any_of(perLink.cbegin(), perLink.cend(), [instanceToken](const auto &entry) {
                    return !entry.timeDependent
                        || entry.lastAcceptedInstanceToken != instanceToken;
                })) {
                return true;
            }
        }
        return false;
    }

    void pushEvent(Event event)
    {
        QMutexLocker lock(&eventMutex);
        events.push_back(std::move(event));
    }

    std::deque<Event> takeEvents()
    {
        QMutexLocker lock(&eventMutex);
        std::deque<Event> result;
        result.swap(events);
        return result;
    }

    // Called while coreMutex is held when no renderer owns a Core. Runtime
    // publications from the retired generation must not repopulate Session
    // state after a synchronous unload, while keyed diagnostic events still
    // need to be applied so their producer-owned clears are not lost.
    std::deque<Event> takeDiagnosticsAndDiscardRuntimeEvents()
    {
        QMutexLocker lock(&eventMutex);
        std::deque<Event> diagnostics;
        for (Event &event : events) {
            if (event.kind == EventKind::Error)
                diagnostics.push_back(std::move(event));
        }
        events.clear();
        return diagnostics;
    }

    QMutex commandMutex;
    std::deque<Command> commands;

    QMutex eventMutex;
    std::deque<Event> events;

    QMutex desiredMutex;
    DesiredState desired;

    QMutex inputMutex;
    QHash<QString, std::deque<PendingInputCommand>> inputCommands;
    QHash<QString, QVariant> durableInputValues;
    QHash<QString, QString> inputQueueDiagnostics;

    QMutex coreMutex;
    std::weak_ptr<TouchEngineCore> core;
    QRhi *coreRhi = nullptr;
    quint64 coreGeneration = 0;

    std::atomic_bool rendererAttached = false;
    std::atomic_bool renderLoopNeeded = false;
    std::atomic_bool callbackDrainPending = false;
    std::atomic_bool shuttingDown = false;
    std::atomic<quint64> nextSerial = 1;
    std::atomic<quint64> nextInstanceToken = 1;

private:
    void setInputQueueDiagnosticLocked(const QString &link, const QString &message)
    {
        if (inputQueueDiagnostics.value(link) == message)
            return;
        if (message.isEmpty())
            inputQueueDiagnostics.remove(link);
        else
            inputQueueDiagnostics.insert(link, message);

        Event event;
        event.kind = EventKind::Error;
        event.diagnosticKey = inputQueueDiagnosticKey(link);
        event.message = message;
        pushEvent(std::move(event));
    }
};

} // namespace dsqt::touchengine::detail
