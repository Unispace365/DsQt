#include "touchenginemanager.h"
#include <QDebug>
#include <QOpenGLContext>

TouchEngineManager* TouchEngineManager::s_instance = nullptr;

TouchEngineManager *TouchEngineManager::create(QQmlEngine *engine, QJSEngine *scriptEngine) {
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)
    qInfo()<<"using create to make TouchEngineManager singleton";
    return TouchEngineManager::inst();
}

TouchEngineManager* TouchEngineManager::inst()
{
    if (!s_instance) {
        s_instance = new TouchEngineManager();
    }
    return s_instance;
}

TouchEngineManager::TouchEngineManager(QObject* parent)
    : QObject(parent)
{
    qDebug() << "TouchEngineManager created";
}

TouchEngineManager::~TouchEngineManager()
{
    // Clean up all instances
    qDebug() << "TouchEngineManager destroyed, cleaning up" << m_instances.count() << "instances";
    qDeleteAll(m_instances);
    m_instances.clear();
}

QString TouchEngineManager::createInstance()
{
    if(!m_isInitialized){
        qWarning() << "TouchEngineManager is not initialized. Cannot create instance.";
        return QString();
    }
    QRhi* rhi = nullptr;
    if(m_window){
        rhi = m_window->rhi();
    }
    auto* instance = new TouchEngineInstance(this,m_window);

    // Initialize with graphics device if available
    if (m_nativeDevice || m_graphicsAPI == TouchEngineInstance::TEGraphicsAPI_OpenGL) {
        if (!instance->initialize(rhi, m_graphicsAPI)) {
            qWarning() << "Failed to initialize new instance with graphics API:" << m_graphicsAPI;
        }
    }

    QUuid id = instance->instanceId();
    QString idString = id.toString(QUuid::WithoutBraces);
    m_instances[id] = instance;
    emit instanceCountChanged();
    emit instanceCreated(idString);

    qDebug() << "Created TouchEngine instance:" << idString << "with API:" << m_graphicsAPI;

    return idString;
}

void TouchEngineManager::destroyInstance(const QString& idString)
{
    QUuid id = stringToUuid(idString);

    if (m_instances.contains(id)) {
        TouchEngineInstance* instance = m_instances.take(id);
        instance->deleteLater();

        emit instanceCountChanged();
        emit instanceDestroyed(idString);

        qDebug() << "Destroyed TouchEngine instance:" << idString;
    } else {
        qWarning() << "Attempted to destroy non-existent instance:" << idString;
    }
}

TouchEngineInstance* TouchEngineManager::getInstance(const QString& idString)
{
    QUuid id = stringToUuid(idString);
    return m_instances.value(id, nullptr);
}

TouchEngineInstance *TouchEngineManager::getInstanceByName(const QString &nameString)
{
    for(auto instance : m_instances) {
        if(instance->name() == nameString) {
            return instance;
        }
    }
    qWarning() << "Instance with name" << nameString << "not found";
    return nullptr;
}

bool TouchEngineManager::initializeGraphics(QRhi* rhi,void* nativeDevice)
{
    m_nativeDevice = nativeDevice;

    qDebug() << "Initializing graphics with API:" << m_graphicsAPI
             << "Device:" << nativeDevice;

    // Initialize all existing instances
    bool allSuccess = true;
    for (TouchEngineInstance* instance : m_instances) {
        if (!instance->initialize(rhi, m_graphicsAPI)) {
            qWarning() << "Failed to initialize graphics for instance:" << instance->instanceId();
            allSuccess = false;
        }
    }

    if (allSuccess) {
        qDebug() << "Successfully initialized graphics for" << m_instances.count() << "instances";
    }

    return allSuccess;
}

QQuickWindow *TouchEngineManager::window() const
{
    return m_window;
}

void TouchEngineManager::setWindow(QQuickWindow *newWindow)
{
    if (m_window == newWindow)
        return;
    if(m_window) disconnect(m_window);
    m_window = newWindow;
    if(m_window) {
        connect(m_window,&QQuickWindow::beforeSynchronizing,this,[this](){
            // Initialize graphics if not already done
            if (!m_glContextQt) {
                qDebug() << "Setting up shared OpenGL context for TouchEngineManager";
                const QRhiGles2NativeHandles* handles = static_cast<const QRhiGles2NativeHandles*>(m_window->rhi()->nativeHandles());
                auto currentContext = handles->context;
                m_glContextQt = currentContext;
                m_glShareContext = new QOpenGLContext;
                m_glShareContext->setFormat(m_glContextQt->format());
                m_glShareContext->setShareContext(m_glContextQt);
                QSurface *surface = m_glContextQt->surface();
                m_glContextQt->doneCurrent();
                if (!m_glShareContext->create())
                    qWarning() << "Failed to create share context";
                m_glContextQt->makeCurrent(surface);
                setIsInitialized(true);
            }
        },Qt::DirectConnection);
    }
    emit windowChanged();
}

bool TouchEngineManager::isInitialized() const
{
    return m_isInitialized;
}

void TouchEngineManager::setIsInitialized(bool newIsInitialized)
{
    if (m_isInitialized == newIsInitialized)
        return;
    m_isInitialized = newIsInitialized;
    emit isInitializedChanged();
}
