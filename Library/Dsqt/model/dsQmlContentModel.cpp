#include "model/dsQmlContentModel.h"
#include "model/dsContentModel.h"
#include "core/dsQmlApplicationEngine.h"
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

QVariantMap DsQmlContentModel::toVariantMap()
{
    QVariantMap result;

    // Get all keys from the property map
    QStringList keys = this->keys();

    for (const QString& key : keys) {
        QVariant value = this->value(key);

        // Handle special case for "children" property
        if (key == "children" && value.canConvert<QVariantList>()) {
            QVariantList childrenList = value.toList();
            QStringList uidList;

            for (const QVariant& child : childrenList) {
                if (child.canConvert<QVariantMap>()) {
                    QVariantMap childMap = child.toMap();
                    if (childMap.contains("uid")) {
                        uidList.append(childMap.value("uid").toString());
                    }
                }
            }

            result.insert(key, QVariant(uidList));
        } else {
            result.insert(key, value);
        }
    }

    return result;
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

DsQmlContentModel *DsQmlContentModel::getQmlContentModel(ContentModelRef model)
{
    auto engine = DsQmlApplicationEngine::DefEngine();
    auto gmap = engine->getReferenceMap();
    auto id   = model.getId();
    if (id.isEmpty()) {
        id = model.getName();
    }
    if (id.isEmpty()) {
        Q_ASSERT(false);
    }
    DsQmlContentModel* result = gmap->value(id, nullptr);
    if (!result) {
        qCDebug(lgContentModelVerbose) << "Did not find " << id;
        auto map = new DsQmlContentModel(model, engine);
        result = map;
    }
    return result;
}

QVariant DsQmlContentModel::updateValue(const QString &key, const QVariant &input)
{
    qCDebug(lgContentModel)<< "cannot update ContentModel from qml";
    return input;
}

} // namespace dsqt::model
