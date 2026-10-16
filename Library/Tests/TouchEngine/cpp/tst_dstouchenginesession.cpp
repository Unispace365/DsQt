#include <Dsqt/TouchEngine/DsTouchEngineSession.h>

#include "../../../DsQt/TouchEngine/src/private/DsTouchEngineSessionPrivate_p.h"

#include <QFileInfo>
#include <QQuickItem>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QTest>

#include <memory>
#include <utility>

using dsqt::touchengine::DsTouchEngineSession;
using dsqt::touchengine::DsTouchEngineTypes;

class DsTouchEngineSessionTest final : public QObject
{
    Q_OBJECT

private slots:
    void defaultsAreStable();
    void propertySettersNormalizeClampAndNotifyOnce();
    void emptyComponentPathFailsSynchronously();
    void keyedDiagnosticsAggregateAndRecoverIndependently();
    void pendingTimedInputsAreBoundedAndAcknowledged();
    void lifecycleCommandFencesInputSnapshot();
    void textureBindingAndClearAreLastWriterWins();
    void runtimeEventPurgePreservesDiagnostics();
    void loadWaitsForRendererWithoutTouchEngine();
    void unloadWithoutRendererRestoresIdle();
    void commandsAreSafeWithoutRenderer();
    void inputValueAndTextureClearingAreSafe();
    void destructionWithPendingCommandsIsSafe();
};

void DsTouchEngineSessionTest::defaultsAreStable()
{
    DsTouchEngineSession session;

    QVERIFY(session.componentPath().isEmpty());
    QVERIFY(session.preferredEnginePath().isEmpty());
    QCOMPARE(session.frameRate(), 60.0);
    QCOMPARE(session.timeMode(), DsTouchEngineTypes::TimeMode::External);
    QVERIFY(session.isRunning());
    QCOMPARE(session.state(), DsTouchEngineTypes::State::Idle);
    QCOMPARE(session.graphicsApi(), DsTouchEngineTypes::GraphicsApi::Unknown);
    QVERIFY(!session.isLoaded());
    QVERIFY(!session.isReady());
    QVERIFY(session.errorString().isEmpty());
    QVERIFY(session.links().isEmpty());
    QCOMPARE(session.frameCount(), quint64{0});
    QCOMPARE(session.cpuFrameTimeMs(), 0.0);
    QCOMPARE(session.gpuFrameTimeMs(), -1.0);
    QCOMPARE(session.statisticsFrames(), qint64{0});
    QCOMPARE(session.framesDropped(), qint64{-1});
    QCOMPARE(session.cpuMemoryBytes(), qint64{0});
    QCOMPARE(session.gpuMemoryBytes(), qint64{0});
    QCOMPARE(session.touchDesignerVersion(), QStringLiteral("unknown"));
    QVERIFY(!session.outputValue(QStringLiteral("missing")).isValid());
}

void DsTouchEngineSessionTest::propertySettersNormalizeClampAndNotifyOnce()
{
    DsTouchEngineSession session;
    QSignalSpy componentPathSpy(&session, &DsTouchEngineSession::componentPathChanged);
    QSignalSpy enginePathSpy(&session, &DsTouchEngineSession::preferredEnginePathChanged);
    QSignalSpy frameRateSpy(&session, &DsTouchEngineSession::frameRateChanged);
    QSignalSpy timeModeSpy(&session, &DsTouchEngineSession::timeModeChanged);
    QSignalSpy runningSpy(&session, &DsTouchEngineSession::runningChanged);

    QVERIFY(componentPathSpy.isValid());
    QVERIFY(enginePathSpy.isValid());
    QVERIFY(frameRateSpy.isValid());
    QVERIFY(timeModeSpy.isValid());
    QVERIFY(runningSpy.isValid());

    session.setComponentPath(QStringLiteral("show/../component.tox"));
    QCOMPARE(session.componentPath(), QStringLiteral("component.tox"));
    QCOMPARE(componentPathSpy.count(), 1);
    session.setComponentPath(QStringLiteral("component.tox"));
    QCOMPARE(componentPathSpy.count(), 1);
    session.setComponentPath(QString());
    QVERIFY(session.componentPath().isEmpty());
    QCOMPARE(componentPathSpy.count(), 2);

    session.setPreferredEnginePath(QStringLiteral("engine/../TouchEngine.dll"));
    QCOMPARE(session.preferredEnginePath(), QStringLiteral("TouchEngine.dll"));
    QCOMPARE(enginePathSpy.count(), 1);
    session.setPreferredEnginePath(QStringLiteral("TouchEngine.dll"));
    QCOMPARE(enginePathSpy.count(), 1);
    session.setPreferredEnginePath(QString());
    QVERIFY(session.preferredEnginePath().isEmpty());
    QCOMPARE(enginePathSpy.count(), 2);

    session.setFrameRate(120.0);
    QCOMPARE(session.frameRate(), 120.0);
    QCOMPARE(frameRateSpy.count(), 1);
    session.setFrameRate(120.0);
    QCOMPARE(frameRateSpy.count(), 1);
    session.setFrameRate(0.0);
    QCOMPARE(session.frameRate(), 1.0);
    QCOMPARE(frameRateSpy.count(), 2);
    session.setFrameRate(2000.0);
    QCOMPARE(session.frameRate(), 1000.0);
    QCOMPARE(frameRateSpy.count(), 3);

    session.setTimeMode(DsTouchEngineTypes::TimeMode::Internal);
    QCOMPARE(session.timeMode(), DsTouchEngineTypes::TimeMode::Internal);
    QCOMPARE(timeModeSpy.count(), 1);
    session.setTimeMode(DsTouchEngineTypes::TimeMode::Internal);
    QCOMPARE(timeModeSpy.count(), 1);

    session.setRunning(false);
    QVERIFY(!session.isRunning());
    QCOMPARE(runningSpy.count(), 1);
    session.setRunning(false);
    QCOMPARE(runningSpy.count(), 1);
    session.setRunning(true);
    QVERIFY(session.isRunning());
    QCOMPARE(runningSpy.count(), 2);
}

void DsTouchEngineSessionTest::emptyComponentPathFailsSynchronously()
{
    DsTouchEngineSession session;
    QSignalSpy stateSpy(&session, &DsTouchEngineSession::stateChanged);
    QSignalSpy errorSpy(&session, &DsTouchEngineSession::errorStringChanged);

    session.load();

    QCOMPARE(session.state(), DsTouchEngineTypes::State::Error);
    QCOMPARE(session.errorString(), QStringLiteral("componentPath is empty"));
    QVERIFY(!session.isLoaded());
    QVERIFY(!session.isReady());
    QCOMPARE(stateSpy.count(), 1);
    QCOMPARE(errorSpy.count(), 1);

    session.load();
    session.reload();
    QCOMPARE(stateSpy.count(), 1);
    QCOMPARE(errorSpy.count(), 1);

    session.setComponentPath(QStringLiteral("queued-component.tox"));
    session.load();
    QCOMPARE(session.state(), DsTouchEngineTypes::State::WaitingForRenderer);
    QVERIFY(session.errorString().isEmpty());
    QCOMPARE(stateSpy.count(), 2);
    QCOMPARE(errorSpy.count(), 2);
}

void DsTouchEngineSessionTest::keyedDiagnosticsAggregateAndRecoverIndependently()
{
    dsqt::touchengine::detail::DsTouchEngineSessionPrivate state;

    QVERIFY(state.updateDiagnostic(
        dsqt::touchengine::detail::textureSourceDiagnosticKey(2, QStringLiteral("same")),
        QStringLiteral("texture b")));
    QCOMPARE(state.errorString, QStringLiteral("texture b"));

    QVERIFY(state.updateDiagnostic(dsqt::touchengine::detail::coreDiagnosticKey(),
                                   QStringLiteral("core failure")));
    QCOMPARE(state.errorString, QStringLiteral("core failure\ntexture b"));

    QVERIFY(state.updateDiagnostic(
        dsqt::touchengine::detail::textureSourceDiagnosticKey(1, QStringLiteral("same")),
        QStringLiteral("texture a")));
    QCOMPARE(state.errorString,
             QStringLiteral("core failure\ntexture a\ntexture b"));

    QVERIFY(state.updateDiagnostic(
        dsqt::touchengine::detail::textureSourceDiagnosticKey(2, QStringLiteral("same")), {}));
    QCOMPARE(state.errorString, QStringLiteral("core failure\ntexture a"));

    QVERIFY(!state.updateDiagnostic(dsqt::touchengine::detail::coreDiagnosticKey(),
                                    QStringLiteral("core failure")));
    QVERIFY(state.updateDiagnostic(dsqt::touchengine::detail::coreDiagnosticKey(), {}));
    QCOMPARE(state.errorString, QStringLiteral("texture a"));

    QVERIFY(state.updateDiagnostic(
        dsqt::touchengine::detail::textureSourceDiagnosticKey(1, QStringLiteral("same")), {}));
    QVERIFY(state.errorString.isEmpty());
}

void DsTouchEngineSessionTest::pendingTimedInputsAreBoundedAndAcknowledged()
{
    using namespace dsqt::touchengine::detail;
    TouchEngineSharedState shared;
    const QString link = QStringLiteral("samples");

    for (quint64 serial = 1;
         serial <= quint64(TouchEngineSharedState::MaxPendingTimeDependentBuffersPerLink + 1);
         ++serial) {
        Command command;
        command.kind = CommandKind::SetInput;
        command.link = link;
        command.value = QVariantMap{{QStringLiteral("channels"), QVariantList{}},
                                    {QStringLiteral("timeDependent"), true},
                                    {QStringLiteral("startTime"), qlonglong(serial)}};
        command.serial = serial;
        shared.pushInputCommand(std::move(command), true);
    }

    auto pending = shared.pendingInputSnapshot();
    QCOMPARE(pending.size(), TouchEngineSharedState::MaxPendingTimeDependentBuffersPerLink);
    QCOMPARE(pending.front().command.serial, quint64{2});
    QCOMPARE(pending.back().command.serial,
             quint64(TouchEngineSharedState::MaxPendingTimeDependentBuffersPerLink + 1));
    QVERIFY(shared.durableInputSnapshot().isEmpty());
    auto queueEvents = shared.takeEvents();
    QCOMPARE(queueEvents.size(), size_t{1});
    QCOMPARE(queueEvents.front().diagnosticKey, inputQueueDiagnosticKey(link));
    QVERIFY(!queueEvents.front().message.isEmpty());

    // Merely taking a snapshot (as a Core recreation does) does not consume or
    // duplicate entries. Acceptance suppresses a range only for that exact
    // TEInstance token; bounded history remains replayable after recreation.
    QCOMPARE(shared.pendingInputSnapshot().size(), pending.size());
    for (const PendingInputCommand &entry : std::as_const(pending))
        shared.markInputCommandAccepted(link, entry.command.serial, 7);
    pending = shared.pendingInputSnapshot();
    QCOMPARE(pending.size(), TouchEngineSharedState::MaxPendingTimeDependentBuffersPerLink);
    QVERIFY(!shared.hasPendingInputCommandsForInstance(7));
    QVERIFY(shared.hasPendingInputCommandsForInstance(8));
    QVERIFY(shared.durableInputSnapshot().isEmpty());
    queueEvents = shared.takeEvents();
    QCOMPARE(queueEvents.size(), size_t{1});
    QCOMPARE(queueEvents.front().diagnosticKey, inputQueueDiagnosticKey(link));
    QVERIFY(queueEvents.front().message.isEmpty());

    Command staticValue;
    staticValue.kind = CommandKind::SetInput;
    staticValue.link = link;
    staticValue.value = 12.0;
    staticValue.serial = 100;
    shared.pushInputCommand(staticValue, false);
    pending = shared.pendingInputSnapshot();
    QCOMPARE(pending.size(), 1);
    QCOMPARE(pending.front().command.serial, quint64{100});
    shared.consumeInputCommand(link, 100, true);
    QCOMPARE(shared.durableInputSnapshot().value(link), QVariant(12.0));
    QVERIFY(!shared.hasPendingInputCommandsForInstance(7));
}

void DsTouchEngineSessionTest::lifecycleCommandFencesInputSnapshot()
{
    using namespace dsqt::touchengine::detail;
    TouchEngineSharedState shared;

    Command input;
    input.kind = CommandKind::SetInput;
    input.link = QStringLiteral("samples");
    input.value = QVariantMap{{QStringLiteral("timeDependent"), true}};
    input.serial = 1;
    shared.pushInputCommand(input, true);

    Command load;
    load.kind = CommandKind::Load;
    load.serial = 2;
    shared.pushCommand(load);

    QVector<PendingInputCommand> snapshot;
    QVERIFY(!shared.pendingInputSnapshotIfNoCommands(&snapshot));
    QVERIFY(snapshot.isEmpty());
    QCOMPARE(shared.takeCommands().size(), size_t{1});
    QVERIFY(shared.pendingInputSnapshotIfNoCommands(&snapshot));
    QCOMPARE(snapshot.size(), 1);
    QCOMPARE(snapshot.front().command.serial, quint64{1});
}

void DsTouchEngineSessionTest::textureBindingAndClearAreLastWriterWins()
{
    using namespace dsqt::touchengine::detail;
    TouchEngineSharedState shared;
    const QString link = QStringLiteral("textureIn");

    Command clear;
    clear.kind = CommandKind::ClearInput;
    clear.link = link;
    clear.serial = 1;
    shared.pushInputCommand(clear, false);
    shared.rememberTextureBinding(link);
    QVERIFY(shared.pendingInputSnapshot().isEmpty());

    // Reversing the calls must retain the later clear.
    shared.rememberTextureBinding(link);
    clear.serial = 2;
    shared.pushInputCommand(clear, false);
    auto pending = shared.pendingInputSnapshot();
    QCOMPARE(pending.size(), 1);
    QCOMPARE(pending.front().command.kind, CommandKind::ClearInput);
    QCOMPARE(pending.front().command.serial, quint64{2});

    clear.serial = 3;
    shared.pushInputCommand(clear, false);
    pending = shared.pendingInputSnapshot();
    QCOMPARE(pending.size(), 1);
    QCOMPARE(pending.front().command.serial, quint64{3});
}

void DsTouchEngineSessionTest::runtimeEventPurgePreservesDiagnostics()
{
    using namespace dsqt::touchengine::detail;
    TouchEngineSharedState shared;

    shared.pushEvent(Event{.kind = EventKind::State,
                           .state = DsTouchEngineTypes::State::Ready});
    shared.pushEvent(Event{.kind = EventKind::GraphicsApi,
                           .graphicsApi = DsTouchEngineTypes::GraphicsApi::Vulkan});
    shared.pushEvent(Event{.kind = EventKind::ConfiguredEngine,
                           .configuredEnginePath = QStringLiteral("C:/TouchDesigner")});
    shared.pushEvent(Event{.kind = EventKind::Links,
                           .links = QVariantList{QVariantMap{{QStringLiteral("identifier"),
                                                             QStringLiteral("old")}}}});
    shared.pushEvent(Event{.kind = EventKind::OutputValue,
                           .link = QStringLiteral("old"),
                           .value = 1});
    shared.pushEvent(Event{.kind = EventKind::FrameFinished,
                           .frameNumber = 42});
    shared.pushEvent(Event{.kind = EventKind::Statistics,
                           .statisticsCpuMemoryBytes = 1024,
                           .statisticsGpuMemoryBytes = 2048,
                           .statisticsCpuFrameTimeNs = 3'000'000,
                           .statisticsGpuFrameTimeNs = 4'000'000,
                           .statisticsFrames = 2,
                           .statisticsFramesDropped = 1});
    shared.pushEvent(Event{.kind = EventKind::Error,
                           .message = QString(),
                           .diagnosticKey = coreDiagnosticKey()});

    auto diagnostics = shared.takeDiagnosticsAndDiscardRuntimeEvents();
    QCOMPARE(diagnostics.size(), size_t{1});
    QCOMPARE(diagnostics.front().kind, EventKind::Error);
    QCOMPARE(diagnostics.front().diagnosticKey, coreDiagnosticKey());
    QVERIFY(shared.takeEvents().empty());
}

void DsTouchEngineSessionTest::loadWaitsForRendererWithoutTouchEngine()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString missingComponent = directory.filePath(QStringLiteral("missing.tox"));
    QVERIFY(!QFileInfo::exists(missingComponent));

    DsTouchEngineSession session;
    QSignalSpy stateSpy(&session, &DsTouchEngineSession::stateChanged);
    QSignalSpy errorSpy(&session, &DsTouchEngineSession::errorStringChanged);

    session.setComponentPath(missingComponent);
    session.load();

    QCOMPARE(session.state(), DsTouchEngineTypes::State::WaitingForRenderer);
    QCOMPARE(session.graphicsApi(), DsTouchEngineTypes::GraphicsApi::Unknown);
    QVERIFY(!session.isLoaded());
    QVERIFY(!session.isReady());
    QVERIFY(session.errorString().isEmpty());
    QCOMPARE(stateSpy.count(), 1);
    QCOMPARE(errorSpy.count(), 0);

    session.reload();
    QCOMPARE(session.state(), DsTouchEngineTypes::State::WaitingForRenderer);
    QCOMPARE(stateSpy.count(), 1);
}

void DsTouchEngineSessionTest::unloadWithoutRendererRestoresIdle()
{
    DsTouchEngineSession waiting;
    waiting.setComponentPath(QStringLiteral("queued-component.tox"));
    waiting.load();
    QCOMPARE(waiting.state(), DsTouchEngineTypes::State::WaitingForRenderer);
    waiting.unload();
    QCOMPARE(waiting.state(), DsTouchEngineTypes::State::Idle);
    QVERIFY(waiting.errorString().isEmpty());

    DsTouchEngineSession failed;
    failed.load();
    QCOMPARE(failed.state(), DsTouchEngineTypes::State::Error);
    QVERIFY(!failed.errorString().isEmpty());
    failed.unload();
    QCOMPARE(failed.state(), DsTouchEngineTypes::State::Idle);
    QVERIFY(failed.errorString().isEmpty());
}

void DsTouchEngineSessionTest::commandsAreSafeWithoutRenderer()
{
    DsTouchEngineSession session;
    QSignalSpy stateSpy(&session, &DsTouchEngineSession::stateChanged);
    QSignalSpy frameSpy(&session, &DsTouchEngineSession::frameFinished);

    session.unload();
    session.requestFrame();
    session.requestFrame();

    QCOMPARE(session.state(), DsTouchEngineTypes::State::Idle);
    QCOMPARE(session.frameCount(), quint64{0});
    QCOMPARE(stateSpy.count(), 0);
    QCOMPARE(frameSpy.count(), 0);

    session.setComponentPath(QStringLiteral("queued-component.tox"));
    session.load();
    QCOMPARE(session.state(), DsTouchEngineTypes::State::WaitingForRenderer);
    QCOMPARE(stateSpy.count(), 1);

    session.unload();
    session.requestFrame();
    QCOMPARE(session.state(), DsTouchEngineTypes::State::Idle);
    QCOMPARE(session.frameCount(), quint64{0});
    QCOMPARE(stateSpy.count(), 2);
    QCOMPARE(frameSpy.count(), 0);
}

void DsTouchEngineSessionTest::inputValueAndTextureClearingAreSafe()
{
    DsTouchEngineSession session;
    QSignalSpy outputSpy(&session, &DsTouchEngineSession::outputValueChanged);
    QQuickItem sourceItem;

    session.setInputValue(QString(), 12);
    session.clearInputValue(QString());
    session.setInputValue(QStringLiteral("gain"), 0.75);
    session.clearInputValue(QStringLiteral("gain"));
    session.clearInputValue(QStringLiteral("gain"));

    session.setTextureInput(QString(), &sourceItem);
    session.setTextureInput(QStringLiteral("inputTexture"), &sourceItem);
    session.clearTextureInput(QStringLiteral("inputTexture"));
    session.clearTextureInput(QStringLiteral("inputTexture"));
    session.setTextureInput(QStringLiteral("inputTexture"), nullptr);

    QVERIFY(!session.outputValue(QStringLiteral("gain")).isValid());
    QVERIFY(!session.outputValue(QStringLiteral("inputTexture")).isValid());
    QCOMPARE(outputSpy.count(), 0);

    auto transientSource = std::make_unique<QQuickItem>();
    session.setTextureInput(QStringLiteral("transientTexture"), transientSource.get());
    transientSource.reset();
    session.clearTextureInput(QStringLiteral("transientTexture"));
    QVERIFY(!session.outputValue(QStringLiteral("transientTexture")).isValid());
    QCOMPARE(outputSpy.count(), 0);
}

void DsTouchEngineSessionTest::destructionWithPendingCommandsIsSafe()
{
    QQuickItem sourceItem;
    auto *session = new DsTouchEngineSession;
    QSignalSpy destroyedSpy(session, &QObject::destroyed);

    session->setComponentPath(QStringLiteral("pending-component.tox"));
    session->setRunning(false);
    session->setInputValue(QStringLiteral("gain"), 1.0);
    session->setTextureInput(QStringLiteral("inputTexture"), &sourceItem);
    session->load();
    session->requestFrame();
    session->unload();
    session->reload();

    delete session;

    QCOMPARE(destroyedSpy.count(), 1);
}

QTEST_MAIN(DsTouchEngineSessionTest)

#include "tst_dstouchenginesession.moc"
