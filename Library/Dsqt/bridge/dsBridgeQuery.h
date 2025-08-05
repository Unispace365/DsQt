#ifndef DSBRIDGEQUERY_H
#define DSBRIDGEQUERY_H

#include "bridge/dsBridgeWatcher.h"
#include "core/dsQmlApplicationEngine.h"
#include "model/dsPropertyMapDiff.h"

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

Q_DECLARE_LOGGING_CATEGORY(lgBridgeSyncApp)
Q_DECLARE_LOGGING_CATEGORY(lgBridgeSyncQuery)
Q_DECLARE_LOGGING_CATEGORY(lgBridgeSyncAppVerbose)
Q_DECLARE_LOGGING_CATEGORY(lgBridgeSyncQueryVerbose)
namespace dsqt::bridge {

/**
 * @brief A guard class to manage the lifecycle of a QProcess for BridgeSync.
 *
 * This class ensures that the associated QProcess is properly started and terminated,
 * including handling platform-specific behaviors like Job Objects on Windows to kill
 * child processes on exit.
 */
class BridgeSyncProcessGuard {
  public:
    /**
     * @brief Constructs a BridgeSyncProcessGuard and starts the associated process.
     * @param process The QProcess to manage.
     */
    BridgeSyncProcessGuard(QProcess& process);

    /**
     * @brief Destructs the BridgeSyncProcessGuard and terminates the associated process.
     */
    ~BridgeSyncProcessGuard();

  private:
    QProcess& mProcess;
#ifdef Q_OS_WIN
    HANDLE mJobHandle     = nullptr;
    HANDLE mProcessHandle = nullptr;
#endif
};

/**
 * @brief A RAII guard class for managing a QSqlDatabase connection.
 *
 * This class opens the database on construction if it wasn't already open and closes it
 * on destruction only if it opened it, ensuring proper resource management.
 */
class DatabaseGuard {
  public:
    /**
     * @brief Constructs a DatabaseGuard and opens the database if not already open.
     * @param database The QSqlDatabase to manage.
     */
    explicit DatabaseGuard(QSqlDatabase& database);

    /**
     * @brief Destructs the DatabaseGuard and closes the database if it was opened by this guard.
     */
    ~DatabaseGuard();

    // Non-copyable to prevent accidental copying of the database connection
    DatabaseGuard(const DatabaseGuard&)            = delete;
    DatabaseGuard& operator=(const DatabaseGuard&) = delete;

    /**
     * @brief Move constructor to transfer ownership of the database guard.
     * @param other The DatabaseGuard to move from.
     */
    DatabaseGuard(DatabaseGuard&& other) noexcept;

    /**
     * @brief Move assignment operator to transfer ownership of the database guard.
     * @param other The DatabaseGuard to move from.
     * @return Reference to this DatabaseGuard.
     */
    DatabaseGuard& operator=(DatabaseGuard&& other) noexcept;

    /**
     * @brief Checks if the managed database is open.
     * @return True if the database is open, false otherwise.
     */
    bool isOpen() const;

    /**
     * @brief Gets a reference to the managed database.
     * @return Reference to the QSqlDatabase.
     */
    QSqlDatabase& database();

  private:
    QSqlDatabase& mDatabase;
    bool          mWasOpen; // Tracks if the database was already open to avoid closing it unnecessarily
};

/**
 * @brief Structure holding settings for BridgeSync.
 *
 * This struct contains configuration options for launching and running the BridgeSync process.
 */
struct DsBridgeSyncSettings {
    bool     doLaunch;
    QString  appPath;
    QVariant server;
    QVariant authServer;
    QVariant clientId;
    QVariant clientSecret;
    QVariant verbose;
    QVariant asyncRecords;
    QVariant interval;
    QVariant directory;
};

/**
 * @brief Class for querying a SQL database bridged from a sync process.
 *
 * This class manages the interaction with a SQL database populated by BridgeSync,
 * including launching the sync process if configured, watching for updates, and querying
 * the database to update content models.
 */
class DsBridgeSqlQuery : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY(DsBridgeSqlQuery)
  public:
    /**
     * @brief Constructs a DsBridgeSqlQuery instance.
     * @param parent The parent DsQmlApplicationEngine (optional).
     */
    explicit DsBridgeSqlQuery(DsQmlApplicationEngine* parent = nullptr);

    /**
     * @brief Destructs the DsBridgeSqlQuery, closing resources.
     */
    ~DsBridgeSqlQuery() override;

  public:
    /**
     * @brief Checks if the BridgeSync process is currently running.
     * @return True if running, false otherwise.
     */
    bool isBridgeSyncRunning();

  public slots:
    /**
     * @brief Queries the database and updates content models.
     */
    void QueryDatabase();
  signals:
    /**
     * @brief Signal emitted when a sync completes with differences.
     * @param diff Shared pointer to the PropertyMapDiff containing changes.
     */
    void syncCompleted(QSharedPointer<model::PropertyMapDiff> diff);

  private:
    /**
     * @brief Handles errors from a QSqlQuery.
     * @param query The query that may have errored.
     * @param queryName Name of the query for logging.
     * @return True if no error, false if error occurred.
     */
    bool handleQueryError(const QSqlQuery& query, const QString& queryName);

    /**
     * @brief Retrieves the BridgeSync settings from configuration.
     * @return The DsBridgeSyncSettings struct.
     */
    DsBridgeSyncSettings getBridgeSyncSettings();

    /**
     * @brief Validates the BridgeSync settings.
     * @param settings The settings to validate.
     * @return True if valid, false otherwise.
     */
    bool validateBridgeSyncSettings(const DsBridgeSyncSettings& settings) const;

    /**
     * @brief Attempts to launch the BridgeSync process if not running.
     * @return True if launched or already running, false on failure.
     */
    bool tryLaunchBridgeSync();
    // void startBridgeSync();

    /**
     * @brief Stops the BridgeSync process.
     */
    void stopBridgeSync();

    /**
     * @brief Starts or uses an existing database connection.
     * @return True on success, false on failure.
     */
    bool startOrUseConnection();

    /**
     * @brief Starts a new database connection.
     * @return True on success, false on failure.
     */
    bool startConnection();

    /**
     * @brief Queries the database tables and updates models.
     * @return True on success, false on failure.
     */
    bool queryTables();

    /**
     * @brief Slugifies a key for use in property names.
     * @param appKey The key to slugify.
     * @return The slugified QString.
     */
    QString slugifyKey(QString appKey);

  private:
    QSqlDatabase     mDatabase;
    DsBridgeWatcher* mWatcher = nullptr;

#ifndef Q_OS_WASM
    QProcess                                mBridgeSyncProcess;
    std::unique_ptr<BridgeSyncProcessGuard> mProcessGuard;
    QList<QMetaObject::Connection>          mConnections;
#endif
};
} // namespace dsqt::bridge
#endif // DSBRIDGEQUERY_H
