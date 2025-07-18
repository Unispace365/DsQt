#ifndef DSBRIDGEQUERY_H
#define DSBRIDGEQUERY_H

#include <QLoggingCategory>
#include <QObject>
#include <QProcess>
#include <QQmlPropertyMap>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>

#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#endif

#include <core/dsqmlapplicationengine.h>
#include <model/content_model.h>
#include <model/dsresource.h>
#include <settings/dssettings_proxy.h>
#include <model/property_map_diff.h>

Q_DECLARE_LOGGING_CATEGORY(lgBridgeSyncApp)
Q_DECLARE_LOGGING_CATEGORY(lgBridgeSyncQuery)
Q_DECLARE_LOGGING_CATEGORY(lgBridgeSyncAppVerbose)
Q_DECLARE_LOGGING_CATEGORY(lgBridgeSyncQueryVerbose)
namespace dsqt::bridge {

class BridgeSyncProcessGuard {
public:
    BridgeSyncProcessGuard(QProcess& process);
    ~BridgeSyncProcessGuard();
private:
    QProcess& mProcess;
#ifdef Q_OS_WIN
    HANDLE mJobHandle = nullptr;
    HANDLE mProcessHandle = nullptr;
#endif
};

struct DsBridgeSyncSettings
{
    bool doLaunch;
    QString appPath;
    QVariant server;
    QVariant authServer;
    QVariant clientId;
    QVariant clientSecret;
    QVariant verbose;
    QVariant asyncRecords;
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

public slots:
    void QueryDatabase();
signals:
    void syncCompleted(QSharedPointer<model::PropertyMapDiff> diff);

private:
    bool handleQueryError(const QSqlQuery& query, const QString& queryName);

    DsBridgeSyncSettings getBridgeSyncSettings();
    bool validateBridgeSyncSettings(const DsBridgeSyncSettings& settings) const;
    bool tryLaunchBridgeSync();
    //void startBridgeSync();
    void stopBridgeSync();
    bool startOrUseConnection();
    bool startConnection();
    void queryTables();
    QString slugifyKey(QString appKey);

private:
    QSqlDatabase mDatabase;

#ifndef Q_OS_WASM
    QProcess mBridgeSyncProcess;
    std::unique_ptr<BridgeSyncProcessGuard> mProcessGuard;
    QList<QMetaObject::Connection> mConnections;
#endif
};
} // namespace dsqt::bridge
#endif // DSBRIDGEQUERY_H
