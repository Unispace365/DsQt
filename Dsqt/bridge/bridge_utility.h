#ifndef BRIDGEUTILITY_H
#define BRIDGEUTILITY_H

#include <QObject>
#include <QQmlEngine>
#include <QQmlPropertyMap>
#include <model/content_model.h>
#include <model/qmlcontentmodel.h>

Q_DECLARE_LOGGING_CATEGORY(lgBrUt)
Q_DECLARE_LOGGING_CATEGORY(lgBrUtVerbose)

namespace dsqt::bridge {


class BridgeUtility : public QObject
{

    Q_OBJECT
    QML_ELEMENT
public:
    Q_PROPERTY(dsqt::model::QmlContentModel *platform READ platform NOTIFY platformChanged FINAL)
    BridgeUtility(QObject *parent = nullptr,
                  dsqt::model::ContentModelRef contentRoot = dsqt::model::ContentModelRef());

    Q_INVOKABLE bool isEventNow(QString event_id, QString ldt);
    bool isEventNow(model::ContentModelRef event, QDateTime ldt);

    Q_INVOKABLE QVariantList getEventsForSpan(QString start, QString end);
    std::vector<model::ContentModelRef> getEventsForSpan(QDateTime start, QDateTime end);
    /*getEvents();
    getContent();

    //qml
    getQmlPlatform();
    getQmlContent();
    getQmlContentModel();
    */
    /**
     * @brief getRecord searches for a record with an app_id
     * of name in the parent record with the id of id.
     * @param id
     * @param name
     * @return
     */
    Q_INVOKABLE model::QmlContentModel *getRecord(QString id = "", QString name = "") const;
    void setRoot(model::ContentModelRef root);

    model::QmlContentModel *platform() const;

signals:
    void platformChanged();


private:

    model::ContentModelRef m_contentRoot;
};
} // namespace dsqt::bridge

#endif // BRIDGEUTILITY_H
