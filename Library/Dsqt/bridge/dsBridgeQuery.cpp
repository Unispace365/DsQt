#include "dsBridgeQuery.h"

#include <QSharedPointer>
#include <QString>

#include "core/dsenvironment.h"
#include "model/content_model.h"
#include "model/dsresource.h"
#include "model/property_map_diff.h"
#include "network/dsnodewatcher.h"
#include "qqmlcontext.h"
#include "settings/dssettings.h"
#include "settings/dssettings_proxy.h"

Q_LOGGING_CATEGORY(lgBridgeSyncApp, "bridgeSync.app")
Q_LOGGING_CATEGORY(lgBridgeSyncQuery, "bridgeSync.query")
Q_LOGGING_CATEGORY(lgBridgeSyncAppVerbose, "bridgeSync.app.verbose")
Q_LOGGING_CATEGORY(lgBridgeSyncQueryVerbose, "bridgeSync.query.verbose")

using namespace dsqt::model;
namespace dsqt::bridge {

DsBridgeSqlQuery::DsBridgeSqlQuery(DSQmlApplicationEngine* parent)
    : QObject(parent) {
    using namespace Qt::StringLiterals;

    connect(
        parent, &DSQmlApplicationEngine::onInit, this,
        [this]() {
            // If bridge sync is not running, try to launch it.
            if (!isBridgeSyncRunning()) {
                tryLaunchBridgeSync();
            }

            // Setup the connections first.
            auto engine = DSQmlApplicationEngine::DefEngine();
            connect(
                engine->getNodeWatcher(), &network::DsNodeWatcher::messageArrived, this,
                [this](dsqt::network::Message msg) { QueryDatabase(); }, Qt::ConnectionType::QueuedConnection);

            connect(
                this, &DsBridgeSqlQuery::syncCompleted, engine,
                [engine](QSharedPointer<PropertyMapDiff> diff) { engine->updateContentRoot(diff); },
                Qt::ConnectionType::QueuedConnection);

            //
            qCDebug(lgBridgeSyncQuery) << "Starting Query";
            mDatabase = QSqlDatabase::addDatabase("QSQLITE");
            DSSettingsProxy engSettings;
            engSettings.setTarget("engine");
            engSettings.setPrefix("engine.resource");

            QString resourceLocation = DSEnvironment::expandq(engSettings.getString("location", "").value<QString>());
            auto    opFile           = engSettings.getString("resource_db", "").value<QString>();
            if (opFile != "") {
                // Find DB file.
                auto file = DSEnvironment::expandq(opFile);
                if (QDir::isRelativePath(file)) {
                    file = QDir::cleanPath(resourceLocation + "/" + file);
                }

                // Open DB file in read-only mode.
                mDatabase.setDatabaseName(file);
                mDatabase.setConnectOptions("QSQLITE_OPEN_READONLY");

                if (!mDatabase.open()) {
                    qCWarning(lgBridgeSyncQuery) << "Could not open database at " << file;
                } else {
                    // Verify WAL mode (optional, for debugging).
                    QSqlQuery pragmaQuery(mDatabase);
                    if (pragmaQuery.exec("PRAGMA journal_mode;") && pragmaQuery.next()) {
                        QString journalMode = pragmaQuery.value(0).toString();
                        if (journalMode.toLower() != "wal") {
                            qCDebug(lgBridgeSyncQuery)
                                << "Warning: Database is not in WAL mode, current mode:" << journalMode;

                            // Either BridgeSync console is an older version using DELETE mode,
                            // the BridgeSync console is not running, or it has no open connection to the database.
                        } else {
                            qCDebug(lgBridgeSyncQuery) << "Database is in WAL mode";
                        }
                    } else {
                        qCDebug(lgBridgeSyncQuery) << "Error checking journal mode:" << pragmaQuery.lastError().text();
                    }

                    // Set busy timeout to handle potential contention (e.g., during checkpoints).
                    if (!pragmaQuery.exec("PRAGMA busy_timeout=5000;")) {
                        qCDebug(lgBridgeSyncQuery) << "Error setting busy timeout:" << pragmaQuery.lastError().text();
                    }

                    // Fake a nodewatcher message.
                    emit DSQmlApplicationEngine::DefEngine()->getNodeWatcher()->messageArrived(
                        dsqt::network::Message());
                }
            }
        },
        Qt::ConnectionType::QueuedConnection);
}

DsBridgeSqlQuery::~DsBridgeSqlQuery() {
    // Properly close the DB file.
    qCInfo(lgBridgeSyncApp) << "Closing database";
    mDatabase.close();

#ifndef Q_OS_WASM
    // Properly stop the BridgeSync process.
    qCInfo(lgBridgeSyncApp) << "Closing BridgeSync";
    stopBridgeSync();
#endif
}

DsBridgeSyncSettings DsBridgeSqlQuery::getBridgeSyncSettings() {
    DsBridgeSyncSettings settings;
    DSSettingsProxy      engSettings;
    engSettings.setTarget("engine");
    engSettings.setPrefix("engine.bridgesync");
    auto launchBridgeSync = engSettings.getBool("launch_bridgesync", false);
    auto appPath          = engSettings.getString("app_path", "%SHARE%/bridgesync/bridge_sync_console.exe").toString();

    settings.server       = engSettings.getString("connection.server");
    settings.authServer   = engSettings.getString("connection.auth_server");
    settings.clientId     = engSettings.getString("connection.client_id");
    settings.clientSecret = engSettings.getString("connection.client_secret");
    settings.directory    = engSettings.getString("connection.directory");
    settings.interval     = engSettings.getInt("connection.interval", 10);
    settings.verbose      = engSettings.getBool("connection.verbose", false);
    settings.asyncRecords = engSettings.getBool("connection.asyncRecords", true);

    settings.appPath  = DSEnvironment::expandq(appPath);
    settings.doLaunch = launchBridgeSync.toBool();
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
    const auto appPath = DSEnvironment::expandq(bridgeSyncSettings.appPath);
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
        args << "--directory" << DSEnvironment::expandq(bridgeSyncSettings.directory.toString());
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
        qCInfo(lgBridgeSyncApp) << mBridgeSyncProcess.readAllStandardOutput();
        // auto byteArray = mBridgeSyncProcess.readAllStandardOutput();
        // QString output(byteArray);
        // qDebug()<<byteArray;
    }));
    mConnections.append(connect(&mBridgeSyncProcess, &QProcess::readyReadStandardError, this, [this]() {
        qCWarning(lgBridgeSyncApp) << mBridgeSyncProcess.readAllStandardError();
        // auto byteArray = mBridgeSyncProcess.readAllStandardError();
        // QString output(byteArray);
        // qDebug()<<byteArray;
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

void DsBridgeSqlQuery::QueryDatabase() {
    // invalidate qml versions of models.
    // queryTables will set used models to valid.
    // invalidate will skip any model that doesn't have the appEngine
    // as a parent

    ReferenceMap tempRefMap;
    tempRefMap.isTemp         = true;
    ContentModelRef  root     = DSQmlApplicationEngine::DefEngine()->getContentRoot();
    QmlContentModel* preModel = root.getQml(&tempRefMap, nullptr);

    QElapsedTimer timer;
    timer.start();
    queryTables();
    qDebug() << "QueryTables() took" << timer.elapsed() << "milliseconds";

    ReferenceMap tempRefMap2;
    tempRefMap2.isTemp         = true;
    root                       = DSQmlApplicationEngine::DefEngine()->getContentRoot();
    QmlContentModel* postModel = root.getQml(&tempRefMap2, nullptr);

    QSharedPointer<PropertyMapDiff> diff = QSharedPointer<PropertyMapDiff>::create(*preModel, *postModel);
    emit syncCompleted(diff);
}

bool DsBridgeSqlQuery::handleQueryError(const QSqlQuery& query, const QString& queryName) {
    if (query.lastError().isValid()) {
        qCCritical(lgBridgeSyncQuery) << queryName << " Error: " << query.lastError().text();
        return false;
    }
    return true;
}

void DsBridgeSqlQuery::queryTables() {
    qCInfo(lgBridgeSyncQuery) << "BridgeService is loading content.";

    // Get settings.
    auto appsettings = DSSettings::getSettings("app_settings");

    // Create query objects.
    QSqlQuery slotQuery(mDatabase);
    slotQuery.setForwardOnly(true);
    QSqlQuery recordQuery(mDatabase);
    recordQuery.setForwardOnly(true);
    QSqlQuery selectQuery(mDatabase);
    selectQuery.setForwardOnly(true);
    QSqlQuery defaultsQuery(mDatabase);
    defaultsQuery.setForwardOnly(true);
    QSqlQuery valueQuery(mDatabase);
    valueQuery.setForwardOnly(true);

    // Perform queries inside a transaction. This is very important,
    // because it effectively takes a snapshot of the current database,
    // ensuring data between queries remains consistent.
    if (!mDatabase.transaction()) {
        qCCritical(lgBridgeSyncQuery) << "Error starting transaction:" << mDatabase.lastError().text();
        return;
    }

    // Now, perform all queries as fast as possible,
    // leaving data processing for after the transaction has finished.
    // This way, we don't block access to the database unnecessarily.
    QElapsedTimer timer;

    try {
        QString query = "SELECT "             //
                        " l.uid,"             // 0
                        " l.app_key,"         // 1
                        " l.reverse_ordering" // 2
                        " FROM lookup AS l"
                        " WHERE l.type = 'slot' ";

        timer.start();
        slotQuery.exec(query);
        qDebug() << "slotQuery took" << timer.elapsed() << "milliseconds";

        query = "SELECT "                 //
                " r.uid,"                 // 0
                " r.type_uid,"            // 1
                " l.name as type_name,"   // 2
                " l.app_key as type_key," // 3
                " r.parent_uid,"          // 4
                " r.parent_slot,"         // 5
                " r.variant,"             // 6
                " r.name,"                // 7
                " r.span_type,"           // 8
                " r.span_start_date,"     // 9
                " r.span_end_date,"       // 10
                " r.start_time,"          // 11
                " r.end_time,"            // 12
                " r.effective_days"       // 13
                " FROM record AS r"
                " INNER JOIN lookup AS l ON l.uid = r.type_uid"
                " WHERE r.complete = 1 AND r.visible = 1 AND (r.span_end_date IS NULL OR "
                " date(r.span_end_date, '+5 day') > date('now'))"
                " ORDER BY r.parent_slot ASC, r.rank ASC;";

        timer.start();
        recordQuery.exec(query);
        qDebug() << "recordQuery took" << timer.elapsed() << "milliseconds";

        query = "SELECT "
                " lookup.uid,"    // 0
                " lookup.app_key" // 1
                " FROM lookup"
                " WHERE lookup.type = 'select'";

        timer.start();
        selectQuery.exec(query);
        qDebug() << "selectQuery took" << timer.elapsed() << "milliseconds";

        query = "SELECT "
                " record.uid,"                  // 0
                " defaults.field_uid,"          // 1
                " lookup.app_key,"              // 2
                " defaults.field_type,"         // 3
                " defaults.checked,"            // 4
                " defaults.color,"              // 5
                " defaults.text_value,"         // 6
                " defaults.rich_text,"          // 7
                " defaults.number,"             // 8
                " defaults.number_min,"         // 9
                " defaults.number_max,"         // 10
                " defaults.option_type,"        // 11
                " defaults.option_value,"       // 12
                " lookup.app_key AS option_key" // 13
                " FROM record"
                " LEFT JOIN trait_map ON trait_map.type_uid = record.type_uid"
                " LEFT JOIN lookup ON record.type_uid = lookup.parent_uid OR trait_map.trait_uid = "
                "lookup.parent_uid "
                " LEFT JOIN defaults ON lookup.uid = defaults.field_uid"
                " WHERE EXISTS (select * from defaults where defaults.field_uid = lookup.uid);";

        timer.start();
        defaultsQuery.exec(query);
        qDebug() << "defaultsQuery took" << timer.elapsed() << "milliseconds";

        query = "SELECT "
                " v.uid,"                   // 0
                " v.field_uid,"             // 1
                " v.record_uid,"            // 2
                " v.field_type,"            // 3
                " v.is_default,"            // 4
                " v.is_empty,"              // 5
                " v.checked,"               // 6
                " v.color,"                 // 7
                " v.text_value,"            // 8
                " v.rich_text,"             // 9
                " v.number,"                // 10
                " v.number_min,"            // 11
                " v.number_max,"            // 12
                " v.datetime,"              // 13
                " v.resource_hash,"         // 14
                " v.crop_x,"                // 15
                " v.crop_y,"                // 16
                " v.crop_w,"                // 17
                " v.crop_h,"                // 18
                " v.composite_frame,"       // 19
                " v.composite_x,"           // 20
                " v.composite_y,"           // 21
                " v.composite_w,"           // 22
                " v.composite_h,"           // 23
                " v.option_type,"           // 24
                " v.option_value,"          // 25
                " v.link_url,"              // 26
                " v.link_target_uid,"       // 27
                " res.hash,"                // 28
                " res.type,"                // 29
                " res.uri,"                 // 30
                " res.width,"               // 31
                " res.height,"              // 32
                " res.duration,"            // 33
                " res.pages,"               // 34
                " res.file_size,"           // 35
                " l.name AS field_name,"    // 36
                " l.app_key AS field_key,"  // 37
                " v.preview_resource_hash," // 38
                " preview_res.hash,"        // 39
                " preview_res.type,"        // 40
                " preview_res.uri,"         // 41
                " preview_res.width,"       // 42
                " preview_res.height,"      // 43
                " preview_res.duration,"    // 44
                " preview_res.pages,"       // 45
                " preview_res.file_size,"   // 46
                " v.hotspot_x,"             // 47
                " v.hotspot_y,"             // 48
                " v.hotspot_w,"             // 49
                " v.hotspot_h,"             // 50
                " res.filename"             // 51
                " FROM value AS v"
                " LEFT JOIN lookup AS l ON l.uid = v.field_uid"
                " LEFT JOIN resource AS res ON res.hash = v.resource_hash"
                " LEFT JOIN resource AS preview_res ON preview_res.hash = v.preview_resource_hash;";

        timer.start();
        valueQuery.exec(query);
        qDebug() << "valueQuery took" << timer.elapsed() << "milliseconds";

        // Commit the transaction (see above).
        if (!mDatabase.commit()) {
            qCCritical(lgBridgeSyncQuery) << "Error committing transaction:" << mDatabase.lastError().text();
            mDatabase.rollback(); // Roll back if commit fails
            return;
        }
    } catch (...) {
        qCWarning(lgBridgeSyncQuery) << "Unexpected error in query execution";
        mDatabase.rollback(); // Roll back on any exception
        return;
    }

    //
    model::ContentModelRef content("content"); // this is the content placed in a tree
    model::ContentModelRef platforms("platforms");
    model::ContentModelRef platform("platform");   // this is the matched platform
    model::ContentModelRef events("all_events");   // this is all the event records
    model::ContentModelRef records("all_records"); // this is all the content records in a flat list

    // Process slot query.
    timer.start();

    std::unordered_map<QString, std::pair<QString, bool>> slotReverseOrderingMap;
    if (handleQueryError(slotQuery, "Slot Query")) {
        while (slotQuery.next()) {
            auto result = slotQuery.record();
            bool ok_val2;
            auto uid                    = result.value(0).toString();
            auto app_key                = result.value(1).toString();
            auto reverse_ordering       = result.value(2).toInt(&ok_val2);
            slotReverseOrderingMap[uid] = {app_key, reverse_ordering};
        }
    }

    qDebug() << "Processing slotQuery took" << timer.elapsed() << "milliseconds";

    // Process record query.
    timer.start();

    std::vector<ContentModelRef>                 rankOrderedRecords;
    std::unordered_map<QString, ContentModelRef> recordMap;
    if (handleQueryError(recordQuery, "Record Query")) {
        if (recordQuery.size() == 0) {
            qCWarning(lgBridgeSyncQuery) << "No records were found in database ";
        }
        while (recordQuery.next()) {
            auto            result = recordQuery.record();
            ContentModelRef record(result.value(7).toString() + "(" + result.value(0).toString() + ")");
            auto val = QCryptographicHash::hash(result.value(0).toString().toUtf8(), QCryptographicHash::Md5);
            record.setId(result.value(0).toString());
            if (record.getId() == "CniWJvnMsq3d") {
                qDebug() << "found it";
            }

            record.setProperty("record_name", result.value(7).toString());
            record.setProperty("uid", result.value(0).toString());
            record.setProperty("type_uid", result.value(1).toString());
            record.setProperty("type_name", result.value(2).toString());
            record.setProperty("type_key", result.value(3).toString());
            if (!result.value(4).toString().isEmpty()) record.setProperty("parent_uid", result.value(4).toString());
            if (!result.value(5).toString().isEmpty()) {
                record.setProperty("parent_slot", result.value(5).toString());
                record.setProperty("label", slotReverseOrderingMap[result.value(5).toString()].first);
                record.setProperty("reverse_ordered", slotReverseOrderingMap[result.value(5).toString()].second);
            }

            const auto& variant = result.value(6).toString();
            record.setProperty("variant", variant);
            if (variant == "SCHEDULE") {
                record.setProperty("span_type", result.value(8).toString());
                record.setProperty("start_date", result.value(9).toString());
                record.setProperty("end_date", result.value(10).toString());
                record.setProperty("start_time", result.value(11).toString());
                record.setProperty("end_time", result.value(12).toString());
                record.setProperty("effective_days", result.value(13).toInt());
            }

            // Put it in the map so we can wire everything up into the tree
            recordMap[result.value(0).toString()] = record;

            rankOrderedRecords.push_back(record);
            //++it;
        }

        for (const auto& record : rankOrderedRecords) {
            records.addChild(record);
            auto type = record.getPropertyString("variant");
            if (type == "ROOT_CONTENT") {
                content.addChild(record);
            } else if (type == "ROOT_PLATFORM") {
                if (record.getPropertyString("uid") == appsettings->getOr("platform.id", QString(""))) {
                    platform.addChild(record);
                }
                platforms.addChild(record);
            } else if (type == "SCHEDULE") {
                auto list = record.getPropertyString("parent_uid").split(",", Qt::SkipEmptyParts);
                for (const auto& parentUid : std::as_const(list)) {
                    // Create/Add the event to its specific platform
                    auto platformEvents = recordMap[parentUid].getChildByName("scheduled_events");
                    platformEvents.setName("scheduled_events");
                    platformEvents.addChild(record);
                    recordMap[parentUid].replaceChild(platformEvents);
                }

                // Also add to the 'all_events' table in case an application needs events not specifically
                // assigned to it
                events.addChild(record);
            } else if (type == "RECORD") {
                // it's has a record for a parent!
                recordMap[record.getPropertyString("parent_uid")].addChild(record);
            } else {
                qCInfo(lgBridgeSyncApp) << "Found unknown record variant of: " << type;
            }
        }
    }

    qDebug() << "Processing recordQuery took" << timer.elapsed() << "milliseconds";

    // Process select query.
    timer.start();

    std::unordered_map<QString, QString> selectMap;
    if (handleQueryError(selectQuery, "Select Query")) {
        while (selectQuery.next()) {
            auto result                           = selectQuery.record();
            selectMap[result.value(0).toString()] = result.value(1).toString();
        }
    }

    qDebug() << "Processing selectQuery took" << timer.elapsed() << "milliseconds";

    // Process defaults query.
    timer.start();

    if (handleQueryError(defaultsQuery, "Defaults Query")) {
        while (defaultsQuery.next()) {
            auto result    = defaultsQuery.record();
            auto recordUid = result.value(0).toString();
            if (recordMap.find(recordUid) == recordMap.end()) {
                continue;
            }
            auto& record = recordMap[recordUid];

            auto field_uid = result.value(1).toString();
            auto field_key = slugifyKey(result.value(2).toString());
            if (!field_key.isEmpty()) {
                field_uid = field_key;
            }
            auto type = result.value(3).toString();

            if (type == "TEXT") {
                record.setProperty(field_uid, result.value(6).toString());
            } else if (type == "RICH_TEXT") {
                record.setProperty(field_uid, result.value(7).toString());
            } else if (type == "NUMBER") {
                record.setProperty(field_uid, result.value(8).toFloat());
            } else if (type == "OPTIONS") {
                auto key   = result.value(12).toString();
                auto value = selectMap[key];
                record.setProperty(field_uid, value);
                record.setProperty(field_uid + "_value_uid", key);
            } else if (type == "CHECKBOX") {
                record.setProperty(field_uid, bool(result.value(4).toInt()));
            } else {
                qCWarning(lgBridgeSyncApp) << "UNHANDLED(Defaults): " << type;
            }
            record.setProperty(field_uid + "_field_uid", result.value(1).toString());
        }
    }

    qDebug() << "Processing defaultsQuery took" << timer.elapsed() << "milliseconds";

    // Process value query.
    timer.start();

    const dsqt::DSResource::Id cms(dsqt::DSResource::Id::CMS_TYPE, 0);
    if (handleQueryError(valueQuery, "Value Query")) {
        while (valueQuery.next()) {
            auto        result    = valueQuery.record();
            const auto& recordUid = result.value(2).toString();
            if (recordMap.find(recordUid) == recordMap.end()) {
                continue;
            }
            auto& record = recordMap[recordUid];

            auto field_uid = result.value(1).toString();
            auto field_key = slugifyKey(result.value(37).toString());
            if (!field_key.isEmpty()) {
                field_uid = field_key;
            }
            auto type       = result.value(3).toString();
            auto resourceId = result.value(28).toString();
            auto previewId  = result.value(38).toString();
            if (type == "TEXT") {
                record.setProperty(field_uid, result.value(8).toString());
            } else if (type == "RICH_TEXT") {
                record.setProperty(field_uid, result.value(9).toString());
            } else if (type == "FILE_IMAGE" || type == "FILE_VIDEO" || type == "FILE_PDF") {
                if (!resourceId.isEmpty()) {
                    // TODO: finish converting this section.
                    auto res = dsqt::DSResource(
                        resourceId, dsqt::DSResource::Id::CMS_TYPE, double(result.value(33).toDouble()),
                        float(result.value(31).toInt()), float(result.value(32).toInt()), result.value(51).toString(),
                        result.value(30).toString(), -1, cms.getResourcePath() + result.value(30).toString());
                    if (type == "FILE_IMAGE")
                        res.setType(dsqt::DSResource::IMAGE_TYPE);
                    else if (type == "FILE_VIDEO")
                        res.setType(dsqt::DSResource::VIDEO_TYPE);
                    else if (type == "FILE_PDF")
                        res.setType(dsqt::DSResource::PDF_TYPE);
                    else
                        continue;

                    auto cropX = result.value(15).toFloat();
                    auto cropY = result.value(16).toFloat();
                    auto cropW = result.value(17).toFloat();
                    auto cropH = result.value(18).toFloat();
                    if (cropX > 0.f || cropY > 0.f || cropW > 0.f || cropH > 0.f) {
                        res.setCrop(cropX, cropY, cropW, cropH);
                    }

                    record.setPropertyResource(field_uid, res);
                }
                // handle preview

                if (!previewId.isEmpty()) {
                    const auto& previewType = result.value(40).toString();

                    auto res = dsqt::DSResource(
                        previewId, dsqt::DSResource::Id::CMS_TYPE, double(result.value(44).toFloat()),
                        float(result.value(42).toInt()), float(result.value(43).toInt()), result.value(41).toString(),
                        result.value(41).toString(), -1, cms.getResourcePath() + result.value(41).toString());
                    res.setType(previewType == "FILE_IMAGE" ? dsqt::DSResource::IMAGE_TYPE
                                                            : dsqt::DSResource::VIDEO_TYPE);

                    auto preview_uid = field_uid + "_preview";
                    record.setPropertyResource(preview_uid, res);
                }

            } else if (type == "LINKS") {
                auto toUpdate = record.getPropertyString(field_uid);
                if (!toUpdate.isEmpty()) {
                    toUpdate.append(", ");
                }
                toUpdate.append(result.value(27).toString());
                record.setProperty(field_uid, toUpdate);
            } else if (type == "LINK_WEB") {
                const auto& linkUrl = result.value(30).toString();
                auto        res     = dsqt::DSResource(resourceId, dsqt::DSResource::Id::CMS_TYPE,
                                                       double(result.value(33).toFloat()), float(result.value(31).toInt()),
                                                       float(result.value(32).toInt()), linkUrl, linkUrl, -1, linkUrl);

                // Assumes app settings are not changed concurrently on another thread.
                auto webSize = appsettings->getOr<glm::vec2>("web:default_size", glm::vec2(1920.f, 1080.f));
                res.setWidth(webSize.x);
                res.setHeight(webSize.y);
                res.setType(dsqt::DSResource::WEB_TYPE);
                record.setPropertyResource(field_uid, res);

                if (!previewId.isEmpty()) {
                    const auto& previewType = result.value(40).toString();
                    auto        res         = dsqt::DSResource(
                        previewId, dsqt::DSResource::Id::CMS_TYPE, double(result.value(44).toFloat()),
                        float(result.value(42).toInt()), float(result.value(43).toInt()), result.value(41).toString(),
                        result.value(41).toString(), -1, cms.getResourcePath() + result.value(41).toString());
                    res.setType(type == "FILE_IMAGE" ? dsqt::DSResource::IMAGE_TYPE : dsqt::DSResource::VIDEO_TYPE);

                    auto preview_uid = field_uid + "_preview";
                    record.setPropertyResource(preview_uid, res);
                }
            } else if (type == "NUMBER") {
                record.setProperty(field_uid, result.value(10).toFloat());
            } else if (type == "COMPOSITE_AREA") {
                const auto& frameUid = result.value(19).toString();
                record.setProperty(frameUid + "_x", result.value(20).toFloat());
                record.setProperty(frameUid + "_y", result.value(21).toFloat());
                record.setProperty(frameUid + "_w", result.value(22).toFloat());
                record.setProperty(frameUid + "_h", result.value(23).toFloat());
            } else if (type == "IMAGE_AREA" || type == "IMAGE_SPOT") {
                record.setProperty("hotspot_x", result.value(47).toFloat());
                record.setProperty("hotspot_y", result.value(48).toFloat());
                record.setProperty("hotspot_w", result.value(49).toFloat());
                record.setProperty("hotspot_h", result.value(50).toFloat());
            } else if (type == "OPTIONS") {
                // this should be the following
                auto key   = result.value(25).toString();
                auto value = selectMap[key];
                record.setProperty(field_uid, value);
                record.setProperty(field_uid + "_value_uid", key);
            } else if (type == "CHECKBOX") {
                record.setProperty(field_uid, bool(result.value(6).toInt()));
            } else {
                qCWarning(lgBridgeSyncApp) << "UNHANDLED( values): " << type;
            }
            record.setProperty(field_uid + "_field_uid", result.value(1).toString());
        }
    }

    qDebug() << "Processing valueQuery took" << timer.elapsed() << "milliseconds";

    // Update our content.
    ContentModelRef root = DSQmlApplicationEngine::DefEngine()->getContentRoot();
    root.replaceChild(content);
    root.replaceChild(events);
    root.replaceChild(platforms);
    root.replaceChild(platform);
    root.replaceChild(records);
    root.setReferences("ally_records", recordMap);

    /*
    auto isMainThread = QThread::currentThread() == QCoreApplication::instance()->thread();
    if(!isMainThread){
        QMetaObject::invokeMethod(QCoreApplication::instance(),[](){
            QmlContentModel::cleanInvalid();
            QmlContentModel::updateAll();
        },Qt::BlockingQueuedConnection);
    } else {
        QmlContentModel::cleanInvalid();
        QmlContentModel::updateAll();
    }*/
    // auto newQml = root.getQml();

    // DSQmlApplicationEngine::DefEngine()->updateContentRoot(root);
}

QString DsBridgeSqlQuery::slugifyKey(QString appKey) {
    static QRegularExpression badRe("\\W|^(?=\\d)");
    QString                   result = appKey.replace(badRe, "_");
    return result;
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

} // namespace dsqt::bridge
