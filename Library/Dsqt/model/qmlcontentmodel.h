#ifndef QMLCONTENTMODEL_H
#define QMLCONTENTMODEL_H


#include <QQmlPropertyMap>
#include <QObject>
#include <QtQml>
#include "content_model.h"
#include "reference_map.h"
namespace dsqt::model {
// wrapper around QQmlPropertyMap with functions around creating a
// QmlContentModel from a ContentModelRef

class QmlContentModel : public QQmlPropertyMap
{
    Q_OBJECT
    QML_ELEMENT
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
