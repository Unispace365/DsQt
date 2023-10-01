#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QShortcut>

#include <dsqmlimportpath.h>

#include <core/dsqmlapplicationengine.h>
#include <dsBridgeQuery.h>
#include <dsnodewatcher.h>
#include <QIcon>

int main(int argc, char *argv[])
{

    qputenv("QT_IM_MODULE", QByteArray("qtvirtualkeyboard"));
    qputenv("QT_ENABLE_HIGHDPI_SCALING", QByteArray("0"));

    QGuiApplication app(argc, argv);

    dsqt::DSQmlApplicationEngine engine;
    engine.addImportPath(DS_QML_IMPORT_PATH);
    
    auto query = new dsqt::DsBridgeSqlQuery(&engine);
    //this initalizes and reads in the settings files.
    engine.initialize();
    auto nw = dsqt::network::DsNodeWatcher(&engine);
    nw.start();

    const QUrl url("dsqt-development/qml/main.qml");
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        //check that we actually loaded our main.qml
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);

    engine.load(url);

    auto exitcode =  app.exec();
    delete query;
    return exitcode;
}

