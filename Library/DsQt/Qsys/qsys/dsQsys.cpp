#include "dsQsys.h"
#include "dsSettings.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QQmlEngine>
#include <QSet>
#include <QThread>
#include <QTimer>
#include <QWebSocket>
#include <QWebSocketServer>

Q_LOGGING_CATEGORY(lgQsys, "qsys")

namespace dsqt {

DsQsys* DsQsys::instance() {
    Q_ASSERT(qApp && QThread::currentThread() == qApp->thread());
    static QPointer<DsQsys> singleton;
    if (!singleton) singleton = new DsQsys(qApp);
    return singleton;
}

DsQsys* DsQsys::create(QQmlEngine* engine, QJSEngine*) {
    auto* singleton = instance();
    Q_ASSERT(engine->thread() == singleton->thread());
    QQmlEngine::setObjectOwnership(singleton, QQmlEngine::CppOwnership);
    return singleton;
}

DsQsys::DsQsys(QObject* parent)
    : QObject(parent)
    , m_server(new QWebSocketServer(QStringLiteral("DsQt Q-Sys"), QWebSocketServer::NonSecureMode, this)) {
    connect(m_server, &QWebSocketServer::newConnection, this, &DsQsys::acceptConnection);
    // One binding applies enabled and port together, including live reloads.
    Settings::bind<QVariantMap>("engine", "engine.qsys", this,
                               [this](const QVariantMap& settings) { configure(settings); });
}

DsQsys::~DsQsys() { stop(); }

bool DsQsys::listening() const { return m_server->isListening(); }

void DsQsys::setServerName(const QString& name) {
    if (m_serverName == name) return;
    m_serverName = name;
    emit serverNameChanged();
}

void DsQsys::configure(const QVariantMap& settings) {
    const bool enabled = settings.value("enabled", false).toBool();
    bool valid = false;
    // Accept an integer or its decimal string, rejecting fractions and overflow.
    const int port = settings.value("port", 9988).toString().toInt(&valid);
    const int checkedPort = valid && port >= 1 && port <= 65535 ? port : 0;
    if (m_enabled == enabled && m_port == checkedPort) return;
    m_enabled = enabled;
    m_port = checkedPort;
    emit configurationChanged();
    restart();
}

void DsQsys::restart() {
    stop();
    if (!m_enabled) return;
    if (!m_port) {
        reportError(QStringLiteral("engine.qsys.port must be an integer from 1 to 65535"));
        return;
    }
    if (!m_server->listen(QHostAddress::Any, quint16(m_port))) {
        reportError(QStringLiteral("Cannot listen on port %1: %2").arg(m_port).arg(m_server->errorString()));
        return;
    }
    if (!m_lastError.isEmpty()) {
        m_lastError.clear();
        emit lastErrorChanged();
    }
    qCInfo(lgQsys) << "Listening on port" << m_port;
    emit listeningChanged();
}

void DsQsys::stop() {
    const bool wasListening = listening();
    m_server->close();
    if (m_client) {
        auto* socket = m_client.data();
        m_client.clear();
        disconnect(socket, nullptr, this, nullptr);
        socket->close();
        socket->deleteLater();
        clearSession();
        emit connectedChanged();
    } else {
        clearSession();
    }
    if (wasListening) emit listeningChanged();
}

void DsQsys::acceptConnection() {
    while (m_server->hasPendingConnections()) {
        auto* socket = m_server->nextPendingConnection();
        // A dictionary belongs to one plugin session. Never merge two devices.
        if (m_client || !m_enabled || !listening()) {
            connect(socket, &QWebSocket::disconnected, socket, &QObject::deleteLater);
            socket->close(QWebSocketProtocol::CloseCodePolicyViolated,
                          QStringLiteral("Only one Q-Sys plugin may connect"));
            continue;
        }
        clearSession();
        m_client = socket;
        connect(socket, &QWebSocket::textMessageReceived, this, &DsQsys::receive);
        connect(socket, &QWebSocket::disconnected, this, [this, socket] {
            if (m_client == socket) {
                m_client.clear();
                clearSession();
                emit connectedChanged();
            }
            socket->deleteLater();
        });
        qCInfo(lgQsys) << "Q-Sys connected from" << socket->peerAddress();
        emit connectedChanged();
    }
}

void DsQsys::setReady(bool ready) {
    if (m_ready == ready) return;
    m_ready = ready;
    emit readyChanged();
}

void DsQsys::clearSession() {
    m_pulses.clear();
    m_values.clear();
    m_controls.clear();
    setReady(false);
    emit valuesChanged();
    emit controlsChanged();
    emit cleared();
}

QString DsQsys::controlType(const QString& parameter) const {
    for (const auto& entry : m_controls) {
        const auto control = entry.toMap();
        if (control.value("parameter").toString() == parameter)
            return control.value("control_type").toString();
    }
    return {};
}

void DsQsys::updateValue(const QString& parameter, const QVariant& value) {
    m_values.insert(parameter, value);
    emit valuesChanged();
    emit updated(parameter, value);
}

void DsQsys::receive(const QString& text) {
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(text.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        reportError(QStringLiteral("Ignoring malformed Q-Sys JSON"));
        return;
    }
    const auto message = document.object();
    const QString parameter = message.value("parameter").toString();
    if (parameter.isEmpty()) {
        reportError(QStringLiteral("Q-Sys message has no parameter name"));
        return;
    }
    if (parameter == "qsys_manifest") {
        if (!message.value("controls").isArray()) {
            reportError(QStringLiteral("Q-Sys manifest has no controls array"));
            return;
        }
        QVariantList controls;
        QSet<QString> names;
        for (const auto& entry : message.value("controls").toArray()) {
            const auto control = entry.toObject();
            const QString name = control.value("parameter").toString();
            if (name.isEmpty() || name.startsWith("qsys_") || name == "error" || names.contains(name)
                || control.value("control_type").toString().isEmpty()) {
                reportError(QStringLiteral("Ignoring invalid Q-Sys manifest"));
                return;
            }
            names.insert(name);
            controls.append(control.toVariantMap());
        }
        clearSession();
        m_controls = controls;
        emit controlsChanged();
        emit manifestReceived();
        return;
    }
    if (parameter == "qsys_init_finished") {
        if (!m_ready) {
            setReady(true);
            emit snapshotReady();
        }
        return;
    }
    if (parameter == "qsys_control_renamed") {
        const QString from = message.value("from").toString();
        const QString to = message.value("to").toString();
        if (from.isEmpty() || to.isEmpty() || to.startsWith("qsys_") || to == "error") {
            reportError(QStringLiteral("Ignoring invalid Q-Sys rename"));
            return;
        }
        if (from == to) return;
        if (m_values.contains(to) || !controlType(to).isEmpty()) {
            reportError(QStringLiteral("Ignoring Q-Sys rename to an existing parameter: %1").arg(to));
            return;
        }
        if (m_values.contains(from)) m_values.insert(to, m_values.take(from));
        // Move a pending pulse reset too; its callback locates this token by value.
        if (m_pulses.contains(from)) m_pulses.insert(to, m_pulses.take(from));
        for (auto& entry : m_controls) {
            auto control = entry.toMap();
            if (control.value("parameter").toString() == from) {
                control.insert("parameter", to);
                entry = control;
            }
        }
        emit valuesChanged();
        emit controlsChanged();
        emit parameterRenamed(from, to);
        return;
    }
    if (parameter == "error") {
        reportError(QStringLiteral("Q-Sys rejected a command: %1").arg(message.value("message").toString()));
        return;
    }
    // Future protocol messages must not leak into the parameter dictionary.
    if (parameter.startsWith("qsys_")) return;
    if (!message.contains("value")) return;

    const auto value = message.value("value");
    m_pulses.remove(parameter);
    if (value.isBool() && value.toBool() && controlType(parameter) == "momentary") {
        const auto token = ++m_revision;
        m_pulses.insert(parameter, token);
        QTimer::singleShot(500, this, [this, token] {
            const QString currentName = m_pulses.key(token);
            if (currentName.isEmpty()) return;
            m_pulses.remove(currentName);
            updateValue(currentName, false);
        });
    }
    updateValue(parameter, value.toVariant());
}

bool DsQsys::transmit(const QString& parameter, QJsonObject fields) {
    if (parameter.trimmed().isEmpty() || parameter.startsWith("qsys_") || parameter == "error") {
        reportError(QStringLiteral("Invalid Q-Sys parameter name"));
        return false;
    }
    if (!m_client || m_client->state() != QAbstractSocket::ConnectedState) {
        reportError(QStringLiteral("Q-Sys is disconnected; dropping command for %1").arg(parameter));
        return false;
    }
    fields.insert("parameter", parameter);
    fields.insert("server_name", m_serverName);
    if (m_client->sendTextMessage(QString::fromUtf8(QJsonDocument(fields).toJson(QJsonDocument::Compact))) < 0) {
        reportError(QStringLiteral("Failed to send Q-Sys command for %1").arg(parameter));
        return false;
    }
    return true;
}

bool DsQsys::send(const QString& parameter, const QVariant& value) {
    auto wireValue = QJsonValue::fromVariant(value);
    const auto type = controlType(parameter);
    if (wireValue.isString() && (type == "toggle" || type == "momentary" || type == "indicator")) {
        const auto text = wireValue.toString().trimmed().toLower();
        if (text == "true" || text == "false") wireValue = (text == "true");
    }
    return transmit(parameter, {{"value", wireValue}});
}

bool DsQsys::sendList(const QString& parameter, const QStringList& choices) {
    return transmit(parameter, {{"choices", QJsonArray::fromStringList(choices)}});
}

bool DsQsys::selectListItem(const QString& parameter, const QString& item) {
    return transmit(parameter, {{"value", item}});
}

void DsQsys::reportError(const QString& message) {
    m_lastError = message;
    qCWarning(lgQsys) << message;
    emit lastErrorChanged();
    emit errorOccurred(message);
}

} // namespace dsqt
