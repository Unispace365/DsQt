#include "dsQmlTouchEngineManager.h"
#include <QDebug>
#include <QOpenGLContext>
#include <rhi/qrhi.h>

DsQmlTouchEngineManager* DsQmlTouchEngineManager::s_instance = nullptr;

DsQmlTouchEngineManager *DsQmlTouchEngineManager::create(QQmlEngine *engine, QJSEngine *scriptEngine) {
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)
    qInfo()<<"using create to make DsQmlTouchEngineManager singleton";
    return DsQmlTouchEngineManager::inst();
}

DsQmlTouchEngineManager* DsQmlTouchEngineManager::inst()
{
    if (!s_instance) {
        s_instance = new DsQmlTouchEngineManager();
    }
    return s_instance;
}

DsQmlTouchEngineManager::DsQmlTouchEngineManager(QObject* parent)
    : QObject(parent)
{
    qDebug() << "DsQmlTouchEngineManager created";
}

DsQmlTouchEngineManager::~DsQmlTouchEngineManager()
{
    // Clean up all instances
    qDebug() << "DsQmlTouchEngineManager destroyed, cleaning up" << m_instances.count() << "instances";
    qDeleteAll(m_instances);
    m_instances.clear();
}

QString DsQmlTouchEngineManager::createInstance()
{
    if(!m_isInitialized){
        qWarning() << "DsQmlTouchEngineManager is not initialized. Cannot create instance.";
        return QString();
    }
    QRhi* rhi = nullptr;
    if(m_window){
        rhi = m_window->rhi();
    }
    auto* instance = new DsQmlTouchEngineInstance(this,m_window);

    // Initialize with graphics device if available
    if (m_nativeDevice || m_graphicsAPI == DsQmlTouchEngineInstance::TEGraphicsAPI_OpenGL) {
        if (!instance->initialize(rhi, m_graphicsAPI)) {
            qWarning() << "Failed to initialize new instance with graphics API:" << m_graphicsAPI;
        }
    }

    QUuid id = instance->instanceId();
    QString idString = id.toString(QUuid::WithoutBraces);
    m_instances[id] = instance;
    emit instanceCountChanged();
    emit instanceCreated(idString);

    qDebug() << "DsQmlTouchEngineManager: Created TouchEngine instance:" << idString << "with API:" << m_graphicsAPI;

    return idString;
}

void DsQmlTouchEngineManager::destroyInstance(const QString& idString)
{
    QUuid id = stringToUuid(idString);

    if (m_instances.contains(id)) {
        DsQmlTouchEngineInstance* instance = m_instances.take(id);
        instance->deleteLater();

        emit instanceCountChanged();
        emit instanceDestroyed(idString);

        qDebug() << "Destroyed TouchEngine instance:" << idString;
    } else {
        qWarning() << "Attempted to destroy non-existent instance:" << idString;
    }
}

DsQmlTouchEngineInstance* DsQmlTouchEngineManager::getInstance(const QString& idString)
{
    QUuid id = stringToUuid(idString);
    return m_instances.value(id, nullptr);
}

DsQmlTouchEngineInstance *DsQmlTouchEngineManager::getInstanceByName(const QString &nameString)
{
    for(auto instance : m_instances) {
        if(instance->name() == nameString) {
            return instance;
        }
    }
    qWarning() << "Instance with name" << nameString << "not found";
    return nullptr;
}

bool DsQmlTouchEngineManager::initializeGraphics(QRhi* rhi,void* nativeDevice)
{
    m_nativeDevice = nativeDevice;
    qDebug() << "DsQmlTouchEngineManager: Initializing graphics with API:" <<m_graphicsAPI
             << "Device:" << nativeDevice;

    // Initialize all existing instances
    bool allSuccess = true;
    for (DsQmlTouchEngineInstance* instance : m_instances) {
        if (!instance->initialize(rhi, m_graphicsAPI)) {
            qWarning() << "Failed to initialize graphics for instance:" << instance->instanceId();
            allSuccess = false;
        }
    }

    if (allSuccess) {
        qDebug() << "DsQmlTouchEngineManager: Successfully initialized graphics for" << m_instances.count() << "instances";
    }

    return allSuccess;
}

QQuickWindow *DsQmlTouchEngineManager::window() const
{
    return m_window;
}

void DsQmlTouchEngineManager::setWindow(QQuickWindow *newWindow)
{
    if (m_window == newWindow)
        return;
    if(m_window) disconnect(m_window);
    m_window = newWindow;
    if(m_window) {
        connect(m_window, &QQuickWindow::beforeSynchronizing, this, [this]() {
            if (m_isInitialized)
                return;

            QRhi* rhi = m_window->rhi();
            if (!rhi) {
                qWarning() << "DsQmlTouchEngineManager: No RHI available";
                return;
            }

            // Detect graphics API from RHI backend
            QRhi::Implementation backend = rhi->backend();
            qDebug() << "DsQmlTouchEngineManager: RHI backend is" << backend;

            switch (backend) {
                case QRhi::OpenGLES2:
                    m_graphicsAPI = DsQmlTouchEngineInstance::TEGraphicsAPI_OpenGL;
                    initializeOpenGL();
                    break;
#ifdef _WIN32
                case QRhi::D3D11:
                    // D3D11 not fully implemented, fall through to D3D12 or use OpenGL
                    qWarning() << "DsQmlTouchEngineManager: D3D11 backend detected but not fully supported, using D3D12";
                    m_graphicsAPI = DsQmlTouchEngineInstance::TEGraphicsAPI_D3D12;
                    initializeD3D12();
                    break;
                case QRhi::D3D12:
                    m_graphicsAPI = DsQmlTouchEngineInstance::TEGraphicsAPI_D3D12;
                    initializeD3D12();
                    break;
                case QRhi::Vulkan:
                    m_graphicsAPI = DsQmlTouchEngineInstance::TEGraphicsAPI_Vulkan;
                    initializeVulkan();
                    break;
#endif
                default:
                    qWarning() << "DsQmlTouchEngineManager: Unsupported RHI backend:" << backend;
                    break;
            }
        }, Qt::DirectConnection);
    }
    emit windowChanged();
}

void DsQmlTouchEngineManager::initializeOpenGL()
{
    qDebug() << "DsQmlTouchEngineManager: Initializing OpenGL";

    if (m_glContextQt) {
        // Already initialized
        return;
    }

    QRhi* rhi = m_window->rhi();
    const QRhiGles2NativeHandles* handles = static_cast<const QRhiGles2NativeHandles*>(rhi->nativeHandles());
    if (!handles || !handles->context) {
        qWarning() << "DsQmlTouchEngineManager: Failed to get OpenGL context from RHI";
        return;
    }

    m_glContextQt = handles->context;
    m_glShareContext = new QOpenGLContext;
    m_glShareContext->setFormat(m_glContextQt->format());
    m_glShareContext->setShareContext(m_glContextQt);

    QSurface* surface = m_glContextQt->surface();
    m_glContextQt->doneCurrent();

    if (!m_glShareContext->create()) {
        qWarning() << "DsQmlTouchEngineManager: Failed to create OpenGL share context";
        delete m_glShareContext;
        m_glShareContext = nullptr;
        m_glContextQt->makeCurrent(surface);
        return;
    }

    m_glContextQt->makeCurrent(surface);
    qDebug() << "DsQmlTouchEngineManager: OpenGL initialized successfully";
    setIsInitialized(true);
}

void DsQmlTouchEngineManager::initializeD3D12()
{
#ifdef _WIN32
    qDebug() << "DsQmlTouchEngineManager: Initializing D3D12";

    // D3D12 doesn't require special context sharing like OpenGL
    // Each DsQmlTouchEngineInstance will create its own D3D12 device
    // and communicate with TouchEngine via shared NT handles

    qDebug() << "DsQmlTouchEngineManager: D3D12 initialized successfully";
    setIsInitialized(true);
#else
    qWarning() << "DsQmlTouchEngineManager: D3D12 not available on this platform";
#endif
}

void DsQmlTouchEngineManager::initializeVulkan()
{
#ifdef _WIN32
    qDebug() << "DsQmlTouchEngineManager: Initializing Vulkan";

    // Vulkan doesn't require special context sharing like OpenGL
    // Each DsQmlTouchEngineInstance will create its own Vulkan device
    // and communicate with TouchEngine via external memory handles

    qDebug() << "DsQmlTouchEngineManager: Vulkan initialized successfully";
    setIsInitialized(true);
#else
    qWarning() << "DsQmlTouchEngineManager: Vulkan not available on this platform";
#endif
}

bool DsQmlTouchEngineManager::isInitialized() const
{
    return m_isInitialized;
}

void DsQmlTouchEngineManager::setIsInitialized(bool newIsInitialized)
{
    if (m_isInitialized == newIsInitialized)
        return;
    m_isInitialized = newIsInitialized;
    emit isInitializedChanged();
}
