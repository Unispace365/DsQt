#ifndef DSQMLAPPLICATIONENGINE_H
#define DSQMLAPPLICATIONENGINE_H

#include <QObject>
#include <QQmlApplicationEngine>

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
	QJsonModel*					   mRootModel;
	static DSQmlApplicationEngine* sDefaultEngine;
};


}// namespace dsqt
#endif	// DSQMLAPPLICATIONENGINE_H
