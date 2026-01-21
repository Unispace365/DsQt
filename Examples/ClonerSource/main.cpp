#ifdef _WIN32
#ifdef DS_QT_DLLS_SUBDIR
// Put a manifest dependency to the qt/ directory so we can keep qt dlls in their own directory
#pragma comment(linker, "/manifestdependency:\"name='qt' version='1.0.0.0' type='win32'\"")
#endif
#endif

#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QtWebEngineQuick>
#include <dsBridgeQuery.h>
#include <dsEnvironment.h>
#include <dsGuiApplication.h>
#include <dsNodeWatcher.h>
#include <dsQmlApplicationEngine.h>
#include <dsReloadUrlInterceptor.h>

int main(int argc, char *argv[])
{
    //ensure the data.qrc is initialized
    Q_INIT_RESOURCE(data);

    //(@NOTE:KEYBOARD)
    //ensure the keyboard.qrc is initialized
    Q_INIT_RESOURCE(keyboard);
    QLoggingCategory::setFilterRules("*.verbose=false\n"
                                     "reloadUrl.*=false\n"
                                     "bridgeSync.app=false\n"
                                     "bridgeSync.query=true\n"
                                     "settings.parser=false\n"
                                     "idle=false\n"
                                     "qt.multimedia.ffmpeg.*=false\n"
                                     );
    qputenv("QT_IM_MODULE", QByteArray("qtvirtualkeyboard"));
    qputenv("QT_ENABLE_HIGHDPI_SCALING", QByteArray("0"));

    QtWebEngineQuick::initialize();
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
    engine.loadFromModule("ClonerSource", mainQml);

    auto retval = app.exec();

    //for(const dsqt::DsReloadUrlInterceptor* interceptor:interceptors ){
    //    delete interceptor;
    //}
    return retval;
}
