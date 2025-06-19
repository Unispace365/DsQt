#ifndef DSQMLAPPLICATIONENGINE_H
#define DSQMLAPPLICATIONENGINE_H

#include <model/qjsonmodel.h>
#include <QFileSystemWatcher>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QTimer>
#include "dsimgui_item.h"
#include "model/content_model.h"
#include "model/qmlcontentmodel.h"
#include "settings/dssettings.h"
#include "model/icontent_helper.h"
#include "model/property_map_diff.h"

Q_DECLARE_LOGGING_CATEGORY(lgAppEngine)
Q_DECLARE_LOGGING_CATEGORY(lgAppEngineVerbose)
namespace dsqt{

namespace network {
class DsNodeWatcher;
}

class DSSettingsProxy;
class DSEnvironmentQML;
class DSQmlApplicationEngine : public QQmlApplicationEngine {
	Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Ya don't need to make an engine. get it from DS.engine")
  public:
	explicit DSQmlApplicationEngine(QObject *parent = nullptr);
	void initialize();
	DSSettingsRef				   getAppSettings();
	model::ContentModelRef		   getContentRoot();
	void						   setDefaultEngine(DSQmlApplicationEngine* engine);
    static DSQmlApplicationEngine* DefEngine();
    void                           updateContentRoot(model::PropertyMapDiff* diff);
    DsImguiItem *imgui();
	Q_INVOKABLE void			   clearQmlCache();
    DSEnvironmentQML*              getEnvQml() const;
    DSSettingsProxy*               getAppSettingsProxy() const;
    model::IContentHelper*         getContentHelper();
    void                           setContentHelper(model::IContentHelper* helper);
    network::DsNodeWatcher*        getNodeWatcher() const;
    const model::ReferenceMap*           getReferenceMap() const;
    
    void readSettings(bool reset=false);
    
private:
	virtual void  preInit();
	virtual void  init();
	virtual void  postInit();
	void		  addRecursive(const QString& path, bool recurse = true);
	DSSettingsRef mSettings;


  signals:
	void onPreInit();
	void onInit();
	void onPostInit();
	void fileChanged(const QString& path);
    void rootUpdated();

  protected:
	model::ContentModelRef		   mContentRoot;
    model::ReferenceMap            mQmlRefMap;
    model::QmlContentModel*		   mRootMap	  = nullptr;
	static DSQmlApplicationEngine* sDefaultEngine;
	DsImguiItem*				   mImgui;
	QFileSystemWatcher*			   mWatcher = nullptr;
	QElapsedTimer				   mLastUpdate;
	QTimer						   mTrigger;
    DSSettingsProxy*               mAppProxy=nullptr;
    DSEnvironmentQML*              mQmlEnv = nullptr;
    model::IContentHelper*         mContentHelper = nullptr;
    network::DsNodeWatcher*        mNodeWatcher = nullptr;
};


}// namespace dsqt
#endif	// DSQMLAPPLICATIONENGINE_H
