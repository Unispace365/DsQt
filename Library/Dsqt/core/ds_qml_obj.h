#ifndef DS_QML_OBJ_H
#define DS_QML_OBJ_H

#include <QObject>
#include <QQmlEngine>
#include <QDebug>
#include "core/dsqmlapplicationengine.h"
#include "core/dsenvironmentqml.h"
#include "settings/dssettings_proxy.h"
#include "model/content_model.h"
#include "model/content_helper.h"
#include "model/qmlcontentmodel.h"
#include "model/reference_map.h"
#include <qqmlintegration.h>

Q_DECLARE_LOGGING_CATEGORY(lgQmlObj)
Q_DECLARE_LOGGING_CATEGORY(lgQmlObjVerbose)
namespace dsqt {
class DsQmlObj : public QObject
{

    Q_OBJECT
    QML_SINGLETON
    QML_NAMED_ELEMENT(DS)

    Q_PROPERTY(dsqt::DSEnvironmentQML* env READ env NOTIFY envChanged)
    Q_PROPERTY(dsqt::DSSettingsProxy* appSettings READ appSettings NOTIFY appSettingsChanged)
    Q_PROPERTY(dsqt::DSQmlApplicationEngine* engine READ engine NOTIFY engineChanged)
    Q_PROPERTY(dsqt::model::QmlContentModel* platform READ platform NOTIFY platformChanged)
    //Q_PROPERTY(dsqt::model::ContentHelper name READ name WRITE setname NOTIFY nameChanged FINAL)
public:
    explicit DsQmlObj(int force,QObject *parent = nullptr);
    static DsQmlObj* create(QQmlEngine *qmlEngine, QJSEngine *);
    DSSettingsProxy* appSettings() const;
    DSEnvironmentQML* env() const;
    DSQmlApplicationEngine* engine() const;
    model::QmlContentModel *platform();

    //QML_INVOKABLES
    Q_INVOKABLE bool isEventNow(QString event_id, QString ldt);
    Q_INVOKABLE bool isEventNow(model::QmlContentModel* event_id, QString ldt);
    bool isEventNow(model::ContentModelRef event, QDateTime ldt);

    Q_INVOKABLE QVariantList getEventsForSpan(QString start, QString end);
    std::vector<model::ContentModelRef> getEventsForSpan(QDateTime start, QDateTime end);

    /**
     * @brief getRecord searches for a record with an app_id
     * of name in the parent record with the id of id.
     * @param id
     * @param name
     * @return
     */
    Q_INVOKABLE model::QmlContentModel *getRecordById(QString id) const;

signals:

    void envChanged();
    void appSettingsChanged();
    void engineChanged();
    void platformChanged();

private:
    DSQmlApplicationEngine* mEngine = nullptr;
    model::QmlContentModel* m_platform_qml = nullptr;
    model::ContentModelRef m_platform;

private slots:
    void updatePlatform();

};
}

#endif // DS_QML_OBJ_H
