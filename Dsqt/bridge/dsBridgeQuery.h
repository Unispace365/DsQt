#ifndef DSBRIDGEQUERY_H
#define DSBRIDGEQUERY_H

#include <QLoggingCategory>
#include <QObject>
#include <QProcess>
#include <QQmlPropertyMap>
#include <QtSql/QSql>
#include <QtSql/QSqlDatabase>

#include <bridge_utility.h>
#include <core/dsqmlapplicationengine.h>
#include <model/content_model.h>
#include <model/dsresource.h>
#include <settings/dssettings_proxy.h>

Q_DECLARE_LOGGING_CATEGORY(lgBridgeSyncApp)
Q_DECLARE_LOGGING_CATEGORY(lgBridgeSyncQuery)
Q_DECLARE_LOGGING_CATEGORY(lgBridgeSyncAppVerbose)
Q_DECLARE_LOGGING_CATEGORY(lgBridgeSyncQueryVerbose)
namespace dsqt::bridge {

struct DsBridgeSyncSettings
{
    bool doLaunch;
    QString appPath;
    QVariant server;
    QVariant authServer;
    QVariant clientId;
    QVariant clientSecret;
    QVariant verbose;
    QVariant interval;
    QVariant directory;
};

class DsBridgeSqlQuery : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(DsBridgeSqlQuery)
public:
    explicit DsBridgeSqlQuery(DSQmlApplicationEngine *parent = nullptr);
    ~DsBridgeSqlQuery() override;

public:
    bool isBridgeSyncRunning();
    const BridgeUtility *getBridgeUtility() { return mBridgeUtility; }
public slots:
    void QueryDatabase();
signals:
    void syncCompleted();

private:
    BridgeUtility *mBridgeUtility = nullptr;
    QSqlDatabase mDatabase;
    model::ContentModelRef mContent;
    model::ContentModelRef mPlatforms;
    model::ContentModelRef mPlatform;
    model::ContentModelRef mEvents;
    model::ContentModelRef mRecords;

    std::unordered_map<int, DSResource> mAllResources;
    QString mResourceLocation;

#ifndef Q_OS_WASM
    QProcess mBridgeSyncProcess;
#endif
    DSSettingsProxy mSettingsProxy;

    DsBridgeSyncSettings getBridgeSyncSettings();
    bool tryLaunchBridgeSync();
    void startBridgeSync();
    bool startOrUseConnection();
    bool startConnection();
    void queryTables();
    QString slugifyKey(QString appKey);
};
} // namespace dsqt::bridge
#endif // DSBRIDGEQUERY_H
