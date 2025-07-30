#ifndef DSQMLCONTENTMODEL_H
#define DSQMLCONTENTMODEL_H

#include "model/dsContentModel.h"
#include "model/dsReferenceMap.h"

#include <QObject>
#include <QQmlPropertyMap>
#include <QtQml>

namespace dsqt::model {
// wrapper around QQmlPropertyMap with functions around creating a
// QmlContentModel from a ContentModelRef

class QmlContentModel : public QQmlPropertyMap
{
    Q_OBJECT
    QML_NAMED_ELEMENT(DsContentModel)
    QML_UNCREATABLE("QmlContentModels are proxys of ContentModels")
    //Q_PROPERTY(QJsonModel* qmlModel READ qmlModel NOTIFY qmlModelChanged FINAL)
public:
    explicit QmlContentModel(ContentModelRef origin,QObject *parent = nullptr);

    //QJsonModel* qmlModel();
    void updateQmlModel(ContentModelRef model);
    ContentModelRef origin() const {return mOrigin;}
    static QmlContentModel* getQmlContentModel(ContentModelRef model,ReferenceMap* referenceMap,QObject* parent=nullptr);

signals:
    //void qmlModelChanged();
private:
    //QJsonModel* m_qmlModel=nullptr;
    ContentModelRef mOrigin;

};
} //namespace dsqt::model

#endif // QMLCONTENTMODEL_H
