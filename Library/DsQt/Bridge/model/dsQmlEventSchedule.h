#ifndef DSQMLEVENTSCHEDULE_H
#define DSQMLEVENTSCHEDULE_H

/**
 * @file dsQmlEventSchedule.h
 * @brief Event scheduling classes for QML integration.
 *
 * ## Overview
 *
 * This module provides two classes for exposing time-based scheduled events to QML:
 *
 * - **DsQmlEvent** — A read-only wrapper around a single scheduled event record from the
 *   bridge database. Exposes start/end times, title, type, effective days, and display order.
 *
 * - **DsQmlEventSchedule** (QML: `DsEventSchedule`) — Queries the bridge database for events,
 *   filters and sorts them, and exposes two views to QML:
 *   - `events` — all matching events sorted by priority (highest priority first).
 *   - `timeline` — a non-overlapping chronological timeline resolved from the priority-sorted
 *     events. At any moment in time the highest-priority event wins, and consecutive slots
 *     occupied by the same event are merged into a single entry.
 *   - `current` — a convenience pointer to the highest-priority event that is active right now.
 *
 * ## How It Works
 *
 * `DsQmlEventSchedule` reacts to two signals:
 *   1. **Clock ticks** — it connects to a `DsQmlClock` (a default internal clock is created
 *      automatically). Every time the clock emits `secondsChanged` the schedule re-evaluates
 *      which events are active.
 *   2. **Database updates** — it also connects to `DsQmlBridge::databaseChanged` so the event
 *      list is refreshed whenever the underlying data changes.
 *
 * The heavy lifting (database query, filtering, sorting, timeline construction) is performed on
 * a background thread via `QtConcurrent::run`. The result is handed back to the main thread
 * through a `QFutureWatcher`, which then updates the exposed lists and emits `eventsChanged`
 * only when the data has actually changed.
 *
 * ## Timeline Algorithm
 *
 * 1. Retrieve all events for the current day from the bridge database.
 * 2. Optionally filter to a single event type (see `DsQmlEventSchedule::type`).
 * 3. Sort by priority relative to the current moment.
 * 4. Collect all unique start/end times as *checkpoints*.
 * 5. At each checkpoint, re-sort the events and record the winner (highest priority).
 * 6. Truncate each slot so it ends when the next checkpoint begins.
 * 7. Remove zero- or negative-duration slots.
 * 8. Merge adjacent slots that resolve to the same event.
 *
 * ## QML Usage
 *
 * @code{.qml}
 * import Dsqt
 *
 * DsEventSchedule {
 *     id: schedule
 *     type: "meeting"   // leave empty to include all event types
 *
 *     // Iterate over all priority-sorted events for today:
 *     Repeater { model: schedule.events; delegate: MyEventDelegate {} }
 *
 *     // Or use the resolved, non-overlapping timeline:
 *     Repeater { model: schedule.timeline; delegate: MyTimelineDelegate {} }
 *
 *     // Access the currently active event:
 *     Text { text: schedule.current ? schedule.current.title : "No event" }
 * }
 * @endcode
 *
 * An external `DsQmlClock` can be supplied via the `clock` property to control (or mock) the
 * current time, which is especially useful for testing or for "preview at time" features.
 */

#include "bridge/dsBridgeDatabase.h"
#include "ui/dsQmlClock.h"
#include "model/dsContentModel.h"

#include <QColor>
#include <QDateTime>
#include <QFutureWatcher>
#include <QObject>
#include <QQmlEngine>
#include <QQmlListProperty>
#include <QQmlPropertyMap>
#include <QTimer>
#include <QtConcurrentRun>
// #include <qloggingcategory.h>

// Q_DECLARE_LOGGING_CATEGORY(lgEventSchedule)
// Q_DECLARE_LOGGING_CATEGORY(lgEventScheduleVerbose)
namespace dsqt::model {

/**
 * @brief A QML-accessible wrapper around a single scheduled event record.
 *
 * `DsQmlEvent` is created and owned by `DsQmlEventSchedule`; it should not be
 * instantiated directly from QML (hence `QML_ANONYMOUS`).
 *
 * The `order` property reflects the event's position in the priority-sorted list produced
 * by `DsQmlEventSchedule`. It can be used together with `color()` to assign a consistent
 * color to each event across repeaters or delegates.
 */
class DsQmlEvent : public QObject {
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(QString uid READ uid CONSTANT)
    Q_PROPERTY(QString type READ type CONSTANT)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(QDateTime start READ start WRITE setStart NOTIFY startChanged)
    Q_PROPERTY(QDateTime end READ end WRITE setEnd NOTIFY endChanged)
    Q_PROPERTY(qint8 days READ days CONSTANT)
    Q_PROPERTY(qsizetype order READ order WRITE setOrder NOTIFY orderChanged)
    Q_PROPERTY(double secondsSinceMidnight READ secondsSinceMidnight CONSTANT)
    Q_PROPERTY(double durationInSeconds READ durationInSeconds CONSTANT)
    Q_PROPERTY(dsqt::model::ContentModel* model READ model FINAL)
    Q_PROPERTY(bool isNow READ isNow WRITE setIsNow NOTIFY isNowChanged FINAL)
  public:
    explicit DsQmlEvent(QObject* parent = nullptr)
        : QObject(parent) {}

    DsQmlEvent(const bridge::DatabaseRecord& record, qsizetype order, QObject* parent = nullptr);

    DsQmlEvent* duplicate() const { return new DsQmlEvent(m_record, m_order, parent()); }

    QString uid() const { return m_record.value("uid").toString(); }

    QString type() const { return m_record.value("type_name").toString(); }

    QString title() const { return m_title; }
    void    setTitle(const QString& title) {
        if (m_title == title) return;
        m_title = title;
        emit titleChanged();
    }

    QDateTime start() const { return m_start; }
    void      setStart(const QDateTime& start) {
        if (m_start == start) return;
        m_start = start;
        emit startChanged();
    }

    QDateTime end() const { return m_end; }
    void      setEnd(const QDateTime& end) {
        if (m_end == end) return;
        m_end = end;
        emit endChanged();
    }

    int days() const;
    ContentModel* model() const;

    qsizetype order() const { return m_order; }
    void      setOrder(qsizetype order) {
        if (m_order != order) {
            m_order = order;
            emit orderChanged();
        }
    }

    Q_INVOKABLE QString color(const QVariantList& colors) const {
        if (colors.isEmpty()) {
            return QStringLiteral("red"); // Fallback if no colors provided
        }
        return colors.at(m_order % colors.size()).toString();
    }

    double secondsSinceMidnight() const { return QTime(0, 0).secsTo(m_start.time()); }
    double durationInSeconds() const { return m_start.time().secsTo(m_end.time()); }

    const bridge::DatabaseRecord& record() const { return m_record; }

    bool operator==(const DsQmlEvent& rhs) const {
        // Do not compare titles.
        return (m_order == rhs.m_order && m_start == rhs.m_start && m_end == rhs.m_end && m_isNow == rhs.m_isNow && uid() == rhs.uid());
    }
    bool operator!=(const DsQmlEvent& rhs) const { return !(*this == rhs); }

    bool isNow() const;
    void setIsNow(bool newIsNow);

  signals:
    void titleChanged();
    void startChanged();
    void endChanged();
    void orderChanged();

    void isNowChanged();

  private:
    bridge::DatabaseRecord m_record;
    QString                m_title;
    QDateTime              m_start;
    QDateTime              m_end;
    qsizetype              m_order;
    mutable dsqt::model::ContentModel *m_model = nullptr;
    bool m_isNow=false;
};

/**
 * @brief Queries scheduled events from the bridge database and exposes them to QML.
 *
 * Registered in QML as `DsEventSchedule`.
 *
 * The schedule listens for two update triggers:
 *  - **Clock ticks**: via a connected `DsQmlClock`. A default internal clock is created
 *    automatically; supply an external one via the `clock` property to control or mock time.
 *  - **Database changes**: via `DsQmlBridge::databaseChanged`.
 *
 * On each update the schedule runs a background pipeline that filters, sorts, and resolves
 * the day's events, then notifies QML through `eventsChanged` only when the result differs
 * from the previous state.
 *
 * @note All three list properties (`events`, `timeline`, `current`) share a single
 *       `eventsChanged` notification signal.
 */
class DsQmlEventSchedule : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(DsEventSchedule)
    Q_PROPERTY(QString type READ type WRITE setType NOTIFY typeChanged)
    Q_PROPERTY(QQmlListProperty<DsQmlEvent> events READ events NOTIFY eventsChanged)
    Q_PROPERTY(QQmlListProperty<DsQmlEvent> timeline READ timeline NOTIFY eventsChanged)
    Q_PROPERTY(dsqt::model::DsQmlEvent* current READ current NOTIFY eventsChanged)
    Q_PROPERTY(dsqt::ui::DsQmlClock* clock READ clock WRITE setClock NOTIFY clockChanged)

  public:
    /// Database column name for an event's start date/time.
    inline static const char* StartDateTime = "start_date_time";
    /// Database column name for an event's end date/time.
    inline static const char* EndDateTime   = "end_date_time";
    /// Database column name for the bitmask of days on which an event is effective.
    inline static const char* EffectiveDays = "effective_days";

    explicit DsQmlEventSchedule(QObject* parent = nullptr);
    DsQmlEventSchedule(const QString& type_name, QObject* parent = nullptr);
    ~DsQmlEventSchedule();

    /// Returns the event type filter, or an empty string if all types are included.
    const QString& type() const { return m_type_name; }
    /// Sets the event type filter. When non-empty only events of this type are returned.
    void setType(const QString& type) {
        if (type == m_type_name) return;
        m_type_name = type;
        emit typeChanged();
    }

    /// Returns all matching events for the current day, sorted from highest to lowest priority.
    QQmlListProperty<DsQmlEvent> events();
    /// Returns a non-overlapping chronological timeline. Overlapping slots are resolved to the
    /// highest-priority event; adjacent slots for the same event are merged.
    QQmlListProperty<DsQmlEvent> timeline();

    /// Returns the highest-priority event that is currently active right now, or nullptr if none.
    DsQmlEvent* current() const {
        if (m_events.isEmpty() || !m_events.front()->isNow()) return nullptr;
        return m_events.front();
    }

    /// Returns the clock used to determine the current time.
    ui::DsQmlClock* clock() const { return m_clock; }
    /// Sets the clock used to determine the current time. An internal default clock is used
    /// when no external clock is supplied.
    void setClock(ui::DsQmlClock* clock) {
        if (m_clock == clock) return;

        if (m_clock) {
            disconnect(m_clock, &ui::DsQmlClock::secondsChanged, this, &DsQmlEventSchedule::updateNow);
        }

        m_clock = clock;

        if (m_clock) {
            connect(m_clock, &ui::DsQmlClock::secondsChanged, this, &DsQmlEventSchedule::updateNow);
        }

        emit clockChanged();
    }

  signals:
    void typeChanged();
    void eventsChanged();
    void clockChanged();

  private:
    void updateNow();
    void update(const QDateTime& localDateTime);

  private slots:
    void onUpdated();

  private:
    struct FutureResult {
        QList<DsQmlEvent*> events;
        QList<DsQmlEvent*> timeline;
    };

    QString                      m_type_name = "";  // Event record type. If empty, all events are included.
    QDateTime                    m_local_date_time; // Local date and time.
    QList<DsQmlEvent*>           m_events;          // All events.
    QList<DsQmlEvent*>           m_timeline; // List of events where overlapping events are resolved to a single event.
    QFutureWatcher<FutureResult> m_watcher;  // Tracks the async task.
    dsqt::ui::DsQmlClock*        m_clock        = nullptr;
    dsqt::ui::DsQmlClock*        m_defaultClock = nullptr;
};

} // namespace dsqt::model

Q_DECLARE_METATYPE(dsqt::model::DsQmlEvent)
// Q_DECLARE_METATYPE(QList<dsqt::model::DsQmlEvent*>)

#endif // SCHEDULE_HELPER_H
