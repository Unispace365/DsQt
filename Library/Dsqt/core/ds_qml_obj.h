#ifndef DS_QML_OBJ_H
#define DS_QML_OBJ_H

#include "core/dsenvironmentqml.h"
#include "core/dsqmlapplicationengine.h"
#include "model/content_model.h"
#include "model/qmlcontentmodel.h"
#include "model/reference_map.h"
#include "settings/dssettings_proxy.h"
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
    QML_NAMED_ELEMENT(DS)

    Q_PROPERTY(dsqt::DSEnvironmentQML* env READ env NOTIFY envChanged)
    Q_PROPERTY(dsqt::DSSettingsProxy* appSettings READ appSettings NOTIFY appSettingsChanged)
    Q_PROPERTY(dsqt::DSQmlApplicationEngine* engine READ engine NOTIFY engineChanged)
    Q_PROPERTY(dsqt::model::QmlContentModel* platform READ platform NOTIFY platformChanged)
    // Q_PROPERTY(dsqt::model::ContentHelper name READ name WRITE setname NOTIFY nameChanged FINAL)

  public:
    explicit DsQmlObj(int force, QObject* parent = nullptr);
    //
    static DsQmlObj* create(QQmlEngine* qmlEngine, QJSEngine*);
    //
    DSSettingsProxy* appSettings() const;
    //
    DSEnvironmentQML* env() const;
    //
    DSQmlApplicationEngine* engine() const;
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
    //
    Q_INVOKABLE bool isEventNow(QString event_id, QString localDateTime) const;
    //
    Q_INVOKABLE bool isEventNow(model::QmlContentModel* model, QString localDateTime) const;
    //
    Q_INVOKABLE bool isEventToday(QString event_id, QString localDateTime) const;
    //
    Q_INVOKABLE bool isEventToday(model::QmlContentModel* model, QString localDateTime) const;
    // Returns all events overlapping the specified span. Does not check for specific times or weekdays.
    Q_INVOKABLE QVariantList getEventsForSpan(QString start, QString end);

    // Returns all scheduled events. Does not sort the events.
    std::vector<model::ContentModelRef> getScheduledEvents();
    // Returns all events active at the specific date and time. Does not sort the events.
    std::vector<model::ContentModelRef> getEventsAtTime(QDateTime localDateTime);
    // Returns all events active at the specific date. Does not sort the events.
    std::vector<model::ContentModelRef> getEventsAtDate(QDate localDate);
    // Returns all events overlapping the specified span. Does not check for specific times or weekdays. Does not sort
    // the events.
    std::vector<model::ContentModelRef> getEventsForSpan(QDateTime spanStart, QDateTime spanEnd);

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
    DSQmlApplicationEngine* mEngine      = nullptr;
    model::QmlContentModel* mPlatformQml = nullptr;
    model::ContentModelRef  mPlatform;

  private slots:
    void updatePlatform();
};
} // namespace dsqt

#endif // DS_QML_OBJ_H
