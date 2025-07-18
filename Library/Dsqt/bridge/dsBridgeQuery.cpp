﻿#include "dsBridgeQuery.h"


#include <regex>

#include <QSharedPointer>
#include <QString>
#include <QtSql/QtSql>

#include "core/dsenvironment.h"
#include "model/content_model.h"
#include "model/dsresource.h"
#include "qqmlcontext.h"
#include "settings/dssettings.h"
#include "settings/dssettings_proxy.h"
#include "network/dsnodewatcher.h"
#include "model/property_map_diff.h"

Q_LOGGING_CATEGORY(lgBridgeSyncApp, "bridgeSync.app")
Q_LOGGING_CATEGORY(lgBridgeSyncQuery, "bridgeSync.query")
Q_LOGGING_CATEGORY(lgBridgeSyncAppVerbose, "bridgeSync.app.verbose")
Q_LOGGING_CATEGORY(lgBridgeSyncQueryVerbose, "bridgeSync.query.verbose")
#define CREATE_NEW_CONSOLE 0x00000010;
using namespace dsqt::model;
namespace dsqt::bridge {

DsBridgeSqlQuery::DsBridgeSqlQuery(DSQmlApplicationEngine *parent)
    : QObject(parent)
{
    using namespace Qt::StringLiterals;

    connect(
        parent,
        &DSQmlApplicationEngine::onInit,
        this,
        [this]() {
            // if bridge sync is not running, try to launch it.
            if (!isBridgeSyncRunning()) {
                tryLaunchBridgeSync();
            }
            //setup the connections first
            auto engine = DSQmlApplicationEngine::DefEngine();
            connect(engine->getNodeWatcher(),&network::DsNodeWatcher::messageArrived,this,[this](dsqt::network::Message msg){
                QueryDatabase();
            },Qt::ConnectionType::QueuedConnection);

            connect(this,&DsBridgeSqlQuery::syncCompleted,engine,[engine](model::PropertyMapDiff* diff){
                engine->updateContentRoot(diff);
            },Qt::ConnectionType::QueuedConnection);

            qCDebug(lgBridgeSyncQuery) << "Starting Query";
            mDatabase = QSqlDatabase::addDatabase("QSQLITE");
            DSSettingsProxy engSettings;
            engSettings.setTarget("engine");
            engSettings.setPrefix("engine.resource");

            mResourceLocation = DSEnvironment::expandq(
                engSettings.getString("location", "").value<QString>());
            auto opFile = engSettings.getString("resource_db", "").value<QString>();
            if (opFile != "") {
                auto file = DSEnvironment::expandq(opFile);
                if (QDir::isRelativePath(file)) {
                    file = QDir::cleanPath(mResourceLocation + "/" + file);
                }
                mDatabase.setDatabaseName(file);

                if (!mDatabase.open()) {
                    qCWarning(lgBridgeSyncQuery) << "Could not open database at " << file;
                } else {
                    //fake a nodewatcher message.
                    emit DSQmlApplicationEngine::DefEngine()->getNodeWatcher()->messageArrived(dsqt::network::Message());
                }
            }

        },
        Qt::ConnectionType::QueuedConnection);


}

DsBridgeSqlQuery::~DsBridgeSqlQuery() {}

DsBridgeSyncSettings DsBridgeSqlQuery::getBridgeSyncSettings()
{
    DsBridgeSyncSettings settings;
    DSSettingsProxy engSettings;
    engSettings.setTarget("engine");
    engSettings.setPrefix("engine.bridgesync");
    auto launchBridgeSync = engSettings.getBool("launch_bridgesync", false);
    auto appPath = engSettings.getString("app_path", "%SHARE%/bridgesync/bridge_sync_console.exe")
                       .toString();

    settings.server = engSettings.getString("connection.server");
    settings.authServer = engSettings.getString("connection.auth_server");
    settings.clientId = engSettings.getString("connection.client_id");
    settings.clientSecret = engSettings.getString("connection.client_secret");
    settings.directory = engSettings.getString("connection.directory");
    settings.interval = engSettings.getInt("connection.interval");
    settings.verbose = engSettings.getBool("connection.verbose");

    settings.appPath = DSEnvironment::expandq(appPath);
    settings.doLaunch = launchBridgeSync.toBool();
    return settings;
}

bool DsBridgeSqlQuery::tryLaunchBridgeSync()
{
#ifndef Q_OS_WASM
    //static int tries = 0;
    qCInfo(lgBridgeSyncApp) << "Launching BridgeSync";

    //check if process is running
    if (mBridgeSyncProcess.state() == QProcess::Running) {
        qCInfo(lgBridgeSyncApp) << "BridgeSync is already running";
        return true;
    }

    auto bridgeSyncSettings = getBridgeSyncSettings();
    bool launch = true;
    if (!bridgeSyncSettings.doLaunch) {
        qCInfo(lgBridgeSyncApp) << "BridgeSync is not configured to launch";
        return false;
    }
    if (bridgeSyncSettings.appPath.isEmpty()) {
        qCWarning(lgBridgeSyncApp) << "BridgeSync's app path is empty. BridgeSync will not launch";
        return false;
    }
    if (bridgeSyncSettings.server.toString().isEmpty()) {
        qCWarning(lgBridgeSyncApp) << "BridgeSync's server is empty. BridgeSync will not launch";
        return false;
    }
    if (bridgeSyncSettings.directory.toString().isEmpty()) {
        qCWarning(lgBridgeSyncApp) << "BridgeSync's directory is empty. BridgeSync will not launch";
        return false;
    }

    //check that appPath exist
    auto appPath = DSEnvironment::expandq(bridgeSyncSettings.appPath);
    if (!QFile::exists(appPath)) {
        qCWarning(lgBridgeSyncApp)
            << "BridgeSync's app path does not exist. BridgeSync will not launch\n"
            << appPath;
        return false;
    }

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
    mBridgeSyncProcess.setArguments(args);
    qCInfo(lgBridgeSyncApp) << "BridgeSync is launching with args: "
                            << mBridgeSyncProcess.arguments();

    //connect to possible outcomes for start.
    connect(&mBridgeSyncProcess, &QProcess::started, this, [this]() {
        qCInfo(lgBridgeSyncApp) << "BridgeSync has started";
    });
    connect(
        &mBridgeSyncProcess,
        &QProcess::errorOccurred,
        this,
        [this](QProcess::ProcessError error) {
            qCWarning(lgBridgeSyncApp) << "BridgeSync has encountered an error: " << error;
            tryLaunchBridgeSync();
        },
        Qt::QueuedConnection);
    connect(
        &mBridgeSyncProcess,
        &QProcess::finished,
        this,
        [this](int exitCode) {
            qCInfo(lgBridgeSyncApp) << mBridgeSyncProcess.readAllStandardOutput();
            qCInfo(lgBridgeSyncApp) << "BridgeSync has finished with exit code: " << exitCode;
            tryLaunchBridgeSync();
        },
        Qt::QueuedConnection);
    connect(&mBridgeSyncProcess, &QProcess::stateChanged, this, [this](QProcess::ProcessState state) {
        qCInfo(lgBridgeSyncApp) << "BridgeSync has changed state: " << state;
    });
    connect(&mBridgeSyncProcess, &QProcess::readyReadStandardOutput, this, [this]() {
        auto byteArray = mBridgeSyncProcess.readAllStandardOutput();
        //QString output(byteArray);
        //qDebug()<<byteArray;
    });
    connect(&mBridgeSyncProcess, &QProcess::readyReadStandardError, this, [this]() {
        auto byteArray = mBridgeSyncProcess.readAllStandardError();
        //QString output(byteArray);
        //qDebug()<<byteArray;
    });

    mBridgeSyncProcess.start();
    return true;
#endif
    return true;
}

//checks to see if process is started
bool DsBridgeSqlQuery::isBridgeSyncRunning()
{
#ifdef Q_OS_WASM
    return true;
#else
    return mBridgeSyncProcess.state() == QProcess::Running;
#endif
}

void DsBridgeSqlQuery::QueryDatabase()
{
    //invalidate qml versions of models.
    //queryTables will set used models to valid.
    //invalidate will skip any model that doesn't have the appEngine
    //as a parent

    ReferenceMap tempRefMap;
    tempRefMap.isTemp = true;
    ContentModelRef root = DSQmlApplicationEngine::DefEngine()->getContentRoot();
    QmlContentModel* preModel = root.getQml(&tempRefMap,nullptr);

    queryTables();

    ReferenceMap tempRefMap2;
    tempRefMap2.isTemp = true;
    root = DSQmlApplicationEngine::DefEngine()->getContentRoot();
    QmlContentModel* postModel = root.getQml(&tempRefMap2,nullptr);

    PropertyMapDiff* diff = new PropertyMapDiff(*preModel,*postModel);
    emit syncCompleted(diff);
}

void DsBridgeSqlQuery::queryTables()
{
    qCInfo(lgBridgeSyncQuery) << "BridgeService is loading content.";

    //create a QmlContentModel of the current root;

    ContentModelRef root = DSQmlApplicationEngine::DefEngine()->getContentRoot();

    //QMetaObject::invokeMethod(QCoreApplication::instance(), func, Qt::BlockingQueuedConnection
    auto isMainThread = QThread::currentThread() == QCoreApplication::instance()->thread();
    const dsqt::DSResource::Id cms(dsqt::DSResource::Id::CMS_TYPE, 0);

    std::unordered_map<QString, ContentModelRef> recordMap;
    std::unordered_map<QString, QString> selectMap;

    //appsettings
    auto appsettings = DSSettings::getSettings("app_settings");

    QSqlQuery query(mDatabase);
    query.setForwardOnly(true);

    std::unordered_map<QString, std::pair<QString, bool>> slotReverseOrderingMap;
    {
        QString slotQuery = "SELECT "             //
                            " l.uid,"             // 0
                            " l.app_key,"         // 1
                            " l.reverse_ordering" // 2
                            " FROM lookup AS l"
                            " WHERE l.type = 'slot' ";
        query.exec(slotQuery);
        if (query.lastError().isValid()) {
            qCCritical(lgBridgeSyncQuery) << "Slot Query Error: " << query.lastError();
        } else {
            while (query.next()) {
                auto result = query.record();
                bool ok_val2;
                auto uid = result.value(0).toString();
                auto app_key = result.value(1).toString();
                auto reverse_ordering = result.value(2).toInt(&ok_val2);
                slotReverseOrderingMap[uid] = {app_key, reverse_ordering};
            }
        }
    }

    std::vector<ContentModelRef> rankOrderedRecords;
    {
        QString recordQuery
            = "SELECT "                 //
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

        query.exec(recordQuery);
        if (query.lastError().isValid()) {
            qCCritical(lgBridgeSyncQuery) << "Record Query Error: " << query.lastError();
        } else {
            if(query.size() == 0){
                qCWarning(lgBridgeSyncQuery) << "No records were found in database ";
            }
            while (query.next()) {
                auto result = query.record();
                ContentModelRef record(result.value(7).toString() + "(" + result.value(0).toString()
                                       + ")");
                auto val = QCryptographicHash::hash(result.value(0).toString().toUtf8(),
                                                    QCryptographicHash::Md5);
                record.setId(result.value(0).toString());
                if (record.getId() == "CniWJvnMsq3d") {
                    qDebug() << "found it";
                }

                record.setProperty("record_name", result.value(7).toString());
                record.setProperty("uid", result.value(0).toString());
                record.setProperty("type_uid", result.value(1).toString());
                record.setProperty("type_name", result.value(2).toString());
                record.setProperty("type_key", result.value(3).toString());
                if (!result.value(4).toString().isEmpty())
                    record.setProperty("parent_uid", result.value(4).toString());
                if (!result.value(5).toString().isEmpty()) {
                    record.setProperty("parent_slot", result.value(5).toString());
                    record.setProperty("label",
                                       slotReverseOrderingMap[result.value(5).toString()].first);
                    record.setProperty("reverse_ordered",
                                       slotReverseOrderingMap[result.value(5).toString()].second);
                }

                const auto &variant = result.value(6).toString();
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

            //this is the content placed in a tree
            mContent = ContentModelRef("content");
            //this is the matched platform
            mPlatforms = ContentModelRef("platforms");
            mPlatform = ContentModelRef("platform");
            //this is all the event records
            mEvents = ContentModelRef("all_events");
            //this is all the content records in a flat list
            mRecords = ContentModelRef("all_records");
            //mRecords.setNotToQml(true);

            for (const auto &record : rankOrderedRecords) {
                mRecords.addChild(record);
                auto type = record.getPropertyString("variant");
                if (type == "ROOT_CONTENT") {
                    mContent.addChild(record);
                } else if (type == "ROOT_PLATFORM") {
                    if (record.getPropertyString("uid")
                        == appsettings->getOr("platform.id", QString(""))) {
                        mPlatform.addChild(record);
                    }
                    mPlatforms.addChild(record);
                } else if (type == "SCHEDULE") {
                    auto list = record.getPropertyString("parent_uid").split(",", Qt::SkipEmptyParts);
                    for (const auto &parentUid :
                         std::as_const(list)) {
                        // Create/Add the event to its specific platform
                        auto platformEvents = recordMap[parentUid].getChildByName(
                            "scheduled_events");
                        platformEvents.setName("scheduled_events");
                        platformEvents.addChild(record);
                        recordMap[parentUid].replaceChild(platformEvents);
                    }

                    // Also add to the 'all_events' table in case an application needs events not specifically
                    // assigned to it
                    mEvents.addChild(record);
                } else if (type == "RECORD") {
                    // it's has a record for a parent!
                    recordMap[record.getPropertyString("parent_uid")].addChild(record);
                } else {
                    qCInfo(lgBridgeSyncApp) << "Found unknown record variant of: " << type;
                }
            }
        }
    }

    //get the select table
    {
        QString selectQuery = "SELECT "
                              " lookup.uid,"    // 0
                              " lookup.app_key" // 1
                              " FROM lookup"
                              " WHERE lookup.type = 'select'";
        query.exec(selectQuery);
        if (query.lastError().isValid()) {
            qCCritical(lgBridgeSyncQuery) << "Select Query - Query Error: " << query.lastError();
        } else {
            while (query.next()) {
                auto result = query.record();
                selectMap[result.value(0).toString()] = result.value(1).toString();
            }
        }
    }

    // Insert default values
    {
        /* select defaults and the type they belong to (from traits) */
        QString defaultsQuery
            = "SELECT "
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

        query.exec(defaultsQuery);
        if (query.lastError().isValid()) {
            qCCritical(lgBridgeSyncQuery) << "Defaults Query error:" << query.lastError();
        } else {
            while (query.next()) {
                auto result = query.record();
                auto recordUid = result.value(0).toString();
                if (recordMap.find(recordUid) == recordMap.end()) {
                    continue;
                }
                auto &record = recordMap[recordUid];

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
                    auto key = result.value(12).toString();
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
    }
    // Insert values
    {
        QString valueQuery
            = "SELECT "
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
              " v.date,"                  // 13
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
              " res.filename,"            // 51
              " v.time"                   // 52
              " FROM value AS v"
              " LEFT JOIN lookup AS l ON l.uid = v.field_uid"
              " LEFT JOIN resource AS res ON res.hash = v.resource_hash"
              " LEFT JOIN resource AS preview_res ON preview_res.hash = v.preview_resource_hash;";

        query.exec(valueQuery);
        if (query.lastError().isValid()) {
            qCCritical(lgBridgeSyncQuery) << "Value Query error:" << query.lastError();
        } else {
            while (query.next()) {
                auto result = query.record();
                const auto &recordUid = result.value(2).toString();
                if (recordMap.find(recordUid) == recordMap.end()) {
                    continue;
                }
                auto &record = recordMap[recordUid];

                auto field_uid = result.value(1).toString();
                auto field_key = slugifyKey(result.value(37).toString());
                if (!field_key.isEmpty()) {
                    field_uid = field_key;
                }
                auto type = result.value(3).toString();
                auto resourceId = result.value(28).toString();
                auto previewId = result.value(38).toString();
                if (type == "TEXT") {
                    record.setProperty(field_uid, result.value(8).toString());
                } else if (type == "RICH_TEXT") {
                    record.setProperty(field_uid, result.value(9).toString());
                } else if (type == "FILE_IMAGE" || type == "FILE_VIDEO" || type == "FILE_PDF") {
                    if (!resourceId.isEmpty()) {
                        //TODO: finish converting this section.
                        auto res = dsqt::DSResource(resourceId,
                                                  dsqt::DSResource::Id::CMS_TYPE,
                                                  double(result.value(33).toDouble()),
                                                  float(result.value(31).toInt()),
                                                  float(result.value(32).toInt()),
                                                  result.value(51).toString(),
                                                  result.value(30).toString(),
                                                  -1,
                                                  cms.getResourcePath()
                                                      + result.value(30).toString());
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
                    //handle preview

                    if (!previewId.isEmpty()) {
                        const auto &previewType = result.value(40).toString();

                        auto res = dsqt::DSResource(previewId,
                                                  dsqt::DSResource::Id::CMS_TYPE,
                                                  double(result.value(44).toFloat()),
                                                  float(result.value(42).toInt()),
                                                  float(result.value(43).toInt()),
                                                  result.value(41).toString(),
                                                  result.value(41).toString(),
                                                  -1,
                                                  cms.getResourcePath()
                                                      + result.value(41).toString());
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
                    const auto &linkUrl = result.value(30).toString();
                    auto res = dsqt::DSResource(resourceId,
                                              dsqt::DSResource::Id::CMS_TYPE,
                                              double(result.value(33).toFloat()),
                                              float(result.value(31).toInt()),
                                              float(result.value(32).toInt()),
                                              linkUrl,
                                              linkUrl,
                                              -1,
                                              linkUrl);

                    // Assumes app settings are not changed concurrently on another thread.
                    auto webSize = appsettings->getOr<glm::vec2>("web:default_size",
                                                                 glm::vec2(1920.f, 1080.f));
                    res.setWidth(webSize.x);
                    res.setHeight(webSize.y);
                    res.setType(dsqt::DSResource::WEB_TYPE);
                    record.setPropertyResource(field_uid, res);

                    if (!previewId.isEmpty()) {
                        const auto &previewType = result.value(40).toString();
                        auto res = dsqt::DSResource(previewId,
                                                  dsqt::DSResource::Id::CMS_TYPE,
                                                  double(result.value(44).toFloat()),
                                                  float(result.value(42).toInt()),
                                                  float(result.value(43).toInt()),
                                                  result.value(41).toString(),
                                                  result.value(41).toString(),
                                                  -1,
                                                  cms.getResourcePath()
                                                      + result.value(41).toString());
                        res.setType(type == "FILE_IMAGE" ? dsqt::DSResource::IMAGE_TYPE
                                                         : dsqt::DSResource::VIDEO_TYPE);

                        auto preview_uid = field_uid + "_preview";
                        record.setPropertyResource(preview_uid, res);
                    }
                } else if (type == "NUMBER") {
                    record.setProperty(field_uid, result.value(10).toFloat());
                } else if (type == "COMPOSITE_AREA") {
                    const auto &frameUid = result.value(19).toString();
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
                    //this should be the following
                    auto key = result.value(25).toString();
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
    }
    //update root
    //ContentModelRef root = DSQmlApplicationEngine::DefEngine()->getContentRoot();
    root.replaceChild(mContent);
    root.replaceChild(mEvents);
    root.replaceChild(mPlatforms);
    root.replaceChild(mPlatform);
    root.replaceChild(mRecords);
    root.setReferences("ally_records", recordMap);
    /*
    if(!isMainThread){
        QMetaObject::invokeMethod(QCoreApplication::instance(),[](){
            QmlContentModel::cleanInvalid();
            QmlContentModel::updateAll();
        },Qt::BlockingQueuedConnection);
    } else {
        QmlContentModel::cleanInvalid();
        QmlContentModel::updateAll();
    }*/
    //auto newQml = root.getQml();



    //DSQmlApplicationEngine::DefEngine()->updateContentRoot(root);
}

QString DsBridgeSqlQuery::slugifyKey(QString appKey)
{
    static QRegularExpression badRe("\\W|^(?=\\d)");
    QString result = appKey.replace(badRe,"_");
    return result;
}

} // namespace dsqt::bridge
