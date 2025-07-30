#include "model/dsQmlContentModel.h"
#include "model/dsContentModel.h"

#include <QVector>

namespace dsqt::model {

DsQmlContentModel::DsQmlContentModel(ContentModelRef origin, QObject* parent)
    : QQmlPropertyMap(this, parent) {
    mOrigin = origin.duplicate();
}

/*QJsonModel* QmlContentModel::qmlModel()
{
    //if(m_qmlModel == nullptr){
    //    m_qmlModel = mOrigin.getModel();
    //}
    return m_qmlModel;
}*/

void DsQmlContentModel::updateQmlModel(ContentModelRef model) {
    model.updateQml();
}

DsQmlContentModel* DsQmlContentModel::getQmlContentModel(ContentModelRef model, ReferenceMap* referenceMap,
                                                         QObject* parent) {
    auto gmap = referenceMap;
    auto id   = model.getId();
    if (id.isEmpty()) {
        id = model.getName();
    }
    if (id.isEmpty()) {
        Q_ASSERT(false);
    }
    DsQmlContentModel* result = referenceMap->value(id, nullptr);
    if (!result) {
        qCDebug(lgContentModelVerbose) << "Creating QmlContentModel for " << id;
        auto map = new DsQmlContentModel(model, parent);
        QQmlEngine::setObjectOwnership(map, QQmlEngine::CppOwnership);
        if (!id.isEmpty()) {
            referenceMap->insert(id, map);
        }
        result = map;
    }
    return result;
}

} // namespace dsqt::model
