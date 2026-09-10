#include <QFile>
#include <QGuiApplication>
#include <QQmlApplicationEngine>

#include <Dsqt/TouchEngine/DsTouchEngineSession.h>

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);

    dsqt::touchengine::DsTouchEngineSession cppSession;
    if (cppSession.state() != dsqt::touchengine::DsTouchEngineTypes::State::Idle)
        return 4;

    QQmlApplicationEngine engine;
    engine.loadFromModule("Dsqt.PackageSmoke", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;

    const QObject *root = engine.rootObjects().constFirst();
    if (!root->findChild<QObject *>("touchEngineSession")
        || !root->findChild<QObject *>("touchEngineView")) {
        return 2;
    }

    if (!QFile::exists(":/dsqt/touchengine/shaders/texture.vert.qsb")
        || !QFile::exists(":/dsqt/touchengine/shaders/texture.frag.qsb")) {
        return 3;
    }

    return 0;
}
