#ifndef DSSQLQUERY_H
#define DSSQLQUERY_H

#include <QObject>
#include <QLoggingCategory>
#include <QtSql/QSql>
#include <QtSql/QSqlDatabase>

Q_DECLARE_LOGGING_CATEGORY(settingsParser)
Q_DECLARE_LOGGING_CATEGORY(settingsParserWarn)

namespace dsqt {
class DsSqlQuery : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(DsSqlQuery)
public:
    explicit DsSqlQuery(QObject *parent = nullptr);
    ~DsSqlQuery() override;
private:
    QSqlDatabase mDatabase;
    void queryTables();
};
}
#endif // DSSQLQUERY_H
