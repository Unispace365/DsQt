#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <dsqmlapplicationengine.h>
#include <dsBridgeQuery.h>
#include <dsnodewatcher.h>

int main(int argc, char *argv[])
{
    Q_INIT_RESOURCE(waffles);
    QLoggingCategory::setFilterRules("*.verbose=false"
                                     "");
    qputenv("QT_IM_MODULE", QByteArray("qtvirtualkeyboard"));
    qputenv("QT_ENABLE_HIGHDPI_SCALING", QByteArray("0"));


    QGuiApplication app(argc, argv);
    QDirIterator qrc(":", QDirIterator::Subdirectories);
    while(qrc.hasNext())
        qDebug() << qrc.next();
    dsqt::DSQmlApplicationEngine engine;
    //engine.addImportPath("qrc:/qt/qml/Dsqt");

    //create the query object
    //the query object will create the database connection and
    //the root content model.
    //it will also try to start the bridge sync process
    //if it is configured to do so.
    auto query = new dsqt::bridge::DsBridgeSqlQuery(&engine);

    //this initalizes and reads in the settings files.
    //engine.setBaseUrl(QUrl(".\\..\\..\\"));
    engine.initialize();

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    //engine.load(".\\..\\..\\Main.qml");
    engine.loadFromModule("DsqtApp", "Main");

    return app.exec();
}
