#ifndef SCHEDULE_HELPER_H
#define SCHEDULE_HELPER_H

#include "content_model.h"

#include <QColor>
#include <QDateTime>
#include <QFutureWatcher>
#include <QObject>
#include <QQmlEngine>
#include <QQmlListProperty>
#include <QtConcurrentRun>

namespace dsqt::model {

class ScheduledEvent : public QObject {
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
    explicit ScheduledEvent(QObject* parent = nullptr)
        : QObject(parent) {}

    ScheduledEvent(ContentModelRef model, qsizetype order, QObject* parent = nullptr)
        : QObject(parent)
        , m_model(model)
        , m_order(order) {
        m_title = m_model.getPropertyString("record_name");
        m_start = m_model.getPropertyDateTime("start_date", "start_time");
        m_end   = m_model.getPropertyDateTime("end_date", "end_time");
    }

    ScheduledEvent* duplicate() const { return new ScheduledEvent(m_model, m_order, parent()); }

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

    int days() const { return m_model.getPropertyInt("effective_days"); }

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

    bool operator==(const ScheduledEvent& rhs) const {
        // Do not compare titles.
        return (m_order == rhs.m_order && m_start == rhs.m_start && m_end == rhs.m_end && uid() == rhs.uid());
    }
    bool operator!=(const ScheduledEvent& rhs) const { return !(*this == rhs); }

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

class ScheduledEvents : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QList<ScheduledEvent*> all READ all NOTIFY eventsChanged)
    Q_PROPERTY(QList<ScheduledEvent*> timeline READ timeline NOTIFY eventsChanged)
    Q_PROPERTY(ScheduledEvent* current READ current NOTIFY eventsChanged)

  public:
    explicit ScheduledEvents(QObject* parent = nullptr);
    ScheduledEvents(const QString& type_name, QObject* parent = nullptr);

    // Returns all events, sorted from highest to lowest priority.
    QList<ScheduledEvent*> all() const { return m_events; }
    // Returns all events as a timeline, with all overlapping events resolved to a single event.
    QList<ScheduledEvent*> timeline() const;
    // Returns the currently active event, or nullptr if no event is active.
    ScheduledEvent* current() const {
        if (m_events.isEmpty()) return nullptr;
        return m_events.front();
    }

  signals:
    void eventsChanged();

  private:
    void updateNow() {
        if (!m_use_clock) m_local_date_time = QDateTime::currentDateTime();
        update(m_local_date_time);
    }
    void update(const QDateTime& localDateTime);

    static void sortEvents(QList<ScheduledEvent*>& events, const QDateTime& localDateTime);

  private slots:
    void onUpdated();

  private:
    QString                                m_type_name;        // Event record type. If empty, all events are included.
    QDateTime                              m_local_date_time;  // Local date and time.
    QList<ScheduledEvent*>                 m_events;           // All events.
    QHash<QString, QList<ScheduledEvent*>> m_events_by_type;   //
    QFutureWatcher<QList<ScheduledEvent*>> m_watcher;          // Tracks the async task.
    bool                                   m_use_clock{false}; // Whether to listen to the UI clock.
};

} // namespace dsqt::model

#endif // SCHEDULE_HELPER_H
