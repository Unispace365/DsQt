#include <QQmlApplicationEngine>
#include <QQuickStyle>
// #include <QtWebEngineQuick/QtWebEngineQuick>  // Uncomment if QtWebEngineQuick::initialize() is needed
#include <QIcon>
#include <QtQml/qqmlextensionplugin.h>
#include <dsBridgeQuery.h>

#include <dsEnvironment.h>
#include <dsGuiApplication.h>
#include <dsNodeWatcher.h>
#include <dsQmlApplicationEngine.h>
#include <dsReloadUrlInterceptor.h>

#include <QQuickWindow>
#include <QSGRendererInterface>
#ifdef DSQT_HAS_TouchEngine
#include "dsQmlTouchEngineManager.h"
#include "dsQmlTouchEngineInstance.h"
#include "dsQmlTouchEngineTextureOutputView.h"
#endif

// Import statically linked Dsqt QML plugins
#ifdef DSQT_HAS_Core
Q_IMPORT_QML_PLUGIN(Dsqt_CorePlugin)
#endif
#ifdef DSQT_HAS_Bridge
Q_IMPORT_QML_PLUGIN(Dsqt_BridgePlugin)
#endif
#ifdef DSQT_HAS_TouchEngine
Q_IMPORT_QML_PLUGIN(Dsqt_TouchEnginePlugin)
#endif
#ifdef DSQT_HAS_Waffles
Q_IMPORT_QML_PLUGIN(Dsqt_WafflesPlugin)
#endif
#ifdef DSQT_HAS_Spout
Q_IMPORT_QML_PLUGIN(Dsqt_SpoutPlugin)
#endif

//activate high performance graphics on windows laptops with dual graphics cards
#ifdef Q_OS_WIN
extern "C" {
// Enable dedicated graphics for NVIDIA
__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;

// Enable dedicated graphics for AMD Radeon
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

int main(int argc, char *argv[])
{
    // Set Vulkan as the preferred graphics API
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D12);


    // Note: Resources from Dsqt::Core (data.qrc, keyboard.qrc) are auto-initialized
    // when the QML module is loaded. Explicit Q_INIT_RESOURCE is not needed.
    QLoggingCategory::setFilterRules("*.verbose=false\n"
                                     "reloadUrl.*=false\n"
                                     "bridgeSync.app=false\n"
                                     "bridgeSync.query=true\n"
                                     "settings.parser=true\n"
                                     "idle=false\n"
                                     "qt.multimedia.ffmpeg.*=false\n"
                                     );
    qputenv("QT_IM_MODULE", QByteArray("qtvirtualkeyboard"));
    qputenv("QT_ENABLE_HIGHDPI_SCALING", QByteArray("0"));

    //QtWebEngineQuick::initialize();

    DsGuiApplication app(argc, argv);

    //set the DS icon for the app
    app.setWindowIcon(QIcon(":/newds.ico"));

    // Support for Windows Dark theme.
    QQuickStyle::setStyle("Fusion");


    //for debugging imports

    //QDirIterator qrc(":", QDirIterator::Subdirectories);
    //while(qrc.hasNext())
    //    qDebug() << qrc.next();

    //create our custom engine.
    dsqt::DsQmlApplicationEngine engine;

    //(@NOTE:KEYBOARD)
    //add the import path for the keyboard(s).
    // this is the prefix of the keyboard.qrc file.
    // the rest of the path for a style needs to
    // be QtQuick/VirtualKeyboard/Styles/<style name>/
    engine.addImportPath("qrc:/keyboard");

    //for debugging imports
    //auto list = engine.importPathList();
    //for(auto path:list){
    //    qDebug()<<"Import path: "<<path;
    //};




    //create the query object
    //the query object will create the database connection and
    //the root content model.
    //it will also try to start the bridge sync process
    //if it is configured to do so.
    dsqt::bridge::DsBridgeSqlQuery query(&engine);

    //this initalizes and reads in the settings files.

    engine.initialize();

    //load waffles settings
    dsqt::DsEnvironment::loadSettings("waffles","waffle_settings.toml","qrc:/waffles/settings");

    /*
    auto prefixes = dsqt::DsEnvironment::engineSettings()->getOr<QVariantList>("engine.reload.prefixes",QVariantList()).toList();
    QList<dsqt::DsReloadUrlInterceptor*> interceptors;
    for(const QVariant& prefix:std::as_const(prefixes)){
        auto prefixMap = prefix.toMap();
        auto id = prefixMap["name"].toString();
        auto fromPrefix = dsqt::DsEnvironment::expandq(prefixMap["from"].toString());
        auto toPrefix = dsqt::DsEnvironment::expandq(prefixMap["to"].toString());
        dsqt::DsReloadUrlInterceptor* interceptor = new dsqt::DsReloadUrlInterceptor();
        interceptor->setPrefixes(fromPrefix,toPrefix);
        engine.addUrlInterceptor(interceptor);
        interceptors.push_back(interceptor);
    }*/

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    auto mainQml = engine.getAppSettings()->getOr<QString>("app.mainView","Main");

    QObject::connect(&engine,&QQmlApplicationEngine::objectCreated,&app,[mainQml](QObject *obj, const QUrl &objUrl){
        auto checkFile = objUrl.fileName().replace(".qml","");
        if(checkFile != mainQml){
            qDebug()<<checkFile<<" != "<<mainQml;
            return;
        }

        // Initialize TouchEngine graphics after window is created
#ifdef DSQT_HAS_TouchEngine
        if (auto* window = qobject_cast<QQuickWindow*>(obj)) {
            DsQmlTouchEngineManager::initializeFromWindow(window);
        }
#endif
    },Qt::DirectConnection);


    engine.loadFromModule("ClonerSource", mainQml);

    auto retval = app.exec();

    //for(const dsqt::DsReloadUrlInterceptor* interceptor:interceptors ){
    //    delete interceptor;
    //}
    return retval;
}
