#include "qmlcontentmodel.h"
#include "content_model.h"
#include "dsQmlApplicationEngine.h"
#include <QVector>
#include <execution>

namespace dsqt::model {

QmlContentModel::QmlContentModel(ContentModelRef origin,QObject *parent)
    : QQmlPropertyMap(this, parent)
{
    mOrigin = origin.duplicate();
}

/*QJsonModel* QmlContentModel::qmlModel()
{
    //if(m_qmlModel == nullptr){
    //    m_qmlModel = mOrigin.getModel();
    //}
    return m_qmlModel;
}*/

void QmlContentModel::updateQmlModel(ContentModelRef model)
{
    model.updateQml();
}

QmlContentModel *QmlContentModel::getQmlContentModel(ContentModelRef model,ReferenceMap *referenceMap,QObject* parent)
{
    auto gmap=referenceMap;
    auto id = model.getId();
    if(id.isEmpty()){
        id = model.getName();
    }
    if (id.isEmpty()){
        Q_ASSERT(false);
    }
    QmlContentModel* result = referenceMap->value(id,nullptr);
    if(!result){
        qCDebug(lgContentModelVerbose)<<"Creating QmlContentModel for "<<id;
        auto map = new QmlContentModel(model,parent);
        QQmlEngine::setObjectOwnership(map,QQmlEngine::CppOwnership);
        if(!id.isEmpty()){
            referenceMap->insert(id,map);
        }
        result = map;
    }
    return result;
}

} //namespace dsqt::model
