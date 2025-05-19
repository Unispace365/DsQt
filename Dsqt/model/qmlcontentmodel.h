#ifndef QMLCONTENTMODEL_H
#define QMLCONTENTMODEL_H


#include <QQmlPropertyMap>
#include <QObject>
#include <QtQml>
#include "qjsonmodel.h"
#include "content_model.h"

namespace dsqt::model {
class QmlContentModel : public QQmlPropertyMap
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("QmlContentModels are proxys of ContentModels")
    Q_PROPERTY(QJsonModel* qmlModel READ qmlModel NOTIFY qmlModelChanged FINAL)
public:
    explicit QmlContentModel(ContentModelRef origin,QObject *parent = nullptr);

    QJsonModel* qmlModel();
    void updateQmlModel();
    ContentModelRef origin() const {return mOrigin;}
signals:
    void qmlModelChanged();
private:
    QJsonModel* m_qmlModel=nullptr;
    ContentModelRef mOrigin;
};
} //namespace dsqt::model

#endif // QMLCONTENTMODEL_H
