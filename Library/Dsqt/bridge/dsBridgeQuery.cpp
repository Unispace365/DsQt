#include "bridge/dsBridgeQuery.h"
#include "core/dsEnvironment.h"
#include "model/dsResource.h"
#include "network/dsNodeWatcher.h"
#include "settings/dsQmlSettingsProxy.h"
#include "settings/dsSettings.h"

#include "qqmlcontext.h"

#include <QSharedPointer>
#include <QSqlField>
#include <QString>

Q_LOGGING_CATEGORY(lgBridgeSyncApp, "bridgeSync.app")
Q_LOGGING_CATEGORY(lgBridgeSyncQuery, "bridgeSync.query")
Q_LOGGING_CATEGORY(lgBridgeSyncAppVerbose, "bridgeSync.app.verbose")
Q_LOGGING_CATEGORY(lgBridgeSyncQueryVerbose, "bridgeSync.query.verbose")

using namespace dsqt::model;

namespace dsqt::bridge {

DsBridgeSqlQuery::DsBridgeSqlQuery(DsQmlApplicationEngine* parent)
    : QObject(parent) {
    // Initialize when the engine is ready.
    connect(parent, &DsQmlApplicationEngine::endInitialize, this, [this]() {
        // Construct or update the content tree.
        auto root = DsQmlApplicationEngine::DefEngine()->getRwContentRoot();
        DsQmlApplicationEngine::DefEngine()->rootContext()->setContextProperty("bridge", root);
        // DsQmlApplicationEngine::DefEngine()->setBridge(root);

        // If bridge sync is not running, try to launch it.
        if (!isBridgeSyncRunning()) {
            tryLaunchBridgeSync();
        }

        // Create database.
        mDatabase = QSqlDatabase::addDatabase("QSQLITE");

        // Find DB file.
        DsQmlSettingsProxy engSettings;
        engSettings.setTarget("engine");
        engSettings.setPrefix("engine.resource");

        const auto dbFileName = engSettings.getString("resource_db", "").value<QString>();
        if (!dbFileName.isEmpty()) {
            QFileInfo file(DsEnvironment::expandq(dbFileName));
            if (file.isRelative()) {
                QString resourceLocation =
                    DsEnvironment::expandq(engSettings.getString("location", "").value<QString>());
                file.setFile(QDir::cleanPath(resourceLocation), dbFileName);
            }

            // Open DB file in read-only mode. This facilitates concurrency.
            mDatabase.setDatabaseName(file.absoluteFilePath());
            mDatabase.setConnectOptions("QSQLITE_OPEN_READONLY");

            // Make sure we get notified on the main thread when the background process finishes.
            connect(&mFutures, &QFutureWatcher<DatabaseContent>::finished, this, &DsBridgeSqlQuery::onUpdated);

            // Read the database now.
            queryDatabase();

            // Watch for database updates using notification file (BridgeSync > v4.4.0)
            if (!mWatcher) mWatcher = new DsBridgeWatcher(file, this);
            connect(mWatcher, &bridge::DsBridgeWatcher::databaseUpdated, this, [this]() { queryDatabase(); });

            // Watch for database updates using UDP message (BridgeSync <= v4.4.0)
            const auto engine      = DsQmlApplicationEngine::DefEngine();
            const auto nodeWatcher = engine->getNodeWatcher();
            if (nodeWatcher) {
                connect(nodeWatcher, &network::DsNodeWatcher::messageArrived, this,
                        [this](dsqt::network::Message msg) { queryDatabase(); });
            }
        }
    });
}

DsBridgeSqlQuery::~DsBridgeSqlQuery() {
    // Ignore content updates.
    disconnect(&mFutures, &QFutureWatcher<DatabaseContent>::finished, this, &DsBridgeSqlQuery::onUpdated);

    // Wait for the background process to finish.
    mFutures.waitForFinished();

    // Properly close the DB file.
    if (mDatabase.isOpen()) {
        qCInfo(lgBridgeSyncApp) << "Closing database";
        mDatabase.close();
    }

#ifndef Q_OS_WASM
    // Properly stop the BridgeSync process.
    qCInfo(lgBridgeSyncApp) << "Closing BridgeSync";
    stopBridgeSync();
#endif
}

DsBridgeSyncSettings DsBridgeSqlQuery::getBridgeSyncSettings() {
    DsQmlSettingsProxy engSettings;
    engSettings.setTarget("engine");
    engSettings.setPrefix("engine.bridgesync");

    DsBridgeSyncSettings settings;
    settings.server       = engSettings.getString("connection.server", "");
    settings.authServer   = engSettings.getString("connection.auth_server", "");
    settings.clientId     = engSettings.getString("connection.client_id", "");
    settings.clientSecret = engSettings.getString("connection.client_secret", "");
    settings.directory    = engSettings.getString("connection.directory", "");
    settings.interval     = engSettings.getInt("connection.interval", 10);
    settings.verbose      = engSettings.getBool("connection.verbose", false);
    settings.asyncRecords = engSettings.getBool("connection.asyncRecords", true);

    const auto appPath = engSettings.getString("app_path", "%SHARE%/bridgesync/bridge_sync_console.exe").toString();
    settings.appPath   = DsEnvironment::expandq(appPath);

    const auto launchBridgeSync = engSettings.getBool("launch_bridgesync", false);
    settings.doLaunch           = launchBridgeSync.toBool();
    return settings;
}

bool DsBridgeSqlQuery::validateBridgeSyncSettings(const DsBridgeSyncSettings& settings) const {
    if (!settings.doLaunch) {
        qCInfo(lgBridgeSyncApp) << "BridgeSync is not configured to launch";
        return false;
    }
    if (settings.appPath.isEmpty()) {
        qCWarning(lgBridgeSyncApp) << "BridgeSync's app path is empty. BridgeSync will not launch";
        return false;
    }
    if (settings.server.toString().isEmpty()) {
        qCWarning(lgBridgeSyncApp) << "BridgeSync's server is empty. BridgeSync will not launch";
        return false;
    }
    if (settings.directory.toString().isEmpty()) {
        qCWarning(lgBridgeSyncApp) << "BridgeSync's directory is empty. BridgeSync will not launch";
        return false;
    }

    return true;
}

bool DsBridgeSqlQuery::tryLaunchBridgeSync() {
#ifndef Q_OS_WASM
    qCInfo(lgBridgeSyncApp) << "Launching BridgeSync";

    // Check if process is running.
    if (mBridgeSyncProcess.state() == QProcess::Running) {
        qCInfo(lgBridgeSyncApp) << "BridgeSync is already running";
        return true;
    }

    // Initialize settings.
    auto bridgeSyncSettings = getBridgeSyncSettings();
    if (!validateBridgeSyncSettings(bridgeSyncSettings)) return false;

    // Check path to executable.
    const auto appPath = DsEnvironment::expandq(bridgeSyncSettings.appPath);
    if (!QFile::exists(appPath)) {
        qCWarning(lgBridgeSyncApp) << "BridgeSync's app path does not exist. BridgeSync will not launch\n" << appPath;
        return false;
    }

    // Stop process if needed.
    stopBridgeSync();

    // Prepare launching the process.
    mBridgeSyncProcess.setProgram(appPath);
    QStringList args;

    args << "--server" << bridgeSyncSettings.server.toString();
    if (!bridgeSyncSettings.authServer.toString().isEmpty()) {
        args << "--authServer" << bridgeSyncSettings.authServer.toString();
    }
    if (!bridgeSyncSettings.clientId.toString().isEmpty()) {
        args << "--clientId" << bridgeSyncSettings.clientId.toString();
    }
    if (!bridgeSyncSettings.clientSecret.toString().isEmpty()) {
        args << "--clientSecret" << bridgeSyncSettings.clientSecret.toString();
    }
    if (!bridgeSyncSettings.directory.toString().isEmpty()) {
        args << "--directory" << DsEnvironment::expandq(bridgeSyncSettings.directory.toString());
    }
    if (bridgeSyncSettings.interval.toFloat() > 0) {
        args << "--interval" << bridgeSyncSettings.interval.toString();
    }
    if (bridgeSyncSettings.verbose.toBool()) {
        args << "--verbose";
    }
    if (bridgeSyncSettings.asyncRecords.toBool()) {
        args << "--asyncRecords";
    }
    mBridgeSyncProcess.setArguments(args);
    qCInfo(lgBridgeSyncApp) << "BridgeSync is launching with args: " << mBridgeSyncProcess.arguments();

    // Connect to possible outcomes for start.
    mConnections.append(connect(&mBridgeSyncProcess, &QProcess::started, this,
                                [this]() { qCInfo(lgBridgeSyncApp) << "BridgeSync has started"; }));

    mConnections.append(connect(
        &mBridgeSyncProcess, &QProcess::errorOccurred, this,
        [this](QProcess::ProcessError error) {
            qCWarning(lgBridgeSyncApp) << "BridgeSync has encountered an error: " << error;
            // Try again in a few.
            QTimer::singleShot(5000, this, &DsBridgeSqlQuery::tryLaunchBridgeSync);
        },
        Qt::QueuedConnection));

    mConnections.append(connect(
        &mBridgeSyncProcess, &QProcess::finished, this,
        [this](int exitCode) {
            qCInfo(lgBridgeSyncApp) << mBridgeSyncProcess.readAllStandardOutput();
            qCInfo(lgBridgeSyncApp) << "BridgeSync has finished with exit code: " << exitCode;
            // Try again in a few.
            QTimer::singleShot(5000, this, &DsBridgeSqlQuery::tryLaunchBridgeSync);
        },
        Qt::QueuedConnection));

    mConnections.append(
        connect(&mBridgeSyncProcess, &QProcess::stateChanged, this, [this](QProcess::ProcessState state) {
            qCDebug(lgBridgeSyncApp) << "BridgeSync has changed state: " << state;
        }));

    mConnections.append(connect(&mBridgeSyncProcess, &QProcess::readyReadStandardOutput, this, [this]() {
        mBridgeSyncProcess.setReadChannel(QProcess::StandardOutput); // Switch to stdout channel
        while (mBridgeSyncProcess.canReadLine()) {
            QString output = QString::fromUtf8(mBridgeSyncProcess.readLine()).trimmed();
            if (!output.isEmpty()) {
                qCInfo(lgBridgeSyncApp) << output;
            }
        }
    }));

    mConnections.append(connect(&mBridgeSyncProcess, &QProcess::readyReadStandardError, this, [this]() {
        mBridgeSyncProcess.setReadChannel(QProcess::StandardError); // Switch to stderr channel
        while (mBridgeSyncProcess.canReadLine()) {
            QString output = QString::fromUtf8(mBridgeSyncProcess.readLine()).trimmed();
            if (!output.isEmpty()) {
                qCWarning(lgBridgeSyncApp) << output;
            }
        }
    }));

    // Start the process, making sure the process is terminated upon exit.
    mProcessGuard = std::make_unique<BridgeSyncProcessGuard>(mBridgeSyncProcess);

    return true;
#endif

    return true;
}

void DsBridgeSqlQuery::stopBridgeSync() {
    // Disconnect from process events.
    for (const auto& connection : std::as_const(mConnections)) {
        QObject::disconnect(connection);
    }
    mConnections.clear();

    // Make sure our current process is stopped.
    mProcessGuard.reset();
}

// Checks to see if process is started.
bool DsBridgeSqlQuery::isBridgeSyncRunning() {
#ifdef Q_OS_WASM
    return true;
#else
    return mBridgeSyncProcess.state() == QProcess::Running;
#endif
}

void DsBridgeSqlQuery::onUpdated() {
    QElapsedTimer timer;
    timer.start();

    // This runs in the main thread when the background task completes.
    DatabaseContent result = mFutures.result();

    const auto isMainThread = QThread::currentThread() == QCoreApplication::instance()->thread();
    qCDebug(lgBridgeSyncQuery) << "BridgeService found" << result.records.size() << "records in the database";

    // Traverse content tree.
    auto t1 = timer.elapsed();

    DatabaseTree roots(result, DatabaseTree::Traversal::Roots);
    for (const auto& root : roots) {
        DatabaseTree tree(result.records, root.value("uid").toString());
        for (const auto& record : tree) {
            rework::RwContentModel::createOrUpdate(record);
        }
    }
    // Make sure all content is linked up (parent-child relations are enforced).
    auto t2 = timer.elapsed();

    rework::RwContentModel::linkUp();

    // Construct or update the content tree.
    auto root = DsQmlApplicationEngine::DefEngine()->getRwContentRoot();

    // // Update root.
    // auto _content = QVariant::fromValue(rework::RwContentModel::find(result.content.front()));
    // root->setProperty("content", _content);
    // auto _events = QVariant::fromValue(rework::RwContentModel::find(result.events));
    // root->setProperty("events", _events);
    // auto _platforms = QVariant::fromValue(rework::RwContentModel::find(result.platforms));
    // root->setProperty("platforms", _platforms);
    // auto _records = QVariant::fromValue(rework::RwContentModel::find(result.records.keys()));
    // root->setProperty("records", _records);

    DsQmlApplicationEngine::DefEngine()->updateContentRoot();

    auto t3 = timer.elapsed();

    qInfo() << "Updating content took" << timer.elapsed() / 1000.f << "seconds";
    qInfo() << "- Retrieving data from background thread took" << t1 / 1000.f << "seconds";
    qInfo() << "- Creating content tree took" << (t2 - t1) / 1000.f << "seconds";
    qInfo() << "- Linking content tree took" << (t3 - t2) / 1000.f << "seconds";
}

QVariantHash DsBridgeSqlQuery::toVariantHash(const QSqlRecord& record) {
    QVariantHash hash;
    for (int i = 0; i < record.count(); ++i) {
        const QSqlField field = record.field(i);
        const QString   key   = field.name().toLower(); // Optional: normalize to lowercase
        const QVariant  value = record.value(i);        // Auto-handles type and NULL (as invalid QVariant)
        if (value.isValid() && !value.isNull()) hash[key] = value;
    }
    return hash;
}

void DsBridgeSqlQuery::queryDatabase() {
    if (mFutures.isRunning()) return; // TODO handle this better.

    // Capture main thread pointer.
    QThread* mainThread = QThread::currentThread();

    // Perform update on a background thread.
    QFuture<DatabaseContent> future = QtConcurrent::run([=]() { return queryTables(); });

    // Set the future in the watcher to track completion.
    mFutures.setFuture(future);
}

DsBridgeSqlQuery::DatabaseContent DsBridgeSqlQuery::queryTables() {
    const auto isMainThread = QThread::currentThread() == QCoreApplication::instance()->thread();

    qCInfo(lgBridgeSyncQuery) << "BridgeService is loading content"
                              << (isMainThread ? "on the main thread" : "on a background thread");

    // Create content instance.
    DatabaseContent content;

    // Get settings.
    auto appsettings = DsSettings::getSettings("app_settings");

    // Make sure the database is opened.
    DatabaseGuard guard(mDatabase);
    if (!guard.isOpen()) return content;

    QString sSlotQuery = QStringLiteral("SELECT "                   //
                                        " l.uid,"                   // 0
                                        " l.app_key,"               // 1
                                        " l.reverse_ordering"       // 2
                                        " FROM lookup AS l"         //
                                        " WHERE l.type = 'slot' "); //

    QString sRecordQuery =                                                                        //
        QStringLiteral("SELECT "                                                                  //
                       " r.uid,"                                                                  // 0
                       " r.type_uid,"                                                             // 1
                       " l.name as type_name,"                                                    // 2
                       " l.app_key as type_key,"                                                  // 3
                       " r.parent_uid,"                                                           // 4
                       " r.parent_slot,"                                                          // 5
                       " r.variant,"                                                              // 6
                       " r.name,"                                                                 // 7
                       " r.span_type,"                                                            // 8
                       " r.span_start_date,"                                                      // 9
                       " r.span_end_date,"                                                        // 10
                       " r.start_time,"                                                           // 11
                       " r.end_time,"                                                             // 12
                       " r.effective_days"                                                        // 13
                       " FROM record AS r"                                                        //
                       " INNER JOIN lookup AS l ON l.uid = r.type_uid"                            //
                       " WHERE r.complete = 1 AND r.visible = 1 AND (r.span_end_date IS NULL OR " //
                       " date(r.span_end_date, '+5 day') > date('now'))"                          //
                       " ORDER BY r.parent_slot ASC, r.rank ASC;");                               //

    QString sSelectQuery =                               //
        QStringLiteral("SELECT "                         //
                       " lookup.uid,"                    // 0
                       " lookup.app_key"                 // 1
                       " FROM lookup"                    //
                       " WHERE lookup.type = 'select'"); //

    QString sDefaultsQuery =                                                          //
        QStringLiteral("SELECT "                                                      //
                       " record.uid,"                                                 // 0
                       " defaults.field_uid,"                                         // 1
                       " lookup.app_key,"                                             // 2
                       " defaults.field_type,"                                        // 3
                       " defaults.checked,"                                           // 4
                       " defaults.color,"                                             // 5
                       " defaults.text_value,"                                        // 6
                       " defaults.rich_text,"                                         // 7
                       " defaults.number,"                                            // 8
                       " defaults.number_min,"                                        // 9
                       " defaults.number_max,"                                        // 10
                       " defaults.option_type,"                                       // 11
                       " defaults.option_value,"                                      // 12
                       " lookup.app_key AS option_key"                                // 13
                       " FROM record"                                                 //
                       " LEFT JOIN trait_map ON trait_map.type_uid = record.type_uid" //
                       " LEFT JOIN lookup ON record.type_uid = lookup.parent_uid OR trait_map.trait_uid = "
                       "lookup.parent_uid " //                                                           //
                       " LEFT JOIN defaults ON lookup.uid = defaults.field_uid"                          //
                       " WHERE EXISTS (select * from defaults where defaults.field_uid = lookup.uid);"); //

    // Get column information for the 'value' table.
    QSqlRecord record      = mDatabase.record("value");
    bool       hasDatetime = record.contains("datetime");
    bool       hasDate     = record.contains("date");
    bool       hasTime     = record.contains("time");

    // Determine the date and time field expression.
    QStringList datetimeSelect;
    if (hasDatetime) datetimeSelect.append("v.datetime");
    if (hasDate) datetimeSelect.append("v.date");
    if (hasTime) datetimeSelect.append("v.time");

    QString sValueQuery =                                      //
        QString("SELECT "                                      //
                " v.uid,"                                      // 0
                " v.field_uid,"                                // 1
                " v.record_uid,"                               // 2
                " v.field_type,"                               // 3
                " v.is_default,"                               // 4
                " v.is_empty,"                                 // 5
                " v.checked,"                                  // 6
                " v.color,"                                    // 7
                " v.text_value,"                               // 8
                " v.rich_text,"                                // 9
                " v.number,"                                   // 10
                " v.number_min,"                               // 11
                " v.number_max,"                               // 12
                " %1,"                                         // 13 date, time and/or datetime
                " v.resource_hash,"                            // 14
                " v.crop_x,"                                   // 15
                " v.crop_y,"                                   // 16
                " v.crop_w,"                                   // 17
                " v.crop_h,"                                   // 18
                " v.composite_frame,"                          // 19
                " v.composite_x,"                              // 20
                " v.composite_y,"                              // 21
                " v.composite_w,"                              // 22
                " v.composite_h,"                              // 23
                " v.option_type,"                              // 24
                " v.option_value,"                             // 25
                " v.link_url,"                                 // 26
                " v.link_target_uid,"                          // 27
                " res.hash,"                                   // 28
                " res.type,"                                   // 29
                " res.uri,"                                    // 30
                " res.width,"                                  // 31
                " res.height,"                                 // 32
                " res.duration,"                               // 33
                " res.pages,"                                  // 34
                " res.file_size,"                              // 35
                " l.name AS field_name,"                       // 36
                " l.app_key AS field_key,"                     // 37
                " v.preview_resource_hash,"                    // 38
                " preview_res.hash AS preview_hash,"           // 39
                " preview_res.type AS preview_type,"           // 40
                " preview_res.uri AS preview_uri,"             // 41
                " preview_res.width AS preview_width,"         // 42
                " preview_res.height AS preview_height,"       // 43
                " preview_res.duration AS preview_duration,"   // 44
                " preview_res.pages AS preview_pages,"         // 45
                " preview_res.file_size AS preview_file_size," // 46
                " v.hotspot_x,"                                // 47
                " v.hotspot_y,"                                // 48
                " v.hotspot_w,"                                // 49
                " v.hotspot_h,"                                // 50
                " res.filename"                                // 51
                " FROM value AS v"
                " LEFT JOIN lookup AS l ON l.uid = v.field_uid"
                " LEFT JOIN resource AS res ON res.hash = v.resource_hash"
                " LEFT JOIN resource AS preview_res ON preview_res.hash = v.preview_resource_hash;")
            .arg(datetimeSelect.join(", "));

    // Create data structures.
    DatabaseQuery slotQuery(mDatabase, sSlotQuery);
    DatabaseQuery recordQuery(mDatabase, sRecordQuery);
    DatabaseQuery selectQuery(mDatabase, sSelectQuery);
    DatabaseQuery defaultsQuery(mDatabase, sDefaultsQuery);
    DatabaseQuery valueQuery(mDatabase, sValueQuery);

    // Perform queries inside a transaction. This is very important,
    // because it effectively takes a snapshot of the current database,
    // ensuring data between queries remains consistent.
    if (!mDatabase.transaction()) {
        qCCritical(lgBridgeSyncQuery) << "Error starting transaction:" << mDatabase.lastError().text();
        return content;
    }

    // Now, perform all queries as fast as possible,
    // leaving data processing for after the transaction has finished.
    // This way, we don't block access to the database unnecessarily.
    try {
        slotQuery.execute();
        recordQuery.execute();
        selectQuery.execute();
        defaultsQuery.execute();
        valueQuery.execute();

        // Commit the transaction (see above).
        if (!mDatabase.commit()) {
            qCCritical(lgBridgeSyncQuery) << "Error committing transaction:" << mDatabase.lastError().text();
            mDatabase.rollback(); // Roll back if commit fails
            return content;
        }
    } catch (...) {
        qCWarning(lgBridgeSyncQuery) << "Unexpected error in query execution";
        mDatabase.rollback(); // Roll back on any exception
        return content;
    }

    // Process slot query.
    std::unordered_map<QString, std::pair<QString, bool>> slotReverseOrdering;
    slotQuery.process([&](const QSqlRecord& result) {
        const auto uid              = result.value("uid").toString();
        const auto app_key          = result.value("app_key").toString();
        const auto reverse_ordering = result.value("reverse_ordering").toBool();
        slotReverseOrdering[uid]    = {app_key, reverse_ordering};
    });

    // Process record query.
    recordQuery.process([&](const QSqlRecord& result) {
        const auto uid = result.value("uid").toString();

        // Create or retrieve the record.
        auto& record = content.records[uid];

        // Extract properties.
        record.insertOrAssign("uid", uid);
        record.insertOrAssign("type_uid", result.value("type_uid").toString());
        record.insertOrAssign("type_name", result.value("type_name").toString());
        record.insertOrAssign("type_key", result.value("type_key").toString());
        record.insertOrAssign("record_name", result.value("name").toString());

        // Note: parent_uid can be a list, separated by comma's. Always convert to a QStringList for convenience.
        auto parent_uids = result.value("parent_uid").toString().split(",", Qt::SkipEmptyParts);
        for (auto& parent_uid : parent_uids)
            parent_uid = parent_uid.trimmed();
        if (!parent_uids.isEmpty()) record.insertOrAssign("parent_uid", parent_uids);

        const auto parent_slot = result.value("parent_slot").toString();
        if (!parent_slot.isEmpty()) {
            record.insertOrAssign("parent_slot", parent_slot);
            record.insertOrAssign("label", slotReverseOrdering[parent_slot].first);
            record.insertOrAssign("reverse_ordered", slotReverseOrdering[parent_slot].second);
        }
        const auto variant = result.value("variant").toString();
        record.insertOrAssign("variant", variant);

        if (variant == "SCHEDULE") {
            content.events.append(uid);
            record.insertOrAssign("span_type", result.value("span_type").toString());
            record.insertOrAssign("start_date", result.value("span_start_date").toString());
            record.insertOrAssign("end_date", result.value("span_end_date").toString());
            record.insertOrAssign("start_time", result.value("start_time").toString());
            record.insertOrAssign("end_time", result.value("end_time").toString());
            record.insertOrAssign("effective_days", result.value("effective_days").toInt());
        } else if (variant == "ROOT_CONTENT") {
            content.content.append(uid);
        } else if (variant == "ROOT_PLATFORM") {
            content.platforms.append(uid);
        }

        // Store order.
        content.order.append(uid);
    });

    // Process select query.
    std::unordered_map<QString, QString> selectMap;
    selectQuery.process([&](const QSqlRecord& result) {
        selectMap[result.value("uid").toString()] = result.value("app_key").toString();
    });

    // Process defaults query.
    defaultsQuery.process([&](const QSqlRecord& result) {
        // Retrieve the record.
        const auto uid = result.value("uid").toString();
        if (!content.records.contains(uid)) return;
        auto& record = content.records[uid];

        const auto field_key = slugifyKey(result.value("app_key").toString());
        const auto field_uid = field_key.isEmpty() ? result.value("field_uid").toString() : field_key;
        const auto type      = result.value("field_type").toString();

        if (type == "TEXT") {
            record.insertOrAssign(field_uid, result.value("text_value").toString());
        } else if (type == "RICH_TEXT") {
            record.insertOrAssign(field_uid, result.value("rich_text").toString());
        } else if (type == "NUMBER") {
            record.insertOrAssign(field_uid, result.value("number").toFloat());
        } else if (type == "OPTIONS") {
            const auto  key   = result.value("option_value").toString();
            const auto& value = selectMap.at(key);
            record.insertOrAssign(field_uid, value);
            record.insertOrAssign(field_uid + "_value_uid", key);
        } else if (type == "CHECKBOX") {
            record.insertOrAssign(field_uid, result.value("checked").toBool());
        }

        record.insertOrAssign(field_uid + "_field_uid", result.value("field_uid").toString());
    });

    // Process value query.
    const dsqt::DsResource::Id cms(dsqt::DsResource::Id::CMS_TYPE, 0);
    valueQuery.process([&](const QSqlRecord& result) {
        // Retrieve the record to which this value belongs.
        const auto uid = result.value("record_uid").toString();
        if (!content.records.contains(uid)) return;
        auto& record = content.records[uid];

        // Determine field key.
        const auto field_key = slugifyKey(result.value("field_key").toString());
        const auto field_uid = field_key.isEmpty() ? result.value("field_uid").toString() : field_key;

        // Process value based on type.
        const auto field_type = result.value("field_type").toString();
        if (field_type == "TEXT") {
            // Use the provided text.
            record.insertOrAssign(field_uid, result.value("text_value").toString());
        } else if (field_type == "RICH_TEXT") {
            // Use the provided rich text.
            record.insertOrAssign(field_uid, result.value("rich_text").toString());
        } else if (field_type == "FILE_IMAGE" || field_type == "FILE_VIDEO" || field_type == "FILE_PDF") {
            // Create resource.
            const auto resourceId = result.value("hash").toString();
            if (!resourceId.isEmpty()) {
                const auto uri = result.value("uri").toString();

                auto res = dsqt::DsResource(
                    resourceId, dsqt::DsResource::Id::CMS_TYPE, double(result.value("duration").toDouble()),
                    float(result.value("width").toInt()), float(result.value("height").toInt()),
                    result.value("filename").toString(), uri, -1, cms.getResourcePath() + "/" + uri);

                // Set resource type.
                if (field_type == "FILE_IMAGE")
                    res.setType(dsqt::DsResource::IMAGE_TYPE);
                else if (field_type == "FILE_VIDEO")
                    res.setType(dsqt::DsResource::VIDEO_TYPE);
                else if (field_type == "FILE_PDF")
                    res.setType(dsqt::DsResource::PDF_TYPE);
                else {
                    qWarning() << "Unknown file type for resource:" << field_type;
                    return;
                }

                // Set crop parameters if applicable.
                auto cropX = result.value("crop_x").toFloat();
                auto cropY = result.value("crop_y").toFloat();
                auto cropW = result.value("crop_w").toFloat();
                auto cropH = result.value("crop_h").toFloat();
                if (cropX > 0.f || cropY > 0.f || cropW > 0.f || cropH > 0.f) {
                    res.setCrop(cropX, cropY, cropW, cropH);
                }

                // Add resource. TODO getQml() should contain all fields. Perhaps replace DsResource with a type
                // compatible with QVariant.
                record.insertOrAssign(field_uid, res.toQml());

                // Create resource for the preview.
                const auto previewId = result.value("preview_resource_hash").toString();
                if (!previewId.isEmpty()) {
                    const auto previewUri  = result.value("preview_uri").toString();
                    const auto previewType = result.value("preview_type").toString();

                    auto res = dsqt::DsResource(
                        previewId, dsqt::DsResource::Id::CMS_TYPE, double(result.value("preview_duration").toFloat()),
                        float(result.value("preview_width").toInt()), float(result.value("preview_height").toInt()),
                        result.value("filename").toString(), previewUri, -1, cms.getResourcePath() + "/" + previewUri);

                    res.setType(previewType == "FILE_IMAGE" ? dsqt::DsResource::IMAGE_TYPE
                                                            : dsqt::DsResource::VIDEO_TYPE);

                    // Add resource for the preview. TODO getQml() should contain all fields. Perhaps replace DsResource
                    // with a type compatible with QVariant.
                    record.insertOrAssign(field_uid + "_preview", res.toQml());
                }
            }
        } else if (field_type == "LINKS") {
            // Add link.
            auto update = record.value(field_uid).toStringList();
            update.append(result.value("link_target_uid").toString());
            record.insertOrAssign(field_uid, update);
        } else if (field_type == "LINK_WEB") {
            const auto linkUrl = result.value("uri").toString();
            if (!linkUrl.isEmpty()) {
                // Create resource.
                auto res =
                    dsqt::DsResource(linkUrl, dsqt::DsResource::Id::CMS_TYPE,
                                     double(result.value("duration").toFloat()), float(result.value("width").toInt()),
                                     float(result.value("height").toInt()), linkUrl, linkUrl, -1, linkUrl);

                // Assumes app settings are not changed concurrently on another thread.
                auto webSize = appsettings->getOr<glm::vec2>("web:default_size", glm::vec2(1920.f, 1080.f));
                res.setWidth(webSize.x);
                res.setHeight(webSize.y);
                res.setType(dsqt::DsResource::WEB_TYPE);
                record.insertOrAssign(field_uid, res.toQml());

                // Create resource for the preview.
                const auto previewId = result.value("preview_resource_hash").toString();
                if (!previewId.isEmpty()) {
                    const auto previewUri  = result.value("preview_uri").toString();
                    const auto previewType = result.value("preview_type").toString();

                    auto res = dsqt::DsResource(
                        previewId, dsqt::DsResource::Id::CMS_TYPE, double(result.value("preview_duration").toFloat()),
                        float(result.value("preview_width").toInt()), float(result.value("preview_height").toInt()),
                        result.value("filename").toString(), previewUri, -1, cms.getResourcePath() + previewUri);
                    res.setType(previewType == "FILE_IMAGE" ? dsqt::DsResource::IMAGE_TYPE
                                                            : dsqt::DsResource::VIDEO_TYPE);

                    record.insertOrAssign(field_uid + "_preview", res.toQml());
                }
            }
        } else if (field_type == "NUMBER") {
            record.insertOrAssign(field_uid, result.value("number").toFloat());
        } else if (field_type == "COMPOSITE_AREA") {
            const auto frameUid = result.value("composite_frame").toString();
            record.insertOrAssign(frameUid + "_x", result.value("composite_x").toFloat());
            record.insertOrAssign(frameUid + "_y", result.value("composite_y").toFloat());
            record.insertOrAssign(frameUid + "_w", result.value("composite_w").toFloat());
            record.insertOrAssign(frameUid + "_h", result.value("composite_h").toFloat());
        } else if (field_type == "IMAGE_AREA" || field_type == "IMAGE_SPOT") {
            record.insertOrAssign("hotspot_x", result.value("hotspot_x").toFloat());
            record.insertOrAssign("hotspot_y", result.value("hotspot_y").toFloat());
            record.insertOrAssign("hotspot_w", result.value("hotspot_w").toFloat());
            record.insertOrAssign("hotspot_h", result.value("hotspot_h").toFloat());
        } else if (field_type == "OPTIONS") {
            // this should be the following
            const auto key = result.value("option_value").toString();
            if (selectMap.count(key) > 0) {
                record.insertOrAssign(field_uid, selectMap[key]);
                record.insertOrAssign(field_uid + "_value_uid", key);
            } else {
                // TODO log warning
            }
        } else if (field_type == "CHECKBOX") {
            record.insertOrAssign(field_uid, result.value("checked").toBool());
        } else if (field_type == "DATE_TIME") {
            QDateTime datetime = QDateTime::currentDateTime();
            if (hasDatetime) datetime = result.value("datetime").toDateTime();
            if (hasDate && !result.value("date").isNull()) datetime.setDate(result.value("date").toDate());
            if (hasTime && !result.value("time").isNull()) datetime.setTime(result.value("time").toTime());
            record.insertOrAssign(field_uid, datetime);
        } else {
            qWarning() << "Invalid record type" << field_type;
        }
        record.insertOrAssign(field_uid + "_field_uid", result.value("field_uid").toString());
    });

    // Build content tree.
    content.buildTree();

    return content;
}

QString DsBridgeSqlQuery::slugifyKey(QString appKey) {
    static QRegularExpression badRe("\\W|^(?=\\d)");
    return appKey.replace(badRe, "_");
}

BridgeSyncProcessGuard::BridgeSyncProcessGuard(QProcess& process)
    : mProcess(process) {
#ifdef Q_OS_WIN
    // Create a Job Object
    mJobHandle = CreateJobObject(nullptr, nullptr);
    if (!mJobHandle) {
        qCWarning(lgBridgeSyncApp) << "Failed to create Job Object: " << GetLastError();
    } else {
        // Configure Job Object to terminate processes on close.
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = {0};
        info.BasicLimitInformation.LimitFlags     = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!SetInformationJobObject(mJobHandle, JobObjectExtendedLimitInformation, &info, sizeof(info))) {
            qCWarning(lgBridgeSyncApp) << "Failed to set Job Object info: " << GetLastError();
            CloseHandle(mJobHandle);
            mJobHandle = nullptr;
        }
    }
#endif
    // Start the process.
    mProcess.start();
    if (!mProcess.waitForStarted()) {
        qCWarning(lgBridgeSyncApp) << "Failed to start BridgeSync: " << mProcess.errorString();
    }

#ifdef Q_OS_WIN
    if (mJobHandle) {
        mProcessHandle =
            OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_SET_QUOTA | PROCESS_TERMINATE | PROCESS_SUSPEND_RESUME,
                        FALSE, mProcess.processId());

        // Assign process to Job Object
        if (!mProcessHandle || !AssignProcessToJobObject(mJobHandle, mProcessHandle)) {
            qCWarning(lgBridgeSyncApp) << "Failed to assign process to Job Object: " << GetLastError();
        }
    }
#endif
}

BridgeSyncProcessGuard::~BridgeSyncProcessGuard() {
    if (mProcess.state() == QProcess::Running) {
        mProcess.terminate();
        if (!mProcess.waitForFinished(5000)) {
            mProcess.kill();
            mProcess.waitForFinished();
        }
        mProcess.close();
    }

#ifdef Q_OS_WIN
    CloseHandle(mProcessHandle);
    CloseHandle(mJobHandle);
#endif
}

DatabaseGuard::DatabaseGuard(QSqlDatabase& database)
    : mDatabase(database)
    , mWasOpen(database.isOpen()) {
    if (mWasOpen) {
        qCDebug(lgBridgeSyncQuery) << "Database was already open, DatabaseGuard will not close it";
        return;
    }

    // Open the database
    qCInfo(lgBridgeSyncQuery) << "Opening database";
    if (!mDatabase.open()) {
        qCWarning(lgBridgeSyncQuery) << "Could not open database at " << mDatabase.databaseName();
        return;
    }

    // // Verify WAL mode
    // QSqlQuery pragmaQuery(mDatabase);
    // if (pragmaQuery.exec("PRAGMA journal_mode;") && pragmaQuery.next()) {
    //     QString journalMode = pragmaQuery.value(0).toString();
    //     if (journalMode.toLower() != "wal") {
    //         qCDebug(lgBridgeSyncQuery) << "Warning: Database is not in WAL mode, current mode:" << journalMode;
    //     } else {
    //         qCDebug(lgBridgeSyncQuery) << "Database is in WAL mode";
    //     }
    // } else {
    //     qCWarning(lgBridgeSyncQuery) << "Error checking journal mode:" << pragmaQuery.lastError().text();
    // }

    // // Set busy timeout
    // if (!pragmaQuery.exec("PRAGMA busy_timeout=5000;")) {
    //     qCWarning(lgBridgeSyncQuery) << "Error setting busy timeout:" << pragmaQuery.lastError().text();
    // }
}

DatabaseGuard::~DatabaseGuard() {
    // Only close the database if it was not already open when constructed
    if (!mWasOpen && mDatabase.isOpen()) {
        qCInfo(lgBridgeSyncQuery) << "Closing database";
        mDatabase.close();
    }
}

DatabaseGuard::DatabaseGuard(DatabaseGuard&& other) noexcept
    : mDatabase(other.mDatabase)
    , mWasOpen(other.mWasOpen) {
    other.mWasOpen = true; // Prevent the moved-from object from closing the database
}

DatabaseGuard& DatabaseGuard::operator=(DatabaseGuard&& other) noexcept {
    if (this != &other) {
        // Close our database if it was opened by this instance
        if (!mWasOpen && mDatabase.isOpen()) {
            qCInfo(lgBridgeSyncQuery) << "Closing database in move assignment";
            mDatabase.close();
        }
        mDatabase      = other.mDatabase;
        mWasOpen       = other.mWasOpen;
        other.mWasOpen = true; // Prevent the moved-from object from closing the database
    }
    return *this;
}

bool DatabaseGuard::isOpen() const {
    return mDatabase.isOpen();
}

QSqlDatabase& DatabaseGuard::database() {
    return mDatabase;
}

bool DatabaseQuery::execute() {
    Timer t("Executing " + mLabel);
    mQuery.exec();
    if (mQuery.lastError().isValid()) {
        qCCritical(lgBridgeSyncQuery) << "Error:" << mQuery.lastError().text();
        return false;
    }
    return true;
}

void DatabaseQuery::process(const std::function<void(const QSqlRecord&)>& callback) {
    Timer t("Processing " + mLabel);
    while (mQuery.next()) {
        callback(mQuery.record());
    }
}

} // namespace dsqt::bridge
