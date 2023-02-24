#include "dssqlquery.h"

#include <QSharedPointer>
#include <QString>
#include <QtSql/QtSql>
#include "core/dsenvironment.h"
#include "settings/dssettings.h"
#include "settings/dssettings_proxy.h"
#include "model/dscontentmodel.h"
#include "model/dsresource.h"
#include "nw_sql_queries.h"

Q_LOGGING_CATEGORY(sqlQuery, "sqlQuery")
Q_LOGGING_CATEGORY(sqlQueryWarn, "sqlQuery.warning")
using namespace dsqt::model;
namespace dsqt {

DsSqlQuery::DsSqlQuery(DSQmlApplicationEngine *parent)
    : QObject(parent)
{
    using namespace Qt::StringLiterals;
    connect(
        parent,
        &DSQmlApplicationEngine::onInit,
        this,
        [this]() {
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
                    qCWarning(sqlQueryWarn) << "Could not open database at " << file;
                } else {
                    //get the raw nodes

                    queryTables();
                }
            }
        },
        Qt::ConnectionType::DirectConnection);
}

DsSqlQuery::~DsSqlQuery() {}

void DsSqlQuery::queryTables()
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
            = dsqt::DSResource::create(resId, type, duration, width, height, url, thumbId, nullptr);
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
        DSResourceRef stream_res = mAllResources[stream_res_id];
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

} //namespace dsqt
