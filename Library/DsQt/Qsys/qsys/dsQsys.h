#pragma once

#include <QObject>
#include <QJsonObject>
#include <QVariantMap>
#include <QVariantList>
#include <QPointer>
#include <QHash>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;
class QWebSocket;
class QWebSocketServer;

namespace dsqt {

/// Application-wide endpoint for the Elevate Q-Sys plugin. Q-Sys connects IN
/// to this WebSocket server. Use on the application thread, from QML or C++.
/// Settings: engine.qsys.enabled (false), engine.qsys.port (9988).
class DsQsys : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Qsys)
    QML_SINGLETON
    Q_PROPERTY(bool enabled READ enabled NOTIFY configurationChanged)
    Q_PROPERTY(int port READ port NOTIFY configurationChanged)
    Q_PROPERTY(bool listening READ listening NOTIFY listeningChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
    Q_PROPERTY(QVariantMap values READ values NOTIFY valuesChanged)
    Q_PROPERTY(QVariantList controls READ controls NOTIFY controlsChanged)
    Q_PROPERTY(QString serverName READ serverName WRITE setServerName NOTIFY serverNameChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

  public:
    static DsQsys* instance();
    static DsQsys* create(QQmlEngine* engine, QJSEngine*);
    ~DsQsys() override;

    bool enabled() const { return m_enabled; }
    int port() const { return m_port; }
    bool listening() const;
    bool connected() const { return !m_client.isNull(); }
    bool ready() const { return m_ready; }
    QVariantMap values() const { return m_values; }
    QVariantList controls() const { return m_controls; }
    QString serverName() const { return m_serverName; }
    void setServerName(const QString& name);
    QString lastError() const { return m_lastError; }

    Q_INVOKABLE bool hasValue(const QString& parameter) const { return m_values.contains(parameter); }
    Q_INVOKABLE QVariant value(const QString& parameter) const { return m_values.value(parameter); }
    /// True means queued on the connected socket, not acknowledged by hardware.
    /// No offline queue or optimistic update. Boolean controls accept "true"/"false" too.
    Q_INVOKABLE bool send(const QString& parameter, const QVariant& value);
    /// Choices and selection are independent. An empty list explicitly clears choices.
    Q_INVOKABLE bool sendList(const QString& parameter, const QStringList& choices);
    Q_INVOKABLE bool selectListItem(const QString& parameter, const QString& item);
    /// Retry a failed bind, or restart the current listener using engine settings.
    Q_INVOKABLE void restart();

  signals:
    /// Every received value, including initial snapshot and repeated values.
    /// Momentary true pulses also produce a local false after 500 ms of inactivity.
    void updated(const QString& parameter, const QVariant& value);
    void parameterRenamed(const QString& oldParameter, const QString& newParameter);
    void manifestReceived();
    void snapshotReady();
    void cleared();
    void errorOccurred(const QString& message);
    void configurationChanged();
    void listeningChanged();
    void connectedChanged();
    void readyChanged();
    void valuesChanged();
    void controlsChanged();
    void serverNameChanged();
    void lastErrorChanged();

  private:
    explicit DsQsys(QObject* parent);
    void configure(const QVariantMap& settings);
    void acceptConnection();
    void receive(const QString& text);
    bool transmit(const QString& parameter, QJsonObject fields);
    void clearSession();
    void stop();
    void reportError(const QString& message);
    void setReady(bool ready);
    void updateValue(const QString& parameter, const QVariant& value);
    QString controlType(const QString& parameter) const;

    QWebSocketServer* m_server;
    QPointer<QWebSocket> m_client;
    bool m_enabled = false;
    int m_port = 9988;
    bool m_ready = false;
    QString m_serverName = QStringLiteral("DsQt");
    QString m_lastError;
    QVariantMap m_values;
    QVariantList m_controls;
    // Monotonic tokens invalidate pulse timers across repeats, renames and reconnects.
    quint64 m_revision = 0;
    QHash<QString, quint64> m_pulses;
};

} // namespace dsqt
