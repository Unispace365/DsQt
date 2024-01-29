#include "dsBridgeQuery.h"

#include <QSharedPointer>
#include <QString>
#include <QtSql/QtSql>
#include "bridge_sql_queries.h"
#include "core/dsenvironment.h"
#include "model/dscontentmodel.h"
#include "model/dsresource.h"
#include "settings/dssettings.h"
#include "settings/dssettings_proxy.h"

Q_LOGGING_CATEGORY(bridgeSyncApp, "bridgeSync.app")
Q_LOGGING_CATEGORY(bridgeSyncQuery, "bridgeSync.query")
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
            if (!isBridgeSyncRunning()) {
                tryLaunchBridgeSync();
                return;
            }

            mDatabase = QSqlDatabase::addDatabase("QSQLITE");
            DSSettingsProxy engSettings;
            engSettings.setTarget("engine");
            engSettings.setPrefix("engine.resource");

            mResourceLocation = DSEnvironment::expandq(engSettings.getString("location","").value<QString>());
            auto opFile = engSettings.getString("resource_db","").value<QString>();
            if (opFile!="") {
                auto file = DSEnvironment::expandq(opFile);
                if (QDir::isRelativePath(file)) {
                    file = QDir::cleanPath(mResourceLocation + "/" + file);
                }
                mDatabase.setDatabaseName(file);

                if (!mDatabase.open()) {
                    qCWarning(bridgeSyncQuery) << "Could not open database at " << file;
                } else {
                    //get the raw nodes

                    queryTables();
                }
            }
        },
        Qt::ConnectionType::DirectConnection);
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
    //static int tries = 0;

    //check if process is running
    if (mBridgeSyncProcess.state() == QProcess::Running) {
        qCInfo(bridgeSyncApp) << "BridgeSync is already running";
        return true;
    }

    auto bridgeSyncSettings = getBridgeSyncSettings();
    bool launch = true;
    if (!bridgeSyncSettings.doLaunch) {
        qCInfo(bridgeSyncApp) << "BridgeSync is not configured to launch";
        return false;
    }
    if (bridgeSyncSettings.appPath.isEmpty()) {
        qCWarning(bridgeSyncApp) << "BridgeSync's app path is empty. BridgeSync will not launch";
        return false;
    }
    if (bridgeSyncSettings.server.toString().isEmpty()) {
        qCWarning(bridgeSyncApp) << "BridgeSync's server is empty. BridgeSync will not launch";
        return false;
    }
    if (bridgeSyncSettings.directory.toString().isEmpty()) {
        qCWarning(bridgeSyncApp) << "BridgeSync's directory is empty. BridgeSync will not launch";
        return false;
    }

    //check that appPath exist
    auto appPath = DSEnvironment::expandq(bridgeSyncSettings.appPath);
    if (!QFile::exists(appPath)) {
        qCWarning(bridgeSyncApp)
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
    qCInfo(bridgeSyncApp) << "BridgeSync is launching with args: "
                          << mBridgeSyncProcess.arguments();

    //connect to possible outcomes for start.
    connect(&mBridgeSyncProcess, &QProcess::started, this, [this]() {
        qCInfo(bridgeSyncApp) << "BridgeSync has started";
    });
    connect(&mBridgeSyncProcess,
            &QProcess::errorOccurred,
            this,
            [this](QProcess::ProcessError error) {
                qCWarning(bridgeSyncApp) << "BridgeSync has encountered an error: " << error;
            });
    connect(&mBridgeSyncProcess, &QProcess::finished, this, [this](int exitCode) {
        qCInfo(bridgeSyncApp) << mBridgeSyncProcess.readAllStandardOutput();
        qCInfo(bridgeSyncApp) << "BridgeSync has finished with exit code: " << exitCode;
    });
    connect(&mBridgeSyncProcess, &QProcess::stateChanged, this, [this](QProcess::ProcessState state) {
        qCInfo(bridgeSyncApp) << "BridgeSync has changed state: " << state;
    });

    mBridgeSyncProcess.start();
    return true;
}

//checks to see if process is started
bool DsBridgeSqlQuery::isBridgeSyncRunning()
{
    return mBridgeSyncProcess.state() == QProcess::Running;
}

void DsBridgeSqlQuery::loadContent()
{
    qCDebug(bridgeSyncQuery) << "BridgeService::Loop is loading content.";

    //const ds::Resource::Id cms(ds::Resource::Id::CMS_TYPE, 0);

    std::unordered_map<QString, DSContentModelPtr> recordMap;
    std::unordered_map<QString, QString> selectMap;

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
            qCDebug(bridgeSyncQuery) << "Query Error: " << query.lastError();
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

    std::vector<DSContentModelPtr> rankOrderedRecords;
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
            qCDebug(bridgeSyncQuery) << "Query Error: " << query.lastError();
        } else {
            int recordId = 1;
            while (query.next()) {
                auto result = query.record();
                auto record = DSContentModel::create(result.value(7).toString() + "("
                                                     + result.value(0).toString() + ")");
                record->setId(result.value(0).toString());

                record->setProperty("record_name", result.value(7));
                record->setProperty("uid", result.value(0));
                record->setProperty("type_uid", result.value(1));
                record->setProperty("type_name", result.value(2));
                record->setProperty("type_key", result.value(3));
                if (!result.value(4).toString().isEmpty())
                    record->setProperty("parent_uid", result.value(4));
                if (!result.value(5).toString().isEmpty()) {
                    record->setProperty("parent_slot", result.value(5));
                    record->setProperty("label",
                                        slotReverseOrderingMap[result.value(5).toString()].first);
                    record->setProperty("reverse_ordered",
                                        slotReverseOrderingMap[result.value(5).toString()].second);
                }

                const auto &variant = result.value(6).toString();
                record->setProperty("variant", variant);
                if (variant == "SCHEDULE") {
                    record->setProperty("span_type", result.value(8).toString());
                    record->setProperty("start_date", result.value(9).toString());
                    record->setProperty("end_date", result.value(10).toString());
                    record->setProperty("start_time", result.value(11).toString());
                    record->setProperty("end_time", result.value(12).toString());
                    record->setProperty("effective_days", result.value(13).toInt());
                }

                // Put it in the map so we can wire everything up into the tree
                recordMap[result.value(0).toString()] = record;

                rankOrderedRecords.push_back(record);
                //++it;
            }

            DSContentModelPtr root = DSContentModel::getRoot();

            mContent = DSContentModel::create("content");
            mPlatforms = DSContentModel::create("platform");
            mEvents = DSContentModel::create("all_events");
            mRecords = DSContentModel::create("all_records");

            for (const auto &record : rankOrderedRecords) {
                mRecords->addChild(record);
                auto type = record->property("variant").toString();
                if (type == "ROOT_CONTENT") {
                    mContent->addChild(record);
                } else if (type == "ROOT_PLATFORM") {
                    mPlatforms->addChild(record);
                } else if (type == "SCHEDULE") {
                    for (const auto &parentUid :
                         ds::split(record->property("parent_uid").toString(), ",")) {
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
                    DS_LOG_INFO("TYPE: " << type)
                }
            }
        }
    }

    //get the select table
    {
        ds::query::Result result;
        std::string selectQuery = "SELECT "
                                  " lookup.uid,"    // 0
                                  " lookup.app_key" // 1
                                  " FROM lookup"
                                  " WHERE lookup.type = 'select'";
        if (ds::query::Client::query(cms.getDatabasePath(), selectQuery, result)) {
            ds::query::Result::RowIterator it(result);
            while (it.hasValue()) {
                selectMap[result.value(0).toString()] = result.value(1).toString();
                ++it;
            }
        }
    }

    // Insert default values
    {
        ds::query::Result result;

        /* select defaults and the type they belong to (from traits) */
        std::string defaultsQuery
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

        if (ds::query::Client::query(cms.getDatabasePath(), defaultsQuery, result)) {
            ds::query::Result::RowIterator it(result);
            while (it.hasValue()) {
                auto recordUid = result.value(0).toString();
                if (recordMap.find(recordUid) == recordMap.end()) {
                    ++it;
                    continue;
                }
                auto &record = recordMap[recordUid];

                auto field_uid = result.value(1).toString();
                auto field_key = result.value(2).toString();
                if (!field_key.empty()) {
                    field_uid = field_key;
                }
                auto type = result.value(3).toString();

                if (type == "TEXT") {
                    record.setProperty(field_uid, result.value(6).toString());
                } else if (type == "RICH_TEXT") {
                    record.setProperty(field_uid, result.value(7).toString());
                } else if (type == "NUMBER") {
                    record.setProperty(field_uid, it.getFloat(8));
                } else if (type == "OPTIONS") {
                    auto key = result.value(12).toString();
                    auto value = selectMap[key];
                    record.setProperty(field_uid, value);
                    record.setProperty(field_uid + "_value_uid", key);
                } else if (type == "CHECKBOX") {
                    record.setProperty(field_uid, bool(it.getInt(4)));
                } else {
                    DS_LOG_INFO("UNHANDLED(1): " << type)
                }
                record.setProperty(field_uid + "_field_uid", result.value(1).toString());
                ++it;
            }
        }
    }

    {
        ds::query::Result result;

        std::string valueQuery
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

        if (ds::query::Client::query(cms.getDatabasePath(), valueQuery, result)) {
            ds::query::Result::RowIterator it(result);
            while (it.hasValue()) {
                const auto &recordUid = result.value(2).toString();
                if (recordMap.find(recordUid) == recordMap.end()) {
                    ++it;
                    continue;
                }
                auto &record = recordMap[recordUid];

                auto field_uid = result.value(1).toString();
                auto field_key = result.value(37).toString();
                if (!field_key.empty()) {
                    field_uid = field_key;
                }
                auto type = result.value(3).toString();

                if (type == "TEXT") {
                    record.setProperty(field_uid, result.value(8).toString());
                } else if (type == "RICH_TEXT") {
                    record.setProperty(field_uid, result.value(9).toString());
                } else if (type == "FILE_IMAGE" || type == "FILE_VIDEO" || type == "FILE_PDF") {
                    if (!result.value(28).empty().toString()) {
                        auto res = ds::Resource(mResourceId,
                                                ds::Resource::Id::CMS_TYPE,
                                                double(it.getFloat(33)),
                                                float(it.getInt(31)),
                                                float(it.getInt(32)),
                                                result.value(51).toString(),
                                                result.value(30).toString(),
                                                -1,
                                                cms.getResourcePath() + result.value(30).toString());
                        if (type == "FILE_IMAGE")
                            res.setType(ds::Resource::IMAGE_TYPE);
                        else if (type == "FILE_VIDEO")
                            res.setType(ds::Resource::VIDEO_TYPE);
                        else if (type == "FILE_PDF")
                            res.setType(ds::Resource::PDF_TYPE);
                        else
                            continue;

                        auto cropX = it.getFloat(15);
                        auto cropY = it.getFloat(16);
                        auto cropW = it.getFloat(17);
                        auto cropH = it.getFloat(18);
                        if (cropX > 0.f || cropY > 0.f || cropW > 0.f || cropH > 0.f) {
                            res.setCrop(cropX, cropY, cropW, cropH);
                        }

                        record.setPropertyResource(field_uid, res);
                    }

                    if (!result.value(38).empty().toString()) {
                        const auto &previewType = result.value(40).toString();

                        auto res = ds::Resource(mResourceId,
                                                ds::Resource::Id::CMS_TYPE,
                                                double(it.getFloat(44)),
                                                float(it.getInt(42)),
                                                float(it.getInt(43)),
                                                result.value(41).toString(),
                                                result.value(41).toString(),
                                                -1,
                                                cms.getResourcePath() + result.value(41).toString());
                        res.setType(previewType == "FILE_IMAGE" ? ds::Resource::IMAGE_TYPE
                                                                : ds::Resource::VIDEO_TYPE);

                        auto preview_uid = field_uid + "_preview";
                        record.setPropertyResource(preview_uid, res);
                    }

                } else if (type == "LINKS") {
                    auto toUpdate = record.getPropertyString(field_uid);
                    if (!toUpdate.empty()) {
                        toUpdate.append(", ");
                    }
                    toUpdate.append(result.value(27).toString());
                    record.setProperty(field_uid, toUpdate);
                } else if (type == "LINK_WEB") {
                    const auto &linkUrl = result.value(30).toString();
                    auto res = ds::Resource(mResourceId,
                                            ds::Resource::Id::CMS_TYPE,
                                            double(it.getFloat(33)),
                                            float(it.getInt(31)),
                                            float(it.getInt(32)),
                                            linkUrl,
                                            linkUrl,
                                            -1,
                                            linkUrl);

                    // Assumes app settings are not changed concurrently on another thread.
                    auto webSize = mEngine.getAppSettings().getVec2("web:default_size",
                                                                    0,
                                                                    ci::vec2(1920.f, 1080.f));
                    res.setWidth(webSize.x);
                    res.setHeight(webSize.y);
                    res.setType(ds::Resource::WEB_TYPE);
                    record.setPropertyResource(field_uid, res);

                    if (!result.value(38).empty().toString()) {
                        const auto &previewType = result.value(40).toString();
                        auto res = ds::Resource(mResourceId,
                                                ds::Resource::Id::CMS_TYPE,
                                                double(it.getFloat(44)),
                                                float(it.getInt(42)),
                                                float(it.getInt(43)),
                                                result.value(41).toString(),
                                                result.value(41).toString(),
                                                -1,
                                                cms.getResourcePath() + result.value(41).toString());
                        res.setType(type == "FILE_IMAGE" ? ds::Resource::IMAGE_TYPE
                                                         : ds::Resource::VIDEO_TYPE);

                        auto preview_uid = field_uid + "_preview";
                        record.setPropertyResource(preview_uid, res);
                    }
                } else if (type == "NUMBER") {
                    record.setProperty(field_uid, it.getFloat(10));
                } else if (type == "COMPOSITE_AREA") {
                    const auto &frameUid = result.value(19).toString();
                    record.setProperty(frameUid + "_x", it.getFloat(20));
                    record.setProperty(frameUid + "_y", it.getFloat(21));
                    record.setProperty(frameUid + "_w", it.getFloat(22));
                    record.setProperty(frameUid + "_h", it.getFloat(23));
                } else if (type == "IMAGE_AREA" || type == "IMAGE_SPOT") {
                    record.setProperty("hotspot_x", it.getFloat(47));
                    record.setProperty("hotspot_y", it.getFloat(48));
                    record.setProperty("hotspot_w", it.getFloat(49));
                    record.setProperty("hotspot_h", it.getFloat(50));
                } else if (type == "OPTIONS") {
                    //this should be the following
                    auto key = result.value(25).toString();
                    auto value = selectMap[key];
                    record.setProperty(field_uid, value);
                    record.setProperty(field_uid + "_value_uid", key);
                } else if (type == "CHECKBOX") {
                    record.setProperty(field_uid, bool(it.getInt(6)));
                } else {
                    DS_LOG_INFO("UNHANDLED(2): " << type)
                }
                record.setProperty(field_uid + "_field_uid", result.value(1).toString());

                ++it;
            }
        }
    }
}

void DsBridgeSqlQuery::queryTables()
{
    QSqlQuery query(mDatabase);
    query.setForwardOnly(true);
    query.exec(qwaffle_nodes);
    if (query.lastError().isValid())
        qDebug() << "qwaffle_nodes " << query.lastError();

    auto insert = [](const QVariant &node, const QString key, const QVariant value) {
        if(node.isValid()){
            node.value<DSContentModel *>()->insert(key, value);
        }
    };

    auto insertToList = [insert](QVariant &node, QString key, const QVariant value) {
        QVariantList *list = nullptr;
        if(node.isValid()){
            if (node.value<DSContentModel *>()->contains(key)) {
                list = node.value<DSContentModel *>()->value(key).value<QVariantList *>();
            } else {
                list = new QVariantList();
            }
            list->append(value);
            insert(node, key, QVariant::fromValue(list));
        }
    };

    DSContentModel *nodeTree = DSContentModel::mContent->getChildByName("cms_root");
    if (nodeTree == nullptr) {
        nodeTree = new DSContentModel(DSContentModel::mContent.get(), "cms_root");
    }

    auto lookup = std::shared_ptr<QHash<int, QVariant>>(new QHash<int, QVariant>());
    DSContentModel::mContent->setProperty("lookup", QVariant::fromValue(lookup));
    QHash<int,QVariant> nodeMap = *lookup.get();

    //build the nodeMap
    while (query.next()) {
        auto record = query.record();
        auto id = record.field("id").value().value<int>();
        if (query.lastError().isValid())
            qDebug() << "qwaffle_nodes record "<< query.lastError();
        DSContentModel *map = new DSContentModel(this);
        map->unlock();
        for (int j = 0; j < record.count(); j++) {

            auto keyq = record.fieldName(j);
            //auto key = keyq.c_str();
            auto val = record.field(j).value();

            map->insert(keyq, val);

            if (query.lastError().isValid())
                qDebug() << query.lastError();
        }

        nodeMap[id].setValue(map);
    }

    //build node tree
    for (auto &node_variant : nodeMap) {
        if(node_variant.isValid()){
            auto node = node_variant.value<DSContentModel *>();
            auto branch = node->property("branch").value<QString>();

            if (branch == "content") {
                DSContentModel *parentNode = nullptr;
                if (node->property("parent_id").isNull()) {
                    parentNode = nodeTree;
                } else {

                    parentNode = nodeMap.value(node->property("parent_id").value<int>())
                                     .value<DSContentModel *>();
                }
                if (parentNode) {
                    node->setParent(parentNode);
                }
            }
        }
    }

    for (auto &node_variant : nodeMap) {
        if(node_variant.isValid()){
            auto node = node_variant.value<DSContentModel *>();
            auto branch = node->property("branch").value<QString>();

            if (branch == "content") {
                auto childList= node->findChildren<DSContentModel*>(Qt::FindDirectChildrenOnly);
                std::sort(childList.begin(),childList.end(),[](DSContentModel* a,DSContentModel* b)
                    {
                              auto aval = a->property("rank").value<int>();
                              auto bval = b->property("rank").value<int>();

                              return aval > bval;
                    });
                for(auto child:childList){
                    child->setParent(0);
                }
                for(auto child:childList){
                    child->setParent(node);
                }
            }
        }
    }

    auto childList= nodeTree->findChildren<DSContentModel*>(Qt::FindDirectChildrenOnly);
    std::sort(childList.begin(),childList.end(),[](DSContentModel* a,DSContentModel* b)
              {
                  auto aval = a->property("rank").value<int>();
                  auto bval = b->property("rank").value<int>();

                  return aval > bval;
              });
    for(auto child:childList){
        child->setParent(0);
    }
    for(auto child:childList){
        child->setParent(nodeTree);
    }

    //text fields
    auto textCheckColorHandler = [&query, &nodeMap, &insert](QString &queryStr,QString queryid) {
        query.exec(queryStr);
        if (query.lastError().isValid())
            qDebug() << queryid << " "<< query.lastError();
        while (query.next()) {
            auto record = query.record();
            auto nodeId = record.field("node_id").value().value<int>();
            if (query.lastError().isValid())
                qDebug() << query.lastError();

            auto node = nodeMap[nodeId];
            if(!node.isValid()) continue;
            auto appKey = record.field("app_key").value().value<QString>();
            auto value = record.field("value").value().value<QString>().trimmed();
            insert(node, appKey, QVariant(value));
            auto plainValue = record.field("plain_value").value().value<QString>();
            insert(node, appKey + "_plain_value", plainValue);
        }
    };

    textCheckColorHandler(qwaffles_text_fields,"qwaffles_text_fields");
    textCheckColorHandler(qwaffles_checkboxes,"qwaffles_checkboxes");
    textCheckColorHandler(qwaffles_color,"qwaffles_color");

    //get Resources
    query.exec(resources);


    if (query.lastError().isValid())
        qDebug() << "resources "<< query.lastError();
    while (query.next()) {
        auto record = query.record();
        auto resId = record.field("resourcesid").value().value<int>();
        auto strType = record.field("resourcestype").value().value<QString>();
        auto filename = record.field("resourcesfilename").value().value<QString>();
        auto path = record.field("resourcespath").value().value<QString>();
        auto duration = record.field("resourcesduration").value().value<float>();
        auto width = record.field("resourceswidth").value().value<float>();
        auto height = record.field("resourcesheight").value().value<float>();
        auto thumbId = record.field("resourcesthumbid").value().value<int>();
        auto mediaId = record.field("media_id").value().value<int>();

        //build url
        auto type = dsqt::DSResource::makeTypeFromString(strType);
        auto url = filename;
        if (type != DSResource::ResourceType::WEB && type != DSResource::ResourceType::YOUTUBE) {
            url = "file:///" + QDir::cleanPath(mResourceLocation + "/" + path + "/" + filename);
        }

        mAllResources[resId]
            = new dsqt::DSResource(resId, type, duration, width, height, url, thumbId, this);
    }

    //handle media fields
    query.exec(qwaffles_media_fields);
    if (query.lastError().isValid())
        qDebug() << "qwaffles_media_fields "<< query.lastError();
    while (query.next()) {
        auto record = query.record();
        auto nodeId = record.field("node_id").value().value<int>();
        if (query.lastError().isValid())
            qDebug() << query.lastError();

        auto node = nodeMap[nodeId];
        if(!node.isValid()) continue;
        auto appKey = record.field("app_key").value().value<QString>();
        auto media_res_id = record.field("media_res").value().value<int>();
        auto media_thumb_res_id = record.field("media_res").value().value<int>();
        auto floatable = record.field("floatable").value().value<bool>();
        auto float_autoplay = record.field("float_autoplay").value().value<QString>();
        auto float_loop = record.field("float_loop").value().value<QString>();
        auto float_volume = record.field("float_volume").value().value<QString>();
        auto float_touch = record.field("float_touch").value().value<QString>();
        auto float_page = record.field("float_page").value().value<QString>();
        auto float_fullscreen = record.field("float_fullscreen").value().value<QString>();
        auto hide_thumbnail = record.field("hide_thumbnail").value().value<QString>();
        auto media_res = mAllResources[media_res_id];
        auto media_thumb_res = mAllResources[media_thumb_res_id];

        if (media_res) {
            insert(node, appKey + "_media_res", QVariant::fromValue(media_res));
            if (media_thumb_res) {
                insert(node, appKey + "_media_thumb_res", QVariant::fromValue(media_thumb_res));
            }
            if (appKey == "media") {
                insert(node, "media_res", QVariant::fromValue(media_res));
            }
            if (floatable) {
                insert(node, appKey + "_floatable", floatable);
                insert(node, appKey + "_float_autoplay", float_autoplay);
                insert(node, appKey + "_float_loop", float_loop);
                insert(node, appKey + "_float_volume", float_volume);
                insert(node, appKey + "_float_touch", float_touch);
                insert(node, appKey + "_float_page", float_page);
                insert(node, appKey + "_float_fullscreen", float_fullscreen);
                insert(node, appKey + "_hide_thumbnail", hide_thumbnail);
            }
        }
    }

    //dropdowns
    query.exec(qwaffles_dropdown_fields);
    if (query.lastError().isValid())
        qDebug() <<"qwaffles_dropdown_fields "<< query.lastError();
    while (query.next()) {
        auto record = query.record();
        auto nodeId = record.field("node_id").value().value<int>();
        if (query.lastError().isValid())
            qDebug() << query.lastError();

        auto node = nodeMap[nodeId];
        if(!node.isValid()) continue;
        auto appKey = record.field("app_key").value().value<QString>();
        auto allow_multiple = record.field("allow_multiple").value().value<bool>();
        auto option_save_value = record.field("option_save_value").value().value<QString>();
        auto dropdown_label = record.field("dropdown_label").value().value<QString>();
        if (allow_multiple) {
            insertToList(node, appKey, option_save_value);
        } else {
            insert(node, appKey, option_save_value);
            insert(node, appKey + "_dropdown_label", dropdown_label);
        }
    }

    //selections
    query.exec(qwaffles_selections);
    if (query.lastError().isValid())
        qDebug() <<"qwaffles_selections "<< query.lastError();
    while (query.next()) {
        auto record = query.record();
        auto nodeId = record.field("owner_id").value().value<int>();
        if (query.lastError().isValid())
            qDebug() << query.lastError();

        auto node = nodeMap[nodeId];
        if(!node.isValid()) continue;
        auto appKey = record.field("app_key").value().value<QString>();
        auto allow_multiple = record.field("allow_multiple").value().value<bool>();
        auto selection_nodeId = record.field("node_id").value().value<int>();
        auto definition_id = record.field("definition_id").value().value<int>();
        if (appKey == "breifing_pinboard") {
            nodeTree->setProperty("briefing_pinboard_definition_id", definition_id);
        }
        if (allow_multiple) {
            insertToList(node, appKey, selection_nodeId);
        } else {
            insert(node, appKey, selection_nodeId);
        }
    }

    //hotspots
    query.exec(qwaffles_hotspots);
    if (query.lastError().isValid())
        qDebug() <<"qwaffles_hotspots "<< query.lastError();
    while (query.next()) {
        auto record = query.record();
        auto nodeId = record.field("node_id").value().value<int>();
        if (query.lastError().isValid())
            qDebug() << query.lastError();

        auto node = nodeMap[nodeId];
        if(!node.isValid()) continue;
        auto appKey = record.field("app_key").value().value<QString>();
        auto shape = record.field("shape").value().value<QString>();
        auto pos_x = record.field("pos_x").value().value<float>();
        auto pos_y = record.field("pos_y").value().value<float>();
        auto pos_w = record.field("pos_w").value().value<float>();
        auto pos_h = record.field("pos_h").value().value<float>();

        insert(node, appKey + "_shape", shape);
        insert(node, appKey + "_pos_x", pos_x);
        insert(node, appKey + "_pos_y", pos_y);
        insert(node, appKey + "_pos_w", pos_w);
        insert(node, appKey + "_pos_h", pos_h);
    }

    //composites
    query.exec(qwaffles_composites);
    if (query.lastError().isValid())
        qDebug()<<"qwaffles_composites " << query.lastError();
    while (query.next()) {
        auto record = query.record();
        auto nodeId = record.field("node_id").value().value<int>();
        if (query.lastError().isValid())
            qDebug() << query.lastError();

        auto node = nodeMap[nodeId];
        if(!node.isValid()) continue;
        auto appKey = record.field("app_key").value().value<QString>();

        auto pos_x = record.field("pos_x").value().value<float>();
        auto pos_y = record.field("pos_y").value().value<float>();
        auto pos_w = record.field("pos_w").value().value<float>();
        //auto pos_h = record.field("pos_h").value().value<float>();

        insert(node, "composite_pos_x", pos_x);
        insert(node, "composite_pos_y", pos_y);
        insert(node, "composite_pos_w", pos_w);
        //insert(node,appKey+"_pos_h",pos_h);
    }

    //composites
    query.exec(qwaffles_composites);
    if (query.lastError().isValid())
        qDebug()<<"qwaffles_composites " << query.lastError();
    while (query.next()) {
        auto record = query.record();
        auto nodeId = record.field("node_id").value().value<int>();
        if (query.lastError().isValid())
            qDebug() << query.lastError();

        auto node = nodeMap[nodeId];
        if(!node.isValid()) continue;
        auto appKey = record.field("app_key").value().value<QString>();

        auto pos_x = record.field("pos_x").value().value<float>();
        auto pos_y = record.field("pos_y").value().value<float>();
        auto pos_w = record.field("pos_w").value().value<float>();
        //auto pos_h = record.field("pos_h").value().value<float>();

        insert(node, "composite_pos_x", pos_x);
        insert(node, "composite_pos_y", pos_y);
        insert(node, "composite_pos_w", pos_w);
        //insert(node,appKey+"_pos_h",pos_h);
    }

    //composite_details
    query.exec(qwaffles_composite_details);
    if (query.lastError().isValid())
        qDebug() <<"qwaffles_composite_details "<< query.lastError();
    while (query.next()) {
        auto record = query.record();
        auto nodeId = record.field("node_id").value().value<int>();
        if (query.lastError().isValid())
            qDebug() << query.lastError();

        auto node = nodeMap[nodeId];
        if(!node.isValid()) continue;
        auto appKey = record.field("app_key").value().value<QString>();

        auto preview_res_id = record.field("preview_res").value().value<int>();
        auto preview_thumb_res_id = record.field("preview_thumb_res").value().value<int>();
        auto preview_res = mAllResources[preview_res_id];
        auto preview_thumb_res = mAllResources[preview_thumb_res_id];

        if (preview_res) {
            insert(node, appKey + "_preview_res", QVariant::fromValue(preview_res));
            if (preview_thumb_res) {
                insert(node, appKey + "_preview_thumb_res", QVariant::fromValue(preview_thumb_res));
            }
        }
    }

    //streamconfs
    query.exec(qwaffles_streamconfs);
    if (query.lastError().isValid())
        qDebug() <<"qwaffles_streamconfs "<< query.lastError();
    while (query.next()) {
        auto record = query.record();
        auto nodeId = record.field("node_id").value().value<int>();
        if (query.lastError().isValid())
            qDebug() << query.lastError();

        auto node = nodeMap[nodeId];
        if(!node.isValid()) continue;
        auto appKey = record.field("app_key").value().value<QString>();

        QString location = record.field("location").value().value<QString>();
        auto stream_res_id = record.field("stream_res").value().value<int>();
        DSResource* stream_res = mAllResources[stream_res_id];
        QString resourcesfilename = record.field("resourcesfilename").value().value<QString>();

        if (!location.isEmpty()) {
            stream_res->setUrl(location);
            insert(node, resourcesfilename, QVariant::fromValue(stream_res));
        }
    }
    //tags

    query.exec(qwaffles_tags);
    DSContentModel *tags = new DSContentModel();
    auto tagsv = QVariant::fromValue(tags);
    if (query.lastError().isValid())
        qDebug()<<"qwaffles_tags " << query.lastError();
    while (query.next()) {
        auto record = query.record();
        auto nodeId = record.field("node_id").value().value<int>();
        if (query.lastError().isValid())
            qDebug() << query.lastError();

        auto node = nodeMap[nodeId];
        if(!node.isValid()) continue;
        auto appKey = record.field("app_key").value().value<QString>();

        auto tag_class = record.field("class").value().value<QString>();
        auto tag_title = record.field("title").value().value<QString>();
        auto taggable_field = record.field("taggable_field").value().value<QString>();

        insertToList(node, taggable_field, tag_title);
        insertToList(node, "tags_" + tag_class, tag_title);
        bool found = false;
        auto tag_list_ptr = tags->value(tag_class).value<QVariantList *>();
        if (tag_list_ptr) {
            auto &tag_list = *tag_list_ptr;
            for (auto& tag_item : tag_list) {
                auto id = tag_item.value<QString>();
                if (id == tag_title) {
                    found = true;
                    break;
                }
            }
        }
        if (!found) {
            insertToList(tagsv, tag_class, tag_title);
        }
    }
}

} // namespace dsqt::bridge
