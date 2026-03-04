#ifndef DSQMLEVENTSCHEDULE_H
#define DSQMLEVENTSCHEDULE_H

#include "bridge/dsBridgeDatabase.h"
#include "ui/dsQmlClock.h"

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

// Wraps a scheduled event into a QML object.
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
        return (m_order == rhs.m_order && m_start == rhs.m_start && m_end == rhs.m_end && uid() == rhs.uid());
    }
    bool operator!=(const DsQmlEvent& rhs) const { return !(*this == rhs); }

  signals:
    void titleChanged();
    void startChanged();
    void endChanged();
    void orderChanged();

  private:
    bridge::DatabaseRecord m_record;
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
    Q_PROPERTY(QQmlListProperty<DsQmlEvent> events READ events NOTIFY eventsChanged)
    Q_PROPERTY(QQmlListProperty<DsQmlEvent> timeline READ timeline NOTIFY eventsChanged)
    Q_PROPERTY(dsqt::model::DsQmlEvent* current READ current NOTIFY eventsChanged)
    Q_PROPERTY(dsqt::ui::DsQmlClock* clock READ clock WRITE setClock NOTIFY clockChanged)

  public:
    inline static const char* StartDateTime = "start_date_time";
    inline static const char* EndDateTime   = "end_date_time";
    inline static const char* EffectiveDays = "effective_days";

    explicit DsQmlEventSchedule(QObject* parent = nullptr);
    DsQmlEventSchedule(const QString& type_name, QObject* parent = nullptr);
    ~DsQmlEventSchedule();

    // Returns the event type, or an empty string if not set.
    const QString& type() const { return m_type_name; }
    // Sets the event type, causing events to be filtered if not empty.
    void setType(const QString& type) {
        if (type == m_type_name) return;
        m_type_name = type;
        emit typeChanged();
    }

    // Returns all events, sorted from highest to lowest priority.
    QQmlListProperty<DsQmlEvent> events();
    // Returns all events as a timeline, with all overlapping events resolved to a single event.
    QQmlListProperty<DsQmlEvent> timeline();
    // Returns the currently active event, or nullptr if no event is active.
    DsQmlEvent* current() const {
        if (m_events.isEmpty()) return nullptr;
        return m_events.front();
    }
    //
    ui::DsQmlClock* clock() const { return m_clock; }
    //
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
