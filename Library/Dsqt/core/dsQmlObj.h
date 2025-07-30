#ifndef DSQMLOBJ_H
#define DSQMLOBJ_H

#include "core/dsQmlApplicationEngine.h"
#include "core/dsQmlEnvironment.h"
#include "settings/dsQmlSettingsProxy.h"
#include "model/dsContentModel.h"
#include "model/dsReferenceMap.h"

#include <QDebug>
#include <QObject>
#include <QQmlEngine>
#include <qqmlintegration.h>

Q_DECLARE_LOGGING_CATEGORY(lgQmlObj)
Q_DECLARE_LOGGING_CATEGORY(lgQmlObjVerbose)
namespace dsqt {

class DsQmlObj : public QObject {

    Q_OBJECT
    QML_SINGLETON
    QML_NAMED_ELEMENT(Ds)

    Q_PROPERTY(dsqt::DsQmlEnvironment* env READ env NOTIFY envChanged)
    Q_PROPERTY(dsqt::DsQmlSettingsProxy* appSettings READ appSettings NOTIFY appSettingsChanged)
    Q_PROPERTY(dsqt::DsQmlApplicationEngine* engine READ engine NOTIFY engineChanged)
    Q_PROPERTY(dsqt::model::QmlContentModel* platform READ platform NOTIFY platformChanged)

  public:
    explicit DsQmlObj(QQmlEngine* qmlEngine, QJSEngine* jsEngine = nullptr, QObject* parent = nullptr);
    //
    static DsQmlObj* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine = nullptr);
    //
    DsQmlSettingsProxy* appSettings() const;
    DsQmlEnvironment* env() const;
    DsQmlApplicationEngine* engine() const;
    //
    model::QmlContentModel* platform();

    /**
     * @brief getRecord searches for a record with an app_id
     * of name in the parent record with the id of id.
     * @param id
     * @param name
     * @return
     */
    Q_INVOKABLE model::QmlContentModel* getRecordById(QString id) const;
    // //
    // Q_INVOKABLE bool isEventNow(QString event_id, QString localDateTime) const;
    // //
    // Q_INVOKABLE bool isEventNow(model::QmlContentModel* model, QString localDateTime) const;
    // //
    // Q_INVOKABLE bool isEventToday(QString event_id, QString localDateTime) const;
    // //
    // Q_INVOKABLE bool isEventToday(model::QmlContentModel* model, QString localDateTime) const;
    // // Returns all events overlapping the specified span. Does not check for specific times or weekdays.
    // Q_INVOKABLE QVariantList getEventsForSpan(QString start, QString end);

    // // Returns all scheduled events. Does not sort the events.
    // std::vector<model::ContentModelRef> getScheduledEvents();
    // // Returns all events active at the specific date and time. Does not sort the events.
    // std::vector<model::ContentModelRef> getEventsAtTime(QDateTime localDateTime);
    // // Returns all events active at the specific date. Does not sort the events.
    // std::vector<model::ContentModelRef> getEventsAtDate(QDate localDate);
    // // Returns all events overlapping the specified span. Does not check for specific times or weekdays. Does not sort
    // // the events.
    // std::vector<model::ContentModelRef> getEventsForSpan(QDateTime spanStart, QDateTime spanEnd);

    // Returns whether the specified event is currently scheduled, taking into account specific times or weekdays.
    static bool isEventNow(const model::ContentModelRef& event, QDateTime localDateTime);
    // Returns whether the specified event is scheduled for today, taking into account the weekdays.
    static bool isEventToday(const model::ContentModelRef& event, QDate localDate);
    // Returns whether the specified event is within the time span.
    static bool isEventWithinSpan(const model::ContentModelRef& event, QDateTime spanStart, QDateTime spanEnd);

    // Removes all events that are not of the specified type. Returns the number of removed events.
    static size_t filterEvents(std::vector<model::ContentModelRef>& events, const QString& typeName);
    // Removes all events that are not scheduled at the specified date and time. Returns the number of removed events.
    static size_t filterEvents(std::vector<model::ContentModelRef>& events, QDateTime localDateTime);
    // Removes all events that are not scheduled at the specified date. Returns the number of removed events.
    static size_t filterEvents(std::vector<model::ContentModelRef>& events, QDate localDate);
    // Removes all events that are not within the specified time range. Does not check for specific times or weekdays.
    // Returns the number of removed events.
    static size_t filterEvents(std::vector<model::ContentModelRef>& events, QDateTime spanStart, QDateTime spanEnd);

    // Sorts events by the default sorting heuristic. Sorted from highest to lowest priority.
    static void sortEvents(std::vector<model::ContentModelRef>& events, QDateTime localDateTime);

  signals:
    void envChanged();
    void appSettingsChanged();
    void engineChanged();
    void platformChanged();

  private:
    DsQmlApplicationEngine* mEngine = nullptr;
    model::QmlContentModel* mPlatformQml = nullptr;
    model::ContentModelRef  mPlatform;

  private slots:
    void updatePlatform();
};
} // namespace dsqt

#endif // DS_QML_OBJ_H
