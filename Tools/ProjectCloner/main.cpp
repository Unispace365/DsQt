#ifdef _WIN32
#ifdef DS_QT_DLLS_SUBDIR
// Put a manifest dependency to the qt/ directory so we can keep qt dlls in their own directory
#pragma comment(linker, "/manifestdependency:\"name='qt' version='1.0.0.0' type='win32'\"")
#endif
#endif

#include <QGuiApplication>
#include <QQmlApplicationEngine>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    app.setOrganizationName("Downstream");
    app.setOrganizationDomain("downstream.com");
    app.setApplicationName("Project Cloner");

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("ProjectCloner", "Main");

    QObject::connect(&engine, &QQmlApplicationEngine::quit, &QGuiApplication::quit);
    return app.exec();
}
