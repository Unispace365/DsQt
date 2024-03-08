#ifndef DSBRIDGEQUERY_H
#define DSBRIDGEQUERY_H

#include <QLoggingCategory>
#include <QObject>
#include <QProcess>
#include <QQmlPropertyMap>
#include <QtSql/QSql>
#include <QtSql/QSqlDatabase>
#include <core/dsqmlapplicationengine.h>
#include <model/content_model.h>
#include <model/resource.h>
#include <settings/dssettings_proxy.h>

Q_DECLARE_LOGGING_CATEGORY(lgBridgeSyncApp)
Q_DECLARE_LOGGING_CATEGORY(lgBridgeSyncQuery)

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
    model::ContentModelRef mContent;
    model::ContentModelRef mPlatforms;
    model::ContentModelRef mEvents;
    model::ContentModelRef mRecords;

    std::unordered_map<int, Resource> mAllResources;
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
