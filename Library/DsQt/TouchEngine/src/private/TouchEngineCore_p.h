#pragma once

#include "TouchEngineRhiBackend_p.h"
#include "TouchEngineSharedState_p.h"

#include <QElapsedTimer>
#include <QHash>
#include <QMutex>
#include <QSet>
#include <QString>
#include <QVariant>
#include <QVector>

#include <TouchEngine/TouchObject.h>

#include <atomic>
#include <deque>
#include <memory>

namespace dsqt::touchengine::detail {

class TouchEngineCore final
{
public:
    static std::shared_ptr<TouchEngineCore> acquire(
        const std::shared_ptr<TouchEngineSharedState> &shared,
        QRhi *rhi,
        QRhiCommandBuffer *commandBuffer,
        QString *error);

    ~TouchEngineCore();

    bool beginQtFrame(QRhiCommandBuffer *commandBuffer,
                      const QVector<TextureInputSource> &textureInputs);
    void afterFrameEnd();

    TextureOutput textureOutput(const QString &link) const;
    DsTouchEngineTypes::State state() const noexcept;
    bool renderLoopNeeded() const noexcept;
    bool recoveryRequired() const noexcept;

private:
    struct LinkRecord
    {
        DsTouchEngineLinkInfo info;
        TELinkType teType = TELinkTypeGroup;
    };

    struct PendingInputWrite
    {
        QVariant value;
        bool clear = false;
        bool timeDependent = false;
        quint64 sharedSerial = 0;
        int attempts = 0;
    };

    enum class CallbackKind { Instance, Link };
    struct CallbackEvent
    {
        CallbackKind kind = CallbackKind::Instance;
        TEEvent event = TEEventGeneral;
        TELinkEvent linkEvent = TELinkEventAdded;
        TEResult result = TEResultSuccess;
        QString identifier;
        qint64 startTimeValue = 0;
        qint32 startTimeScale = 1;
        qint64 endTimeValue = 0;
        qint32 endTimeScale = 1;
    };

    TouchEngineCore(std::shared_ptr<TouchEngineSharedState> shared, QRhi *rhi);

    bool initialize(QRhiCommandBuffer *commandBuffer, QString *error);
    bool recreateInstance(QString *error);
    void replayDesiredState();
    void processCommands();
    void processCallbacks(QRhiCommandBuffer *commandBuffer);
    void processCommand(Command command);
    void beginLoad(const Command &command);
    void beginUnload(bool retainPendingLoad);
    void completeUnload();
    void handleInstanceEvent(const CallbackEvent &event, QRhiCommandBuffer *commandBuffer);
    void handleLinkEvent(const CallbackEvent &event);

    bool enumerateLinks(QString *error);
    bool enumerateChildren(const char *identifier, DsTouchEngineTypes::LinkScope scope,
                           QVariantList *publishedLinks, QString *error);
    void publishScalarOutput(const QString &identifier);
    QVariant readLinkValue(const LinkRecord &record, TEResult *result) const;
    TEResult writeLinkValue(const LinkRecord &record, const QVariant &value,
                            bool clear, QString *validationError);
    void queueInputWrite(const QString &identifier, const QVariant &value, bool clear,
                         quint64 sharedSerial = 0);
    void syncPendingInputWrites(const QVector<PendingInputCommand> &pending);
    void discardPendingInputWrites(const QString &identifier);
    void applyPendingInputs(const QVector<PendingInputCommand> &pending);
    void updatePendingTextureOutputs(QRhiCommandBuffer *commandBuffer, bool retriesOnly = false);
    void prepareTextureInputs(const QVector<TextureInputSource> &sources,
                              QRhiCommandBuffer *commandBuffer);
    bool frameStartDue(qint64 now) const noexcept;
    void maybeStartFrame();

    void setState(DsTouchEngineTypes::State state);
    void setDiagnostic(const QString &key, const QString &message);
    void setError(const QString &message);
    void setError(TEResult result, const QString &operation);
    void publishGraphicsApi(DsTouchEngineTypes::GraphicsApi api);
    void updateRenderLoopFlag();

    static void instanceCallback(TEInstance *instance, TEEvent event, TEResult result,
                                 int64_t startTimeValue, int32_t startTimeScale,
                                 int64_t endTimeValue, int32_t endTimeScale, void *info);
    static void linkCallback(TEInstance *instance, TELinkEvent event,
                             const char *identifier, void *info);
    void enqueueCallback(CallbackEvent event);
    std::deque<CallbackEvent> takeCallbacks();

    std::shared_ptr<TouchEngineSharedState> m_shared;
    QRhi *m_rhi = nullptr;
    std::unique_ptr<TouchEngineRhiBackend> m_backend;
    TouchObject<TEInstance> m_instance;

    mutable QMutex m_callbackMutex;
    std::deque<CallbackEvent> m_callbacks;
    std::atomic_bool m_acceptCallbacks = true;

    DsTouchEngineTypes::State m_state = DsTouchEngineTypes::State::Idle;
    QHash<QString, LinkRecord> m_links;
    QHash<QString, QVariant> m_inputValues;
    QHash<QString, std::deque<PendingInputWrite>> m_pendingInputWrites;
    QSet<quint64> m_stagedSharedInputSerials;
    QSet<QString> m_dirtyInputs;
    QSet<QString> m_textureInputLinks;
    QSet<QString> m_pendingTextureOutputs;
    QSet<QString> m_retryTextureOutputs;
    QHash<QString, QString> m_diagnostics;
    bool m_linksDirty = false;

    Command m_pendingLoad;
    bool m_hasPendingLoad = false;
    bool m_running = true;
    bool m_componentLoaded = false;
    bool m_inFrame = false;
    bool m_qtFrameOpen = false;
    bool m_transferBlocked = false;
    bool m_recoveryRequired = false;
    bool m_unloadReadyObserved = false;
    bool m_unloadDidUnloadObserved = false;
    bool m_pendingLoadDeferred = false;
    bool m_instanceNeedsReset = false;
    bool m_backendResetPending = false;
    quint64 m_requestedFrames = 0;
    quint64 m_frameCount = 0;
    quint64 m_loadSerial = 0;
    quint64 m_replayedDurableSerial = 0;
    quint64 m_coreGeneration = 0;
    quint64 m_instanceToken = 0;
    double m_frameRate = 60.0;
    DsTouchEngineTypes::TimeMode m_timeMode = DsTouchEngineTypes::TimeMode::External;
    QString m_appliedPreferredEnginePath;
    QElapsedTimer m_clock;
    qint64 m_nextFrameTimeNs = 0;
};

} // namespace dsqt::touchengine::detail
