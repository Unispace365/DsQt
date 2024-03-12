#include "dsqmlapplicationengine.h"
#include <QQmlContext>
#include <QStringLiteral>
#include "dsenvironment.h"
#include "model/content_model.h"
#include "settings/dssettings_proxy.h"


namespace dsqt {
using namespace Qt::Literals::StringLiterals;

DSQmlApplicationEngine* DSQmlApplicationEngine::sDefaultEngine = nullptr;

DSQmlApplicationEngine::DSQmlApplicationEngine(QObject *parent)
    : QQmlApplicationEngine{parent}
{
	if (sDefaultEngine == nullptr) {
		setDefaultEngine(this);
	}
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
	dsqt::DSSettingsProxy appProxy;

	appProxy.setTarget("app_settings");
	updateContentRoot(model::ContentModelRef("root"));
	rootContext()->setContextProperty("contentRoot", mRootModel);
	rootContext()->setContextProperty("app_settings",&appProxy);
    rootContext()->setContextProperty("$QmlEngine", this);
}

void DSQmlApplicationEngine::postInit()
{

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

}  // namespace dsqt
