#include "core/dsQmlApplicationEngine.h"
#include "core/dsEnvironment.h"
#include "core/dsQmlEnvironment.h"
#include "model/dsContentModel.h"
#include "model/dsQmlContentHelper.h"
#include "network/dsNodeWatcher.h"
#include "settings/dsQmlSettingsProxy.h"

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

    mQmlRefMap = new model::ReferenceMap();

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

            readSettings(true);
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

DsSettingsRef DsQmlApplicationEngine::getAppSettings() {
    return mSettings;
}

model::ContentModelRef DsQmlApplicationEngine::getContentRoot() {
    if (mContentRoot.empty()) {
        mContentRoot = model::ContentModelRef("root");
        mContentRoot.setId("root");
    }
    return mContentRoot;
}

model::IContentHelper* DsQmlApplicationEngine::getContentHelper() {
    if (mContentHelper == nullptr) {
        setContentHelper(new model::DsQmlContentHelper(this));
    }
    return mContentHelper;
}

void DsQmlApplicationEngine::setContentHelper(model::IContentHelper* helper) {
    mContentHelper = helper;
    mContentHelper->setEngine(this);
}

network::DsNodeWatcher* DsQmlApplicationEngine::getNodeWatcher() const {
    return mNodeWatcher;
}

model::ReferenceMap* DsQmlApplicationEngine::getReferenceMap() const {
    return mQmlRefMap;
}

void DsQmlApplicationEngine::preInit() {
    dsqt::DsEnvironment::loadEngineSettings();
}

void dsqt::DsQmlApplicationEngine::readSettings(bool reset) {
    qCInfo(lgAppEngine) << "\nLoad Settings >>>>>>>>>>>>>>>>>>>>>>>>";
    mContentRoot = getContentRoot();
    qCInfo(lgAppEngine) << "loading main engine.toml";
    if (reset) {
        DsSettings::forgetSettings("engine");
    }
    dsqt::DsEnvironment::loadEngineSettings();
    auto extra_engine_settings = DsEnvironment::engineSettings()->getRawNode("engine.extra.engine", true);
    if (extra_engine_settings) {
        auto& extra_paths = *extra_engine_settings->as_array();
        for (auto&& path_node : extra_paths) {
            auto path = path_node.as_string()->value_or<std::string>("");
            qCInfo(lgAppEngine) << "Loading engine file " << path;
            dsqt::DsEnvironment::loadSettings("engine", QString::fromStdString(path));
        }
    }

    auto engSettings = dsqt::DsEnvironment::engineSettings();
    std::function<QString(QString)> normPath = [this](QString path) {
        auto ret = path;
        path.replace( '\\', '/');

        ret = QDir::fromNativeSeparators(ret);

        return ret;
    };
    QString resourceLocation = engSettings->getOr<QString>("engine.resource.location","");
    if (resourceLocation.isEmpty()) {
    } else {
        if (resourceLocation.contains("%USERPROFILE%")) {
#ifndef _WIN32
            resourceLocation.replace("%USERPROFILE%", QDir::homePath());
            qCInfo(lgAppEngine)<<"Non-windows workaround: Converting \"%USERPROFILE%\" to \"~\" in resources_location...";
#endif
        }
        resourceLocation = DsEnvironment::expandq(resourceLocation); // allow use of %APP%, etc
        resourceLocation = QUrl::fromLocalFile(resourceLocation).toString();

        DsResource::Id::setupPaths(resourceLocation,
                                   normPath(engSettings->getOr<QString>("engine.resource.resource_db","")),
                                   normPath(engSettings->getOr<QString>("engine.project_path","")));
    }


    qCInfo(lgAppEngine) << "loading main app_settings.toml";
    if (reset) {
        DsSettings::forgetSettings("app_settings");
    }
    mSettings               = dsqt::DsEnvironment::loadSettings("app_settings", "app_settings.toml");
    auto extra_app_settings = DsEnvironment::engineSettings()->getRawNode("engine.extra.app_settings", true);
    if (extra_app_settings) {
        auto& extra_paths = *extra_app_settings->as_array();
        for (auto&& path_node : extra_paths) {
            auto path = path_node.as_string()->value_or<std::string>("");
            qCInfo(lgAppEngine) << "Loading app_settings file " << path;
            dsqt::DsEnvironment::loadSettings("app_settings", QString::fromStdString(path));
        }
    }
    qCInfo(lgAppEngine) << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n";
}

void DsQmlApplicationEngine::init() {
    // setup nodeWatcher
    readSettings();
    if (!mAppProxy) mAppProxy = new DsQmlSettingsProxy(this);
    // auto starts, but could also be this:
    // mNodeWatcher = new network::DsNodeWatcher(this,"localhost",7788,/*autostart*/false)
    // mNodeWatcher->start();

    readSettings();
    mAppProxy = new DsQmlSettingsProxy(this);
    mQmlEnv   = new DsQmlEnvironment(this);
    // get watcher elements
    auto node = dsqt::DsEnvironment::engineSettings()->getRawNode("engine.reload.paths");
    if (node) {
        const auto& paths = *node->as_array();

        for (auto&& path_node : paths) {
            auto path    = path_node.as_table();
            auto oval    = QString::fromStdString((*path)["path"].as_string()->value_or<std::string>(""));
            auto val     = DsEnvironment::expandq(oval);
            auto recurse = (*path)["recurse"].as_boolean()->value_or(false);
            if (QFileInfo::exists(val)) {
                mWatcher->addPath(val);
                qInfo() << "Added " << val << " to watcher";
                auto fi = QFileInfo(val);
                if (fi.isDir()) {
                    addRecursive(val, recurse);
                }
            }
        }
    }
    // get idle timeout
    auto timeoutInSeconds = DsEnvironment::engineSettings()->getOr<int>("engine.idle_timeout", 300);
    mIdle->setIdleTimeout(timeoutInSeconds * 1000);
    mIdle->startIdling(true);

    mAppProxy->setTarget("app_settings");
    updateContentRoot(nullptr);
    connect(this, &DsQmlApplicationEngine::fileChanged, this, &DsQmlApplicationEngine::doReset);
    // rootContext()->setContextProperty("app_settings",mAppProxy);
    // rootContext()->setContextProperty("$QmlEngine", this);
    // rootContext()->setContextProperty("$Env",mQmlEnv);


}

void DsQmlApplicationEngine::postInit() {
}

void DsQmlApplicationEngine::resetIdle() {
    mIdle->clearAllIdlePreventers();
    auto timeoutInSeconds = DsEnvironment::engineSettings()->getOr<int>("engine.idle_timeout", 300);
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

void DsQmlApplicationEngine::updateContentRoot(QSharedPointer<model::PropertyMapDiff> diff) {
    // updateContentRoot is responsible for cleaning up the diff.

    qInfo("Updating Content Root");
    if (mContentRoot.empty()) {
        mContentRoot = model::ContentModelRef("root");
        mContentRoot.setId("root");
    }

    if (mRootMap == nullptr) {
        mRootMap = mContentRoot.getQml(mQmlRefMap, this);
    } else if (diff) {
        diff->dumpChanges();
        diff->apply(*mRootMap, mQmlRefMap);
    }

    // mRootModel		  = mContentRoot.getModel(this);
    // mRootMap		  = mContentRoot.getQml(this);
    // rootContext()->setContextProperty("contentRootModel", mRootModel);
    // rootContext()->setContextProperty("contentRootMap", mRootMap);

    emit rootUpdated();
}

void DsQmlApplicationEngine::clearQmlCache() {
    this->clearComponentCache();
}

DsQmlEnvironment* DsQmlApplicationEngine::getEnvQml() const {
    return mQmlEnv;
}

DsQmlSettingsProxy* DsQmlApplicationEngine::getAppSettingsProxy() const {
    return mAppProxy;
}

DsQmlIdle* DsQmlApplicationEngine::idle() const {
    return mIdle;
}

} // namespace dsqt
