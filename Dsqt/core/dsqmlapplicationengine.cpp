#include "dsqmlapplicationengine.h"
#include <toml++/toml.h>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QImageReader>
#include <QQmlContext>
#include <QStringLiteral>
#include "dsenvironment.h"
#include "model/content_model.h"
#include "settings/dssettings_proxy.h"
#include "dsenvironmentqml.h"

namespace dsqt {
using namespace Qt::Literals::StringLiterals;

DSQmlApplicationEngine* DSQmlApplicationEngine::sDefaultEngine = nullptr;

DSQmlApplicationEngine::DSQmlApplicationEngine(QObject* parent)
  : QQmlApplicationEngine{parent}, mWatcher{new QFileSystemWatcher(this)} {
	if (sDefaultEngine == nullptr) {
		setDefaultEngine(this);
	}

	qInfo() << "connecting file watcher";
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
			qInfo() << "Triggered";
			emit fileChanged("triggered");
		},
		Qt::QueuedConnection);

	connect(
		mWatcher, &QFileSystemWatcher::fileChanged, this,
		[](const QString& path) {
			qInfo() << "Directory changed " << path;
		},
		Qt::QueuedConnection);
}

void DSQmlApplicationEngine::initialize()
{
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
	}
	return mContentRoot;
}

void DSQmlApplicationEngine::preInit()
{

}

void DSQmlApplicationEngine::init()
{
	mContentRoot = getContentRoot();
	dsqt::DSEnvironment::loadEngineSettings();
	mSettings = dsqt::DSEnvironment::loadSettings("app_settings","app_settings.toml");
    mAppProxy=new DSSettingsProxy(this);
    mQmlEnv = new DSEnvironmentQML(this);
	// get watcher elements
	auto node = dsqt::DSEnvironment::engineSettings()->getRawNode("engine.reload.paths");
	if (node) {


		auto& paths = *node->as_array();

		for (auto&& path_node : paths) {
			auto path	 = path_node.as_table();
			auto oval	 = QString::fromStdString((*path)["path"].as_string()->value_or<std::string>(""));
			auto val	 = DSEnvironment::expandq(oval);
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
	updateContentRoot(model::ContentModelRef("root"));
    //rootContext()->setContextProperty("app_settings",mAppProxy);
    //rootContext()->setContextProperty("$QmlEngine", this);
    //rootContext()->setContextProperty("$Env",mQmlEnv);
}

void DSQmlApplicationEngine::postInit()
{

}

void DSQmlApplicationEngine::addRecursive(const QString& path, bool recurse) {
	auto		 flags = recurse ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags;
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

void DSQmlApplicationEngine::updateContentRoot(model::ContentModelRef newRoot) {
	qInfo("Updating Content Root");
	mContentRoot	  = newRoot;
	auto oldRootModel = mRootModel;
	auto oldRootMap	  = mRootMap;
	mRootModel		  = mContentRoot.getModel(this);
	mRootMap		  = mContentRoot.getMap(this);
	rootContext()->setContextProperty("contentRootModel", mRootModel);
	rootContext()->setContextProperty("contentRootMap", mRootMap);
	if (oldRootModel) {
		delete oldRootModel;
	}
	if (oldRootMap) {
		delete oldRootMap;
	}
}

void DSQmlApplicationEngine::clearQmlCache() {
	this->clearComponentCache();
}

DSEnvironmentQML *DSQmlApplicationEngine::getEnvQml() const
{
    return mQmlEnv;
}

DSSettingsProxy *DSQmlApplicationEngine::getAppSettingsProxy() const
{
    return mAppProxy;
}

}  // namespace dsqt
