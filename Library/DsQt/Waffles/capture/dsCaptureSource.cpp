#include "dsCaptureSource.h"

DsCaptureSource::DsCaptureSource(QObject* parent) : QObject(parent) {
    m_poll.setInterval(2000);
    connect(&m_poll, &QTimer::timeout, this, &DsCaptureSource::update);
}
DsCaptureSource::~DsCaptureSource() { release(); }
void DsCaptureSource::setDeviceName(const QString& name) {
    if (m_name == name) return;
    m_name = name; release(); scheduleUpdate(); emit configurationChanged();
}
void DsCaptureSource::setDeviceId(const QString& id) {
    if (m_id == id) return;
    m_id = id; release(); scheduleUpdate(); emit configurationChanged();
}
void DsCaptureSource::setRequestedSize(const QSize& size) {
    if (m_requestedSize == size) return;
    m_requestedSize = size; release(); scheduleUpdate(); emit configurationChanged();
}
void DsCaptureSource::setActive(bool active) {
    if (m_active == active) return;
    m_active = active;
    if (!active) { m_poll.stop(); release(); m_error.clear(); emit changed(); }
    scheduleUpdate(); emit configurationChanged();
}
void DsCaptureSource::setVideoSink(QVideoSink* sink) {
    if (m_sink == sink) return;
    if (m_sink) m_sink->setVideoFrame({});
    m_sink = sink;
    if (m_sink && m_session) m_sink->setVideoFrame(m_session->frame());
    emit videoSinkChanged();
}
void DsCaptureSource::scheduleUpdate() {
    if (m_updatePending) return;
    m_updatePending = true;
    QTimer::singleShot(0, this, [this] { m_updatePending = false; update(); });
}
void DsCaptureSource::release() {
    if (m_session) disconnect(m_session.get(), nullptr, this, nullptr);
    m_session.reset();
    if (m_sink) m_sink->setVideoFrame({});
}
void DsCaptureSource::refresh() {
    update();
    if (m_session) m_session->retry();
}
void DsCaptureSource::update() {
    if (!m_active) return;
    if (!m_poll.isActive()) m_poll.start();
    QString error;
    const auto device = DsCaptureSession::selectDevice(DsCaptureSession::devices(), m_name, m_id, &error);
    if (!DsCaptureSession::hardwareEnabled()) error = tr("Capture is disabled in preview mode.");
    if (device.id.isEmpty()) {
        const bool wasReady = ready();
        const auto previous = errorString();
        release(); m_error = error;
        if (wasReady || previous != m_error) emit changed();
        return;
    }
    if (m_session && m_session->deviceId() == device.id) return;
    release(); m_error.clear();
    m_session = DsCaptureSession::acquire(device, m_requestedSize);
    if (m_session) {
        connect(m_session.get(), &DsCaptureSession::changed, this, &DsCaptureSource::changed);
        connect(m_session.get(), &DsCaptureSession::frameReady, this, [this](const QVideoFrame& frame) {
            if (m_sink) m_sink->setVideoFrame(frame);
        });
        if (m_sink) m_sink->setVideoFrame(m_session->frame());
    }
    emit changed();
}
