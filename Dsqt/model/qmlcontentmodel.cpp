#include "qmlcontentmodel.h"
#include "content_model.h"

namespace dsqt::model {
QmlContentModel::QmlContentModel(ContentModelRef origin,QObject *parent)
    : QQmlPropertyMap{parent}
{
    mOrigin = origin;
}

QJsonModel* QmlContentModel::qmlModel()
{
    if(m_qmlModel == nullptr){
        m_qmlModel = mOrigin.getModel();
    }
    return m_qmlModel;
}

} //namespace dsqt::model
