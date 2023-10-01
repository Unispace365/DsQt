#ifndef DSBRIDGEQUERY_H
#define DSBRIDGEQUERY_H

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

class DsBridgeSqlQuery : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(DsBridgeSqlQuery)
public:
    explicit DsBridgeSqlQuery(DSQmlApplicationEngine *parent = nullptr);
    ~DsBridgeSqlQuery() override;
private:
    QSqlDatabase mDatabase;
    std::shared_ptr<model::DSContentModel*> mRoot;
    std::unordered_map<int,DSResource*> mAllResources;
    QString mResourceLocation;
    void queryTables();
};
}
#endif // DSBRIDGEQUERY_H
