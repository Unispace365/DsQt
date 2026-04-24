#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QIcon>
#include <QtQml/qqmlextensionplugin.h>

#include <dsEnvironment.h>
#include <dsGuiApplication.h>
#include <dsQmlApplicationEngine.h>

#include <QQuickWindow>

// Import statically linked Dsqt QML plugins
#ifdef DSQT_HAS_Core
Q_IMPORT_QML_PLUGIN(Dsqt_CorePlugin)
#endif
#ifdef DSQT_HAS_Touch
Q_IMPORT_QML_PLUGIN(Dsqt_TouchPlugin)
#endif

// Activate high performance graphics on Windows laptops with dual graphics cards
#ifdef Q_OS_WIN
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement             = 0x00000001;
__declspec(dllexport) int           AmdPowerXpressRequestHighPerformance = 1;
}
#endif

int main(int argc, char *argv[])
{
    DsGuiApplication::configureGraphics({
        .graphicsApi = QSGRendererInterface::Direct3D12,
        .colorDepth  = 10,
    });

    QLoggingCategory::setFilterRules("*.verbose=false\n"
                                     "reloadUrl.*=false\n"
                                     "settings.parser=true\n"
                                     );
    qputenv("QT_IM_MODULE",              QByteArray("qtvirtualkeyboard"));
    qputenv("QT_ENABLE_HIGHDPI_SCALING", QByteArray("0"));

    DsGuiApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/newds.ico"));
    QQuickStyle::setStyle("Fusion");

    dsqt::DsQmlApplicationEngine engine;
    engine.addImportPath("qrc:/keyboard");

    engine.initialize();

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    auto mainQml = engine.getAppSettings()->getOr<QString>("app.mainView", "Main");
    engine.loadFromModule("TouchFilter", mainQml);

    return app.exec();
}
