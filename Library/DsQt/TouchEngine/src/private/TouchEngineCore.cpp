#include "TouchEngineCore_p.h"

#include <QFileInfo>
#include <QLoggingCategory>
#include <QMetaType>
#include <QStringList>
#include <QVariantList>

#include <TouchEngine/TEFloatBuffer.h>
#include <TouchEngine/TETable.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace dsqt::touchengine::detail {

Q_LOGGING_CATEGORY(lcTouchEngineCore, "dsqt.touchengine.core")

namespace {

QString resultDescription(TEResult result)
{
    const char *description = TEResultGetDescription(result);
    return description ? QString::fromUtf8(description)
                       : QStringLiteral("TouchEngine error %1").arg(static_cast<int>(result));
}

DsTouchEngineTypes::LinkType publicLinkType(TELinkType type)
{
    switch (type) {
    case TELinkTypeBoolean: return DsTouchEngineTypes::LinkType::Boolean;
    case TELinkTypeInt: return DsTouchEngineTypes::LinkType::Integer;
    case TELinkTypeDouble: return DsTouchEngineTypes::LinkType::Double;
    case TELinkTypeString: return DsTouchEngineTypes::LinkType::String;
    case TELinkTypeTexture: return DsTouchEngineTypes::LinkType::Texture;
    case TELinkTypeFloatBuffer: return DsTouchEngineTypes::LinkType::FloatBuffer;
    case TELinkTypeStringData: return DsTouchEngineTypes::LinkType::Table;
    default: return DsTouchEngineTypes::LinkType::Unknown;
    }
}

TETimeMode teTimeMode(DsTouchEngineTypes::TimeMode mode)
{
    return mode == DsTouchEngineTypes::TimeMode::Internal ? TETimeInternal : TETimeExternal;
}

QVariant collapseList(const QVariantList &values)
{
    return values.size() == 1 ? values.front() : QVariant(values);
}

bool isTimeDependentInputValue(const QVariant &value)
{
    if (value.metaType().id() != QMetaType::QVariantMap)
        return false;
    const QVariant marker = value.toMap().value(QStringLiteral("timeDependent"));
    return marker.metaType().id() == QMetaType::Bool && marker.toBool();
}

bool exactInt64(const QVariant &value, qint64 *result)
{
    if (!result)
        return false;

    switch (value.metaType().id()) {
    case QMetaType::Int:
    case QMetaType::LongLong:
    case QMetaType::Long:
    case QMetaType::Short:
    case QMetaType::Char:
    case QMetaType::SChar:
        *result = value.toLongLong();
        return true;
    case QMetaType::UInt:
    case QMetaType::ULongLong:
    case QMetaType::ULong:
    case QMetaType::UShort:
    case QMetaType::UChar: {
        const quint64 converted = value.toULongLong();
        if (converted > static_cast<quint64>(std::numeric_limits<qint64>::max()))
            return false;
        *result = static_cast<qint64>(converted);
        return true;
    }
    case QMetaType::Double:
    case QMetaType::Float: {
        const double converted = value.toDouble();
        // qint64's positive limit is not exactly representable as a double;
        // compare against the exclusive 2^63 boundary before casting.
        constexpr double lower = -9223372036854775808.0;
        constexpr double upperExclusive = 9223372036854775808.0;
        if (!std::isfinite(converted) || std::trunc(converted) != converted
            || converted < lower || converted >= upperExclusive) {
            return false;
        }
        *result = static_cast<qint64>(converted);
        return true;
    }
    default:
        return false;
    }
}

constexpr int MaxTransientInputWriteAttempts = 3;

bool isRetryableInputWriteResult(TEResult result)
{
    switch (result) {
    case TEResultInsufficientMemory:
    case TEResultGPUAllocationFailed:
    case TEResultInternalError:
    case TEResultMissingResource:
        return true;
    default:
        return false;
    }
}

} // namespace

std::shared_ptr<TouchEngineCore> TouchEngineCore::acquire(
    const std::shared_ptr<TouchEngineSharedState> &shared,
    QRhi *rhi,
    QRhiCommandBuffer *commandBuffer,
    QString *error)
{
    if (!shared || !rhi || !commandBuffer) {
        if (error)
            *error = QStringLiteral("Cannot initialize TouchEngine without a session, QRhi, and command buffer");
        return {};
    }

    QMutexLocker lock(&shared->coreMutex);
    if (auto existing = shared->core.lock()) {
        if (shared->coreRhi != rhi) {
            if (error)
                *error = QStringLiteral("A TouchEngine session cannot span QQuickWindows with different QRhi devices");
            return {};
        }
        return existing;
    }

    auto core = std::shared_ptr<TouchEngineCore>(new TouchEngineCore(shared, rhi));
    if (!core->initialize(commandBuffer, error)) {
        // The failed candidate is destroyed on return and its destructor also
        // touches the shared registry. Release the registry lock first.
        lock.unlock();
        return {};
    }

    shared->core = core;
    shared->coreRhi = rhi;
    core->m_coreGeneration = ++shared->coreGeneration;
    shared->rendererAttached.store(true, std::memory_order_release);
    return core;
}

TouchEngineCore::TouchEngineCore(std::shared_ptr<TouchEngineSharedState> shared, QRhi *rhi)
    : m_shared(std::move(shared))
    , m_rhi(rhi)
{
    m_clock.start();
}

TouchEngineCore::~TouchEngineCore()
{
    // A weak_ptr expires before the object's destructor runs. Serialize the
    // complete teardown with acquire() so a replacement core cannot be
    // installed while this generation is still releasing native resources.
    QMutexLocker registryLock(m_shared ? &m_shared->coreMutex : nullptr);
    const bool ownsRegistry = m_shared
        && m_coreGeneration != 0
        && m_shared->coreGeneration == m_coreGeneration;

    m_acceptCallbacks.store(false, std::memory_order_release);

    if (m_instance) {
        if (m_inFrame)
            TEInstanceCancelFrame(m_instance);
        TEInstanceSuspend(m_instance);
        // TERelease is the lifetime fence for callbacks. Keep the backend and
        // its graphics context alive until the instance has been released.
        m_instance.reset();
    }
    clearCallbacks();

    if (m_backend)
        m_backend->clearTextureOutputs();
    m_backend.reset();

    const QStringList diagnosticKeys = m_diagnostics.keys();
    for (const QString &key : diagnosticKeys)
        setDiagnostic(key, {});

    if (ownsRegistry) {
        m_shared->core.reset();
        m_shared->coreRhi = nullptr;
        m_shared->rendererAttached.store(false, std::memory_order_release);
        m_shared->renderLoopNeeded.store(false, std::memory_order_release);
        // Every published link/output belongs to this instance generation.
        // Invalidate it even if an attached unload command never reached Core;
        // Session's Links handling also clears its scalar output cache.
        m_shared->pushEvent(Event{.kind = EventKind::Links, .links = {}});
        if (!m_shared->shuttingDown.load(std::memory_order_acquire)) {
            const DesiredState desired = m_shared->desiredSnapshot();
            Event state;
            state.kind = EventKind::State;
            state.state = desired.wantsLoad
                ? DsTouchEngineTypes::State::WaitingForRenderer
                : DsTouchEngineTypes::State::Idle;
            m_shared->pushEvent(std::move(state));
            publishGraphicsApi(DsTouchEngineTypes::GraphicsApi::Unknown);
        }
    }
}

bool TouchEngineCore::initialize(QRhiCommandBuffer *commandBuffer, QString *error)
{
    DsTouchEngineTypes::GraphicsApi api = DsTouchEngineTypes::GraphicsApi::Unknown;
    m_backend = createTouchEngineRhiBackend(m_rhi, &api, error);
    publishGraphicsApi(m_backend ? api : DsTouchEngineTypes::GraphicsApi::Unsupported);
    if (!m_backend)
        return false;

    if (!m_backend->initialize(m_rhi, commandBuffer, error))
        return false;

    if (!recreateInstance(error))
        return false;

    // Publish even though the new core's local default is Idle. The previous
    // renderer may have left the public Session in Ready while its QRhi died.
    Event state;
    state.kind = EventKind::State;
    state.state = DsTouchEngineTypes::State::Idle;
    m_shared->pushEvent(std::move(state));
    replayDesiredState();
    return true;
}

bool TouchEngineCore::recreateInstance(QString *error)
{
    // Remain in reset-required state until a fully associated replacement is
    // available. This makes a failed Create/Associate attempt retryable.
    m_instanceNeedsReset = true;
    m_acceptCallbacks.store(false, std::memory_order_release);
    if (m_instance) {
        m_backendResetPending = true;
        m_instance.reset();
    }
    // TEInstance release is the callback fence. Reset backend resources only
    // after it completes so no old-instance object callback can race their
    // destruction or leak transfer state into the replacement instance.
    if (m_backendResetPending) {
        if (!m_backend || !m_backend->resetInstance(error)) {
            m_acceptCallbacks.store(true, std::memory_order_release);
            return false;
        }
        m_backendResetPending = false;
    }
    clearCallbacks();

    TEResult result = TEInstanceCreate(&TouchEngineCore::instanceCallback,
                                       &TouchEngineCore::linkCallback,
                                       this,
                                       m_instance.take());
    if (result != TEResultSuccess) {
        m_instance.reset();
        clearCallbacks();
        m_acceptCallbacks.store(true, std::memory_order_release);
        if (error)
            *error = QStringLiteral("TEInstanceCreate failed: %1").arg(resultDescription(result));
        return false;
    }

    result = TEInstanceSetStatisticsCallback(m_instance,
                                             &TouchEngineCore::statisticsCallback);
    if (result != TEResultSuccess) {
        m_instance.reset();
        clearCallbacks();
        m_acceptCallbacks.store(true, std::memory_order_release);
        if (error) {
            *error = QStringLiteral("TEInstanceSetStatisticsCallback failed: %1")
                         .arg(resultDescription(result));
        }
        return false;
    }

    result = TEInstanceAssociateGraphicsContext(m_instance, m_backend->graphicsContext());
    if (result != TEResultSuccess) {
        m_instance.reset();
        clearCallbacks();
        m_acceptCallbacks.store(true, std::memory_order_release);
        if (error)
            *error = QStringLiteral("TEInstanceAssociateGraphicsContext failed: %1")
                         .arg(resultDescription(result));
        return false;
    }
    m_acceptCallbacks.store(true, std::memory_order_release);

    m_instanceToken = m_shared->nextInstanceToken.fetch_add(1, std::memory_order_relaxed);
    // Rebuild the local queue from canonical shared state. Keeping an
    // unaccepted newer write locally while replaying older accepted history
    // would reverse FIFO order on this replacement instance.
    m_pendingInputWrites.clear();
    m_stagedSharedInputSerials.clear();
    m_dirtyInputs.clear();
    for (auto it = m_inputValues.cbegin(); it != m_inputValues.cend(); ++it)
        queueInputWrite(it.key(), it.value(), false);

    m_instanceNeedsReset = false;
    m_appliedPreferredEnginePath.clear();
    return true;
}

void TouchEngineCore::replayDesiredState()
{
    const DesiredState desired = m_shared->desiredSnapshot();
    m_replayedDurableSerial = desired.latestDurableSerial;
    m_running = desired.running;
    // Only successfully applied static inputs are durable fallbacks. Timed
    // inputs use a separate bounded SharedState history and are replayed once
    // per TEInstance token, including after a reload replaces the instance.
    m_inputValues = m_shared->durableInputSnapshot();
    for (auto it = m_inputValues.cbegin(); it != m_inputValues.cend(); ++it)
        queueInputWrite(it.key(), it.value(), false);

    if (desired.wantsLoad)
        beginLoad(desired.loadCommand);
}

bool TouchEngineCore::beginQtFrame(QRhiCommandBuffer *commandBuffer,
                                   const QVector<TextureInputSource> &textureInputs)
{
    if (!commandBuffer || !m_backend)
        return false;

    bool commandsProcessed = false;
    if (!m_instance && !m_transferBlocked) {
        m_qtFrameOpen = false;
        // A failed instance recreation is recoverable: a later public load()
        // command gets another chance to create the instance without requiring
        // scene-graph destruction to replace the whole renderer/core.
        processCommands();
        commandsProcessed = true;
        if (!m_instance) {
            updateRenderLoopFlag();
            return renderLoopNeeded();
        }
    }

    if (!m_qtFrameOpen) {
        m_qtFrameOpen = true;
        const bool startDeferredLoad = std::exchange(m_pendingLoadDeferred, false);
        processCallbacks(commandBuffer);
        if (m_state == DsTouchEngineTypes::State::Unloading) {
            // Some runtimes complete unload with Ready, while others emit only
            // DidUnload. completeUnload() fences the old instance and prepares an
            // unconfigured replacement before any subsequent Configure call.
            if (m_unloadReadyObserved || m_unloadDidUnloadObserved)
                completeUnload();
        }
        // Do not mutate the instance while a backend still owns an unresolved
        // texture transfer. Commands remain queued until ownership is safe.
        if (!m_transferBlocked && !commandsProcessed)
            processCommands();

        // A reload requested before unload completion is deliberately resumed
        // on a later Qt render turn. This keeps new-engine Configure out of the
        // callback-drain turn that released the previous instance.
        if (startDeferredLoad && m_hasPendingLoad
            && m_state == DsTouchEngineTypes::State::Idle) {
            const DesiredState desired = m_shared->desiredSnapshot();
            if (desired.wantsLoad) {
                const Command pending = m_pendingLoad;
                m_hasPendingLoad = false;
                beginLoad(pending);
            } else {
                m_hasPendingLoad = false;
            }
        }

        if (m_linksDirty && m_state == DsTouchEngineTypes::State::Ready) {
            QString error;
            if (!enumerateLinks(&error)) {
                setDiagnostic(linkEnumerationDiagnosticKey(), error);
            } else {
                m_linksDirty = false;
                setDiagnostic(linkEnumerationDiagnosticKey(), {});
            }
        }

        if (m_state == DsTouchEngineTypes::State::Ready) {
            if (m_transferBlocked) {
                // Some native APIs need a fresh command buffer to finish a
                // release barrier before afterFrameEnd can return ownership.
                updatePendingTextureOutputs(commandBuffer, true);
            } else {
                updatePendingTextureOutputs(commandBuffer);
                // A shared texture remains owned by TouchEngine until its
                // EndUse callback. Publish at most one input set for the
                // engine frame this Qt submission is actually going to start;
                // otherwise a fast or paused Qt render loop can exhaust every
                // bounded backend pool before TouchEngine consumes frame 1.
                if (frameStartDue(m_clock.nsecsElapsed()))
                    prepareTextureInputs(textureInputs, commandBuffer);
            }
        }
        updateRenderLoopFlag();
    }
    return renderLoopNeeded();
}

void TouchEngineCore::afterFrameEnd()
{
    if (!m_qtFrameOpen || !m_instance || !m_backend)
        return;

    QString error;
    const bool transfersReady = m_backend->afterFrameEnd(m_instance, &error);
    if (!transfersReady) {
        m_transferBlocked = true;
        setDiagnostic(transferDiagnosticKey(),
                      error.isEmpty()
                          ? QStringLiteral("A graphics texture transfer could not be completed")
                          : error);
        // It is unsafe to call Configure/Unload while a GPU resource is still
        // owned by the host. If the user requested a lifecycle operation,
        // ask the renderer to quiesce QRhi and replace this entire core. The
        // replacement replays the already-updated durable desired state.
        m_recoveryRequired = m_shared->hasLifecycleCommands();
    } else {
        setDiagnostic(transferDiagnosticKey(), {});
        if (m_transferBlocked) {
            m_transferBlocked = false;
            m_recoveryRequired = false;
        }
    }

    if (transfersReady && m_state == DsTouchEngineTypes::State::Ready) {
        QVector<PendingInputCommand> pendingInputs;
        if (m_shared->pendingInputSnapshotIfNoCommands(&pendingInputs)) {
            applyPendingInputs(pendingInputs);
            maybeStartFrame();
            if (m_inFrame)
                m_backend->afterFrameStart();
        }
    }

    m_qtFrameOpen = false;
    updateRenderLoopFlag();
}

TextureOutput TouchEngineCore::textureOutput(const QString &link) const
{
    return m_backend ? m_backend->textureOutput(link) : TextureOutput{};
}

DsTouchEngineTypes::State TouchEngineCore::state() const noexcept
{
    return m_state;
}

bool TouchEngineCore::renderLoopNeeded() const noexcept
{
    return m_shared && m_shared->renderLoopNeeded.load(std::memory_order_acquire);
}

int TouchEngineCore::nextRenderDelayMilliseconds() const noexcept
{
    if (!renderLoopNeeded())
        return -1;

    // TouchEngine callbacks are noticed by the session's precise event pump,
    // which wakes every attached view. Spinning Qt frames while the engine is
    // still rendering only fills the presentation queue and makes synchronous
    // graphics-interop calls wait behind that queue.
    if (m_inFrame && !m_transferBlocked)
        return -1;

    if (m_transferBlocked || m_pendingLoadDeferred)
        return 0;

    if (m_state != DsTouchEngineTypes::State::Ready)
        return -1;

    const bool immediateWork = m_requestedFrames > 0 || m_linksDirty
        || !m_pendingTextureOutputs.isEmpty()
        || !m_retryTextureOutputs.isEmpty() || !m_dirtyInputs.isEmpty()
        || m_shared->hasPendingInputCommandsForInstance(m_instanceToken);
    if (immediateWork)
        return 0;
    if (!m_running)
        return -1;

    const qint64 remainingNanoseconds = m_nextFrameTimeNs - m_clock.nsecsElapsed();
    if (remainingNanoseconds <= 0)
        return 0;
    return static_cast<int>(std::max<qint64>(
        1, (remainingNanoseconds + 999'999) / 1'000'000));
}

bool TouchEngineCore::recoveryRequired() const noexcept
{
    return m_recoveryRequired;
}

void TouchEngineCore::processCommands()
{
    auto commands = m_shared->takeCommands();
    for (auto &command : commands) {
        const bool durable = command.kind != CommandKind::RequestFrame;
        if (durable && command.serial <= m_replayedDurableSerial)
            continue;
        processCommand(std::move(command));
    }
}

void TouchEngineCore::processCallbacks(QRhiCommandBuffer *commandBuffer)
{
    auto callbacks = takeCallbacks();
    for (const auto &event : callbacks) {
        if (event.kind == CallbackKind::Instance) {
            handleInstanceEvent(event, commandBuffer);
        } else if (event.kind == CallbackKind::Link) {
            handleLinkEvent(event);
        } else {
            handleStatisticsEvent(event);
        }
    }
}

void TouchEngineCore::processCommand(Command command)
{
    switch (command.kind) {
    case CommandKind::Load:
        beginLoad(command);
        break;
    case CommandKind::Unload:
        beginUnload(false);
        break;
    case CommandKind::SetRunning:
        m_running = command.flag;
        break;
    case CommandKind::RequestFrame:
        if (m_requestedFrames != std::numeric_limits<quint64>::max())
            ++m_requestedFrames;
        break;
    case CommandKind::SetInput:
        m_inputValues.insert(command.link, command.value);
        queueInputWrite(command.link, command.value, false);
        break;
    case CommandKind::ClearInput:
        m_inputValues.remove(command.link);
        queueInputWrite(command.link, {}, true);
        break;
    }
}

void TouchEngineCore::beginLoad(const Command &command)
{
    m_pendingLoad = command;
    m_hasPendingLoad = true;
    m_loadSerial = command.serial;
    for (auto it = m_inputValues.cbegin(); it != m_inputValues.cend(); ++it) {
        if (!m_pendingInputWrites.contains(it.key()))
            queueInputWrite(it.key(), it.value(), false);
    }

    if (m_state == DsTouchEngineTypes::State::Unloading)
        return;

    if (m_componentLoaded
        || m_state == DsTouchEngineTypes::State::Ready
        || m_state == DsTouchEngineTypes::State::Loading) {
        beginUnload(true);
        return;
    }

    const QFileInfo file(command.componentPath);
    if (!file.exists() || !file.isFile()) {
        m_hasPendingLoad = false;
        setError(QStringLiteral("TouchEngine component does not exist: %1").arg(command.componentPath));
        setState(DsTouchEngineTypes::State::Error);
        return;
    }

    const QByteArray preferred = command.preferredEnginePath.toUtf8();
    TEResult result = TEResultSuccess;
    if (!m_instance || m_instanceNeedsReset
        || (preferred.isEmpty() && !m_appliedPreferredEnginePath.isEmpty())) {
        QString recreateError;
        if (!recreateInstance(&recreateError)) {
            m_hasPendingLoad = false;
            setError(recreateError);
            setState(DsTouchEngineTypes::State::Error);
            return;
        }
    }

    // A new instance already has no preferred installation. Some shipped
    // TouchEngine runtimes dereference a null path despite the SDK contract,
    // so only call the optional setter when the user supplied a real path.
    if (!preferred.isEmpty()) {
        result = TEInstanceSetPreferredEnginePath(m_instance, preferred.constData());
        if (result != TEResultSuccess) {
            m_hasPendingLoad = false;
            setError(result, QStringLiteral("setting the preferred TouchDesigner path"));
            setState(DsTouchEngineTypes::State::Error);
            return;
        }
        m_appliedPreferredEnginePath = command.preferredEnginePath;
    }

    m_frameRate = command.frameRate;
    m_timeMode = command.timeMode;
    const QByteArray path = QFileInfo(command.componentPath).absoluteFilePath().toUtf8();
    result = TEInstanceConfigure(m_instance, path.constData(), teTimeMode(command.timeMode));
    if (result != TEResultSuccess) {
        m_hasPendingLoad = false;
        setError(result, QStringLiteral("configuring %1").arg(command.componentPath));
        setState(DsTouchEngineTypes::State::Error);
        return;
    }

    setError({});
    setState(DsTouchEngineTypes::State::Configuring);
}

void TouchEngineCore::beginUnload(bool retainPendingLoad)
{
    if (!retainPendingLoad)
        m_hasPendingLoad = false;

    if (m_state == DsTouchEngineTypes::State::Unloading)
        return;

    m_pendingTextureOutputs.clear();
    m_retryTextureOutputs.clear();
    m_pendingInputWrites.clear();
    m_stagedSharedInputSerials.clear();
    m_dirtyInputs.clear();
    m_textureInputLinks.clear();

    const QStringList diagnosticKeys = m_diagnostics.keys();
    for (const QString &key : diagnosticKeys) {
        if (key.startsWith(QStringLiteral("core.input/"))
            || key.startsWith(QStringLiteral("core.texture-prepare/"))
            || key.startsWith(QStringLiteral("core.texture-clear/"))
            || key.startsWith(QStringLiteral("core.scalar-output/"))
            || key.startsWith(QStringLiteral("core.texture-output/"))
            || key == linkEnumerationDiagnosticKey()
            || key == frameStartDiagnosticKey()
            || key == frameFinishDiagnosticKey()
            || key == eventProcessingDiagnosticKey()
            || key == transferDiagnosticKey()) {
            setDiagnostic(key, {});
        }
    }

    m_links.clear();
    m_linksDirty = false;
    m_backend->clearTextureOutputs();
    m_shared->pushEvent(Event{.kind = EventKind::Links, .links = {}});

    if (!m_instance) {
        m_inFrame = false;
        completeUnload();
        return;
    }

    const bool hasConfiguredFile = TEInstanceHasFile(m_instance);
    const bool needsUnload = hasConfiguredFile || m_componentLoaded
        || m_state == DsTouchEngineTypes::State::Configuring
        || m_state == DsTouchEngineTypes::State::Loading
        || m_state == DsTouchEngineTypes::State::Ready;
    qCDebug(lcTouchEngineCore) << "begin unload" << "hasFile=" << hasConfiguredFile
                              << "componentLoaded=" << m_componentLoaded
                              << "inFrame=" << m_inFrame
                              << "retainPendingLoad=" << retainPendingLoad;
    if (!needsUnload) {
        m_inFrame = false;
        completeUnload();
        return;
    }

    // TEInstanceUnload cancels any in-progress frame itself. Suspending first
    // leaves some TouchEngine runtimes at DidUnload without completing the
    // documented transition back to Ready.
    m_unloadReadyObserved = false;
    m_unloadDidUnloadObserved = false;
    const TEResult result = TEInstanceUnload(m_instance);
    if (result != TEResultSuccess) {
        setError(result, QStringLiteral("unloading the TouchEngine component"));
        setState(DsTouchEngineTypes::State::Error);
        return;
    }

    m_inFrame = false;
    setState(DsTouchEngineTypes::State::Unloading);
}

void TouchEngineCore::completeUnload()
{
    const bool completedSdkUnload = m_unloadReadyObserved || m_unloadDidUnloadObserved;
    m_componentLoaded = false;
    m_unloadReadyObserved = false;
    m_unloadDidUnloadObserved = false;

    if (completedSdkUnload && m_instance) {
        // Release/fence the old instance and reset all per-instance graphics
        // resources now. The clean replacement remains unconfigured until a
        // later render turn, avoiding overlap with old-engine teardown.
        QString recreateError;
        if (!recreateInstance(&recreateError)) {
            m_hasPendingLoad = false;
            m_pendingLoadDeferred = false;
            setError(recreateError);
            setState(DsTouchEngineTypes::State::Error);
            return;
        }
    } else {
        // A local/no-instance completion still needs a clean instance before
        // the next load, including when clearing a preferred engine path.
        m_instanceNeedsReset = true;
    }

    setError({});
    setState(DsTouchEngineTypes::State::Idle);
    if (m_hasPendingLoad)
        m_pendingLoadDeferred = true;
}

void TouchEngineCore::handleInstanceEvent(const CallbackEvent &event,
                                          QRhiCommandBuffer *commandBuffer)
{
    Q_UNUSED(commandBuffer)

    if (event.event != TEEventFrameDidFinish) {
        qCDebug(lcTouchEngineCore) << "instance event" << static_cast<int>(event.event)
                                  << "result=" << static_cast<int>(event.result)
                                  << resultDescription(event.result)
                                  << "state=" << DsTouchEngineTypes::stateName(m_state)
                                  << "hasFile=" << TEInstanceHasFile(m_instance);
    }

    switch (event.event) {
    case TEEventInstanceReady:
        if (m_state == DsTouchEngineTypes::State::Unloading) {
            // A cancelled Ready can belong to a configure/load operation that
            // the unload superseded. Only the non-cancelled Ready documents
            // completion of the active unload.
            if (event.result == TEResultSuccess) {
                // A successful Configure Ready queued just before Unload is
                // stale while the configured file is still attached.
                if (!TEInstanceHasFile(m_instance))
                    m_unloadReadyObserved = true;
            } else if (event.result != TEResultCancelled) {
                m_hasPendingLoad = false;
                setError(event.result, QStringLiteral("completing the TouchEngine unload"));
                setState(DsTouchEngineTypes::State::Error);
            }
            return;
        }

        if (event.result == TEResultCancelled)
            return;

        if (m_state != DsTouchEngineTypes::State::Configuring)
            return;
        if (event.result != TEResultSuccess) {
            m_hasPendingLoad = false;
            setError(event.result, QStringLiteral("configuring the TouchEngine instance"));
            setState(DsTouchEngineTypes::State::Error);
            return;
        }

        {
            TouchObject<TEString> configuredEnginePath;
            const TEResult result = TEInstanceGetConfiguredEnginePath(
                m_instance, configuredEnginePath.take());
            Event published;
            published.kind = EventKind::ConfiguredEngine;
            if (result == TEResultSuccess && configuredEnginePath
                && configuredEnginePath->string) {
                published.configuredEnginePath = QString::fromUtf8(
                    configuredEnginePath->string);
                qCDebug(lcTouchEngineCore)
                    << "configured TouchDesigner path=" << published.configuredEnginePath;
            } else {
                qCWarning(lcTouchEngineCore)
                    << "could not query configured TouchDesigner path:"
                    << resultDescription(result);
            }
            m_shared->pushEvent(std::move(published));
        }

        {
            QString error;
            if (!m_backend->configureInstance(m_instance, &error)) {
                m_hasPendingLoad = false;
                setError(error);
                setState(DsTouchEngineTypes::State::Error);
                return;
            }
        }

        {
            TEResult result = TEInstanceSetFloatFrameRate(m_instance, static_cast<float>(m_frameRate));
            if (result == TEResultSuccess)
                result = TEInstanceLoad(m_instance);
            if (result != TEResultSuccess) {
                m_hasPendingLoad = false;
                setError(result, QStringLiteral("loading the TouchEngine component"));
                setState(DsTouchEngineTypes::State::Error);
                return;
            }
        }
        m_hasPendingLoad = false;
        setState(DsTouchEngineTypes::State::Loading);
        break;

    case TEEventInstanceDidLoad:
        if (m_state != DsTouchEngineTypes::State::Loading)
            return;
        if (event.result != TEResultSuccess) {
            m_componentLoaded = false;
            setError(event.result, QStringLiteral("loading the TouchEngine component"));
            setState(DsTouchEngineTypes::State::Error);
            return;
        }
        m_componentLoaded = true;
        {
            const TEResult result = TEInstanceResume(m_instance);
            if (result != TEResultSuccess) {
                setError(result, QStringLiteral("resuming the TouchEngine component"));
                setState(DsTouchEngineTypes::State::Error);
                return;
            }
        }
        {
            QString error;
            if (!enumerateLinks(&error)) {
                setDiagnostic(linkEnumerationDiagnosticKey(), error);
                m_linksDirty = true;
            } else {
                setDiagnostic(linkEnumerationDiagnosticKey(), {});
                m_linksDirty = false;
            }
        }
        m_clock.restart();
        m_nextFrameTimeNs = 0;
        setState(DsTouchEngineTypes::State::Ready);
        break;

    case TEEventInstanceDidUnload:
        if (m_state == DsTouchEngineTypes::State::Unloading) {
            if (event.result == TEResultSuccess) {
                m_componentLoaded = false;
                m_unloadDidUnloadObserved = true;
            } else if (event.result != TEResultCancelled) {
                m_hasPendingLoad = false;
                setError(event.result, QStringLiteral("completing the TouchEngine unload"));
                setState(DsTouchEngineTypes::State::Error);
            }
        }
        break;

    case TEEventFrameDidFinish:
        m_inFrame = false;
        if (event.result != TEResultSuccess && event.result != TEResultCancelled) {
            setDiagnostic(
                frameFinishDiagnosticKey(),
                QStringLiteral("Error rendering a TouchEngine frame: %1")
                    .arg(resultDescription(event.result)));
        } else if (event.result == TEResultSuccess) {
            setDiagnostic(frameFinishDiagnosticKey(), {});
        }
        if (event.result == TEResultSuccess) {
            ++m_frameCount;
            Event published;
            published.kind = EventKind::FrameFinished;
            published.frameNumber = m_frameCount;
            m_shared->pushEvent(std::move(published));
        }
        break;

    case TEEventGeneral:
        if (TEResultGetSeverity(event.result) == TESeverityError) {
            setDiagnostic(
                eventProcessingDiagnosticKey(),
                QStringLiteral("Error processing a TouchEngine event: %1")
                    .arg(resultDescription(event.result)));
        } else {
            setDiagnostic(eventProcessingDiagnosticKey(), {});
        }
        break;
    }
}

void TouchEngineCore::handleLinkEvent(const CallbackEvent &event)
{
    switch (event.linkEvent) {
    case TELinkEventValueChange: {
        const auto it = m_links.constFind(event.identifier);
        if (it == m_links.cend() || it->info.scope != DsTouchEngineTypes::LinkScope::Output) {
            m_linksDirty = true;
            return;
        }
        if (it->teType == TELinkTypeTexture) {
            // A real value callback is authoritative even when the runtime's
            // delayed interest state still reports SubsequentValues. The
            // backend handles the valid no-pending-transfer case; metadata-only
            // enumeration remains guarded by LinkGetInterest below.
            m_pendingTextureOutputs.insert(event.identifier);
        } else {
            publishScalarOutput(event.identifier);
        }
        break;
    }
    case TELinkEventAdded:
    case TELinkEventRemoved:
    case TELinkEventModified:
    case TELinkEventMoved:
    case TELinkEventStateChange:
    case TELinkEventChildChange:
        m_linksDirty = true;
        break;
    }
}

void TouchEngineCore::handleStatisticsEvent(const CallbackEvent &event)
{
    Event published;
    published.kind = EventKind::Statistics;
    published.statisticsCpuMemoryBytes = event.statisticsCpuMemoryBytes;
    published.statisticsGpuMemoryBytes = event.statisticsGpuMemoryBytes;
    published.statisticsCpuFrameTimeNs = event.statisticsCpuFrameTimeNs;
    published.statisticsGpuFrameTimeNs = event.statisticsGpuFrameTimeNs;
    published.statisticsFrames = event.statisticsFrames;
    published.statisticsFramesDropped = event.statisticsFramesDropped;
    m_shared->pushEvent(std::move(published));
}

bool TouchEngineCore::enumerateLinks(QString *error)
{
    QHash<QString, LinkRecord> links;
    QVariantList published;

    // Build into a temporary map so consumers never observe a partially
    // rebuilt link layout.
    const auto previous = std::exchange(m_links, {});

    for (const auto [teScope, publicScope] : {
             std::pair{TEScopeInput, DsTouchEngineTypes::LinkScope::Input},
             std::pair{TEScopeOutput, DsTouchEngineTypes::LinkScope::Output}}) {
        TouchObject<TEStringArray> groups;
        const TEResult result = TEInstanceGetLinkGroups(m_instance, teScope, groups.take());
        if (result != TEResultSuccess) {
            if (error)
                *error = QStringLiteral("Could not enumerate TouchEngine links: %1")
                             .arg(resultDescription(result));
            m_links = previous;
            return false;
        }

        for (int32_t index = 0; groups && index < groups->count; ++index) {
            if (!enumerateChildren(groups->strings[index], publicScope, &published, error)) {
                m_links = previous;
                return false;
            }
        }

        // enumerateChildren writes directly to m_links so preserve results
        // accumulated for the next scope.
        links = m_links;
    }

    m_links = std::move(links);

    for (auto it = previous.cbegin(); it != previous.cend(); ++it) {
        if (it->info.scope != DsTouchEngineTypes::LinkScope::Input)
            continue;
        const auto current = m_links.constFind(it.key());
        const bool sameInput = current != m_links.cend()
            && current->info.scope == DsTouchEngineTypes::LinkScope::Input
            && current->teType == it->teType;
        if (sameInput)
            continue;

        discardPendingInputWrites(it.key());
        // Accepted timed history has already left the local deque. A complete
        // layout change is a generation boundary, so discard that shared
        // history as well instead of resurrecting it if the identifier returns.
        m_shared->discardInputCommands(it.key());
        m_textureInputLinks.remove(it.key());
        setDiagnostic(inputDiagnosticKey(it.key()), {});
        setDiagnostic(texturePrepareDiagnosticKey(it.key()), {});
        setDiagnostic(textureClearDiagnosticKey(it.key()), {});
    }

    for (auto it = m_links.cbegin(); it != m_links.cend(); ++it) {
        if (it->info.scope != DsTouchEngineTypes::LinkScope::Input
            || !m_inputValues.contains(it.key())
            || m_pendingInputWrites.contains(it.key())) {
            continue;
        }
        const auto old = previous.constFind(it.key());
        if (old == previous.cend()
            || old->info.scope != DsTouchEngineTypes::LinkScope::Input
            || old->teType != it->teType) {
            queueInputWrite(it.key(), m_inputValues.value(it.key()), false);
        }
    }

    QSet<QString> previousTextureOutputs;
    for (auto it = previous.cbegin(); it != previous.cend(); ++it) {
        if (it->info.scope == DsTouchEngineTypes::LinkScope::Output
            && it->teType == TELinkTypeTexture) {
            previousTextureOutputs.insert(it.key());
        }
    }

    QSet<QString> currentTextureOutputs;
    for (auto it = m_links.cbegin(); it != m_links.cend(); ++it) {
        if (it->info.scope == DsTouchEngineTypes::LinkScope::Output
            && it->teType == TELinkTypeTexture) {
            currentTextureOutputs.insert(it.key());
        }
    }

    for (const QString &removed : previousTextureOutputs - currentTextureOutputs)
        m_backend->clearTextureOutput(removed);

    for (auto it = previous.cbegin(); it != previous.cend(); ++it) {
        if (it->info.scope != DsTouchEngineTypes::LinkScope::Output)
            continue;
        const auto current = m_links.constFind(it.key());
        const bool sameOutputType = current != m_links.cend()
            && current->info.scope == DsTouchEngineTypes::LinkScope::Output
            && current->teType == it->teType;
        if (!sameOutputType) {
            setDiagnostic(scalarOutputDiagnosticKey(it.key()), {});
            setDiagnostic(textureOutputDiagnosticKey(it.key()), {});
        }
    }
    m_pendingTextureOutputs.intersect(currentTextureOutputs);
    m_retryTextureOutputs.intersect(currentTextureOutputs);

    Event event;
    event.kind = EventKind::Links;
    event.links = std::move(published);
    m_shared->pushEvent(std::move(event));

    for (auto it = m_links.cbegin(); it != m_links.cend(); ++it) {
        if (it->info.scope == DsTouchEngineTypes::LinkScope::Output
            && it->teType != TELinkTypeTexture) {
            publishScalarOutput(it.key());
        }
    }
    return true;
}

bool TouchEngineCore::enumerateChildren(const char *identifier,
                                        DsTouchEngineTypes::LinkScope scope,
                                        QVariantList *publishedLinks,
                                        QString *error)
{
    TouchObject<TELinkInfo> teInfo;
    TEResult result = TEInstanceLinkGetInfo(m_instance, identifier, teInfo.take());
    if (result != TEResultSuccess) {
        if (error)
            *error = QStringLiteral("Could not inspect TouchEngine link %1: %2")
                         .arg(QString::fromUtf8(identifier), resultDescription(result));
        return false;
    }

    if (teInfo->type == TELinkTypeGroup
        || teInfo->type == TELinkTypeComplex
        || teInfo->type == TELinkTypeSequence) {
        TouchObject<TEStringArray> children;
        result = TEInstanceLinkGetChildren(m_instance, identifier, children.take());
        if (result != TEResultSuccess) {
            if (error)
                *error = QStringLiteral("Could not enumerate children of %1: %2")
                             .arg(QString::fromUtf8(identifier), resultDescription(result));
            return false;
        }
        for (int32_t index = 0; children && index < children->count; ++index) {
            if (!enumerateChildren(children->strings[index], scope, publishedLinks, error))
                return false;
        }
        return true;
    }

    LinkRecord record;
    record.teType = teInfo->type;
    record.info.identifier = QString::fromUtf8(teInfo->identifier ? teInfo->identifier : identifier);
    record.info.name = QString::fromUtf8(teInfo->name ? teInfo->name : "");
    record.info.label = QString::fromUtf8(teInfo->label ? teInfo->label : "");
    record.info.scope = scope;
    record.info.type = publicLinkType(teInfo->type);
    record.info.count = teInfo->count;

    m_links.insert(record.info.identifier, record);
    publishedLinks->push_back(record.info.toVariantMap());

    if (scope == DsTouchEngineTypes::LinkScope::Output) {
        // The default interest for a new link is All. Existing links may be
        // SubsequentValues after their current value was consumed; do not
        // reset them to All merely because metadata was re-enumerated.
        if (teInfo->type == TELinkTypeTexture
            && TEInstanceLinkGetInterest(m_instance, identifier) == TELinkInterestAll) {
            m_pendingTextureOutputs.insert(record.info.identifier);
        }
    }
    return true;
}

void TouchEngineCore::publishScalarOutput(const QString &identifier)
{
    const auto it = m_links.constFind(identifier);
    if (it == m_links.cend())
        return;

    TEResult result = TEResultSuccess;
    const QVariant value = readLinkValue(*it, &result);
    if (result != TEResultSuccess) {
        setDiagnostic(
            scalarOutputDiagnosticKey(identifier),
            QStringLiteral("Error reading output link %1: %2")
                .arg(identifier, resultDescription(result)));
        return;
    }
    setDiagnostic(scalarOutputDiagnosticKey(identifier), {});

    Event event;
    event.kind = EventKind::OutputValue;
    event.link = identifier;
    event.value = value;
    m_shared->pushEvent(std::move(event));
}

QVariant TouchEngineCore::readLinkValue(const LinkRecord &record, TEResult *result) const
{
    const QByteArray id = record.info.identifier.toUtf8();
    *result = TEResultSuccess;

    switch (record.teType) {
    case TELinkTypeBoolean: {
        bool value = false;
        *result = TEInstanceLinkGetBooleanValue(m_instance, id.constData(), TELinkValueCurrent, &value);
        return value;
    }
    case TELinkTypeDouble: {
        const int count = std::max(1, record.info.count);
        std::vector<double> values(static_cast<size_t>(count));
        *result = TEInstanceLinkGetDoubleValue(m_instance, id.constData(), TELinkValueCurrent,
                                               values.data(), count);
        QVariantList output;
        output.reserve(count);
        for (const double value : values)
            output.push_back(value);
        return collapseList(output);
    }
    case TELinkTypeInt: {
        const int count = std::max(1, record.info.count);
        std::vector<int32_t> values(static_cast<size_t>(count));
        *result = TEInstanceLinkGetIntValue(m_instance, id.constData(), TELinkValueCurrent,
                                            values.data(), count);
        QVariantList output;
        output.reserve(count);
        for (const int32_t value : values)
            output.push_back(value);
        return collapseList(output);
    }
    case TELinkTypeString: {
        TouchObject<TEString> value;
        *result = TEInstanceLinkGetStringValue(m_instance, id.constData(), TELinkValueCurrent,
                                               value.take());
        return value && value->string ? QString::fromUtf8(value->string) : QString();
    }
    case TELinkTypeStringData: {
        TouchObject<TETable> table;
        *result = TEInstanceLinkGetTableValue(m_instance, id.constData(), TELinkValueCurrent,
                                              table.take());
        QVariantList rows;
        if (!table)
            return rows;
        const int rowCount = TETableGetRowCount(table);
        const int columnCount = TETableGetColumnCount(table);
        rows.reserve(rowCount);
        for (int row = 0; row < rowCount; ++row) {
            QVariantList columns;
            columns.reserve(columnCount);
            for (int column = 0; column < columnCount; ++column) {
                const char *cell = TETableGetStringValue(table, row, column);
                columns.push_back(cell ? QString::fromUtf8(cell) : QString());
            }
            rows.push_back(columns);
        }
        return rows;
    }
    case TELinkTypeFloatBuffer: {
        TouchObject<TEFloatBuffer> buffer;
        *result = TEInstanceLinkGetFloatBufferValue(m_instance, id.constData(), TELinkValueCurrent,
                                                    buffer.take());
        if (!buffer)
            return QVariantMap{};

        const int channels = TEFloatBufferGetChannelCount(buffer);
        const uint32_t samples = TEFloatBufferGetValueCount(buffer);
        const float *const *values = TEFloatBufferGetValues(buffer);
        const char *const *names = TEFloatBufferGetChannelNames(buffer);
        QVariantList channelData;
        QVariantList channelNames;
        channelData.reserve(channels);
        if (names)
            channelNames.reserve(channels);
        for (int channel = 0; channel < channels; ++channel) {
            QVariantList samplesForChannel;
            samplesForChannel.reserve(static_cast<qsizetype>(samples));
            for (uint32_t sample = 0; values && sample < samples; ++sample)
                samplesForChannel.push_back(values[channel][sample]);
            channelData.push_back(samplesForChannel);
            if (names) {
                channelNames.push_back(names[channel]
                                           ? QString::fromUtf8(names[channel])
                                           : QString());
            }
        }
        return QVariantMap{
            {QStringLiteral("channels"), channelData},
            {QStringLiteral("channelNames"), channelNames},
            {QStringLiteral("rate"), TEFloatBufferGetRate(buffer)},
            {QStringLiteral("timeDependent"), TEFloatBufferIsTimeDependent(buffer)},
            {QStringLiteral("startTime"), TEFloatBufferGetStartTime(buffer)},
        };
    }
    default:
        *result = TEResultBadUsage;
        return {};
    }
}

TEResult TouchEngineCore::writeLinkValue(const LinkRecord &record,
                                         const QVariant &value,
                                         bool clear,
                                         QString *validationError)
{
    if (validationError)
        validationError->clear();
    const QByteArray id = record.info.identifier.toUtf8();
    switch (record.teType) {
    case TELinkTypeBoolean: {
        bool output = false;
        if (clear && TEInstanceLinkHasValue(m_instance, id.constData(), TELinkValueDefault, 0))
            TEInstanceLinkGetBooleanValue(m_instance, id.constData(), TELinkValueDefault, &output);
        else if (!clear)
            output = value.toBool();
        return TEInstanceLinkSetBooleanValue(m_instance, id.constData(), output);
    }
    case TELinkTypeDouble: {
        const int count = std::max(1, record.info.count);
        std::vector<double> output(static_cast<size_t>(count), 0.0);
        if (clear) {
            if (TEInstanceLinkHasValue(m_instance, id.constData(), TELinkValueDefault, 0))
                TEInstanceLinkGetDoubleValue(m_instance, id.constData(), TELinkValueDefault,
                                             output.data(), count);
        } else {
            const QVariantList list = value.metaType().id() == QMetaType::QVariantList
                ? value.toList() : QVariantList{value};
            for (int index = 0; index < count && index < list.size(); ++index)
                output[static_cast<size_t>(index)] = list[index].toDouble();
        }
        return TEInstanceLinkSetDoubleValue(m_instance, id.constData(), output.data(), count);
    }
    case TELinkTypeInt: {
        const int count = std::max(1, record.info.count);
        std::vector<int32_t> output(static_cast<size_t>(count), 0);
        if (clear) {
            if (TEInstanceLinkHasValue(m_instance, id.constData(), TELinkValueDefault, 0))
                TEInstanceLinkGetIntValue(m_instance, id.constData(), TELinkValueDefault,
                                          output.data(), count);
        } else {
            const QVariantList list = value.metaType().id() == QMetaType::QVariantList
                ? value.toList() : QVariantList{value};
            for (int index = 0; index < count && index < list.size(); ++index)
                output[static_cast<size_t>(index)] = list[index].toInt();
        }
        return TEInstanceLinkSetIntValue(m_instance, id.constData(), output.data(), count);
    }
    case TELinkTypeString: {
        const QByteArray output = value.toString().toUtf8();
        return TEInstanceLinkSetStringValue(m_instance, id.constData(), clear ? nullptr : output.constData());
    }
    case TELinkTypeTexture:
        return clear
            ? TEInstanceLinkSetTextureValue(m_instance, id.constData(), nullptr, nullptr)
            : TEResultSuccess;
    case TELinkTypeStringData: {
        if (clear)
            return TEInstanceLinkSetTableValue(m_instance, id.constData(), nullptr);
        const QVariantList rows = value.toList();
        int columns = 0;
        for (const QVariant &row : rows)
            columns = std::max(columns, static_cast<int>(row.toList().size()));
        TouchObject<TETable> table;
        table.take(TETableCreate());
        if (!table)
            return TEResultInsufficientMemory;
        TETableResize(table, rows.size(), columns);
        for (int row = 0; row < rows.size(); ++row) {
            const QVariantList cells = rows[row].toList();
            for (int column = 0; column < cells.size(); ++column) {
                const QByteArray cell = cells[column].toString().toUtf8();
                const TEResult result = TETableSetStringValue(table, row, column, cell.constData());
                if (result != TEResultSuccess)
                    return result;
            }
        }
        return TEInstanceLinkSetTableValue(m_instance, id.constData(), table);
    }
    case TELinkTypeFloatBuffer: {
        if (clear)
            return TEInstanceLinkSetFloatBufferValue(m_instance, id.constData(), nullptr);

        const auto reject = [validationError, &record](const QString &reason) {
            if (validationError) {
                *validationError = QStringLiteral("Invalid float-buffer input '%1': %2")
                                       .arg(record.info.identifier, reason);
            }
            return TEResultBadUsage;
        };

        const bool mapInput = value.metaType().id() == QMetaType::QVariantMap;
        const QVariantMap map = value.toMap();
        if (mapInput && !map.contains(QStringLiteral("channels"))) {
            return reject(QStringLiteral("the QVariantMap must contain a 'channels' list"));
        }

        const QVariant channelsValue = mapInput
            ? map.value(QStringLiteral("channels"))
            : value;
        if (channelsValue.metaType().id() != QMetaType::QVariantList) {
            return reject(QStringLiteral("'channels' must be a list of samples or a list of channel sample lists"));
        }

        QVariantList channels = channelsValue.toList();
        if (!channels.isEmpty() && channels.front().metaType().id() != QMetaType::QVariantList)
            channels = QVariantList{channels};
        if (channels.isEmpty() && mapInput)
            return reject(QStringLiteral("'channels' must contain at least one channel"));
        if (channels.isEmpty())
            channels = QVariantList{QVariantList{}};

        if (static_cast<quint64>(channels.size())
            > static_cast<quint64>(std::numeric_limits<int32_t>::max())) {
            return reject(QStringLiteral("the channel count exceeds TouchEngine's int32 limit"));
        }

        quint64 valueCount = 0;
        bool firstChannel = true;
        for (const QVariant &channel : std::as_const(channels)) {
            if (channel.metaType().id() != QMetaType::QVariantList) {
                return reject(QStringLiteral("'channels' must be a list of sample lists"));
            }
            const quint64 channelSize = static_cast<quint64>(channel.toList().size());
            if (!firstChannel && channelSize != valueCount) {
                return reject(QStringLiteral("every channel must contain the same number of samples"));
            }
            valueCount = channelSize;
            firstChannel = false;
        }
        if (valueCount > std::numeric_limits<uint32_t>::max()) {
            return reject(QStringLiteral("a channel's sample count exceeds TouchEngine's uint32 limit"));
        }
        const uint32_t sampleCount = static_cast<uint32_t>(valueCount);
        const uint32_t capacity = std::max<uint32_t>(sampleCount, 1);

        QVariantList channelNames;
        if (mapInput && map.contains(QStringLiteral("channelNames"))) {
            const QVariant namesValue = map.value(QStringLiteral("channelNames"));
            if (namesValue.metaType().id() == QMetaType::QStringList) {
                const QStringList names = namesValue.toStringList();
                channelNames.reserve(names.size());
                for (const QString &name : names)
                    channelNames.push_back(name);
            } else if (namesValue.metaType().id() == QMetaType::QVariantList) {
                channelNames = namesValue.toList();
            } else if (namesValue.isValid() && !namesValue.isNull()) {
                return reject(QStringLiteral("'channelNames' must be a list of strings"));
            }
        }
        if (!channelNames.isEmpty() && channelNames.size() != channels.size()) {
            return reject(QStringLiteral("'channelNames' must be empty or contain exactly one name per channel"));
        }

        std::vector<QByteArray> channelNameStorage;
        std::vector<const char *> channelNamePointers;
        if (!channelNames.isEmpty()) {
            channelNameStorage.reserve(static_cast<size_t>(channelNames.size()));
            for (const QVariant &nameValue : std::as_const(channelNames)) {
                if (nameValue.metaType().id() != QMetaType::QString) {
                    return reject(QStringLiteral("every entry in 'channelNames' must be a string"));
                }
                const QString name = nameValue.toString();
                const QByteArray encoded = name.toUtf8();
                if (encoded.contains('\0')) {
                    return reject(QStringLiteral("channel names cannot contain embedded null characters"));
                }
                channelNameStorage.push_back(encoded);
            }
            channelNamePointers.reserve(channelNameStorage.size());
            for (const QByteArray &name : channelNameStorage)
                channelNamePointers.push_back(name.constData());
        }

        std::vector<std::vector<float>> storage(static_cast<size_t>(channels.size()));
        std::vector<const float *> pointers(static_cast<size_t>(channels.size()));
        for (qsizetype channel = 0; channel < channels.size(); ++channel) {
            storage[static_cast<size_t>(channel)].resize(capacity, 0.0f);
            const QVariantList samples = channels[channel].toList();
            for (qsizetype sample = 0; sample < samples.size(); ++sample) {
                bool converted = false;
                const float convertedSample = samples[sample].toFloat(&converted);
                if (!converted || !std::isfinite(convertedSample)) {
                    return reject(QStringLiteral("channel %1 sample %2 is not a finite number")
                                      .arg(channel)
                                      .arg(sample));
                }
                storage[static_cast<size_t>(channel)][static_cast<size_t>(sample)] = convertedSample;
            }
            pointers[static_cast<size_t>(channel)] = storage[static_cast<size_t>(channel)].data();
        }

        bool rateConverted = false;
        const double rate = mapInput && map.contains(QStringLiteral("rate"))
            ? map.value(QStringLiteral("rate")).toDouble(&rateConverted)
            : (rateConverted = true, -1.0);
        if (!rateConverted || !std::isfinite(rate) || (rate != -1.0 && rate <= 0.0)) {
            return reject(QStringLiteral("'rate' must be -1 (unspecified) or a finite positive number"));
        }

        if (mapInput && map.contains(QStringLiteral("timeDependent"))
            && map.value(QStringLiteral("timeDependent")).metaType().id() != QMetaType::Bool) {
            return reject(QStringLiteral("'timeDependent' must be a boolean"));
        }
        const bool timeDependent = mapInput
            && map.value(QStringLiteral("timeDependent"), false).toBool();
        if (timeDependent && rate <= 0.0) {
            return reject(QStringLiteral("a time-dependent buffer requires a finite positive 'rate'"));
        }

        qint64 startTime = 0;
        const bool hasStartTime = mapInput && map.contains(QStringLiteral("startTime"));
        if (hasStartTime && !exactInt64(map.value(QStringLiteral("startTime")), &startTime))
            return reject(QStringLiteral("'startTime' must be an exact signed 64-bit integer"));

        const int32_t channelCount = static_cast<int32_t>(channels.size());
        const char *const *names = channelNamePointers.empty()
            ? nullptr
            : channelNamePointers.data();
        TouchObject<TEFloatBuffer> buffer;
        buffer.take(timeDependent
                        ? TEFloatBufferCreateTimeDependent(rate, channelCount, capacity, names)
                        : TEFloatBufferCreate(rate, channelCount, capacity, names));
        if (!buffer)
            return TEResultInsufficientMemory;

        TEResult result = TEResultSuccess;
        if (timeDependent || hasStartTime)
            result = TEFloatBufferSetStartTime(buffer, startTime);
        if (result == TEResultSuccess)
            result = TEFloatBufferSetValues(buffer, pointers.data(), sampleCount);
        if (result == TEResultSuccess)
            result = timeDependent
                ? TEInstanceLinkAddFloatBuffer(m_instance, id.constData(), buffer)
                : TEInstanceLinkSetFloatBufferValue(m_instance, id.constData(), buffer);
        return result;
    }
    default:
        return TEResultBadUsage;
    }
}

void TouchEngineCore::queueInputWrite(const QString &identifier,
                                      const QVariant &value,
                                      bool clear,
                                      quint64 sharedSerial)
{
    PendingInputWrite pending;
    pending.value = value;
    pending.clear = clear;
    pending.timeDependent = !clear && isTimeDependentInputValue(value);
    pending.sharedSerial = sharedSerial;

    auto &writes = m_pendingInputWrites[identifier];
    if (!pending.timeDependent) {
        // SharedState has already applied last-writer coalescing. Mirror it in
        // the render-thread staging queue, but preserve the serial-zero static
        // fallback. A replacement instance must restore its last successfully
        // applied value before attempting a newer value that may be invalid or
        // may fail in the SDK.
        if (sharedSerial == 0) {
            std::erase_if(writes, [](const PendingInputWrite &write) {
                return write.sharedSerial == 0;
            });
            writes.push_front(std::move(pending));
            m_dirtyInputs.insert(identifier);
            return;
        }
        std::erase_if(writes, [this](const PendingInputWrite &write) {
            if (write.sharedSerial == 0)
                return false;
            m_stagedSharedInputSerials.remove(write.sharedSerial);
            return true;
        });
    }

    writes.push_back(std::move(pending));
    m_dirtyInputs.insert(identifier);
}

void TouchEngineCore::syncPendingInputWrites(
    const QVector<PendingInputCommand> &pending)
{
    QSet<quint64> liveSerials;
    liveSerials.reserve(pending.size());
    for (const PendingInputCommand &entry : pending)
        liveSerials.insert(entry.command.serial);
    m_stagedSharedInputSerials.intersect(liveSerials);

    for (const PendingInputCommand &entry : pending) {
        if (entry.timeDependent
            && entry.lastAcceptedInstanceToken == m_instanceToken) {
            m_stagedSharedInputSerials.insert(entry.command.serial);
            continue;
        }
        if (m_stagedSharedInputSerials.contains(entry.command.serial))
            continue;
        queueInputWrite(entry.command.link,
                        entry.command.value,
                        entry.command.kind == CommandKind::ClearInput,
                        entry.command.serial);
        m_stagedSharedInputSerials.insert(entry.command.serial);
    }
}

void TouchEngineCore::discardPendingInputWrites(const QString &identifier)
{
    const auto pending = m_pendingInputWrites.find(identifier);
    if (pending == m_pendingInputWrites.end())
        return;
    for (const PendingInputWrite &write : std::as_const(*pending)) {
        if (write.sharedSerial == 0)
            continue;
        m_shared->consumeInputCommand(identifier, write.sharedSerial, false);
        m_stagedSharedInputSerials.remove(write.sharedSerial);
    }
    m_pendingInputWrites.erase(pending);
    m_dirtyInputs.remove(identifier);
}

void TouchEngineCore::applyPendingInputs(
    const QVector<PendingInputCommand> &pendingInputs)
{
    syncPendingInputWrites(pendingInputs);
    const QSet<QString> dirty = std::exchange(m_dirtyInputs, {});
    for (const QString &identifier : dirty) {
        auto pending = m_pendingInputWrites.find(identifier);
        if (pending == m_pendingInputWrites.end())
            continue;

        std::erase_if(*pending, [this, &identifier](const PendingInputWrite &write) {
            if (write.sharedSerial == 0
                || m_shared->isInputCommandPending(identifier, write.sharedSerial)) {
                return false;
            }
            m_stagedSharedInputSerials.remove(write.sharedSerial);
            return true;
        });
        if (pending->empty()) {
            m_pendingInputWrites.erase(pending);
            continue;
        }

        const auto link = m_links.constFind(identifier);
        if (link == m_links.cend()) {
            if (m_linksDirty) {
                // Link enumeration can fail transiently immediately after a
                // load. Keep the shared command unacknowledged until a complete
                // layout proves that this identifier truly does not exist.
                m_dirtyInputs.insert(identifier);
                continue;
            }
            setDiagnostic(inputDiagnosticKey(identifier),
                          QStringLiteral("TouchEngine input link does not exist: %1")
                              .arg(identifier));
            discardPendingInputWrites(identifier);
            continue;
        }
        if (link->info.scope != DsTouchEngineTypes::LinkScope::Input) {
            setDiagnostic(inputDiagnosticKey(identifier),
                          QStringLiteral("TouchEngine link is not an input: %1")
                              .arg(identifier));
            discardPendingInputWrites(identifier);
            continue;
        }

        bool retry = false;
        while (!pending->empty()) {
            PendingInputWrite &write = pending->front();
            if (write.sharedSerial != 0
                && !m_shared->isInputCommandPending(identifier, write.sharedSerial)) {
                m_stagedSharedInputSerials.remove(write.sharedSerial);
                pending->pop_front();
                continue;
            }

            if (write.timeDependent && link->teType != TELinkTypeFloatBuffer) {
                setDiagnostic(
                    inputDiagnosticKey(identifier),
                    QStringLiteral("Invalid input '%1': timeDependent buffers require a TouchEngine FloatBuffer link")
                        .arg(identifier));
                if (write.sharedSerial != 0) {
                    m_shared->consumeInputCommand(identifier, write.sharedSerial, false);
                    m_stagedSharedInputSerials.remove(write.sharedSerial);
                }
                pending->pop_front();
                continue;
            }

            QString validationError;
            const TEResult result = writeLinkValue(*link, write.value, write.clear,
                                                   &validationError);
            if (!validationError.isEmpty()) {
                setDiagnostic(inputDiagnosticKey(identifier), validationError);
                if (write.sharedSerial != 0) {
                    m_shared->consumeInputCommand(identifier, write.sharedSerial, false);
                    m_stagedSharedInputSerials.remove(write.sharedSerial);
                }
                pending->pop_front();
                continue;
            }
            if (result != TEResultSuccess) {
                ++write.attempts;
                const bool retryable = isRetryableInputWriteResult(result);
                const bool willRetry = retryable
                    && write.attempts < MaxTransientInputWriteAttempts;
                const bool textureClear = link->teType == TELinkTypeTexture && write.clear;
                setDiagnostic(
                    textureClear ? textureClearDiagnosticKey(identifier)
                                 : inputDiagnosticKey(identifier),
                    willRetry
                        ? QStringLiteral("Error %1 input link %2: %3 (retry %4 of %5)")
                              .arg(textureClear ? QStringLiteral("clearing texture")
                                                : QStringLiteral("writing"),
                                   identifier,
                                   resultDescription(result))
                              .arg(write.attempts)
                              .arg(MaxTransientInputWriteAttempts)
                        : (textureClear
                               ? QStringLiteral("Error clearing texture input link %1: %2; automatic retries stopped, so call clearTextureInput() again after correcting the link/runtime state")
                                     .arg(identifier, resultDescription(result))
                               : QStringLiteral("Error writing input link %1: %2; the value was dropped")
                                     .arg(identifier, resultDescription(result))));
                if (willRetry) {
                    retry = true;
                    break;
                }
                if (write.sharedSerial != 0) {
                    m_shared->consumeInputCommand(identifier, write.sharedSerial, false);
                    m_stagedSharedInputSerials.remove(write.sharedSerial);
                }
                pending->pop_front();
                continue;
            }

            setDiagnostic(inputDiagnosticKey(identifier), {});
            if (write.sharedSerial != 0) {
                if (write.timeDependent) {
                    // AddFloatBuffer success means this TEInstance retained the
                    // range, not that future samples were consumed. Keep the
                    // bounded history in SharedState and replay it once if a
                    // reload creates a different instance token.
                    m_shared->markInputCommandAccepted(
                        identifier, write.sharedSerial, m_instanceToken);
                } else {
                    m_shared->consumeInputCommand(identifier, write.sharedSerial, true);
                    m_stagedSharedInputSerials.remove(write.sharedSerial);
                }
            }
            if (!write.timeDependent) {
                if (write.clear)
                    m_inputValues.remove(identifier);
                else
                    m_inputValues.insert(identifier, write.value);
            }
            if (link->teType == TELinkTypeTexture && write.clear) {
                setDiagnostic(textureClearDiagnosticKey(identifier), {});
            }
            pending->pop_front();
        }

        if (pending->empty()) {
            m_pendingInputWrites.erase(pending);
        } else if (retry) {
            // One attempt per submitted Qt frame avoids a tight retry loop;
            // the render-loop flag keeps a paused session progressing.
            m_dirtyInputs.insert(identifier);
        }
    }
}

void TouchEngineCore::updatePendingTextureOutputs(QRhiCommandBuffer *commandBuffer,
                                                   bool retriesOnly)
{
    QSet<QString> pending = std::exchange(m_retryTextureOutputs, {});
    if (!retriesOnly)
        pending.unite(std::exchange(m_pendingTextureOutputs, {}));
    for (const QString &identifier : pending) {
        const QByteArray id = identifier.toUtf8();
        TouchObject<TETexture> texture;
        const TEResult result = TEInstanceLinkGetTextureValue(
            m_instance, id.constData(), TELinkValueCurrent, texture.take());
        if (result != TEResultSuccess) {
            setDiagnostic(
                textureOutputDiagnosticKey(identifier),
                QStringLiteral("Error reading texture output %1: %2")
                    .arg(identifier, resultDescription(result)));
            m_retryTextureOutputs.insert(identifier);
            continue;
        }
        if (!texture) {
            m_backend->clearTextureOutput(identifier);
            const TEResult interestResult = TEInstanceLinkSetInterest(
                m_instance, id.constData(), TELinkInterestSubsequentValues);
            if (interestResult == TEResultSuccess) {
                setDiagnostic(textureOutputDiagnosticKey(identifier), {});
            } else {
                setDiagnostic(
                    textureOutputDiagnosticKey(identifier),
                    QStringLiteral("Error updating interest for texture output %1: %2")
                        .arg(identifier, resultDescription(interestResult)));
            }
            continue;
        }

        QString error;
        if (!m_backend->updateTextureOutput(m_instance, identifier, texture,
                                            commandBuffer, &error)) {
            // Backends use an empty error for expected GPU back-pressure (for
            // example, a frame-slot import that is still retiring). Keep the
            // output queued without turning a one-frame retry into a public
            // session error. Real failures always provide a diagnostic.
            if (!error.isEmpty())
                setDiagnostic(textureOutputDiagnosticKey(identifier), error);
            m_retryTextureOutputs.insert(identifier);
        } else {
            // A successful get/update consumed the transfer associated with
            // the current value. SubsequentValues prevents metadata changes
            // and stale callbacks from requesting that same value again;
            // TouchEngine restores All before the next real ValueChange.
            const TEResult interestResult = TEInstanceLinkSetInterest(
                m_instance, id.constData(), TELinkInterestSubsequentValues);
            if (interestResult == TEResultSuccess) {
                setDiagnostic(textureOutputDiagnosticKey(identifier), {});
            } else {
                setDiagnostic(
                    textureOutputDiagnosticKey(identifier),
                    QStringLiteral("Error updating interest for texture output %1: %2")
                        .arg(identifier, resultDescription(interestResult)));
            }
        }
    }
}

void TouchEngineCore::prepareTextureInputs(const QVector<TextureInputSource> &sources,
                                           QRhiCommandBuffer *commandBuffer)
{
    QSet<QString> current;
    for (const TextureInputSource &source : sources) {
        if (source.link.isEmpty())
            continue;
        current.insert(source.link);
        // Merely having a current binding supersedes an older failed clear,
        // even while its provider is temporarily unavailable. SharedState has
        // canceled the corresponding queued clear using the same last-writer
        // ordering before this render-thread snapshot was produced.
        setDiagnostic(textureClearDiagnosticKey(source.link), {});

        if (!source.render) {
            // A null entry still represents a configured mapping that the
            // renderer could not use; it has already published the precise
            // source diagnostic. Preserve the last TE value while transient
            // providers retry. An explicit clear removes the link from
            // 'sources' and is handled below (and by the durable input command).
            continue;
        }

        QString error;
        if (!m_backend->prepareTextureInput(m_instance, source, commandBuffer, &error)) {
            setDiagnostic(texturePrepareDiagnosticKey(source.link),
                          error.isEmpty()
                              ? QStringLiteral("Could not prepare texture input %1")
                                    .arg(source.link)
                              : error);
        } else {
            setDiagnostic(texturePrepareDiagnosticKey(source.link), {});
            setDiagnostic(inputDiagnosticKey(source.link), {});
            setDiagnostic(textureClearDiagnosticKey(source.link), {});
        }
    }

    for (const QString &identifier : m_textureInputLinks - current)
        setDiagnostic(texturePrepareDiagnosticKey(identifier), {});
    m_textureInputLinks = std::move(current);
}

bool TouchEngineCore::frameStartDue(qint64 now) const noexcept
{
    if (m_inFrame || m_state != DsTouchEngineTypes::State::Ready)
        return false;

    return m_requestedFrames > 0
        || (m_running && now >= m_nextFrameTimeNs);
}

void TouchEngineCore::maybeStartFrame()
{
    const qint64 now = m_clock.nsecsElapsed();
    if (!frameStartDue(now))
        return;

    const bool manuallyRequested = m_requestedFrames > 0;

    const TEResult result = TEInstanceStartFrameAtTime(
        m_instance, now, 1'000'000'000, false);
    if (result != TEResultSuccess) {
        setDiagnostic(
            frameStartDiagnosticKey(),
            QStringLiteral("Error starting a TouchEngine frame: %1")
                .arg(resultDescription(result)));
        return;
    }
    setDiagnostic(frameStartDiagnosticKey(), {});

    m_inFrame = true;
    if (manuallyRequested)
        --m_requestedFrames;
    const qint64 interval = static_cast<qint64>(1'000'000'000.0 / std::max(1.0, m_frameRate));
    if (m_nextFrameTimeNs <= now) {
        const qint64 elapsedIntervals = (now - m_nextFrameTimeNs) / interval;
        m_nextFrameTimeNs += (elapsedIntervals + 1) * interval;
    }
}

void TouchEngineCore::setState(DsTouchEngineTypes::State state)
{
    if (m_state == state)
        return;
    m_state = state;
    Event event;
    event.kind = EventKind::State;
    event.state = state;
    m_shared->pushEvent(std::move(event));
    updateRenderLoopFlag();
}

void TouchEngineCore::setDiagnostic(const QString &key, const QString &message)
{
    const QString normalizedKey = key.isEmpty() ? coreDiagnosticKey() : key;
    if (m_diagnostics.value(normalizedKey) == message)
        return;

    if (message.isEmpty())
        m_diagnostics.remove(normalizedKey);
    else
        m_diagnostics.insert(normalizedKey, message);

    Event event;
    event.kind = EventKind::Error;
    event.diagnosticKey = normalizedKey;
    event.message = message;
    m_shared->pushEvent(std::move(event));
}

void TouchEngineCore::setError(const QString &message)
{
    setDiagnostic(coreDiagnosticKey(), message);
}

void TouchEngineCore::setError(TEResult result, const QString &operation)
{
    setError(QStringLiteral("Error %1: %2").arg(operation, resultDescription(result)));
}

void TouchEngineCore::publishGraphicsApi(DsTouchEngineTypes::GraphicsApi api)
{
    Event event;
    event.kind = EventKind::GraphicsApi;
    event.graphicsApi = api;
    m_shared->pushEvent(std::move(event));
}

void TouchEngineCore::updateRenderLoopFlag()
{
    const bool transitional = m_state == DsTouchEngineTypes::State::WaitingForRenderer
        || m_state == DsTouchEngineTypes::State::Configuring
        || m_state == DsTouchEngineTypes::State::Loading
        || m_state == DsTouchEngineTypes::State::Unloading;
    const bool active = m_transferBlocked || transitional || m_pendingLoadDeferred
        || (m_state == DsTouchEngineTypes::State::Ready
            && (m_running || m_inFrame || m_requestedFrames > 0
                || m_linksDirty || !m_pendingTextureOutputs.isEmpty()
                || !m_retryTextureOutputs.isEmpty() || !m_dirtyInputs.isEmpty()
                || m_shared->hasPendingInputCommandsForInstance(m_instanceToken)));
    m_shared->renderLoopNeeded.store(active, std::memory_order_release);
}

void TouchEngineCore::instanceCallback(TEInstance *, TEEvent event, TEResult result,
                                       int64_t startTimeValue, int32_t startTimeScale,
                                       int64_t endTimeValue, int32_t endTimeScale,
                                       void *info)
{
    auto *core = static_cast<TouchEngineCore *>(info);
    if (!core || !core->m_acceptCallbacks.load(std::memory_order_acquire))
        return;
    CallbackEvent queued;
    queued.kind = CallbackKind::Instance;
    queued.event = event;
    queued.result = result;
    queued.startTimeValue = startTimeValue;
    queued.startTimeScale = startTimeScale;
    queued.endTimeValue = endTimeValue;
    queued.endTimeScale = endTimeScale;
    core->enqueueCallback(std::move(queued));
}

void TouchEngineCore::linkCallback(TEInstance *, TELinkEvent event,
                                   const char *identifier, void *info)
{
    auto *core = static_cast<TouchEngineCore *>(info);
    if (!core || !core->m_acceptCallbacks.load(std::memory_order_acquire))
        return;
    CallbackEvent queued;
    queued.kind = CallbackKind::Link;
    queued.linkEvent = event;
    queued.identifier = QString::fromUtf8(identifier ? identifier : "");
    core->enqueueCallback(std::move(queued));
}

void TouchEngineCore::statisticsCallback(TEInstance *,
                                         const TEInstanceStatistics *statistics,
                                         void *info)
{
    auto *core = static_cast<TouchEngineCore *>(info);
    if (!core || !statistics
        || !core->m_acceptCallbacks.load(std::memory_order_acquire)) {
        return;
    }

    CallbackEvent queued;
    queued.kind = CallbackKind::Statistics;
    queued.statisticsCpuMemoryBytes = statistics->memUsedCPU;
    queued.statisticsGpuMemoryBytes = statistics->memUsedGPU;
    queued.statisticsCpuFrameTimeNs = statistics->frameTimeCPU;
    queued.statisticsGpuFrameTimeNs = statistics->frameTimeGPU;
    queued.statisticsFrames = statistics->frames;
    queued.statisticsFramesDropped = statistics->framesDropped;
    core->enqueueCallback(std::move(queued));
}

void TouchEngineCore::enqueueCallback(CallbackEvent event)
{
    QMutexLocker lock(&m_callbackMutex);
    m_callbacks.push_back(std::move(event));
    m_shared->callbackDrainPending.store(true, std::memory_order_release);
}

void TouchEngineCore::clearCallbacks()
{
    QMutexLocker lock(&m_callbackMutex);
    m_callbacks.clear();
    m_shared->callbackDrainPending.store(false, std::memory_order_release);
}

std::deque<TouchEngineCore::CallbackEvent> TouchEngineCore::takeCallbacks()
{
    QMutexLocker lock(&m_callbackMutex);
    std::deque<CallbackEvent> result;
    result.swap(m_callbacks);
    m_shared->callbackDrainPending.store(false, std::memory_order_release);
    return result;
}

} // namespace dsqt::touchengine::detail
