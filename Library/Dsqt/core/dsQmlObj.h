#ifndef DSQMLOBJ_H
#define DSQMLOBJ_H

#include <QObject>
#include <QQmlEngine>
#include <QDebug>
#include "core/dsQmlApplicationEngine.h"
#include "core/dsQmlEnvironment.h"
#include "settings/dsQmlSettingsProxy.h"
#include "model/dsContentModel.h"
#include "model/dsQmlContentHelper.h"
#include "model/dsQmlContentHelper.h"
#include "model/dsReferenceMap.h"
#include <qqmlintegration.h>

Q_DECLARE_LOGGING_CATEGORY(lgQmlObj)
Q_DECLARE_LOGGING_CATEGORY(lgQmlObjVerbose)
namespace dsqt {
class DsQmlObj : public QObject
{

    Q_OBJECT
    QML_SINGLETON
    QML_NAMED_ELEMENT(Ds)

    Q_PROPERTY(dsqt::DsQmlEnvironment* env READ env NOTIFY envChanged)
    Q_PROPERTY(dsqt::DsQmlSettingsProxy* appSettings READ appSettings NOTIFY appSettingsChanged)
    Q_PROPERTY(dsqt::DsQmlApplicationEngine* engine READ engine NOTIFY engineChanged)
    Q_PROPERTY(dsqt::model::QmlContentModel* platform READ platform NOTIFY platformChanged)
    //Q_PROPERTY(dsqt::model::ContentHelper name READ name WRITE setname NOTIFY nameChanged FINAL)
public:
    explicit DsQmlObj(int force,QObject *parent = nullptr);
    static DsQmlObj* create(QQmlEngine *qmlEngine, QJSEngine *);
    DsQmlSettingsProxy* appSettings() const;
    DsQmlEnvironment* env() const;
    DsQmlApplicationEngine* engine() const;
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
    DsQmlApplicationEngine* mEngine = nullptr;
    model::QmlContentModel* m_platform_qml = nullptr;
    model::ContentModelRef m_platform;

private slots:
    void updatePlatform();

};
}

#endif // DSQMLOBJ_H
