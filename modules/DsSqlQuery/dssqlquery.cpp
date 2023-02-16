#include "dssqlquery.h"


#include <QtSql/QtSql>

#include "settings/dssettings.h"
#include "core/dsenvironment.h"
#include "model/dscontentmodel.h"
#include "model/dsresource.h"
#include "nw_sql_queries.h"


Q_LOGGING_CATEGORY(sqlQuery, "sqlQuery")
Q_LOGGING_CATEGORY(sqlQueryWarn, "sqlQuery.warning")
namespace dsqt {
DsSqlQuery::DsSqlQuery(QObject *parent)
    : QObject(parent)
{
    using namespace Qt::StringLiterals;

    mDatabase = QSqlDatabase::addDatabase("QSQLITE");
    auto resourceLocation = DSEnvironment::engineSettings()->getOr<QString>("resource_location","");
    auto opFile = dsqt::DSEnvironment::engineSettings()->get<QString>("resource_db");
    if(opFile){
        auto file = *opFile;
        if(QDir::isRelativePath(file)){
            file = QDir::cleanPath(resourceLocation+"/"+file);
        }
        mDatabase.setDatabaseName(file);

        if(!mDatabase.open()){
            qCWarning(sqlQueryWarn)<<"Could not open database at "<<file;
        } else {
            //get the raw nodes

        }
    }


}


DsSqlQuery::~DsSqlQuery()
{
}


void DsSqlQuery::queryTables(){
    QSqlQuery query(mDatabase);
    query.setForwardOnly(true);
    query.exec(qwaffle_nodes);
    if (query.lastError().isValid())
        qDebug() << query.lastError();
    auto insert = [](QVariant& node,QString& key,const QVariant& value){
        node.value<QQmlPropertyMap*>()->insert(key,value);
    };
    auto insertToList = [insert](QVariant& node,QString & key,const QVariant& value){

        if(node.value<QQmlPropertyMap*>()->contains(key)){
            auto list = node.value<QQmlPropertyMap*>()->value(key).value<QVariantList>();
            list.append(value);
            insert(node,key,list);
        }
    };
    QQmlPropertyMap* nodeTree = new QQmlPropertyMap(this);
    QHash<int,QVariant> nodeMap;

    //build the nodeMap
    while(query.next()){
        auto record = query.record();
        auto id = record.field("id").value().value<int>();
        if (query.lastError().isValid())
            qDebug() << query.lastError();
        QQmlPropertyMap* map = new QQmlPropertyMap(this);
        for(int j=0;j<record.count();j++){
            map->setProperty(record.fieldName(j).toStdString().c_str(),record.field(j).value());
            if (query.lastError().isValid())
                qDebug() << query.lastError();
        }
        nodeMap[id].setValue(map);
    }

    //build node tree
    for(auto& node_variant:nodeMap){
        auto node = node_variant.value<QQmlPropertyMap*>();
        auto branch = node->property("branch").value<QString>();
        if (branch == "content"){
            if(node->property("parent_id").isNull()){
                nodeTree->children().push_back(node);
            }
        }
    }

    //text fields
    auto textCheckColorHandler = [&query,&nodeMap,&insert](QString& queryStr){
        query.exec(queryStr);
        if (query.lastError().isValid())
            qDebug() << query.lastError();
        while(query.next()){
            auto record = query.record();
            auto nodeId = record.field("node_id").value().value<int>();
            if (query.lastError().isValid())
                qDebug() << query.lastError();

            auto node = nodeMap[nodeId];
            auto appKey = record.field("app_key").value().value<QString>();
            auto value = record.field("value").value().value<QString>().trimmed();
            insert(node,appKey,QVariant(value));
            auto plainValue = record.field("plain_value").value().value<QString>();
            insert(node,appKey+"_plain_value",plainValue);
        }
    };

    textCheckColorHandler(qwaffles_text_fields);
    textCheckColorHandler(qwaffles_checkboxes);
    textCheckColorHandler(qwaffles_color);

    //get Resources
    query.exec(resources);
    auto resourceLocation = DSEnvironment::engineSettings()->getOr<QString>("resource_location","");
    resourceLocation = DSEnvironment::expand(resourceLocation);

    if (query.lastError().isValid())
        qDebug() << query.lastError();
    while(query.next()){
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
        if(type != DSResource::ResourceType::WEB && type != DSResource::ResourceType::YOUTUBE){
            url = "file:///"+QDir::cleanPath(resourceLocation+"/"+path+"/"+filename);
        }


        mAllResources[resId] = dsqt::DSResource::create(resId,type,duration,width,height,url,thumbId,this);
    }

    //handle media fields
    query.exec(qwaffles_media_fields);
    if (query.lastError().isValid())
        qDebug() << query.lastError();
    while(query.next()){
        auto record = query.record();
        auto nodeId = record.field("node_id").value().value<int>();
        if (query.lastError().isValid())
            qDebug() << query.lastError();

        auto node = nodeMap[nodeId];
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

        if(media_res){

            insert(node,appKey+"media_res",QVariant::fromValue(media_res));
            if(media_thumb_res){
                insert(node,appKey+"media_thumb_res",QVariant::fromValue(media_thumb_res));
            }
            if(appKey == "media"){
                insert(node,"media_res",QVariant::fromValue(media_res));
            }
            if(floatable){
                insert(node,appKey+"floatable",floatable);
                insert(node,appKey+"float_autoplay",float_autoplay);
                insert(node,appKey+"float_loop",float_loop);
                insert(node,appKey+"float_volume",float_volume);
                insert(node,appKey+"float_touch",float_touch);
                insert(node,appKey+"float_page",float_page);
                insert(node,appKey+"float_fullscreen",float_fullscreen);
                insert(node,appKey+"hide_thumbnail",hide_thumbnail);
            }
        }

    }

    //dropdowns
    query.exec(qwaffles_dropdown_fields);
    if (query.lastError().isValid())
        qDebug() << query.lastError();
    while(query.next()){
        auto record = query.record();
        auto nodeId = record.field("node_id").value().value<int>();
        if (query.lastError().isValid())
            qDebug() << query.lastError();

        auto node = nodeMap[nodeId];
        auto appKey = record.field("app_key").value().value<QString>();
        auto allow_multiple = record.field("allow_multiple").value().value<bool>();
        auto option_save_value = record.field("option_save_value").value().value<QString>();
        auto dropdown_label = record.field("dropdown_label").value().value<QString>();
        if(allow_multiple){
            insertToList(node,appKey,option_save_value);
        } else {
            insert(node,appKey,option_save_value);
            insert(node,appKey+"_dropdown_label",dropdown_label);
        }
    }

    //selections
    query.exec(qwaffles_selections);
    if (query.lastError().isValid())
        qDebug() << query.lastError();
    while(query.next()){
        auto record = query.record();
        auto nodeId = record.field("owner_id").value().value<int>();
        if (query.lastError().isValid())
            qDebug() << query.lastError();

        auto node = nodeMap[nodeId];
        auto appKey = record.field("app_key").value().value<QString>();
        auto allow_multiple = record.field("allow_multiple").value().value<bool>();
        auto selection_nodeId = record.field("node_id").value().value<int>();
        auto definition_id = record.field("definition_id").value().value<int>();
        if(app_key="breifing_pinboard"){
            nodeTree->setProperty("briefing_pinboard_definition_id",definition_id);
        }
        if(allow_multiple){
            insertToList(node,appKey,selection_nodeId);
        } else {
            insert(node,appKey,selection_nodeId);
        }
    }

}



}//namespace dsqt
