#include "dsCaptureSession.h"

#include <QCamera>
#include <QCoreApplication>
#include <QHash>
#include <QLoggingCategory>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QThread>
#include <QVideoSink>
#include <algorithm>
#include <cmath>
#include <tuple>

namespace {
Q_LOGGING_CATEGORY(captureLog, "dsqt.capture", QtWarningMsg)
QHash<QByteArray, QWeakPointer<DsCaptureSession>> sessions;
DsCaptureSession::DeviceProvider deviceProvider;
DsCaptureSession::BackendFactory backendFactory;
bool captureEnabled = true;

void requireGuiThread() {
    Q_ASSERT(QCoreApplication::instance());
    Q_ASSERT(QThread::currentThread() == QCoreApplication::instance()->thread());
}

class QtCaptureBackend final : public DsCaptureBackend {
public:
    QtCaptureBackend() {
        connect(&m_sink, &QVideoSink::videoFrameChanged, this, &DsCaptureBackend::frameReady, Qt::DirectConnection);
        connect(&m_camera, &QCamera::errorOccurred, this, [this](QCamera::Error, const QString& message) {
            emit failed(message);
        });
        m_session.setCamera(&m_camera);
        m_session.setVideoSink(&m_sink);
    }
    ~QtCaptureBackend() override { stop(); }
    void start(const DsCaptureDevice& device, const DsCaptureFormat& format) override {
        const auto devices = QMediaDevices::videoInputs();
        const auto found = std::find_if(devices.begin(), devices.end(), [&](const auto& candidate) { return candidate.id() == device.id; });
        if (found == devices.end()) { emit failed(tr("Capture device disconnected.")); return; }
        m_camera.setCameraDevice(*found);
        const auto formats = found->videoFormats();
        const auto chosen = std::find_if(formats.begin(), formats.end(), [&](const auto& candidate) {
            return candidate.resolution() == format.size && candidate.pixelFormat() == format.pixelFormat
                && candidate.maxFrameRate() == format.maxFrameRate;
        });
        if (chosen == formats.end()) { emit failed(tr("The requested capture format is no longer available.")); return; }
        m_camera.setCameraFormat(*chosen);
        m_camera.start();
    }
    void stop() override { m_camera.stop(); }
private:
    QCamera m_camera;
    QVideoSink m_sink;
    QMediaCaptureSession m_session;
};
}

QList<DsCaptureDevice> DsCaptureSession::devices() {
    requireGuiThread();
    if (!captureEnabled) return {};
    if (deviceProvider) return deviceProvider();
    QList<DsCaptureDevice> result;
    for (const auto& camera : QMediaDevices::videoInputs()) {
        DsCaptureDevice device{camera.id(), camera.description(), {}};
        for (const auto& format : camera.videoFormats())
            device.formats.append({format.resolution(), format.maxFrameRate(), format.pixelFormat()});
        result.append(device);
    }
    return result;
}

DsCaptureDevice DsCaptureSession::selectDevice(const QList<DsCaptureDevice>& devices,
    const QString& name, const QString& hexId, QString* error) {
    QList<DsCaptureDevice> matches;
    for (const auto& device : devices) {
        const bool match = hexId.isEmpty() ? !name.isEmpty() && device.name == name
            : QString::fromLatin1(device.id.toHex()).compare(hexId, Qt::CaseInsensitive) == 0;
        if (match) matches.append(device);
    }
    if (matches.size() == 1) { if (error) error->clear(); return matches.first(); }
    if (error) *error = matches.isEmpty() ? tr("Capture device not found. Check its connection and configured name.")
        : tr("Multiple capture devices have this name. Configure a device ID to select one.");
    return {};
}

DsCaptureFormat DsCaptureSession::selectFormat(const QList<DsCaptureFormat>& formats, const QSize& requested) {
    const QSize target = requested.isEmpty() ? QSize(1920, 1080) : requested;
    const auto score = [&](const DsCaptureFormat& f) {
        const double aspectDifference = std::abs(double(f.size.width()) / f.size.height() - double(target.width()) / target.height());
        const auto pixelDifference = std::abs(qint64(f.size.width()) * f.size.height() - qint64(target.width()) * target.height());
        return std::tuple(f.size != target, aspectDifference, pixelDifference, -f.maxFrameRate, int(f.pixelFormat));
    };
    DsCaptureFormat result;
    for (const auto& format : formats) {
        if (format.size.isEmpty() || format.maxFrameRate <= 0 || format.pixelFormat == QVideoFrameFormat::Format_Invalid) continue;
        if (result.size.isEmpty() || score(format) < score(result)) result = format;
    }
    return result;
}

QSharedPointer<DsCaptureSession> DsCaptureSession::acquire(const DsCaptureDevice& device, const QSize& requested) {
    requireGuiThread();
    if (!captureEnabled || device.id.isEmpty()) return {};
    if (auto existing = sessions.value(device.id).toStrongRef()) return existing;
    for (auto it = sessions.begin(); it != sessions.end();)
        if (it.value().isNull()) it = sessions.erase(it); else ++it;
    auto result = QSharedPointer<DsCaptureSession>(new DsCaptureSession(device, selectFormat(device.formats, requested)));
    sessions.insert(device.id, result.toWeakRef());
    return result;
}

void DsCaptureSession::setHardwareEnabled(bool enabled) {
    requireGuiThread();
    captureEnabled = enabled;
    if (!enabled) {
        const auto activeSessions = sessions.values();
        for (const auto& weak : activeSessions)
            if (auto session = weak.toStrongRef()) session->fail(tr("Capture is disabled in preview mode."));
    }
}
bool DsCaptureSession::hardwareEnabled() { return captureEnabled; }

bool DsCaptureSession::setBackendProvider(DeviceProvider devices, BackendFactory backend) {
    requireGuiThread();
    for (const auto& weak : std::as_const(sessions)) if (!weak.isNull()) return false;
    sessions.clear(); deviceProvider = std::move(devices); backendFactory = std::move(backend);
    return true;
}

DsCaptureSession::DsCaptureSession(const DsCaptureDevice& device, const DsCaptureFormat& format)
    : m_device(device), m_format(format), m_backend(backendFactory ? backendFactory() : std::make_unique<QtCaptureBackend>()) {
    m_watchdog.setInterval(1000);
    m_retry.setSingleShot(true); m_retry.setInterval(2000);
    connect(&m_watchdog, &QTimer::timeout, this, [this] {
        if (m_lastFrame.hasExpired(5000)) fail(tr("No video frames received. Check the capture input signal."));
    });
    connect(&m_retry, &QTimer::timeout, this, &DsCaptureSession::start);
    connect(m_backend.get(), &DsCaptureBackend::frameReady, this, &DsCaptureSession::enqueueFrame, Qt::DirectConnection);
    // Stopping a camera inside its error callback can reenter the platform backend.
    connect(m_backend.get(), &DsCaptureBackend::failed, this, &DsCaptureSession::fail, Qt::QueuedConnection);
    QTimer::singleShot(0, this, &DsCaptureSession::start);
}

DsCaptureSession::~DsCaptureSession() {
    disconnect(m_backend.get(), nullptr, this, nullptr);
    m_backend->stop();
}

void DsCaptureSession::start() {
    // A synchronous observer may release the last viewer during a notification.
    const auto keepAlive = sessions.value(m_device.id).toStrongRef();
    if (!captureEnabled) return;
    m_retry.stop();
    if (m_format.size.isEmpty()) { fail(tr("This capture device advertises no supported video formats.")); return; }
    if (!m_everReady) {
        auto candidates = m_device.formats;
        candidates.removeIf([&](const DsCaptureFormat& format) { return format.size != m_format.size; });
        auto untried = candidates;
        untried.removeIf([&](const DsCaptureFormat& format) { return m_failedFormats.contains(format); });
        if (untried.isEmpty()) { m_failedFormats.clear(); untried = candidates; }
        m_format = selectFormat(untried, m_format.size);
    }
    m_ready = false; m_running = true;
    m_lastFrame.start(); m_watchdog.start();
    qCDebug(captureLog) << "Starting capture:" << m_device.name << "size=" << m_format.size
                       << "max_fps=" << m_format.maxFrameRate << "pixel_format=" << m_format.pixelFormat;
    emit changed();
    m_backend->start(m_device, m_format);
}

void DsCaptureSession::fail(const QString& message) {
    const auto keepAlive = sessions.value(m_device.id).toStrongRef();
    qCDebug(captureLog) << "Capture failed:" << m_device.name << "error=" << message;
    if (!m_everReady && !m_format.size.isEmpty() && !m_failedFormats.contains(m_format)) m_failedFormats.append(m_format);
    m_watchdog.stop(); m_running = false;
    m_backend->stop();
    { QMutexLocker lock(&m_frameMutex); m_pendingFrame = {}; }
    m_frame = {}; m_ready = false;
    m_error = message.isEmpty() ? tr("Capture device is unavailable.") : message;
    emit frameReady({}); emit changed();
    if (captureEnabled && !m_retry.isActive()) m_retry.start();
}

void DsCaptureSession::retry() {
    if (!m_running) { m_retry.stop(); start(); }
}

void DsCaptureSession::enqueueFrame(const QVideoFrame& frame) {
    if (!frame.isValid()) return;
    QMutexLocker lock(&m_frameMutex);
    m_pendingFrame = frame;
    if (m_deliveryPending) return;
    m_deliveryPending = true;
    QMetaObject::invokeMethod(this, &DsCaptureSession::deliverFrame, Qt::QueuedConnection);
}

void DsCaptureSession::deliverFrame() {
    const auto keepAlive = sessions.value(m_device.id).toStrongRef();
    QVideoFrame frame;
    { QMutexLocker lock(&m_frameMutex); frame = m_pendingFrame; m_pendingFrame = {}; m_deliveryPending = false; }
    if (!m_running || !frame.isValid()) return;
    if (!m_ready)
        qCDebug(captureLog) << "Capture receiving frames:" << m_device.name << "startup_ms=" << m_lastFrame.elapsed()
                           << "size=" << frame.size() << "pixel_format=" << frame.pixelFormat();
    const bool changedState = !m_ready || m_frame.size() != frame.size() || !m_error.isEmpty();
    m_frame = frame; m_lastFrame.restart(); m_ready = true; m_everReady = true; m_error.clear();
    emit frameReady(frame);
    if (changedState) emit changed();
}
