#ifndef DSQMLCONTENTMODEL_H
#define DSQMLCONTENTMODEL_H

#include "model/dsContentModel.h"
#include "model/dsReferenceMap.h"

#include <QObject>
#include <QQmlEngine>
#include <QQmlPropertyMap>

namespace dsqt::model {

// Wrapper around QQmlPropertyMap with functions around creating a
// QmlContentModel from a ContentModelRef.
class DsQmlContentModel : public QQmlPropertyMap {
    Q_OBJECT
    QML_NAMED_ELEMENT(DsContentModel)
    QML_UNCREATABLE("QmlContentModels are proxys of ContentModels")

  public:
    explicit DsQmlContentModel(ContentModelRef origin, QObject* parent = nullptr);

    void                      updateQmlModel(ContentModelRef model);
    ContentModelRef           origin() const { return mOrigin; }
    QVariantMap               toVariantMap();
    static DsQmlContentModel* getQmlContentModel(ContentModelRef model, ReferenceMap* referenceMap,
                                                 QObject* parent = nullptr);
    static DsQmlContentModel* getQmlContentModel(ContentModelRef model);



  protected:
    virtual QVariant updateValue(const QString &key,const QVariant &input) override;

  private:
    ContentModelRef mOrigin;
};

} // namespace dsqt::model

#endif // QMLCONTENTMODEL_H
