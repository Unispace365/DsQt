#include "dsqmlapplicationengine.h"
#include "dsenvironment.h"
#include "settings/dssettings_proxy.h"
#include "model/dscontentmodel.h"
#include <QQmlContext>
#include <QStringLiteral>



namespace dsqt {
using namespace Qt::Literals::StringLiterals;
DSQmlApplicationEngine::DSQmlApplicationEngine(QObject *parent)
    : QQmlApplicationEngine{parent}
{

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

void DSQmlApplicationEngine::preInit()
{

}

void DSQmlApplicationEngine::init()
{
	mContentRoot = model::DSContentModel::mContent;
	mContentRoot->unlock();
	dsqt::DSEnvironment::loadEngineSettings();
	dsqt::DSSettingsRef appSettings = dsqt::DSEnvironment::loadSettings("app_settings","app_settings.toml");
	dsqt::DSSettingsProxy appProxy;

	appProxy.setTarget("app_settings");

	rootContext()->setContextProperty("contentRoot",mContentRoot.get());
	rootContext()->setContextProperty("app_settings",&appProxy);
}

void DSQmlApplicationEngine::postInit()
{

}

} //namespace dsqt
