#include "core/dsQmlApplicationEngine.h"
#include "core/dsEnvironment.h"
#include "core/dsQmlEnvironment.h"
// #include "model/dsContentModel.h"
// #include "model/dsQmlContentHelper.h"
#include "core/dsFontManager.h"
#include "core/dsQmlAppHost.h"
#include "network/dsNodeWatcher.h"
#include "settings/dsSettings.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImageReader>
#include <QQmlContext>
#include <QStringLiteral>

#include <toml++/toml.h>

Q_LOGGING_CATEGORY(lgAppEngine, "engine")
Q_LOGGING_CATEGORY(lgAppEngineVerbose, "engine.verbose")
namespace dsqt {
using namespace Qt::Literals::StringLiterals;

DsQmlApplicationEngine* DsQmlApplicationEngine::sDefaultEngine = nullptr;

DsQmlApplicationEngine::DsQmlApplicationEngine(QObject* parent)
    : QQmlApplicationEngine{parent}
    , mWatcher{new QFileSystemWatcher(this)} {

    if (sDefaultEngine == nullptr) {
        setDefaultEngine(this);
    }

    // default idle
    mIdle = new DsQmlIdle(this);
    mIdle->setAreaItemTarget(qGuiApp);

    qCInfo(lgAppEngine) << "connecting file watcher";
    connect(
        mWatcher, &QFileSystemWatcher::directoryChanged, this,
        [this](const QString& path) {
            qInfo() << "Directory changed " << path;

            // addRecursive(path);
            mTrigger.setSingleShot(true);
            mTrigger.stop();
            mTrigger.start(1000);
        },
        Qt::QueuedConnection);

    connect(
        &mTrigger, &QTimer::timeout, this,
        [this]() {
            qCInfo(lgAppEngineVerbose) << "Triggered";

            // readSettings(true);
            emit fileChanged("triggered");
        },
        Qt::QueuedConnection);

    // connect(
    // 	mWatcher, &QFileSystemWatcher::fileChanged, this,
    // 	[](const QString& path) {
    // 		qInfo() << "Directory changed " << path;
    // 	},
    // 	Qt::QueuedConnection);
}

void DsQmlApplicationEngine::initialize() {
    preInit();
    emit beginInitialize();
    init();
    emit endInitialize();
    postInit();
}

void DsQmlApplicationEngine::doReset() {
    preReset();
    emit beginReset();
    readSettings(true);
    resetIdle();
    resetSystem();
    emit endReset();
    postReset();
}

SettingsFile* DsQmlApplicationEngine::getAppSettings() const {
    return dsqt::DsEnvironment::appSettings();
}

SettingsFile* DsQmlApplicationEngine::getEngineSettings() {
    return dsqt::DsEnvironment::engineSettings();
}

network::DsNodeWatcher* DsQmlApplicationEngine::getNodeWatcher() const {
    return mNodeWatcher;
}

void DsQmlApplicationEngine::preInit() {
    // dsqt::DsEnvironment::loadEngineSettings();
}

void dsqt::DsQmlApplicationEngine::readSettings(bool reset) {
    // This is now handled in DsEnvironment.
    DsEnvironment::loadEngineSettings();

    //
    if (reset) {
        qCInfo(lgAppEngine) << "Resetting app_settings";
        auto settings = Settings::instance().settingsFile("app_settings");
        if (settings) settings->resetOverrides();
    }
}

void DsQmlApplicationEngine::init() {
    // setup nodeWatcher
    readSettings();
    // if (!mEngineProxy) mEngineProxy = new DsQmlSettingsProxy(this);
    // if (!mAppProxy) mAppProxy = new DsQmlSettingsProxy(this);
    if (!mQmlEnv) mQmlEnv = new DsQmlEnvironment(this);

    // auto starts, but could also be this:
    // mNodeWatcher = new network::DsNodeWatcher(this,"localhost",7788,/*autostart*/false)
    // mNodeWatcher->start();

    // get watcher elements
    const auto paths = Settings::find<QVariantList>("engine", "engine.reload.paths");
    for (auto& path_node : paths) {
        const auto path = path_node.toMap();
        const auto oval = path.value("path").toString();
        const auto fi   = QFileInfo(DsEnvironment::expandq(oval));
        if (fi.exists()) {
            mWatcher->addPath(fi.filePath());
            qInfo() << "Added " << fi.filePath() << " to watcher";
            if (fi.isDir()) {
                const auto recurse = path.value("recurse", false).toBool();
                addRecursive(fi.filePath(), recurse);
            }
        }
    }

    // get idle timeout
    auto timeoutInSeconds = Settings::find<int>("engine", "engine.idle_timeout", 300);
    mIdle->setIdleTimeout(timeoutInSeconds * 1000);
    mIdle->startIdling(true);

    // mEngineProxy->setTarget("engine");
    // mAppProxy->setTarget("app_settings");

    // lets load some fonts
    DsFontManager fontManager(Settings::instance().settingsFile("engine"));
    fontManager.loadFonts();

    QFont defaultFont = fontManager.createDefaultFont();
    DsFontManager::setApplicationDefaultFont(defaultFont);
    // qGuiApp->setFont(defaultFont);

    connect(this, &DsQmlApplicationEngine::fileChanged, this, &DsQmlApplicationEngine::doReset);
    // rootContext()->setContextProperty("app_settings",mAppProxy);
    // rootContext()->setContextProperty("$QmlEngine", this);
    // rootContext()->setContextProperty("$Env",mQmlEnv);
}

void DsQmlApplicationEngine::postInit() {
}

void DsQmlApplicationEngine::resetIdle() {
    mIdle->clearAllIdlePreventers();
    auto timeoutInSeconds = Settings::find<int>("engine", "engine.idle_timeout", 300);
    mIdle->setIdleTimeout(timeoutInSeconds * 1000);
    mIdle->startIdling(true);
}

void DsQmlApplicationEngine::preReset() {
}

void DsQmlApplicationEngine::resetSystem() {
}

void DsQmlApplicationEngine::postReset() {
}

void DsQmlApplicationEngine::addRecursive(const QString& path, bool recurse) {
    auto         flags = recurse ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags;
    QDirIterator it(path, QDir::Dirs | QDir::NoDotAndDotDot, flags);
    while (it.hasNext()) {
        auto fi = QFileInfo(it.next());

        qInfo() << "Added " << fi.filePath() << " to watcherr";
        mWatcher->addPath(fi.filePath());
    }
}

void DsQmlApplicationEngine::setDefaultEngine(DsQmlApplicationEngine* engine) {
    sDefaultEngine = engine;
}

DsQmlApplicationEngine* DsQmlApplicationEngine::DefEngine() {
    return sDefaultEngine;
}

void DsQmlApplicationEngine::clearQmlCache() {
    this->clearComponentCache();
}

DsQmlEnvironment* DsQmlApplicationEngine::getEnvQml() const {
    return mQmlEnv;
}

// DsQmlSettingsProxy* DsQmlApplicationEngine::getEngineSettingsProxy() const {
//     return mEngineProxy;
// }

// DsQmlSettingsProxy* DsQmlApplicationEngine::getAppSettingsProxy() const {
//     return mAppProxy;
// }

DsQmlIdle* DsQmlApplicationEngine::idle() const {
    return mIdle;
}

void DsQmlApplicationEngine::quit(bool force) {
    // if engine.askAppHostToQuit is set send message to DsAppHost
    auto exitOnQuit = Settings::find<bool>("engine", "appHost.exitOnQuit", false);

    QScopedPointer<DsQmlAppHost> appHost(new DsQmlAppHost(this));
    if (exitOnQuit || force) {
        appHost->exit(true);
    } else {
        qApp->quit();
    }
}

} // namespace dsqt
