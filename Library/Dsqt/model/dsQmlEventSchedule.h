#ifndef DSQMLEVENTSCHEDULE_H
#define DSQMLEVENTSCHEDULE_H

#include "model/dsContentModel.h"
#include "ui/dsQmlClock.h"

#include <QColor>
#include <QDateTime>
#include <QFutureWatcher>
#include <QObject>
#include <QQmlEngine>
#include <QQmlListProperty>
#include <QQmlPropertyMap>
#include <QtConcurrentRun>

Q_DECLARE_LOGGING_CATEGORY(lgEventSchedule)
Q_DECLARE_LOGGING_CATEGORY(lgEventScheduleVerbose)
namespace dsqt::model {

// Wraps a scheduled event into a QML object.
class DsQmlEvent : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString uid READ uid CONSTANT)
    Q_PROPERTY(QString type READ type CONSTANT)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(QDateTime start READ start WRITE setStart NOTIFY startChanged)
    Q_PROPERTY(QDateTime end READ end WRITE setEnd NOTIFY endChanged)
    Q_PROPERTY(qint8 days READ days CONSTANT)
    Q_PROPERTY(qsizetype order READ order WRITE setOrder NOTIFY orderChanged)
    Q_PROPERTY(double secondsSinceMidnight READ secondsSinceMidnight CONSTANT)
    Q_PROPERTY(double durationInSeconds READ durationInSeconds CONSTANT)

  public:
    explicit DsQmlEvent(QObject* parent = nullptr)
        : QObject(parent) {}

    DsQmlEvent(ContentModelRef model, qsizetype order, QObject* parent = nullptr);

    DsQmlEvent* duplicate() const { return new DsQmlEvent(m_model, m_order, parent()); }

    QString uid() const { return m_model.getPropertyString("uid"); }

    QString type() const { return m_model.getPropertyString("type_name"); }

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

    const ContentModelRef& model() const { return m_model; }

    // Returns whether the event is scheduled for the specified date and time, taking into account specific times or
    // weekdays.
    bool isNow(const QDateTime& localDateTime) const {
        if (!isToday(localDateTime.date())) return false;
        if (localDateTime.time() < m_start.time() || localDateTime.time() >= m_end.time()) return false;
        return true;
    }
    // Returns whether the event is scheduled for the specified day, taking into account the weekdays.
    bool isToday(const QDate& localDate) const {
        if (!m_start.isValid()) return false;
        if (!m_end.isValid()) return false;
        if (localDate < m_start.date() || localDate > m_end.date()) return false;

        // Returns the weekday (0 to 6, where 0 = Sunday, 1 = Monday, ..., 6 = Saturday).
        const auto dayNumber = localDate.dayOfWeek() % 7;
        const int  dayFlag   = 0x1 << dayNumber;
        if (days() & dayFlag) return true;

        return false;
    }
    // Returns whether the event is scheduled during the time span.
    bool isWithinSpan(const QDateTime& spanStart, const QDateTime& spanEnd) const {
        if (spanStart > spanEnd) return false; // Invalid span.
        if (!m_start.isValid()) return false;
        if (!m_end.isValid()) return false;
        if (m_end < m_start) return false; // Invalid value.
        return (spanEnd >= m_start && m_end >= spanStart);
    }

    bool operator==(const DsQmlEvent& rhs) const {
        // Do not compare titles.
        return (m_order == rhs.m_order && m_start == rhs.m_start && m_end == rhs.m_end && uid() == rhs.uid());
    }
    bool operator!=(const DsQmlEvent& rhs) const { return !(*this == rhs); }

  signals:
    void titleChanged();
    void startChanged();
    void endChanged();
    void orderChanged();

  private:
    model::ContentModelRef m_model;
    QString                m_title;
    QDateTime              m_start;
    QDateTime              m_end;
    qsizetype              m_order;
};

// Provides a list of scheduled events, optionally filtered by type, and exposes it to QML.
class DsQmlEventSchedule : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(DsEventSchedule)
    Q_PROPERTY(QString type READ type WRITE setType NOTIFY typeChanged)
    Q_PROPERTY(QList<DsQmlEvent*> events READ events NOTIFY eventsChanged)
    Q_PROPERTY(QList<DsQmlEvent*> timeline READ timeline NOTIFY eventsChanged)
    Q_PROPERTY(DsQmlEvent* current READ current NOTIFY eventsChanged)
    Q_PROPERTY(ui::DsQmlClock* clock READ clock WRITE setClock NOTIFY clockChanged)

  public:
    inline static const char* StartDate     = "start_date";
    inline static const char* EndDate       = "end_date";
    inline static const char* StartTime     = "start_time";
    inline static const char* EndTime       = "end_time";
    inline static const char* EffectiveDays = "effective_days";

    explicit DsQmlEventSchedule(QObject* parent = nullptr);
    DsQmlEventSchedule(const QString& type_name, QObject* parent = nullptr);

    // Returns the event type, or an empty string if not set.
    const QString& type() const { return m_type_name; }
    // Sets the event type, causing events to be filtered if not empty.
    void setType(const QString& type) {
        if (type == m_type_name) return;

        m_type_name = type;
        emit typeChanged();

        updateNow();
    }

    // Returns all events, sorted from highest to lowest priority.
    QList<DsQmlEvent*> events() const { return m_events; }
    // Returns all events as a timeline, with all overlapping events resolved to a single event.
    QList<DsQmlEvent*> timeline() const;
    // Returns the currently active event, or nullptr if no event is active.
    DsQmlEvent* current() const {
        if (m_events.isEmpty()) {
            return nullptr;
        }
        return m_events.front();
    }
    //
    ui::DsQmlClock* clock() const { return m_clock; }
    //
    void setClock(ui::DsQmlClock* clock) {
        if (m_clock == clock) return;

        if (m_clock) {
            disconnect(m_clock, &ui::DsQmlClock::minutesChanged, this, &DsQmlEventSchedule::updateNow);
        }

        m_clock = clock;

        if (m_clock) {
            connect(m_clock, &ui::DsQmlClock::minutesChanged, this, &DsQmlEventSchedule::updateNow,
                    Qt::ConnectionType::QueuedConnection);
        }

        emit clockChanged();
    }

  public:
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
    void typeChanged();
    void eventsChanged();
    void clockChanged();

  private:
    void updateNow();
    void update(const QDateTime& localDateTime);

    static void sortEvents(QList<DsQmlEvent*>& events, const QDateTime& localDateTime);

  private slots:
    void onUpdated();

  private:
    QString                    m_type_name = "";  // Event record type. If empty, all events are included.
    QDateTime                  m_local_date_time; // Local date and time.
    QList<DsQmlEvent*>         m_events;          // All events.
    mutable QList<DsQmlEvent*> m_timeline; // List of events where overlapping events are resolved to a single event.
    QFutureWatcher<QList<DsQmlEvent*>> m_watcher;         // Tracks the async task.
    dsqt::ui::DsQmlClock*              m_clock = nullptr; //
};

} // namespace dsqt::model

#endif // SCHEDULE_HELPER_H
