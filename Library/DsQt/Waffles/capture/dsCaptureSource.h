#pragma once

#include "dsCaptureSession.h"
#include <QPointer>
#include <QVideoSink>
#include <QtQml/qqmlregistration.h>

class DsCaptureSource : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString deviceName READ deviceName WRITE setDeviceName NOTIFY configurationChanged)
    Q_PROPERTY(QString deviceId READ deviceId WRITE setDeviceId NOTIFY configurationChanged)
    Q_PROPERTY(QSize requestedSize READ requestedSize WRITE setRequestedSize NOTIFY configurationChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY configurationChanged)
    Q_PROPERTY(QVideoSink* videoSink READ videoSink WRITE setVideoSink NOTIFY videoSinkChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(int error READ error NOTIFY changed)
    Q_PROPERTY(QString errorString READ errorString NOTIFY changed)
    Q_PROPERTY(QSize frameSize READ frameSize NOTIFY changed)
public:
    explicit DsCaptureSource(QObject* parent = nullptr);
    ~DsCaptureSource() override;
    QString deviceName() const { return m_name; }
    QString deviceId() const { return m_id; }
    QSize requestedSize() const { return m_requestedSize; }
    bool active() const { return m_active; }
    QVideoSink* videoSink() const { return m_sink; }
    bool ready() const { return m_session && m_session->ready(); }
    int error() const { return errorString().isEmpty() ? 0 : 1; }
    QString errorString() const { return m_session ? m_session->errorString() : m_error; }
    QSize frameSize() const { return m_session ? m_session->frameSize() : QSize(); }
    void setDeviceName(const QString& name);
    void setDeviceId(const QString& id);
    void setRequestedSize(const QSize& size);
    void setActive(bool active);
    void setVideoSink(QVideoSink* sink);
    Q_INVOKABLE void refresh();
signals:
    void configurationChanged();
    void videoSinkChanged();
    void changed();
private:
    void scheduleUpdate();
    void update();
    void release();
    QString m_name, m_id, m_error;
    QSize m_requestedSize{1920, 1080};
    bool m_active = false;
    bool m_updatePending = false;
    QPointer<QVideoSink> m_sink;
    QSharedPointer<DsCaptureSession> m_session;
    QTimer m_poll;
};
