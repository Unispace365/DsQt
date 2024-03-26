#ifndef BRIDGEUTILITY_H
#define BRIDGEUTILITY_H

#include <QObject>
#include <QQmlEngine>
#include <QQmlPropertyMap>
#include <content_model.h>

namespace dsqt::bridge {
class BridgeUtility : public QObject
{
    Q_OBJECT
    QML_ELEMENT
public:
    Q_PROPERTY(QQmlPropertyMap *platform READ platform NOTIFY platformChanged FINAL)

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
    QQmlPropertyMap *platform() const;
    void setRoot(model::ContentModelRef root);

signals:
    void platformChanged();

private:
    QQmlPropertyMap *m_platform;
    model::ContentModelRef m_contentRoot;
};
} // namespace dsqt::bridge

#endif // BRIDGEUTILITY_H
