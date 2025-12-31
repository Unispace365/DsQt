#pragma once

#include "touchengineinstance.h"
#include <QObject>
#include <QHash>
#include <QUuid>
#include <QtQmlIntegration>
#include <QQuickWindow>

#ifdef _WIN32
#include <QVulkanInstance>
#endif


/**
 * TouchEngineManager - Manages multiple TouchEngine instances
 * Singleton class that coordinates all TouchEngine instances in the application
 *
 * Note: QML API uses QString for instance IDs, internally uses QUuid
 */
class TouchEngineManager : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(TouchEngineManager)
    QML_SINGLETON
    Q_PROPERTY(int instanceCount READ instanceCount NOTIFY instanceCountChanged)
    Q_PROPERTY(QQuickWindow* window READ window WRITE setWindow NOTIFY windowChanged FINAL)
    Q_PROPERTY(bool isInitialized READ isInitialized WRITE setIsInitialized NOTIFY isInitializedChanged FINAL)
public:

    static TouchEngineManager *create(QQmlEngine *engine, QJSEngine *scriptEngine);
    static TouchEngineManager* inst();

    // Create and manage instances - QML API uses QString
    Q_INVOKABLE QString createInstance();
    Q_INVOKABLE void destroyInstance(const QString& idString);
    Q_INVOKABLE TouchEngineInstance* getInstance(const QString& idString);
    Q_INVOKABLE TouchEngineInstance* getInstanceByName(const QString& nameString);

    // Get instance count
    int instanceCount() const { return m_instances.count(); }

    // Set graphics API type
    void setGraphicsAPI(TouchEngineInstance::TEGraphicsAPI apiType) { m_graphicsAPI = apiType; }
    TouchEngineInstance::TEGraphicsAPI graphicsAPI() const { return m_graphicsAPI; }

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

    explicit TouchEngineManager(QObject* parent = nullptr);
    ~TouchEngineManager() override;
    static TouchEngineManager* s_instance;

    // Internal storage uses QUuid for type safety
    QHash<QUuid, TouchEngineInstance*> m_instances;
    void* m_nativeDevice = nullptr;
    QRhi* m_rhi = nullptr;
    TouchEngineInstance::TEGraphicsAPI m_graphicsAPI = TouchEngineInstance::TEGraphicsAPI_D3D11;

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
