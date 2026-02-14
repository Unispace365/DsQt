#include "dsSpoutReceiver.h"

#include <QDebug>

#include "SpoutDX.h"

// ─────────────────────────────────────────────────────────────────────
// Worker — runs on a dedicated QThread, owns the spoutDX instance
// ─────────────────────────────────────────────────────────────────────
class DsSpoutReceiver::Worker : public QObject
{
    Q_OBJECT
public:
    explicit Worker(const QString& senderName, QObject* parent = nullptr)
        : QObject(parent)
        , m_senderName(senderName)
    {}

    ~Worker() override
    {
        stop();
    }

public slots:
    void start()
    {
        if (m_running)
            return;

        // Let SpoutDX create its own internal D3D11 device
        if (!m_spout.OpenDirectX11()) {
            qWarning() << "DsSpoutReceiver::Worker: Failed to open DirectX 11";
            return;
        }

        if (!m_senderName.isEmpty()) {
            m_spout.SetReceiverName(m_senderName.toStdString().c_str());
        }

        m_running = true;

        // Poll at ~120 Hz
        m_pollTimer = new QTimer(this);
        m_pollTimer->setInterval(8);
        connect(m_pollTimer, &QTimer::timeout, this, &Worker::poll);
        m_pollTimer->start();
    }

    void stop()
    {
        if (!m_running)
            return;

        m_running = false;

        if (m_pollTimer) {
            m_pollTimer->stop();
            delete m_pollTimer;
            m_pollTimer = nullptr;
        }

        m_spout.ReleaseReceiver();
        m_spout.CloseDirectX11();

        if (m_wasConnected) {
            m_wasConnected = false;
            emit connectionChanged(false);
        }
    }

    void setSenderName(const QString& name)
    {
        if (m_senderName != name) {
            m_senderName = name;

            if (m_running) {
                // Release current connection and reconnect with new name
                m_spout.ReleaseReceiver();
                if (!m_senderName.isEmpty()) {
                    m_spout.SetReceiverName(m_senderName.toStdString().c_str());
                }
            }
        }
    }

signals:
    void frameReceived(HANDLE handle, int width, int height, double fps, DXGI_FORMAT format);
    void imageReady(const QImage& image);
    void connectionChanged(bool connected);

private slots:
    void poll()
    {
        if (!m_running)
            return;

        // ReceiveTexture() connects to the sender (or the active sender if
        // no name was set) and returns true while connected.
        if (m_spout.ReceiveTexture()) {
            if (!m_wasConnected) {
                m_wasConnected = true;
                emit connectionChanged(true);
            }

            if (m_spout.IsFrameNew()) {
                int w = static_cast<int>(m_spout.GetSenderWidth());
                int h = static_cast<int>(m_spout.GetSenderHeight());
                double fps = m_spout.GetSenderFps();
                HANDLE handle = m_spout.GetSenderHandle();
                DXGI_FORMAT format = static_cast<DXGI_FORMAT>(m_spout.GetSenderFormat());

                emit frameReceived(handle, w, h, fps, format);

                // Only do GPU→CPU copy when someone is listening
                if (receivers(SIGNAL(imageReady(QImage))) > 0) {
                    // Receive into a pixel buffer
                    QImage img(w, h, QImage::Format_RGBA8888);
                    if (m_spout.ReceiveImage(img.bits(), w, h)) {
                        emit imageReady(img);
                    }
                }
            }
        } else {
            if (m_wasConnected) {
                m_wasConnected = false;
                emit connectionChanged(false);
            }
        }
    }

private:
    spoutDX m_spout;
    QString m_senderName;
    QTimer* m_pollTimer = nullptr;
    bool m_running = false;
    bool m_wasConnected = false;
};

// ─────────────────────────────────────────────────────────────────────
// DsSpoutReceiver — public API, lives on the GUI thread
// ─────────────────────────────────────────────────────────────────────

DsSpoutReceiver::DsSpoutReceiver(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<HANDLE>("HANDLE");
    qRegisterMetaType<DXGI_FORMAT>("DXGI_FORMAT");
}

DsSpoutReceiver::~DsSpoutReceiver()
{
    stopWorker();
}

QString DsSpoutReceiver::senderName() const
{
    return m_senderName;
}

void DsSpoutReceiver::setSenderName(const QString& name)
{
    if (m_senderName != name) {
        m_senderName = name;
        emit senderNameChanged();

        if (m_worker) {
            QMetaObject::invokeMethod(m_worker, "setSenderName",
                                      Qt::QueuedConnection, Q_ARG(QString, name));
        }
    }
}

bool DsSpoutReceiver::isConnected() const
{
    QMutexLocker lock(&m_mutex);
    return m_connected;
}

int DsSpoutReceiver::senderWidth() const
{
    QMutexLocker lock(&m_mutex);
    return m_senderWidth;
}

int DsSpoutReceiver::senderHeight() const
{
    QMutexLocker lock(&m_mutex);
    return m_senderHeight;
}

QSize DsSpoutReceiver::senderSize() const
{
    QMutexLocker lock(&m_mutex);
    return QSize(m_senderWidth, m_senderHeight);
}

double DsSpoutReceiver::senderFps() const
{
    QMutexLocker lock(&m_mutex);
    return m_senderFps;
}

bool DsSpoutReceiver::isActive() const
{
    return m_active;
}

void DsSpoutReceiver::setActive(bool active)
{
    if (m_active != active) {
        m_active = active;
        emit activeChanged();

        if (m_active) {
            startWorker();
        } else {
            stopWorker();
        }
    }
}

HANDLE DsSpoutReceiver::sharedTextureHandle() const
{
    QMutexLocker lock(&m_mutex);
    return m_sharedHandle;
}

DXGI_FORMAT DsSpoutReceiver::senderFormat() const
{
    QMutexLocker lock(&m_mutex);
    return m_senderFormat;
}

QImage DsSpoutReceiver::lastFrame() const
{
    QMutexLocker lock(&m_mutex);
    return m_lastFrame;
}

void DsSpoutReceiver::startWorker()
{
    if (m_workerThread)
        return;

    m_workerThread = new QThread(this);
    m_workerThread->setObjectName(QStringLiteral("SpoutReceiverWorker"));

    m_worker = new Worker(m_senderName);
    m_worker->moveToThread(m_workerThread);

    // Wire up worker signals → receiver slots (queued, cross-thread)
    connect(m_worker, &Worker::frameReceived,
            this, &DsSpoutReceiver::onWorkerFrameReceived, Qt::QueuedConnection);
    connect(m_worker, &Worker::imageReady,
            this, &DsSpoutReceiver::onWorkerImageReady, Qt::QueuedConnection);
    connect(m_worker, &Worker::connectionChanged,
            this, &DsSpoutReceiver::onWorkerConnectionChanged, Qt::QueuedConnection);

    // Clean up worker when thread finishes
    connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);

    // Start polling once the thread event loop is running
    connect(m_workerThread, &QThread::started, m_worker, &Worker::start);

    m_workerThread->start();
}

void DsSpoutReceiver::stopWorker()
{
    if (!m_workerThread)
        return;

    // Tell the worker to stop (runs on worker thread)
    QMetaObject::invokeMethod(m_worker, "stop", Qt::QueuedConnection);

    m_workerThread->quit();
    m_workerThread->wait();

    delete m_workerThread;
    m_workerThread = nullptr;
    m_worker = nullptr; // deleted by deleteLater when thread finished

    {
        QMutexLocker lock(&m_mutex);
        m_connected = false;
        m_sharedHandle = nullptr;
        m_senderFormat = DXGI_FORMAT_UNKNOWN;
        m_senderWidth = 0;
        m_senderHeight = 0;
        m_senderFps = 0.0;
        m_lastFrame = QImage();
    }

    emit connectedChanged();
}

void DsSpoutReceiver::onWorkerFrameReceived(HANDLE handle, int width, int height, double fps, DXGI_FORMAT format)
{
    bool sizeChanged = false;
    bool fpsChanged = false;

    {
        QMutexLocker lock(&m_mutex);
        m_sharedHandle = handle;
        m_senderFormat = format;

        if (m_senderWidth != width || m_senderHeight != height) {
            m_senderWidth = width;
            m_senderHeight = height;
            sizeChanged = true;
        }
        if (!qFuzzyCompare(m_senderFps, fps)) {
            m_senderFps = fps;
            fpsChanged = true;
        }
    }

    if (sizeChanged) emit senderSizeChanged();
    if (fpsChanged) emit senderFpsChanged();
    emit frameReceived();
}

void DsSpoutReceiver::onWorkerImageReady(const QImage& image)
{
    {
        QMutexLocker lock(&m_mutex);
        m_lastFrame = image;
    }

    emit frameImageReady(image);
}

void DsSpoutReceiver::onWorkerConnectionChanged(bool connected)
{
    {
        QMutexLocker lock(&m_mutex);
        m_connected = connected;
    }

    emit connectedChanged();
}

#include "dsSpoutReceiver.moc"
