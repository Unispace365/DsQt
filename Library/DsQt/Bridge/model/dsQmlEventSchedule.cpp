#include "model/dsQmlEventSchedule.h"
#include "bridge/dsQmlBridge.h"
#include "ui/dsQmlClock.h"

namespace dsqt::model {

DsQmlEvent::DsQmlEvent(const bridge::DatabaseRecord& model, qsizetype order, QObject* parent)
    : QObject(parent)
    , m_record(model)
    , m_order(order) {
    m_title = m_record.value("record_name").toString();
    m_start = m_record.value(DsQmlEventSchedule::StartDateTime).toDateTime();
    m_end   = m_record.value(DsQmlEventSchedule::EndDateTime).toDateTime();
}

int DsQmlEvent::days() const {
    return m_record.value(DsQmlEventSchedule::EffectiveDays).toInt();
}

DsQmlEventSchedule::DsQmlEventSchedule(QObject* parent)
    : DsQmlEventSchedule("", parent) {
}

DsQmlEventSchedule::DsQmlEventSchedule(const QString& type_name, QObject* parent)
    : QObject(parent)
    , m_type_name(type_name) {
    // Listen for changes.
    connect(this, &DsQmlEventSchedule::clockChanged, this, &DsQmlEventSchedule::updateNow);
    connect(this, &DsQmlEventSchedule::typeChanged, this, &DsQmlEventSchedule::updateNow);

    // Listen to content updates.
    connect(&bridge::DsQmlBridge::instance(), &bridge::DsQmlBridge::databaseChanged, this,
            &DsQmlEventSchedule::updateNow);

    // Connect the watcher to handle task completion.
    connect(&m_watcher, &QFutureWatcher<FutureResult>::finished, this, &DsQmlEventSchedule::onUpdated);

    //
    m_defaultClock = new ui::DsQmlClock();
    setClock(m_defaultClock);
}

DsQmlEventSchedule::~DsQmlEventSchedule() {
    if (m_defaultClock) m_defaultClock->deleteLater();
}

QQmlListProperty<DsQmlEvent> DsQmlEventSchedule::events() {
    return QQmlListProperty<DsQmlEvent>(this, &m_events);
}

QQmlListProperty<DsQmlEvent> DsQmlEventSchedule::timeline() {
    return QQmlListProperty<DsQmlEvent>(this, &m_timeline);
}


/// Reads the current time from the clock (or system time as a fallback) and kicks off an
/// asynchronous update.
void DsQmlEventSchedule::updateNow() {
    if (m_clock)
        m_local_date_time = m_clock->now();
    else
        m_local_date_time = QDateTime::currentDateTime();

    update(m_local_date_time);
}

/// Called on the main thread when the background task completes. Diffs the new event lists
/// against the current ones and emits eventsChanged only when something has changed.
void DsQmlEventSchedule::onUpdated() {
    // This runs in the main thread when the background task completes.
    FutureResult result = m_watcher.result();

    bool hasChanged = m_events.size() != result.events.size();
    hasChanged |= m_timeline.size() != result.timeline.size();

    if (!hasChanged) {
        for (qsizetype i = 0; !hasChanged && i < result.events.size(); ++i)
            hasChanged |= *(m_events[i]) != *(result.events[i]);
        for (qsizetype i = 0; !hasChanged && i < result.timeline.size(); ++i)
            hasChanged |= *(m_timeline[i]) != *(result.timeline[i]);
    }

    // Set parent now that we're in the main thread.
    for (auto event : std::as_const(result.events)) {
        event->setParent(this);
    }
    for (auto event : std::as_const(result.timeline)) {
        event->setParent(this);
    }

    // Remove existing events.
    for (auto event : std::as_const(m_events)) {
        event->deleteLater();
    }
    for (auto event : std::as_const(m_timeline)) {
        event->deleteLater();
    }

    // Store result.
    m_events   = std::move(result.events);
    m_timeline = std::move(result.timeline);

    if (hasChanged) emit eventsChanged();
}

/// Performs the full filter → sort → timeline pipeline on a background thread.
/// The result is handed back to the main thread via m_watcher / onUpdated().
/// If a previous update is still running the call is dropped (see TODO in body).
void DsQmlEventSchedule::update(const QDateTime& localDateTime) {
    if (m_watcher.isRunning()) return; // TODO handle this better.

    // Capture selected type name.
    QString typeName = m_type_name;

    // Capture main thread pointer.
    QThread* mainThread = QThread::currentThread();

    // Perform update on a background thread.
    QFuture<FutureResult> future = QtConcurrent::run([=]() {
        FutureResult result;

        // Obtain all events from bridge.
        const auto& content = bridge::DsQmlBridge::instance().database();

        auto events = content.events();
        if (!events.isEmpty()) {
            // Only keep events of a specific type.
            filterEvents(events, typeName);
            // Only keep today's events.
            filterEvents(events, localDateTime.date());
            // Sort them by priority.
            sortEvents(events, localDateTime);

            // Convert events to internal representation.
            for (const auto& event : std::as_const(events)) {
                // No parent yet, as we're in a different thread.
                DsQmlEvent* item = new DsQmlEvent(event, result.events.size(), nullptr);
                // Move to main thread.
                item->moveToThread(mainThread);
                // Add to result.
                result.events.append(item);
            }

            // Create a sorted list of start and end times.
            std::set<QTime> checkpoints;
            for (auto& event : std::as_const(events)) {
                checkpoints.insert(event.start().time());
                checkpoints.insert(event.end().time());
            }

            // For each of the checkpoints, sort the events and add the first event to the result.
            QDateTime currentDateTime = localDateTime;
            for (const auto& checkpoint : checkpoints) {
                currentDateTime.setTime(checkpoint);
                sortEvents(events, currentDateTime);

                // No parent yet, as we're in a different thread.
                DsQmlEvent* item = new DsQmlEvent(events.front(), result.timeline.size(), nullptr);
                item->setStart(currentDateTime);
                // Move to main thread.
                item->moveToThread(mainThread);
                // Add to result.
                result.timeline.append(item);
            }

            //  Truncate events so they don't overlap.
            for (qsizetype i = 1; i < result.timeline.size(); ++i) {
                DsQmlEvent* item = result.timeline[result.timeline.size() - 1 - i];
                if (item->end().time() > currentDateTime.time()) item->setEnd(currentDateTime);
                currentDateTime = item->start();
            }

            // Remove events with invalid durations.
            result.timeline.removeIf([](const DsQmlEvent* item) { return item->durationInSeconds() <= 0; });

            // Merge events if needed.
            DsQmlEvent* previous = nullptr;
            for (auto itr = result.timeline.begin(); itr != result.timeline.end();) {
                // Set correct order.
                (*itr)->setOrder(std::distance(result.timeline.begin(), itr));
                // Merge with previous if the same.
                if (previous && previous->uid() == (*itr)->uid() && previous->end() == (*itr)->start()) {
                    previous->setEnd((*itr)->end());
                    itr = result.timeline.erase(itr);
                } else {
                    previous = *itr++;
                }
            }
        }

        return result;
    });

    // Set the future in the watcher to track completion.
    m_watcher.setFuture(future);
}

} // namespace dsqt::model
