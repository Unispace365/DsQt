#pragma once

#include <QByteArray>
#include <QElapsedTimer>
#include <QMutex>
#include <QObject>
#include <QSharedPointer>
#include <QSize>
#include <QTimer>
#include <QVideoFrame>
#include <functional>
#include <memory>

struct DsCaptureFormat {
    QSize size;
    float maxFrameRate = 0;
    QVideoFrameFormat::PixelFormat pixelFormat = QVideoFrameFormat::Format_Invalid;
    bool operator==(const DsCaptureFormat&) const = default;
};

struct DsCaptureDevice {
    QByteArray id;
    QString name;
    QList<DsCaptureFormat> formats;
};

// The backend boundary also allows deterministic capture tests without opening hardware.
class DsCaptureBackend : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual void start(const DsCaptureDevice& device, const DsCaptureFormat& format) = 0;
    virtual void stop() = 0;
signals:
    void frameReady(const QVideoFrame& frame);
    void failed(const QString& message);
};

// One session per physical device. All methods except enqueueFrame run on the GUI thread.
class DsCaptureSession : public QObject {
    Q_OBJECT
public:
    using DeviceProvider = std::function<QList<DsCaptureDevice>()>;
    using BackendFactory = std::function<std::unique_ptr<DsCaptureBackend>()>;
    static QList<DsCaptureDevice> devices();
    static DsCaptureDevice selectDevice(const QList<DsCaptureDevice>& devices,
        const QString& name, const QString& hexId, QString* error);
    static DsCaptureFormat selectFormat(const QList<DsCaptureFormat>& formats, const QSize& requested);
    static QSharedPointer<DsCaptureSession> acquire(const DsCaptureDevice& device, const QSize& requested);
    static void setHardwareEnabled(bool enabled);
    static bool hardwareEnabled();
    // May only be changed while no sessions are alive. Empty callbacks restore Qt Multimedia.
    static bool setBackendProvider(DeviceProvider devices = {}, BackendFactory backend = {});

    ~DsCaptureSession() override;
    QByteArray deviceId() const { return m_device.id; }
    bool ready() const { return m_ready; }
    QString errorString() const { return m_error; }
    QSize frameSize() const { return m_frame.size(); }
    QVideoFrame frame() const { return m_frame; }
    void retry();
signals:
    void changed();
    void frameReady(const QVideoFrame& frame);
private:
    DsCaptureSession(const DsCaptureDevice& device, const DsCaptureFormat& format);
    void start();
    void fail(const QString& message);
    void enqueueFrame(const QVideoFrame& frame);
    void deliverFrame();

    DsCaptureDevice m_device;
    DsCaptureFormat m_format;
    QList<DsCaptureFormat> m_failedFormats;
    std::unique_ptr<DsCaptureBackend> m_backend;
    QTimer m_watchdog;
    QTimer m_retry;
    QElapsedTimer m_lastFrame;
    QVideoFrame m_frame;
    QString m_error;
    bool m_ready = false;
    bool m_running = false;
    bool m_everReady = false;
    QMutex m_frameMutex;
    QVideoFrame m_pendingFrame;
    bool m_deliveryPending = false;
};
