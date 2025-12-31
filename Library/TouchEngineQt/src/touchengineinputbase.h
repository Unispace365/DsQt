#ifndef TOUCHENGINEINPUTBASE_H
#define TOUCHENGINEINPUTBASE_H

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QVariant>

#include "touchengineinstance.h"
#include <TouchEngine/TEInstance.h>

class TouchEngineInstance;

/**
 * Base class for all TouchEngine input components
 * Provides common functionality for setting values on TouchEngine links
 */
class TouchEngineInputBase : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(TouchEngineInputBase)
    QML_UNCREATABLE("TouchEngineInputBase is an abstract base class")
    
    Q_PROPERTY(QString instanceId READ instanceId WRITE setInstanceId NOTIFY instanceIdChanged)
    Q_PROPERTY(QString linkName READ linkName WRITE setLinkName NOTIFY linkNameChanged)
    Q_PROPERTY(bool autoUpdate READ autoUpdate WRITE setAutoUpdate NOTIFY autoUpdateChanged)
    Q_PROPERTY(bool isConnected READ isConnected NOTIFY isConnectedChanged)
    
public:
    explicit TouchEngineInputBase(QObject *parent = nullptr);
    virtual ~TouchEngineInputBase();
    
    QString instanceId() const { return m_instanceId; }
    void setInstanceId(const QString &id);
    
    QString linkName() const { return m_linkName; }
    void setLinkName(const QString &name);
    
    bool autoUpdate() const { return m_autoUpdate; }
    void setAutoUpdate(bool auto_update);
    
    bool isConnected() const { return m_instance != nullptr; }
    
    Q_INVOKABLE virtual void updateValue();
    
signals:
    void instanceIdChanged();
    void linkNameChanged();
    void autoUpdateChanged();
    void isConnectedChanged();
    void errorOccurred(const QString &error);
    
protected:
    virtual void applyValue(TEInstance *teInstance) = 0;
    virtual void handleCanUpdateLinks(TEInstance *teInstance);
    void connectToInstance();
    void disconnectFromInstance();
    
    TouchEngineInstance* getInstance() const { return m_instance; }
    
private:
    QString m_instanceId;
    QString m_linkName;
    bool m_autoUpdate = true;
    TouchEngineInstance *m_instance = nullptr;
    bool m_isDirty = false;
};

#endif // TOUCHENGINEINPUTBASE_H
