#ifndef DSCONTENTMODEL_H
#define DSCONTENTMODEL_H

#include <QQmlPropertyMap>
#include <qqml.h>
#include <QtSql/QSql>
#include "dscontexthash.h"

namespace dsqt::model {
class DSContentModel;
class DSMutableContentModel;
typedef std::shared_ptr<DSContentModel> DSContentModelPtr;
typedef std::shared_ptr<DSMutableContentModel> DSMutableContentModelPtr;
class DSContentModel: public QQmlPropertyMap
{
    Q_OBJECT
    QML_ELEMENT
public:
    explicit DSContentModel(QObject *parent=nullptr);
    Q_INVOKABLE DSMutableContentModel* getMutableModel(dsContext context);


private:
    static std::map<int, DSMutableContentModelPtr> mMutables;
    static DSContentModelPtr Root;
    static DSContentModelPtr create();


    // QQmlPropertyMap interface
protected:
    virtual QVariant updateValue(const QString &key, const QVariant &input) override;
};

class DSMutableContentModel: public DSContentModel
{
    Q_OBJECT
    Q_PROPERTY(DSContentModel* base READ base NOTIFY baseChanged)
    QML_ELEMENT
    QML_UNCREATABLE("use DSContentModel.getMutableModel or DSMutableContentModel.addChild")
public:


    virtual DSContentModel* base();
private:
    explicit DSMutableContentModel(QObject *parent=nullptr);
signals:
    void baseChanged();
private:
    DSContentModel* mBase;

};

}//namespace ds::model

#endif // DSCONTENTMODEL_H
