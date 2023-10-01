#ifndef DSNUEWAFFLESQUERY_H
#define DSNUEWAFFLESQUERY_H

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

class DsNueWafflesSqlQuery : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(DsNueWafflesSqlQuery)
public:
    explicit DsNueWafflesSqlQuery(DSQmlApplicationEngine *parent = nullptr);
    ~DsNueWafflesSqlQuery() override;
private:
    QSqlDatabase mDatabase;
    std::shared_ptr<model::DSContentModel*> mRoot;
    std::unordered_map<int,DSResource*> mAllResources;
    QString mResourceLocation;
    void queryTables();
};
}
#endif // DSNUEWAFFLESQUERY_H
