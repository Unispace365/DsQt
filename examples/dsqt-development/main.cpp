#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include <dsqmlimportpath.h>
#include <dscontentmodel.h>
#include <settings/settings.h>
#include <settings/setting_proxy.h>
#include <dsenvironment.h>

int main(int argc, char *argv[])
{
    qputenv("QT_IM_MODULE", QByteArray("qtvirtualkeyboard"));

    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
    dsqt::model::DSContentModel content;
    dsqt::DSEnvironment::loadEngineSettings();
    dsqt::DSSettingsRef appSettings = dsqt::DSEnvironment::loadSettings("app_settings","app_settings.toml");
    dsqt::DSSettingProxy appProxy;
    appProxy.setTarget(u"app_settings"_qs);
    engine.addImportPath(DS_QML_IMPORT_PATH);
    engine.rootContext()->setContextProperty("ds_content",&content);
    engine.rootContext()->setContextProperty("app_settings",&appProxy);
    const QUrl url(u"qrc:/dsqt-development/qml/main.qml"_qs);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}

