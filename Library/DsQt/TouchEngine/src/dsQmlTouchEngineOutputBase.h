#ifndef DSQMLTOUCHENGINEOUTPUTBASE_H
#define DSQMLTOUCHENGINEOUTPUTBASE_H

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QVariant>

#include "dsQmlTouchEngineInstance.h"
#include <TouchEngine/TEInstance.h>

class DsQmlTouchEngineInstance;

/**
 * Base class for all TouchEngine output components
 * Provides common functionality for reading values from TouchEngine output links
 */
class DsQmlTouchEngineOutputBase : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(DsTouchEngineOutputBase)
    QML_UNCREATABLE("DsTouchEngineOutputBase is an abstract base class")

    Q_PROPERTY(QString instanceId READ instanceId WRITE setInstanceId NOTIFY instanceIdChanged)
    Q_PROPERTY(QString linkName READ linkName WRITE setLinkName NOTIFY linkNameChanged)
    Q_PROPERTY(bool autoUpdate READ autoUpdate WRITE setAutoUpdate NOTIFY autoUpdateChanged)
    Q_PROPERTY(bool isConnected READ isConnected NOTIFY isConnectedChanged)

public:
    explicit DsQmlTouchEngineOutputBase(QObject *parent = nullptr);
    virtual ~DsQmlTouchEngineOutputBase();

    QString instanceId() const { return m_instanceId; }
    void setInstanceId(const QString &id);

    QString linkName() const { return m_linkName; }
    void setLinkName(const QString &name);

    bool autoUpdate() const { return m_autoUpdate; }
    void setAutoUpdate(bool auto_update);

    bool isConnected() const { return m_instance != nullptr; }

    Q_INVOKABLE virtual void fetchValue();

signals:
    void instanceIdChanged();
    void linkNameChanged();
    void autoUpdateChanged();
    void isConnectedChanged();
    void errorOccurred(const QString &error);
    void valueUpdated();

protected:
    virtual void readValue(TEInstance *teInstance) = 0;
    virtual void handleFrameFinished();
    virtual void handleOutputLinkValueChanged(const QString& linkName);
    void connectToInstance();
    void disconnectFromInstance();

    DsQmlTouchEngineInstance* getInstance() const { return m_instance; }

private:
    QString m_instanceId;
    QString m_linkName;
    bool m_autoUpdate = true;
    DsQmlTouchEngineInstance *m_instance = nullptr;
    bool m_pendingRead = false;
};

#endif // DSQMLTOUCHENGINEOUTPUTBASE_H
