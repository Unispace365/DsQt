#ifndef DSSQLQUERY_H
#define DSSQLQUERY_H

#include <QObject>
#include <QLoggingCategory>
#include <QtSql/QSql>
#include <QtSql/QSqlDatabase>
#include <QQmlPropertyMap>
#include <model/dsresource.h>
#include <model/dscontentmodel.h>
#include "core/dsqmlapplicationengine.h"

Q_DECLARE_LOGGING_CATEGORY(settingsParser)
Q_DECLARE_LOGGING_CATEGORY(settingsParserWarn)


namespace dsqt {

class DsSqlQuery : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(DsSqlQuery)
public:
    explicit DsSqlQuery(DSQmlApplicationEngine *parent = nullptr);
    ~DsSqlQuery() override;
private:
    QSqlDatabase mDatabase;
    std::shared_ptr<model::DSContentModel*> mRoot;
    std::unordered_map<int,DSResourceRef> mAllResources;
    QString mResourceLocation;
    void queryTables();
};
}
#endif // DSSQLQUERY_H
