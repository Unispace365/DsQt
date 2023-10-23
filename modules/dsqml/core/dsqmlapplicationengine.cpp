#include "dsqmlapplicationengine.h"
#include "dsenvironment.h"
#include "settings/dssettings_proxy.h"
#include "model/dscontentmodel.h"
#include <QQmlContext>
#include <QStringLiteral>
#include "dsimgui_item.h"
#include <imgui/imgui.h>

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

DSSettingsRef DSQmlApplicationEngine::getSettings()
{
	return mSettings;
}

DsImguiItem *DSQmlApplicationEngine::imgui()
{
	return mImgui;
}

void DSQmlApplicationEngine::preInit()
{

}

void DSQmlApplicationEngine::init()
{
	mContentRoot = model::DSContentModel::mContent;
	mContentRoot->unlock();
	dsqt::DSEnvironment::loadEngineSettings();
	mSettings = dsqt::DSEnvironment::loadSettings("app_settings","app_settings.toml");
	dsqt::DSSettingsProxy appProxy;

	appProxy.setTarget("app_settings");

	rootContext()->setContextProperty("contentRoot",mContentRoot.get());
	rootContext()->setContextProperty("app_settings",&appProxy);
    rootContext()->setContextProperty("$QmlEngine", this);
	connect(this,&QQmlApplicationEngine::objectCreated,this,[this](QObject *obj, const QUrl &objUrl){
		if (obj && obj->parent() == nullptr){
			DsImguiItem *gui = obj->findChild<DsImguiItem *>("imgui");
			if(gui){
				mImgui = gui;
				imgui()->callbacks<<[this](){
					if (ImGui::BeginMainMenuBar())
					{
						if (ImGui::BeginMenu("File"))
						{
							if (ImGui::MenuItem("Open..", "Ctrl+O")) { /* Do stuff */ }
							if (ImGui::MenuItem("Save", "Ctrl+S"))   { /* Do stuff */ }
							if (ImGui::MenuItem("Close", "Ctrl+W"))  {  }
							ImGui::EndMenu();
						}
						ImGui::EndMainMenuBar();
					}
				};
			} else {
				qWarning()<<"No IMGUI element found in QML";
			}
		}
	});


}

void DSQmlApplicationEngine::postInit()
{

}



} //namespace dsqt
