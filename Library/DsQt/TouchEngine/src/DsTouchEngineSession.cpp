#include <Dsqt/TouchEngine/DsTouchEngineSession.h>
#include <Dsqt/TouchEngine/DsTouchEngineView.h>

#include "private/DsTouchEngineSessionPrivate_p.h"

#include <QDir>
#include <QMetaType>
#include <QQuickItem>
#include <QStringList>
#include <QtMath>

#include <algorithm>
#include <utility>

namespace dsqt::touchengine {

using detail::Command;
using detail::CommandKind;
using detail::EventKind;

namespace {

bool isTimeDependentInput(const QVariant &value)
{
    if (value.metaType().id() != QMetaType::QVariantMap)
        return false;
    const QVariant marker = value.toMap().value(QStringLiteral("timeDependent"));
    return marker.metaType().id() == QMetaType::Bool && marker.toBool();
}

} // namespace

DsTouchEngineSession::DsTouchEngineSession(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<detail::DsTouchEngineSessionPrivate>())
{
    connect(&d->eventPump, &QTimer::timeout, this, &DsTouchEngineSession::drainEvents);
    d->eventPump.start();
}

DsTouchEngineSession::~DsTouchEngineSession()
{
    d->shared->shuttingDown.store(true, std::memory_order_release);
    d->eventPump.stop();
    const auto views = std::exchange(d->views, {});
    for (const auto &view : views) {
        if (view)
            view->setSession(nullptr);
    }
}

QString DsTouchEngineSession::componentPath() const { return d->componentPath; }

void DsTouchEngineSession::setComponentPath(const QString &path)
{
    const QString cleanPath = path.isEmpty() ? QString() : QDir::cleanPath(path);
    if (d->componentPath == cleanPath)
        return;
    d->componentPath = cleanPath;
    emit componentPathChanged();
}

QString DsTouchEngineSession::preferredEnginePath() const { return d->preferredEnginePath; }

void DsTouchEngineSession::setPreferredEnginePath(const QString &path)
{
    const QString cleanPath = path.isEmpty() ? QString() : QDir::cleanPath(path);
    if (d->preferredEnginePath == cleanPath)
        return;
    d->preferredEnginePath = cleanPath;
    emit preferredEnginePathChanged();
}

double DsTouchEngineSession::frameRate() const { return d->frameRate; }

void DsTouchEngineSession::setFrameRate(double framesPerSecond)
{
    const double bounded = qBound(1.0, framesPerSecond, 1000.0);
    if (qFuzzyCompare(d->frameRate, bounded))
        return;
    d->frameRate = bounded;
    emit frameRateChanged();

    // Refresh the durable load command even when the renderer has not attached
    // yet or TouchEngine is still configuring/loading. Otherwise a renderer
    // recreated later would replay the frame rate that was captured by the
    // earlier load() call instead of this public property value.
    if (d->shared->desiredSnapshot().wantsLoad)
        reload();
}

DsTouchEngineTypes::TimeMode DsTouchEngineSession::timeMode() const { return d->timeMode; }

void DsTouchEngineSession::setTimeMode(DsTouchEngineTypes::TimeMode mode)
{
    if (d->timeMode == mode)
        return;
    d->timeMode = mode;
    emit timeModeChanged();

    if (d->shared->desiredSnapshot().wantsLoad)
        reload();
}

bool DsTouchEngineSession::isRunning() const { return d->running; }

void DsTouchEngineSession::setRunning(bool running)
{
    if (d->running == running)
        return;
    d->running = running;
    emit runningChanged();

    Command command;
    command.kind = CommandKind::SetRunning;
    command.flag = running;
    command.serial = d->shared->nextSerial.fetch_add(1, std::memory_order_relaxed);
    d->shared->rememberRunning(running, command.serial);
    d->shared->pushCommand(std::move(command));
    wakeViews();
}

DsTouchEngineTypes::State DsTouchEngineSession::state() const { return d->state; }
DsTouchEngineTypes::GraphicsApi DsTouchEngineSession::graphicsApi() const { return d->graphicsApi; }
bool DsTouchEngineSession::isLoaded() const { return d->state == DsTouchEngineTypes::State::Ready; }
bool DsTouchEngineSession::isReady() const { return d->state == DsTouchEngineTypes::State::Ready; }
QString DsTouchEngineSession::errorString() const { return d->errorString; }
QVariantList DsTouchEngineSession::links() const { return d->links; }
quint64 DsTouchEngineSession::frameCount() const { return d->frameCount; }

void DsTouchEngineSession::load()
{
    if (d->componentPath.isEmpty()) {
        if (d->updateDiagnostic(detail::sessionLoadDiagnosticKey(),
                                QStringLiteral("componentPath is empty"))) {
            emit errorStringChanged();
        }
        if (d->state != DsTouchEngineTypes::State::Error) {
            d->state = DsTouchEngineTypes::State::Error;
            emit stateChanged();
        }
        return;
    }

    if (d->updateDiagnostic(detail::sessionLoadDiagnosticKey(), {}))
        emit errorStringChanged();

    Command command;
    command.kind = CommandKind::Load;
    command.componentPath = d->componentPath;
    command.preferredEnginePath = d->preferredEnginePath;
    command.frameRate = d->frameRate;
    command.timeMode = d->timeMode;
    command.serial = d->shared->nextSerial.fetch_add(1, std::memory_order_relaxed);
    d->shared->rememberLoad(command);
    d->shared->pushCommand(std::move(command));

    if (!d->shared->rendererAttached.load(std::memory_order_acquire)
        && d->state != DsTouchEngineTypes::State::WaitingForRenderer) {
        d->state = DsTouchEngineTypes::State::WaitingForRenderer;
        emit stateChanged();
    }
    wakeViews();
}

void DsTouchEngineSession::unload()
{
    if (d->updateDiagnostic(detail::sessionLoadDiagnosticKey(), {}))
        emit errorStringChanged();

    Command command;
    command.kind = CommandKind::Unload;
    command.serial = d->shared->nextSerial.fetch_add(1, std::memory_order_relaxed);
    d->shared->rememberUnload(command.serial);
    d->shared->pushCommand(std::move(command));

    bool errorChanged = false;
    bool stateChangedSynchronously = false;
    bool graphicsApiChangedSynchronously = false;
    bool linksChangedSynchronously = false;
    QStringList invalidatedOutputs;
    {
        // Core teardown/acquisition also holds this mutex and takes eventMutex
        // second. Waiting here fences the retiring generation and prevents a
        // replacement Core from publishing between the purge and Idle reset.
        QMutexLocker coreLock(&d->shared->coreMutex);
        if (!d->shared->rendererAttached.load(std::memory_order_acquire)) {
            auto diagnostics = d->shared->takeDiagnosticsAndDiscardRuntimeEvents();
            for (auto &event : diagnostics) {
                errorChanged |= d->updateDiagnostic(std::move(event.diagnosticKey),
                                                    event.message);
            }

            linksChangedSynchronously = !d->links.isEmpty();
            invalidatedOutputs = d->outputValues.keys();
            d->links.clear();
            d->outputValues.clear();
            if (d->state != DsTouchEngineTypes::State::Idle) {
                d->state = DsTouchEngineTypes::State::Idle;
                stateChangedSynchronously = true;
            }
            if (d->graphicsApi != DsTouchEngineTypes::GraphicsApi::Unknown) {
                d->graphicsApi = DsTouchEngineTypes::GraphicsApi::Unknown;
                graphicsApiChangedSynchronously = true;
            }
        }
    }

    if (errorChanged)
        emit errorStringChanged();
    if (stateChangedSynchronously)
        emit stateChanged();
    if (graphicsApiChangedSynchronously)
        emit graphicsApiChanged();
    if (linksChangedSynchronously)
        emit linksChanged();
    for (const QString &identifier : std::as_const(invalidatedOutputs))
        emit outputValueChanged(identifier, {});
    wakeViews();
}

void DsTouchEngineSession::reload()
{
    load();
}

void DsTouchEngineSession::requestFrame()
{
    Command command;
    command.kind = CommandKind::RequestFrame;
    command.serial = d->shared->nextSerial.fetch_add(1, std::memory_order_relaxed);
    d->shared->pushCommand(std::move(command));
    wakeViews();
}

void DsTouchEngineSession::setInputValue(const QString &link, const QVariant &value)
{
    if (link.isEmpty())
        return;

    Command command;
    command.kind = CommandKind::SetInput;
    command.link = link;
    command.value = value;
    command.serial = d->shared->nextSerial.fetch_add(1, std::memory_order_relaxed);
    d->shared->rememberInput(link, value, false, command.serial);
    d->shared->pushInputCommand(std::move(command), isTimeDependentInput(value));
    wakeViews();
}

void DsTouchEngineSession::clearInputValue(const QString &link)
{
    if (link.isEmpty())
        return;

    Command command;
    command.kind = CommandKind::ClearInput;
    command.link = link;
    command.serial = d->shared->nextSerial.fetch_add(1, std::memory_order_relaxed);
    d->shared->rememberInput(link, {}, true, command.serial);
    d->shared->pushInputCommand(std::move(command), false);
    wakeViews();
}

QVariant DsTouchEngineSession::outputValue(const QString &link) const
{
    return d->outputValues.value(link);
}

void DsTouchEngineSession::setTextureInput(const QString &link, QQuickItem *sourceItem)
{
    if (link.isEmpty())
        return;

    if (sourceItem) {
        // Binding is ordered after earlier input commands for this link. In
        // particular, it cancels a clear that has not reached TouchEngine yet.
        d->shared->rememberTextureBinding(link);
        d->textureInputs.insert(link, sourceItem);
    } else {
        d->textureInputs.remove(link);
        clearInputValue(link);
    }
    wakeViews();
}

void DsTouchEngineSession::clearTextureInput(const QString &link)
{
    if (link.isEmpty())
        return;
    d->textureInputs.remove(link);
    // Idempotent by design: a previous clear may have reached a terminal SDK
    // failure after the binding was removed, so every explicit retry must
    // enqueue a fresh ordered clear command.
    clearInputValue(link);
}

void DsTouchEngineSession::wakeViews()
{
    auto it = d->views.begin();
    while (it != d->views.end()) {
        if (*it) {
            (*it)->update();
            ++it;
        } else {
            it = d->views.erase(it);
        }
    }
}

void DsTouchEngineSession::drainEvents()
{
    auto events = d->shared->takeEvents();
    for (auto &event : events) {
        switch (event.kind) {
        case EventKind::State:
            if (d->state != event.state) {
                d->state = event.state;
                emit stateChanged();
            }
            break;
        case EventKind::GraphicsApi:
            if (d->graphicsApi != event.graphicsApi) {
                d->graphicsApi = event.graphicsApi;
                emit graphicsApiChanged();
            }
            break;
        case EventKind::Error:
            if (d->updateDiagnostic(std::move(event.diagnosticKey), event.message))
                emit errorStringChanged();
            break;
        case EventKind::Links:
            d->links = std::move(event.links);
            {
                // A layout rebuild can also change a link's scalar type while
                // retaining its identifier. Clear all cached outputs before
                // notifying about the new layout; Core republishes every
                // current scalar output immediately after this event.
                const QStringList invalidatedOutputs = d->outputValues.keys();
                d->outputValues.clear();
                for (const QString &invalidated : invalidatedOutputs)
                    emit outputValueChanged(invalidated, {});
            }
            emit linksChanged();
            break;
        case EventKind::OutputValue:
            d->outputValues.insert(event.link, event.value);
            emit outputValueChanged(event.link, event.value);
            break;
        case EventKind::FrameFinished:
            d->frameCount = event.frameNumber;
            emit frameFinished(d->frameCount);
            break;
        }
    }
}

} // namespace dsqt::touchengine
