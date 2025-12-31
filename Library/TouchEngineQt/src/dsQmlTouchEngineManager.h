#pragma once

#include "dsQmlTouchEngineInstance.h"
#include <QObject>
#include <QHash>
#include <QUuid>
#include <QtQmlIntegration>
#include <QQuickWindow>

#ifdef _WIN32
#include <QVulkanInstance>
#endif


/**
 * DsQmlTouchEngineManager - Manages multiple TouchEngine instances
 * Singleton class that coordinates all TouchEngine instances in the application
 *
 * Note: QML API uses QString for instance IDs, internally uses QUuid
 */
class DsQmlTouchEngineManager : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(DsTouchEngineManager)
    QML_SINGLETON
    Q_PROPERTY(int instanceCount READ instanceCount NOTIFY instanceCountChanged)
    Q_PROPERTY(QQuickWindow* window READ window WRITE setWindow NOTIFY windowChanged FINAL)
    Q_PROPERTY(bool isInitialized READ isInitialized WRITE setIsInitialized NOTIFY isInitializedChanged FINAL)
public:

    static DsQmlTouchEngineManager *create(QQmlEngine *engine, QJSEngine *scriptEngine);
    static DsQmlTouchEngineManager* inst();

    // Create and manage instances - QML API uses QString
    Q_INVOKABLE QString createInstance();
    Q_INVOKABLE void destroyInstance(const QString& idString);
    Q_INVOKABLE DsQmlTouchEngineInstance* getInstance(const QString& idString);
    Q_INVOKABLE DsQmlTouchEngineInstance* getInstanceByName(const QString& nameString);

    // Get instance count
    int instanceCount() const { return m_instances.count(); }

    // Set graphics API type
    void setGraphicsAPI(DsQmlTouchEngineInstance::TEGraphicsAPI apiType) { m_graphicsAPI = apiType; }
    DsQmlTouchEngineInstance::TEGraphicsAPI graphicsAPI() const { return m_graphicsAPI; }

    // Initialize graphics for all instances
    bool initializeGraphics(QRhi* rhi,void* nativeDevice = nullptr);

    QQuickWindow *window() const;
    void setWindow(QQuickWindow *newWindow);
    QOpenGLContext* getShareContext() const { return m_glShareContext; }
    QOpenGLContext* getRenderContext() const { return m_glContextQt; }
    bool isInitialized() const;
    void setIsInitialized(bool newIsInitialized);

signals:
    void instanceCountChanged();
    void instanceCreated(const QString& idString);
    void instanceDestroyed(const QString& idString);
    void windowChanged();
    void isInitializedChanged();

private:

    explicit DsQmlTouchEngineManager(QObject* parent = nullptr);
    ~DsQmlTouchEngineManager() override;
    static DsQmlTouchEngineManager* s_instance;

    // Internal storage uses QUuid for type safety
    QHash<QUuid, DsQmlTouchEngineInstance*> m_instances;
    void* m_nativeDevice = nullptr;
    QRhi* m_rhi = nullptr;
    DsQmlTouchEngineInstance::TEGraphicsAPI m_graphicsAPI = DsQmlTouchEngineInstance::TEGraphicsAPI_D3D11;

    // Conversion helper
    QUuid stringToUuid(const QString& str) const {
        QUuid uuid = QUuid::fromString(str);
        return uuid.isNull() ? QUuid() : uuid;
    }
    QQuickWindow *m_window = nullptr;

    // OpenGL context sharing
    QOpenGLContext *m_glContextQt = nullptr;
    QOpenGLContext *m_glShareContext = nullptr;

    bool m_isInitialized = false;

    // Helper methods for different graphics APIs
    void initializeOpenGL();
    void initializeD3D12();
    void initializeVulkan();
};
