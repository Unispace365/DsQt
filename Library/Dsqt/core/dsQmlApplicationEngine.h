#ifndef DSQMLAPPLICATIONENGINE_H
#define DSQMLAPPLICATIONENGINE_H

#include <model/qJsonModel.h>
#include <QFileSystemWatcher>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QTimer>
#include "dsQmlImguiItem.h"
#include "model/dsContentModel.h"
#include "model/dsQmlContentModel.h"
#include "settings/dsSettings.h"
#include "model/dsIContentHelper.h"
#include "model/dsPropertyMapDiff.h"
#include "dsQmlIdle.h"
Q_DECLARE_LOGGING_CATEGORY(lgAppEngine)
Q_DECLARE_LOGGING_CATEGORY(lgAppEngineVerbose)
namespace dsqt{

namespace network {
class DsNodeWatcher;
}

class DsQmlSettingsProxy;
class DsQmlEnvironment;
class DsQmlApplicationEngine : public QQmlApplicationEngine {
	Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Ya don't need to make an engine. get it from DS.engine")
    Q_PROPERTY(DsQmlIdle* idle READ idle NOTIFY idleChanged FINAL)
  public:
    explicit DsQmlApplicationEngine(QObject *parent = nullptr);
	void initialize();
    void doReset();
	DSSettingsRef				   getAppSettings();
	model::ContentModelRef		   getContentRoot();
    void						   setDefaultEngine(DsQmlApplicationEngine* engine);
    static DsQmlApplicationEngine* DefEngine();
	void                           updateContentRoot(QSharedPointer<model::PropertyMapDiff> diff);
    DsQmlImguiItem *imgui();
	Q_INVOKABLE void			   clearQmlCache();
    DsQmlEnvironment*              getEnvQml() const;
    DsQmlSettingsProxy*               getAppSettingsProxy() const;
    model::IContentHelper*         getContentHelper();
    void                           setContentHelper(model::IContentHelper* helper);
    network::DsNodeWatcher*        getNodeWatcher() const;
    const model::ReferenceMap*           getReferenceMap() const;
    
    void readSettings(bool reset=false);
    
    DsQmlIdle *idle() const;


  private:
	virtual void  preInit();
	virtual void  init();
	virtual void  postInit();

    void resetIdle();
    virtual void preReset();
    virtual void resetSystem();
    virtual void postReset();


	void		  addRecursive(const QString& path, bool recurse = true);
	DSSettingsRef mSettings;


  signals:
    void willInitialize();
    void initializing();
    void hasInitialized();

    void willReset();
    void resetting();
    void hasReset();
	void fileChanged(const QString& path);

    void rootUpdated();


    void idleChanged();

  protected:
	model::ContentModelRef		   mContentRoot;
    model::ReferenceMap            mQmlRefMap;
    model::QmlContentModel*		   mRootMap	  = nullptr;
    static DsQmlApplicationEngine* sDefaultEngine;
	DsQmlImguiItem*				   mImgui;
	QFileSystemWatcher*			   mWatcher = nullptr;
	QElapsedTimer				   mLastUpdate;
	QTimer						   mTrigger;
    DsQmlSettingsProxy*               mAppProxy=nullptr;
    DsQmlEnvironment*              mQmlEnv = nullptr;
    model::IContentHelper*         mContentHelper = nullptr;
    network::DsNodeWatcher*        mNodeWatcher = nullptr;
    DsQmlIdle*                          mIdle = nullptr;
};


}// namespace dsqt
#endif	// DSQMLAPPLICATIONENGINE_H
