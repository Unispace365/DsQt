#ifndef DSQMLAPPLICATIONENGINE_H
#define DSQMLAPPLICATIONENGINE_H

#include <QObject>
#include <QQmlApplicationEngine>

#include "model/dscontentmodel.h"
#include "settings/dssettings.h"

namespace dsqt{

class DSQmlApplicationEngine : public QQmlApplicationEngine {
	Q_OBJECT
  public:
	explicit DSQmlApplicationEngine(QObject *parent = nullptr);
	void initialize();
	DSSettingsRef getSettings();
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
};

}// namespace dsqt
#endif	// DSQMLAPPLICATIONENGINE_H
