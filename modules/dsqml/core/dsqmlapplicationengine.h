#ifndef DSQMLAPPLICATIONENGINE_H
#define DSQMLAPPLICATIONENGINE_H

#include <QObject>
#include <QQmlApplicationEngine>

#include "model/dscontentmodel.h"
#include "settings/dssettings.h"
#include "dsimgui_item.h"

namespace dsqt{

class DSQmlApplicationEngine : public QQmlApplicationEngine {
	Q_OBJECT
  public:
	explicit DSQmlApplicationEngine(QObject *parent = nullptr);
	void initialize();
	DSSettingsRef getSettings();
	DsImguiItem* imgui();
  private:
	virtual void preInit();
	virtual void init();
	virtual void postInit();
	DSSettingsRef mSettings;

  signals:
	void onPreInit();
	void onInit();
	void onPostInit();

  protected:
	model::DSContentModelPtr mContentRoot;
	DsImguiItem* mImgui;
};

}// namespace dsqt
#endif	// DSQMLAPPLICATIONENGINE_H
