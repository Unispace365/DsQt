#include <QtTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QtQml/qqmlextensionplugin.h>
#include <QTcpServer>
#include <QWebSocket>
#include <memory>
#include "dsQsys.h"
#include "dsSettings.h"

Q_IMPORT_QML_PLUGIN(Dsqt_QsysPlugin)

using namespace dsqt;

class QsysTest : public QObject {
    Q_OBJECT
    DsQsys* qsys = nullptr;
    SettingsFile* settings = nullptr;
    std::unique_ptr<QWebSocket> plugin;

    void frame(const QJsonObject& message) {
        plugin->sendTextMessage(QString::fromUtf8(QJsonDocument(message).toJson(QJsonDocument::Compact)));
    }
    void manifest() {
        frame({{"parameter", "qsys_manifest"}, {"controls", QJsonArray{
            QJsonObject{{"parameter", "Entry Door"}, {"control_type", "indicator"}, {"owner", "qsys"}},
            QJsonObject{{"parameter", "Lighting Preset 2"}, {"control_type", "toggle"}, {"owner", "qsys"}},
            QJsonObject{{"parameter", "Pulse"}, {"control_type", "momentary"}, {"owner", "qsys"}},
            QJsonObject{{"parameter", "Stories"}, {"control_type", "listbox"}, {"owner", "fe"}}
        }}});
    }
    int freePort() {
        QTcpServer probe;
        if (!probe.listen(QHostAddress::LocalHost, 0)) return 0;
        return probe.serverPort();
    }

  private slots:
    void initTestCase() {
        // Import alone must start the service, without a Qsys property reference.
        // Also exercise construction before engine settings have loaded.
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData("import QtQml\nimport Dsqt.Qsys\nQtObject {}", QUrl());
        QTRY_VERIFY_WITH_TIMEOUT(component.status() != QQmlComponent::Loading, 5000);
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        std::unique_ptr<QObject> object(component.create());
        QVERIFY(object);
        Settings::instance().setSearchPaths({});
        Settings::add("engine");
        settings = Settings::instance().settingsFile("engine");
        QVERIFY(settings);
        const int port = freePort();
        settings->setOverride("engine.qsys", QVariantMap{{"enabled", true}, {"port", port}});
        QWebSocket probe;
        probe.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(port)));
        QTRY_COMPARE(probe.state(), QAbstractSocket::ConnectedState);
        qsys = DsQsys::instance();
        settings->resetOverrides();
        QVERIFY(!qsys->enabled());
        QCOMPARE(qsys->port(), 9988);
        QVERIFY(!qsys->listening());
    }
    void init() {
        const int port = freePort();
        QVERIFY(port > 0);
        settings->setOverride("engine.qsys", QVariantMap{{"enabled", true}, {"port", port}});
        QVERIFY(qsys->listening());
        plugin = std::make_unique<QWebSocket>();
        plugin->open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(port)));
        QTRY_VERIFY(qsys->connected());
        QTRY_COMPARE(plugin->state(), QAbstractSocket::ConnectedState);
        manifest();
        QTRY_COMPARE(qsys->controls().size(), 4);
    }
    void cleanup() {
        settings->setOverride("engine.qsys.enabled", false);
        QTRY_VERIFY(!qsys->connected());
        QVERIFY(!qsys->listening());
        QVERIFY(qsys->values().isEmpty());
        QVERIFY(qsys->controls().isEmpty());
        QVERIFY(!qsys->ready());
        plugin.reset();
        settings->resetOverrides();
    }
    void snapshotAndRepeatedUpdates() {
        QSignalSpy updates(qsys, &DsQsys::updated);
        QSignalSpy ready(qsys, &DsQsys::snapshotReady);
        QVERIFY(!qsys->ready());
        QVERIFY(!qsys->hasValue("Entry Door"));
        frame({{"parameter", "Entry Door"}, {"value", false}});
        frame({{"parameter", "qsys_init_finished"}});
        QTRY_COMPARE(ready.size(), 1);
        QCOMPARE(updates.size(), 1);
        QVERIFY(qsys->hasValue("Entry Door"));
        QCOMPARE(qsys->value("Entry Door").metaType().id(), QMetaType::Bool);
        QCOMPARE(qsys->value("Entry Door").toBool(), false);
        QVERIFY(qsys->ready());
        frame({{"parameter", "Entry Door"}, {"value", false}});
        QTRY_COMPARE(updates.size(), 2);
        QVERIFY(!qsys->hasValue("qsys_init_finished"));
    }
    void commandsAndFeedback() {
        QSignalSpy incoming(plugin.get(), &QWebSocket::textMessageReceived);
        QVERIFY(qsys->send("Lighting Preset 2", QStringLiteral("true")));
        QTRY_COMPARE(incoming.size(), 1);
        auto message = QJsonDocument::fromJson(incoming.takeFirst().at(0).toString().toUtf8()).object();
        QCOMPARE(message.value("value"), QJsonValue(true));
        QCOMPARE(message.value("parameter").toString(), QStringLiteral("Lighting Preset 2"));
        QVERIFY(message.contains("server_name"));
        QVERIFY(!qsys->hasValue("Lighting Preset 2"));
        frame({{"parameter", "Lighting Preset 2"}, {"value", true}});
        QTRY_VERIFY(qsys->value("Lighting Preset 2").toBool());
        QVERIFY(qsys->sendList("Stories", {"true", "Story B"}));
        QVERIFY(qsys->selectListItem("Stories", "true"));
        QVERIFY(qsys->sendList("Stories", {}));
        QTRY_COMPARE(incoming.size(), 3);
        message = QJsonDocument::fromJson(incoming[0][0].toString().toUtf8()).object();
        QVERIFY(!message.contains("value"));
        QCOMPARE(message.value("choices").toArray().size(), 2);
        message = QJsonDocument::fromJson(incoming[1][0].toString().toUtf8()).object();
        QCOMPARE(message.value("value"), QJsonValue("true"));
        QVERIFY(!message.contains("choices"));
        message = QJsonDocument::fromJson(incoming[2][0].toString().toUtf8()).object();
        QVERIFY(message.value("choices").toArray().isEmpty());
    }
    void renameAndPulseTimers() {
        frame({{"parameter", "Pulse"}, {"value", true}});
        QTRY_VERIFY(qsys->value("Pulse").toBool());
        QTest::qWait(300);
        frame({{"parameter", "Pulse"}, {"value", true}});
        frame({{"parameter", "qsys_control_renamed"}, {"from", "Pulse"}, {"to", "Trigger"}});
        QTRY_VERIFY(qsys->hasValue("Trigger"));
        QVERIFY(!qsys->hasValue("Pulse"));
        QCOMPARE(qsys->controls()[2].toMap().value("parameter").toString(), QStringLiteral("Trigger"));
        QTest::qWait(250); // The first pulse's timer must not reset the second pulse.
        QVERIFY(qsys->value("Trigger").toBool());
        QTRY_VERIFY(!qsys->value("Trigger").toBool());
        frame({{"parameter", "Trigger"}, {"value", true}});
        QTRY_VERIFY(qsys->value("Trigger").toBool());
        plugin->close();
        QTRY_VERIFY(!qsys->connected());
        QTest::qWait(550);
        QVERIFY(qsys->values().isEmpty());
    }
    void malformedErrorsAndReconnect() {
        QSignalSpy errors(qsys, &DsQsys::errorOccurred);
        plugin->sendTextMessage("not json");
        frame({{"parameter", "qsys_manifest"}, {"controls", QJsonArray{42}}});
        frame({{"parameter", "error"}, {"message", "read only"}});
        frame({{"parameter", "qsys_future"}, {"value", true}});
        frame({{"parameter", "Stories"}, {"choices", QJsonArray{"A"}}});
        QTRY_COMPARE(errors.size(), 3);
        QCOMPARE(qsys->controls().size(), 4);
        QVERIFY(qsys->values().isEmpty());
        frame({{"parameter", "Entry Door"}, {"value", true}});
        QTRY_VERIFY(qsys->hasValue("Entry Door"));
        plugin->close();
        QTRY_VERIFY(!qsys->connected());
        QVERIFY(!qsys->send("Lighting Preset 2", true));
        plugin->open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(qsys->port())));
        QTRY_VERIFY(qsys->connected());
        QTRY_COMPARE(plugin->state(), QAbstractSocket::ConnectedState);
        QVERIFY(qsys->values().isEmpty());
        QVERIFY(!qsys->ready());
        manifest();
        QTRY_COMPARE(qsys->controls().size(), 4);
    }
    void settingsAndSecondClient() {
        QWebSocket extra;
        QSignalSpy disconnected(&extra, &QWebSocket::disconnected);
        extra.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(qsys->port())));
        QTRY_COMPARE(disconnected.size(), 1);
        QVERIFY(qsys->connected());
        QCOMPARE(qsys->controls().size(), 4);
        const int port = freePort();
        settings->setOverride("engine.qsys.port", QString::number(port));
        QCOMPARE(qsys->port(), port);
        QVERIFY(qsys->listening());
        QVERIFY(!qsys->connected());
        settings->setOverride("engine.qsys.port", 65536);
        QVERIFY(!qsys->listening());
        QVERIFY(!qsys->lastError().isEmpty());
        QTcpServer occupied;
        QVERIFY(occupied.listen(QHostAddress::Any, 0));
        settings->setOverride("engine.qsys.port", occupied.serverPort());
        QVERIFY(!qsys->listening());
        occupied.close();
        qsys->restart();
        QVERIFY(qsys->listening());
    }
    void qmlSingletonAndConnections() {
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQml
            import Dsqt.Qsys
            QtObject {
                property var endpoint: Qsys
                property string receivedParameter
                property var receivedValue
                property Connections updates: Connections {
                    target: Qsys
                    function onUpdated(parameter, value) {
                        receivedParameter = parameter
                        receivedValue = value
                    }
                }
                function light() { return Qsys.send("Lighting Preset 2", "true") }
            }
        )", QUrl());
        QTRY_VERIFY_WITH_TIMEOUT(component.status() != QQmlComponent::Loading, 5000);
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        QCOMPARE(object->property("endpoint").value<QObject*>(), qsys);
        frame({{"parameter", "Entry Door"}, {"value", false}});
        QTRY_COMPARE(object->property("receivedParameter").toString(), QStringLiteral("Entry Door"));
        QCOMPARE(object->property("receivedValue").toBool(), false);
        QSignalSpy incoming(plugin.get(), &QWebSocket::textMessageReceived);
        QVERIFY(QMetaObject::invokeMethod(object.get(), "light"));
        QTRY_COMPARE(incoming.size(), 1);
        QCOMPARE(QJsonDocument::fromJson(incoming[0][0].toString().toUtf8()).object().value("value"), QJsonValue(true));
    }
};

QTEST_GUILESS_MAIN(QsysTest)
#include "tst_dsqsys.moc"
