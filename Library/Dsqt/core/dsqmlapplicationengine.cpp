#include "dsqmlapplicationengine.h"
#include "dsenvironment.h"
#include "dsenvironmentqml.h"
#include "model/content_helper.h"
#include "model/content_model.h"
#include "network/dsnodewatcher.h"
#include "settings/dssettings_proxy.h"
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QImageReader>
#include <QQmlContext>
#include <QStringLiteral>
#include <toml++/toml.h>

Q_LOGGING_CATEGORY(lgAppEngine, "engine")
Q_LOGGING_CATEGORY(lgAppEngineVerbose, "engine.verbose")
namespace dsqt {
using namespace Qt::Literals::StringLiterals;

DSQmlApplicationEngine* DSQmlApplicationEngine::sDefaultEngine = nullptr;

DSQmlApplicationEngine::DSQmlApplicationEngine(QObject* parent)
    : QQmlApplicationEngine{parent}
    , mWatcher{new QFileSystemWatcher(this)} {

    if (sDefaultEngine == nullptr) {
        setDefaultEngine(this);
    }

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

    connect(
        mWatcher, &QFileSystemWatcher::fileChanged, this,
        [](const QString& path) { qInfo() << "Directory changed " << path; }, Qt::QueuedConnection);
}

void DSQmlApplicationEngine::initialize() {
    preInit();
    emit onPreInit();
    init();
    emit onInit();
    postInit();
    emit onPostInit();
}

DSSettingsRef DSQmlApplicationEngine::getAppSettings() {
    return mSettings;
}

model::ContentModelRef DSQmlApplicationEngine::getContentRoot() {
    if (mContentRoot.empty()) {
        mContentRoot = model::ContentModelRef("root");
        mContentRoot.setId("root");
    }
    return mContentRoot;
}

model::IContentHelper* DSQmlApplicationEngine::getContentHelper() {
    if (mContentHelper == nullptr) {
        setContentHelper(new model::ContentHelper(this));
    }
    return mContentHelper;
}

void DSQmlApplicationEngine::setContentHelper(model::IContentHelper* helper) {
    mContentHelper = helper;
    mContentHelper->setEngine(this);
}

network::DsNodeWatcher* DSQmlApplicationEngine::getNodeWatcher() const {
    return mNodeWatcher;
}

const model::ReferenceMap* DSQmlApplicationEngine::getReferenceMap() const {
    return &mQmlRefMap;
}

void DSQmlApplicationEngine::preInit() {
}

void dsqt::DSQmlApplicationEngine::readSettings(bool reset) {
    qCInfo(lgAppEngine) << "\nLoad Settings >>>>>>>>>>>>>>>>>>>>>>>>";
    mContentRoot = getContentRoot();
    qCInfo(lgAppEngine) << "loading main engine.toml";
    if (reset) {
        DSSettings::forgetSettings("engine");
    }
    dsqt::DSEnvironment::loadEngineSettings();
    auto extra_engine_settings = DSEnvironment::engineSettings()->getRawNode("engine.extra.engine", true);
    if (extra_engine_settings) {
        auto& extra_paths = *extra_engine_settings->as_array();
        for (auto&& path_node : extra_paths) {
            auto path = path_node.as_string()->value_or<std::string>("");
            qCInfo(lgAppEngine) << "Loading engine file " << path;
            dsqt::DSEnvironment::loadSettings("engine", path);
        }
    }

    qCInfo(lgAppEngine) << "loading main app_settings.toml";
    if (reset) {
        DSSettings::forgetSettings("app_settings");
    }
    mSettings               = dsqt::DSEnvironment::loadSettings("app_settings", "app_settings.toml");
    auto extra_app_settings = DSEnvironment::engineSettings()->getRawNode("engine.extra.app_settings", true);
    if (extra_app_settings) {
        auto& extra_paths = *extra_app_settings->as_array();
        for (auto&& path_node : extra_paths) {
            auto path = path_node.as_string()->value_or<std::string>("");
            qCInfo(lgAppEngine) << "Loading app_settings file " << path;
            dsqt::DSEnvironment::loadSettings("app_settings", path);
        }
    }
    qCInfo(lgAppEngine) << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n";
}

void DSQmlApplicationEngine::init() {
    //
    readSettings();
    if (!mAppProxy) mAppProxy = new DSSettingsProxy(this);

    // get watcher elements
    auto node = dsqt::DSEnvironment::engineSettings()->getRawNode("engine.reload.paths");
    if (node) {
        const auto& paths = *node->as_array();

        for (auto&& path_node : paths) {
            auto path    = path_node.as_table();
            auto oval    = QString::fromStdString((*path)["path"].as_string()->value_or<std::string>(""));
            auto val     = DSEnvironment::expandq(oval);
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

    mAppProxy->setTarget("app_settings");
    updateContentRoot(nullptr);
    // rootContext()->setContextProperty("app_settings",mAppProxy);
    // rootContext()->setContextProperty("$QmlEngine", this);
    // rootContext()->setContextProperty("$Env",mQmlEnv);

    // setup QML environment
    if (!mQmlEnv) mQmlEnv = new DSEnvironmentQML(this);

    // setup nodeWatcher
    if (!mNodeWatcher) mNodeWatcher = new network::DsNodeWatcher(this);
    // auto starts, but could also be this:
    // mNodeWatcher = new network::DsNodeWatcher(this,"localhost",7788,/*autostart*/false)
    // mNodeWatcher->start();
}

void DSQmlApplicationEngine::postInit() {
}

void DSQmlApplicationEngine::addRecursive(const QString& path, bool recurse) {
    auto         flags = recurse ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags;
    QDirIterator it(path, QDir::Dirs | QDir::NoDotAndDotDot, flags);
    while (it.hasNext()) {
        auto fi = QFileInfo(it.next());

        qInfo() << "Added " << fi.filePath() << " to watcherr";
        mWatcher->addPath(fi.filePath());
    }
}

void DSQmlApplicationEngine::setDefaultEngine(DSQmlApplicationEngine* engine) {
    sDefaultEngine = engine;
}

DSQmlApplicationEngine* DSQmlApplicationEngine::DefEngine() {
    return sDefaultEngine;
}

void DSQmlApplicationEngine::updateContentRoot(QSharedPointer<model::PropertyMapDiff> diff) {
    // updateContentRoot is responsible for cleaning up the diff.

    qInfo("Updating Content Root");
    if (mContentRoot.empty()) {
        mContentRoot = model::ContentModelRef("root");
        mContentRoot.setId("root");
    }

    if (mRootMap == nullptr) {
        mRootMap = mContentRoot.getQml(&mQmlRefMap, this);
    } else {
        if (diff) {
            diff->dumpChanges();
            diff->apply(*mRootMap, &mQmlRefMap);
        }
    }

    // mRootModel		  = mContentRoot.getModel(this);
    // mRootMap		  = mContentRoot.getQml(this);
    // rootContext()->setContextProperty("contentRootModel", mRootModel);
    // rootContext()->setContextProperty("contentRootMap", mRootMap);

    emit rootUpdated();
}

void DSQmlApplicationEngine::clearQmlCache() {
    this->clearComponentCache();
}

DSEnvironmentQML* DSQmlApplicationEngine::getEnvQml() const {
    return mQmlEnv;
}

DSSettingsProxy* DSQmlApplicationEngine::getAppSettingsProxy() const {
    return mAppProxy;
}

} // namespace dsqt
