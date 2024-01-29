#ifndef DSBRIDGEQUERY_H
#define DSBRIDGEQUERY_H

#include <QLoggingCategory>
#include <QObject>
#include <QProcess>
#include <QQmlPropertyMap>
#include <QtSql/QSql>
#include <QtSql/QSqlDatabase>
#include "core/dsqmlapplicationengine.h"
//#include <model/dscontentmodel.h>
#include <model/content_model.h>
#include <model/dsresource.h>
#include <settings/dssettings_proxy.h>

Q_DECLARE_LOGGING_CATEGORY(bridgeSyncApp)
Q_DECLARE_LOGGING_CATEGORY(bridgeSyncQuery)

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

private:
    QSqlDatabase mDatabase;
    model::DSContentModelPtr mContent;
    model::DSContentModelPtr mPlatforms;
    model::DSContentModelPtr mEvents;
    model::DSContentModelPtr mRecords;

    std::unordered_map<int,DSResource*> mAllResources;
    QString mResourceLocation;

    QProcess mBridgeSyncProcess;
    DSSettingsProxy mSettingsProxy;

    DsBridgeSyncSettings getBridgeSyncSettings();
    bool tryLaunchBridgeSync();
    void startBridgeSync();
    bool startOrUseConnection();
    bool startConnection();
    void queryTables();
};
} // namespace dsqt::bridge
#endif // DSBRIDGEQUERY_H
