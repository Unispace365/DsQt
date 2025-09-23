#ifndef DSBRIDGEQUERY_H
#define DSBRIDGEQUERY_H

#include "bridge/dsBridgeWatcher.h"
#include "core/dsQmlApplicationEngine.h"

#include <QFutureWatcher>
#include <QLoggingCategory>
#include <QObject>
#include <QProcess>
#include <QQmlPropertyMap>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <qloggingcategory.h>

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

class DatabaseQuery {
    class Timer {
      public:
        Timer(const QString& label)
            : mLabel(label) {
            mTimer.start();
        }
        ~Timer() { qDebug(lgBridgeSyncAppVerbose) << mLabel << "took" << mTimer.elapsed() << "milliseconds"; }

      private:
        QString       mLabel;
        QElapsedTimer mTimer;
    };

  public:
    DatabaseQuery(const QSqlDatabase& database, const QString& query, const QString& label = {})
        : mQuery(query, database) {
        mQuery.setForwardOnly(true);
    }

    bool execute();

    void process(const std::function<void(const QSqlRecord&)>& callback);

  private:
    QSqlQuery mQuery;
    QString   mLabel;
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

    /**
     * @brief Queries the database and updates content models.
     */
    void queryDatabase();

  public:
    /**
     * @brief Checks if the BridgeSync process is currently running.
     * @return True if running, false otherwise.
     */
    bool isBridgeSyncRunning();

  public slots:
    void onUpdated();

  signals:
    /**
     * @brief Signal emitted when a sync completes.
     */
    void syncCompleted();

  private:
    static QVariantHash toVariantHash(const QSqlRecord& record);

    struct DatabaseContent {
        QHash<QString, QVariantHash> records;   // All records.
        QStringList                  content;   // All content records.
        QStringList                  platforms; // All platform records.
        QStringList                  events;    // All event records.

        void buildTree() {
            // Clear existing list of children.
            for (auto node : records.asKeyValueRange()) {
                node.second["child_uid"] = QStringList();
            }
            // Populate list of children. Children are not sorted in order!
            for (auto node : records.asKeyValueRange()) {
                const auto uid         = node.second["uid"].toString();
                const auto parent_uids = node.second["parent_uid"].toStringList();
                if (parent_uids.size() != 1) continue; // Skip records with multiple parents, or no parents at all.
                for (const auto& parent_uid : parent_uids) {
                    const auto id = parent_uid.trimmed();
                    if (records.contains(id)) {
                        auto children = records[id]["child_uid"].toStringList();
                        if (!children.contains(uid)) children.append(uid);
                        records[id]["child_uid"] = children;
                    }
                }
            }
        }
    };

    // Iterator class for tree traversal
    class DatabaseIterator {
      public:
        DatabaseIterator(QHash<QString, QVariantHash>& nodes, const QList<QString>& order, qsizetype index)
            : m_nodes(nodes)
            , m_order(order)
            , m_index(index) {}

        // Dereference operator
        const QVariantHash& operator*() const { return m_nodes[m_order[m_index]]; }

        // Pointer operator
        const QVariantHash* operator->() const { return &m_nodes[m_order[m_index]]; }

        // Increment operator (pre-increment)
        DatabaseIterator& operator++() {
            if (m_index < m_order.size()) {
                ++m_index;
            }
            return *this;
        }

        // Equality operator
        bool operator==(const DatabaseIterator& other) const {
            return m_index == other.m_index && &m_nodes == &other.m_nodes;
        }

        // Inequality operator
        bool operator!=(const DatabaseIterator& other) const { return !(*this == other); }

      private:
        QHash<QString, QVariantHash>& m_nodes; // Reference to the node hash
        const QList<QString>&         m_order; // UIDs in the correct order
        qsizetype                     m_index; // Current position in traversal
    };

    // Helper class to traverse the database.
    class DatabaseTree {
      public:
        enum class Traversal { PreOrder, PostOrder, Roots };

        DatabaseTree(QHash<QString, QVariantHash>& nodes, const QString& rootUid,
                     Traversal order = Traversal::PostOrder)
            : m_nodes(nodes) {
            if (m_nodes.contains(rootUid)) {
                switch (order) {
                case Traversal::PreOrder: // Visits the root node first, then recursively traverses its children.
                    preOrderTraversal(rootUid);
                    break;
                case Traversal::PostOrder: // Visits the child nodes before their parents, with the root node visited last.
                    postOrderTraversal(rootUid);
                    break;
                case Traversal::Roots: // Only visits nodes that have no parent.
                    rootsTraversal();
                    break;
                }
            }
        }

        DatabaseTree(DatabaseContent& content, Traversal order = Traversal::PostOrder)
            : DatabaseTree(content.records, content.content.front(), order) {}

        // Begin iterator
        DatabaseIterator begin() const { return DatabaseIterator(m_nodes, m_order, 0); }

        // End iterator
        DatabaseIterator end() const { return DatabaseIterator(m_nodes, m_order, m_order.size()); }

      private:
        // Recursive pre-order traversal
        void preOrderTraversal(const QString& uid) {
            // Visit the node itself first
            m_order.append(uid);
            // Then visit children
            const QStringList children = m_nodes[uid]["child_uid"].toStringList();
            for (const QString& childUid : children) {
                if (m_nodes.contains(childUid)) {
                    preOrderTraversal(childUid);
                }
            }
        }

        // Recursive post-order traversal
        void postOrderTraversal(const QString& uid) {
            // Visit children first
            const QStringList children = m_nodes[uid]["child_uid"].toStringList();
            for (const QString& childUid : children) {
                if (m_nodes.contains(childUid)) {
                    postOrderTraversal(childUid);
                }
            }
            // Then visit the node itself
            m_order.append(uid);
        }

        // Roots traversal
        void rootsTraversal() {
            for (const auto itr : m_nodes.asKeyValueRange()) {
                const auto& record = itr.second;
                if (!record.contains("parent_uid")) continue;
                const auto parents   = record["parent_uid"].toStringList();
                bool       hasParent = false;
                for (const auto& parent : parents) {
                    hasParent = m_nodes.contains(parent.trimmed());
                    if (hasParent) break;
                }
                if (hasParent) continue;
                m_order.append(itr.first);
            }
        }

        QHash<QString, QVariantHash>& m_nodes; // Reference to the node hash (non-const for modification)
        QList<QString>                m_order; // UIDs of nodes in post-order
    };

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
    [[nodiscard]] DatabaseContent queryTables();

    /**
     * @brief Slugifies a key for use in property names.
     * @param appKey The key to slugify.
     * @return The slugified QString.
     */
    static QString slugifyKey(QString appKey);

  private:
    QSqlDatabase                    mDatabase;
    DsBridgeWatcher*                mWatcher = nullptr;
    QFutureWatcher<DatabaseContent> mFutures; // Tracks the async task.

#ifndef Q_OS_WASM
    QProcess                                mBridgeSyncProcess;
    std::unique_ptr<BridgeSyncProcessGuard> mProcessGuard;
    QList<QMetaObject::Connection>          mConnections;
#endif
};
} // namespace dsqt::bridge
#endif // DSBRIDGEQUERY_H
