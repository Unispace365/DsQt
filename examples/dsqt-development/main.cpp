#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QShortcut>

#include <dsqtimportpath.h>

#include <core/dsqmlapplicationengine.h>
#include <dsBridgeQuery.h>
#include <dsnodewatcher.h>
#include <QIcon>

int main(int argc, char *argv[])
{
    QLoggingCategory::setFilterRules("*.verbose=false"
                                     "");
    qputenv("QT_IM_MODULE", QByteArray("qtvirtualkeyboard"));
    qputenv("QT_ENABLE_HIGHDPI_SCALING", QByteArray("0"));

    QGuiApplication app(argc, argv);

    dsqt::DSQmlApplicationEngine engine;
    engine.addImportPath(DS_QML_IMPORT_PATH);

    //create the query object
    //the query object will create the database connection and
    //the root content model.
    //it will also try to start the bridge sync process
    //if it is configured to do so.
    auto query = new dsqt::bridge::DsBridgeSqlQuery(&engine);

    //this initalizes and reads in the settings files.
    engine.initialize();

    //set up the node watcher to listen for messages from BridgeSync
    auto nw = dsqt::network::DsNodeWatcher(&engine);
    //connect the messageArrived signal to the queryDatabase slot
    QObject::connect(&nw,
                     &dsqt::network::DsNodeWatcher::messageArrived,
                     query,
                     &dsqt::bridge::DsBridgeSqlQuery::QueryDatabase,
                     Qt::QueuedConnection);

    const QUrl url("../../qml/main.qml");
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

