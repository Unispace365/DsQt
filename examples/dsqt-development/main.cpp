#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include <dsqmlimportpath.h>
#include <dscontentmodel.h>

int main(int argc, char *argv[])
{
    qputenv("QT_IM_MODULE", QByteArray("qtvirtualkeyboard"));

    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
    dsqt::model::DSContentModel content;
    engine.addImportPath(DS_QML_IMPORT_PATH);
    engine.rootContext()->setContextProperty("ds_content",&content);
    const QUrl url(u"qrc:/dsqt-development/qml/main.qml"_qs);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
