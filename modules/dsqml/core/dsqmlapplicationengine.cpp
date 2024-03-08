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

void DSQmlApplicationEngine::preInit()
{

}

void DSQmlApplicationEngine::init()
{
	mContentRoot = model::ContentModelRef("root");
	dsqt::DSEnvironment::loadEngineSettings();
	mSettings = dsqt::DSEnvironment::loadSettings("app_settings","app_settings.toml");
	dsqt::DSSettingsProxy appProxy;

	appProxy.setTarget("app_settings");
	mRootModel = mContentRoot.getModel(this);
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
	mContentRoot	  = newRoot;
	auto oldRootModel = mRootModel;
	mRootModel		  = mContentRoot.getModel(this);
	rootContext()->setContextProperty("contentRoot", mRootModel);
	if (oldRootModel) {
		delete oldRootModel;
	}
}

}  // namespace dsqt
