#ifndef DSQTBASE_H
#define DSQTBASE_H

#include <QtQuick/QQuickPaintedItem>

class DsqtBase : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_DISABLE_COPY(DsqtBase)
public:
    explicit DsqtBase(QQuickItem *parent = nullptr);
    void paint(QPainter *painter) override;
    ~DsqtBase() override;
};

#endif // DSQTBASE_H
