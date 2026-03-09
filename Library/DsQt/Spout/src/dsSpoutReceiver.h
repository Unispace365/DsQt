#pragma once

#include <QObject>
#include <QThread>
#include <QTimer>
#include <QImage>
#include <QMutex>
#include <QSize>
#include <QtQml/qqml.h>

#include <windows.h>
#include <dxgi.h>

/**
 * DsSpoutReceiver - Receives Spout2 video frames from other applications.
 *
 * Wraps the SpoutDX receiver in a worker thread. Provides both a zero-copy
 * GPU path (shared texture handle) and a CPU path (QImage).
 *
 * QML usage:
 *   DsSpoutReceiver {
 *       senderName: "MyTouchDesignerOutput"
 *       active: true
 *   }
 */
class DsSpoutReceiver : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(DsSpoutReceiver)

    Q_PROPERTY(QString senderName READ senderName WRITE setSenderName NOTIFY senderNameChanged)
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged)
    Q_PROPERTY(int senderWidth READ senderWidth NOTIFY senderSizeChanged)
    Q_PROPERTY(int senderHeight READ senderHeight NOTIFY senderSizeChanged)
    Q_PROPERTY(double senderFps READ senderFps NOTIFY senderFpsChanged)
    Q_PROPERTY(bool active READ isActive WRITE setActive NOTIFY activeChanged)

public:
    explicit DsSpoutReceiver(QObject* parent = nullptr);
    ~DsSpoutReceiver() override;

    QString senderName() const;
    void setSenderName(const QString& name);

    bool isConnected() const;

    int senderWidth() const;
    int senderHeight() const;
    QSize senderSize() const;

    double senderFps() const;

    bool isActive() const;
    void setActive(bool active);

    /**
     * Returns the native DX11 shared texture handle for zero-copy GPU access.
     * Only valid when connected() is true.
     */
    HANDLE sharedTextureHandle() const;

    /**
     * Returns the DXGI_FORMAT of the sender's shared texture.
     * Only valid when connected() is true.
     */
    DXGI_FORMAT senderFormat() const;

    /**
     * Returns the most recent frame as a QImage (GPU→CPU copy).
     * Only valid when connected() is true.
     */
    QImage lastFrame() const;

signals:
    /** Emitted when a new GPU texture frame is available. */
    void frameReceived();

    /** Emitted when a new frame is available as QImage. */
    void frameImageReady(const QImage& image);

    void senderNameChanged();
    void connectedChanged();
    void senderSizeChanged();
    void senderFpsChanged();
    void activeChanged();

private:
    class Worker;

    void startWorker();
    void stopWorker();

    // Called by worker on its thread, forwarded via queued connections
    void onWorkerFrameReceived(HANDLE handle, int width, int height, double fps, DXGI_FORMAT format);
    void onWorkerImageReady(const QImage& image);
    void onWorkerConnectionChanged(bool connected);

    QString m_senderName;
    bool m_active = false;

    // Guarded by m_mutex (read from GUI thread, written from worker thread)
    mutable QMutex m_mutex;
    bool m_connected = false;
    int m_senderWidth = 0;
    int m_senderHeight = 0;
    double m_senderFps = 0.0;
    HANDLE m_sharedHandle = nullptr;
    DXGI_FORMAT m_senderFormat = DXGI_FORMAT_UNKNOWN;
    QImage m_lastFrame;

    QThread* m_workerThread = nullptr;
    Worker* m_worker = nullptr;
};
