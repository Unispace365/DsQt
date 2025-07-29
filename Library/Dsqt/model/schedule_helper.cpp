#include "schedule_helper.h"

#include "ds_qml_obj.h"
#include "dsqmlapplicationengine.h"
#include "ui/ds_qml_clock.h"

#include <bitset>

namespace dsqt::model {

DsQmlEventSchedule::DsQmlEventSchedule(QObject* parent)
    : DsQmlEventSchedule("", parent) {
}

DsQmlEventSchedule::DsQmlEventSchedule(const QString& type_name, QObject* parent)
    : QObject(parent)
    , m_type_name(type_name) {
    auto engine = DSQmlApplicationEngine::DefEngine();

    // Listen to content updates.
    connect(engine, &DSQmlApplicationEngine::rootUpdated, this, &DsQmlEventSchedule::updateNow,
            Qt::ConnectionType::QueuedConnection);

    // Connect the watcher to handle task completion.
    connect(&m_watcher, &QFutureWatcher<QList<DsQmlEvent*>>::finished, this, &DsQmlEventSchedule::onUpdated);

    // Refresh now.
    updateNow();
}

QList<DsQmlEvent*> DsQmlEventSchedule::timeline() const {
    if (m_events.isEmpty() || !m_timeline.isEmpty()) return m_timeline;

    if (!m_events.isEmpty()) {
        // Create a sorted list of start and end times.
        std::set<QTime> checkpoints;
        for (auto event : std::as_const(m_events)) {
            checkpoints.insert(event->start().time());
            checkpoints.insert(event->end().time());
        }

        // Create a copy of the events, that we can freely sort.
        QList<DsQmlEvent*> sortable = m_events;

        // For each of the checkpoints, sort the events and add the first event to the result.
        auto localDateTime = m_local_date_time;
        for (const auto& checkpoint : checkpoints) {
            localDateTime.setTime(checkpoint);
            sortEvents(sortable, localDateTime);
            // if (sortable.front()->isNow(localDateTime)) {
            //  Truncate previous event if needed.
            if (!m_timeline.isEmpty() && m_timeline.back()->end().time() > localDateTime.time()) {
                m_timeline.back()->setEnd(localDateTime);
                if (m_timeline.back()->durationInSeconds() <= 0) m_timeline.pop_back();
            }
            // Add next event.
            m_timeline.append(sortable.front()->duplicate());
            m_timeline.back()->setStart(localDateTime);
            //}
        }

        // Handle the last event.
        if (!m_timeline.isEmpty()) {
            m_timeline.back()->setEnd(localDateTime);
            if (m_timeline.back()->durationInSeconds() <= 0) m_timeline.pop_back();
        }

        // Merge events if needed.
        DsQmlEvent* previous = nullptr;
        for (auto itr = m_timeline.begin(); itr != m_timeline.end();) {
            // Set correct order.
            (*itr)->setOrder(std::distance(m_timeline.begin(), itr));
            // Merge with previous if the same.
            if (previous && previous->uid() == (*itr)->uid() && previous->end() == (*itr)->start()) {
                previous->setEnd((*itr)->end());
                itr = m_timeline.erase(itr);
            } else {
                previous = *itr++;
            }
        }
    }

    return m_timeline;
}

void DsQmlEventSchedule::updateNow() {
    auto engine = DSQmlApplicationEngine::DefEngine();
    if (m_clock)
        m_local_date_time = m_clock->now();
    else
        m_local_date_time = QDateTime::currentDateTime();
    update(m_local_date_time);
}

void DsQmlEventSchedule::onUpdated() {
    // This runs in the main thread when the background task completes.
    QList<DsQmlEvent*> result = m_watcher.result();

    // Check if something changed.
    bool areEqual = (result.size() == m_events.size());
    if (areEqual) {
        for (int i = 0; i < result.size(); ++i) {
            if (*result[i] != *m_events[i]) {
                areEqual = false;
                break;
            }
        }
    }

    if (!areEqual) {
        // Clear timeline, it will be lazily reconstructed if needed.
        m_timeline.clear();
        // Set parent now that we're in the main thread.
        for (auto event : std::as_const(result)) {
            event->setParent(this);
        }
        // Remove existing events.
        for (auto event : std::as_const(m_events)) {
            event->deleteLater();
        }
        // Store result.
        m_events = std::move(result);
        //
        emit eventsChanged();
    }
}

void DsQmlEventSchedule::update(const QDateTime& localDateTime) {
    if (m_watcher.isRunning()) return; // TODO handle this better.

    // Capture main thread pointer.
    QThread* mainThread = QThread::currentThread();

    // Perform update on a background thread.
    QFuture<QList<DsQmlEvent*>> future = QtConcurrent::run([=]() {
        // Obtain engine pointer.
        auto engine = DSQmlApplicationEngine::DefEngine();

        // Obtain all events from engine.
        auto content   = engine->getContentRoot();
        auto allEvents = content.getChildByName("all_events");
        auto events    = allEvents.getChildren();

        // Only keep events of a specific type.
        DsQmlObj::filterEvents(events, m_type_name);
        // Only keep today's events.
        DsQmlObj::filterEvents(events, localDateTime.date());
        // Sort them by priority.
        DsQmlObj::sortEvents(events, localDateTime);

        // Convert events to internal representation.
        QList<DsQmlEvent*> result;
        for (const auto& event : events) {
            // No parent yet, as we're in a different thread.
            DsQmlEvent* item = new DsQmlEvent(event, result.size(), nullptr);
            // Move to main thread.
            item->moveToThread(mainThread);

            result.append(item);
        }

        return result;
    });

    // Set the future in the watcher to track completion.
    m_watcher.setFuture(future);
}

void DsQmlEventSchedule::sortEvents(QList<DsQmlEvent*>& events, const QDateTime& localDateTime) {
    auto heuristic = [&localDateTime](const DsQmlEvent* a, const DsQmlEvent* b) -> bool {
        // Active Event trumps Inactive event
        const auto isActiveA = a->isNow(localDateTime);
        const auto isActiveB = b->isNow(localDateTime);
        if (isActiveA != isActiveB) return isActiveA;

        // Recently Started trumps Previously started
        const auto startA      = a->start().time();
        const auto startB      = b->start().time();
        const auto sinceStartA = startA.secsTo(localDateTime.time());
        const auto sinceStartB = startB.secsTo(localDateTime.time());
        if (sinceStartA == sinceStartB) { // Starting at the same time.
            const auto durationA = a->durationInSeconds();
            const auto durationB = b->durationInSeconds();
            if (durationA == durationB)                    // Same duration:
                return std::bitset<8>(a->days()).count() < // Fewer days has higher priority
                       std::bitset<8>(b->days()).count();
            else                                           // Different duration:
                return durationA < durationB;              // Shorter duration has higher priority.
        } else if ((sinceStartA < 0) != (sinceStartB < 0)) // Only one has already started.
            return sinceStartA >= 0;                       // A has started, priority over B.

        return sinceStartA >= 0
                   ? sinceStartA < sinceStartB  // Both have started: later start time has higher priority.
                   : sinceStartA > sinceStartB; // Both have not yet started: earlier start time has higher priority.
    };

    // Sort from highest priority to lowest priority.
    std::stable_sort(events.begin(), events.end(), heuristic);
}

} // namespace dsqt::model
