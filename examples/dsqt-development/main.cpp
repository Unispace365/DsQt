#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QShortcut>

#include <dsqmlimportpath.h>

#include <core/dsqmlapplicationengine.h>
#include <dssqlquery.h>



int main(int argc, char *argv[])
{

    qputenv("QT_IM_MODULE", QByteArray("qtvirtualkeyboard"));
    qputenv("QT_ENABLE_HIGHDPI_SCALING", QByteArray("0"));

    QGuiApplication app(argc, argv);

    dsqt::DSQmlApplicationEngine engine;
    engine.addImportPath(DS_QML_IMPORT_PATH);

     auto query = new dsqt::DsSqlQuery(&engine);
    //this initalizes and reads in the settings files.
    engine.initialize();


    const QUrl url("dsqt-development/qml/main.qml");
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        //check that we actually loaded our main.qml
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);

    engine.load(url);

    return app.exec();
}

