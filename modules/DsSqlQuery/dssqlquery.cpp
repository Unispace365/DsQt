#include "dssqlquery.h"

#include <QtSql/QSqlQueryModel>
#include <QtSql/QSqlRecord>
#include <QtSql/QSqlField>
#include "settings/dssettings.h"
#include "dsenvironment.h"
#include "dscontentmodel.h"
#include "nw_sql_queries.h"


Q_LOGGING_CATEGORY(sqlQuery, "sqlQuery")
Q_LOGGING_CATEGORY(sqlQueryWarn, "sqlQuery.warning")
namespace dsqt {
DsSqlQuery::DsSqlQuery(QObject *parent)
    : QObject(parent)
{
    mDatabase = QSqlDatabase::addDatabase("QSQLITE");
    auto opFile = dsqt::DSEnvironment::engineSettings()->get<QString>("resource_db");
    if(opFile){
        mDatabase.setDatabaseName(opFile.value());

        if(!mDatabase.open()){
            qCWarning(sqlQueryWarn)<<"Could not open database;";
        } else {
            //get the raw nodes

        }
    }


}


DsSqlQuery::~DsSqlQuery()
{
}


void DsSqlQuery::queryTables(){
    QSqlQueryModel model;
    model.setQuery(qwaffle_nodes);
    auto rowcount = model.rowCount();
    model::DSContentModelPtr nodeTree;
    model::DSContentModelPtr nodeMap;
    for(int i=0;i<rowcount;i++){
        auto record = model.record(i);
        auto id = record.field(u"id"_qs).value().value<int>();

    }
}


}//namespace dsqt
