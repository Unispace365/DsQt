#ifndef DSQMLAPPLICATIONENGINE_H
#define DSQMLAPPLICATIONENGINE_H

#include <qjsonmodel.h>
#include <QFileSystemWatcher>
#include <QObject>
#include <QQmlApplicationEngine>
#include "dsimgui_item.h"
#include "model/content_model.h"
#include "settings/dssettings.h"

namespace dsqt{

class DSQmlApplicationEngine : public QQmlApplicationEngine {
	Q_OBJECT
  public:
	explicit DSQmlApplicationEngine(QObject *parent = nullptr);
	void initialize();
	DSSettingsRef				   getAppSettings();
	model::ContentModelRef		   getContentRoot();
	void						   setDefaultEngine(DSQmlApplicationEngine* engine);
	static DSQmlApplicationEngine* DefEngine();
	void						   updateContentRoot(model::ContentModelRef newRoot);
	DsImguiItem* imgui();
	Q_INVOKABLE void			   clearCache();

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
	model::ContentModelRef		   mContentRoot;
	QJsonModel*					   mRootModel = nullptr;
	QQmlPropertyMap*			   mRootMap	  = nullptr;
	static DSQmlApplicationEngine* sDefaultEngine;
	DsImguiItem*				   mImgui;
	QFileSystemWatcher*			   mWatcher = nullptr;
};


}// namespace dsqt
#endif	// DSQMLAPPLICATIONENGINE_H
