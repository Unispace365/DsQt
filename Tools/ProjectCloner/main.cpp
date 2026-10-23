
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    app.setOrganizationName("Downstream");
    app.setOrganizationDomain("downstream.com");
    app.setApplicationName("Project Cloner");
    app.setWindowIcon(QIcon(QStringLiteral(":/project-cloner/images/project-cloner.png")));

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("ProjectCloner", "Main");

    QObject::connect(&engine, &QQmlApplicationEngine::quit, &QGuiApplication::quit);
    qInfo()<<"Started Cloaner Tool";
    return app.exec();
}
