#ifndef DSSQLQUERY_H
#define DSSQLQUERY_H

#include <QtQuick/QQuickPaintedItem>
#include <dssettings.h>
class DsSqlQuery : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_DISABLE_COPY(DsSqlQuery)
public:
    explicit DsSqlQuery(QQuickItem *parent = nullptr);
    void paint(QPainter *painter) override;
    ~DsSqlQuery() override;
};

#endif // DSSQLQUERY_H
