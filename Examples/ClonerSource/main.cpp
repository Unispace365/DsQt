#ifdef _WIN32
#ifdef DS_QT_DLLS_SUBDIR
// Put a manifest dependency to the qt/ directory so we can keep qt dlls in their own directory
#pragma comment(linker, "/manifestdependency:\"name='qt' version='1.0.0.0' type='win32'\"")
#endif
#endif

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <dsqmlapplicationengine.h>
#include <dsBridgeQuery.h>
#include <dsnodewatcher.h>
#include <reloadurlinterceptor.h>

int main(int argc, char *argv[])
{
    Q_INIT_RESOURCE(waffles);
    QLoggingCategory::setFilterRules("*.verbose=false"
                                     "");
    qputenv("QT_IM_MODULE", QByteArray("qtvirtualkeyboard"));
    qputenv("QT_ENABLE_HIGHDPI_SCALING", QByteArray("0"));

    QGuiApplication app(argc, argv);

    //set the DS icon for the app
    app.setWindowIcon(QIcon(":/newds.ico"));

    //for debugging imports
    //QDirIterator qrc(":", QDirIterator::Subdirectories);
    //while(qrc.hasNext())
    //    qDebug() << qrc.next();

    //create our custom engine.
    dsqt::DSQmlApplicationEngine engine;

    //setup url interceptor so we can reload QML files on change.
    dsqt::ReloadUrlInterceptor* interceptor = new dsqt::ReloadUrlInterceptor();
    QString projectFrom = "qrc:/qt/qml/WhiteLabelWaffles/";
    QString projectTo = "file:///"+QCoreApplication::applicationDirPath()+"/../../";
    interceptor->setPrefixes(projectFrom,projectTo);
    engine.addUrlInterceptor(interceptor);


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
    engine.loadFromModule("ClonerSource", "Main");

    auto retval = app.exec();
    delete query;
    return retval;
}
