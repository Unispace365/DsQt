#include "schedule_helper.h"

#include "ds_qml_obj.h"
#include "dsqmlapplicationengine.h"
#include "ui/clockqml.h"

#include <bitset>

namespace dsqt::model {

ScheduledEvents::ScheduledEvents(QObject* parent)
    : ScheduledEvents("", parent) {
}

ScheduledEvents::ScheduledEvents(const QString& type_name, QObject* parent)
    : QObject(parent)
    , m_type_name(type_name) {
    auto engine = DSQmlApplicationEngine::DefEngine();

    // Listen to content updates.
    connect(engine, &DSQmlApplicationEngine::rootUpdated, this, &ScheduledEvents::updateNow,
            Qt::ConnectionType::QueuedConnection);

    // Connect the watcher to handle task completion.
    connect(&m_watcher, &QFutureWatcher<QList<ScheduledEvent*>>::finished, this, &ScheduledEvents::onUpdated);

    // Use the built-in QML clock if available. Refresh every minute.
    ui::ClockQML* clock = engine->rootContext()->contextProperty("clock").value<ui::ClockQML*>();
    if (clock) {
        m_use_clock = true;
        connect(
            clock, &ui::ClockQML::minutesChanged, this,
            [this, clock]() {
                m_local_date_time = clock->now();
                update(m_local_date_time);
            },
            Qt::ConnectionType::QueuedConnection);
    }

    // Refresh now.
    updateNow();
}

QList<ScheduledEvent*> ScheduledEvents::timeline() const {
    QList<ScheduledEvent*> result;

    if (!m_events.isEmpty()) {
        // Create a sorted list of start and end times.
        std::set<QTime> checkpoints;
        for (auto event : std::as_const(m_events)) {
            checkpoints.insert(event->start().time());
            checkpoints.insert(event->end().time());
        }

        // Create a copy of the events, that we can freely sort.
        QList<ScheduledEvent*> sortable = m_events;

        // For each of the checkpoints, sort the events and add the first event to the result.
        auto localDateTime = m_local_date_time;
        for (const auto& checkpoint : checkpoints) {
            localDateTime.setTime(checkpoint);
            sortEvents(sortable, localDateTime);
            // if (sortable.front()->isNow(localDateTime)) {
            //  Truncate previous event if needed.
            if (!result.isEmpty() && result.back()->end().time() > localDateTime.time()) {
                result.back()->setEnd(localDateTime);
                if (result.back()->durationInSeconds() <= 0) result.pop_back();
            }
            // Add next event.
            result.append(sortable.front()->duplicate());
            result.back()->setStart(localDateTime);
            //}
        }

        // Handle the last event.
        if (!result.isEmpty()) {
            result.back()->setEnd(localDateTime);
            if (result.back()->durationInSeconds() <= 0) result.pop_back();
        }

        // Merge events if needed.
        ScheduledEvent* previous = nullptr;
        for (auto itr = result.begin(); itr != result.end();) {
            // Set correct order.
            (*itr)->setOrder(std::distance(result.begin(), itr));
            // Merge with previous if the same.
            if (previous && previous->uid() == (*itr)->uid() && previous->end() == (*itr)->start()) {
                previous->setEnd((*itr)->end());
                itr = result.erase(itr);
            } else {
                previous = *itr++;
            }
        }
    }

    return result;
}

void ScheduledEvents::onUpdated() {
    // This runs in the main thread when the background task completes.
    QList<ScheduledEvent*> result = m_watcher.result();
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
        // Set parent now that we're in the main thread.
        for (auto event : std::as_const(result)) {
            event->setParent(this);
        }
        // Remove existing events.
        for (auto event : std::as_const(m_events)) {
            event->deleteLater();
        }
        //
        m_events = std::move(result);
        //
        emit eventsChanged();
    }
}

void ScheduledEvents::update(const QDateTime& localDateTime) {
    if (m_watcher.isRunning()) return; // TODO handle this better.

    // Capture main thread pointer.
    QThread* mainThread = QThread::currentThread();

    // Perform update on a background thread.
    QFuture<QList<ScheduledEvent*>> future = QtConcurrent::run([=]() {
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
        QList<ScheduledEvent*> result;
        for (const auto& event : events) {
            // No parent yet, as we're in a different thread.
            ScheduledEvent* item = new ScheduledEvent(event, result.size(), nullptr);
            // Move to main thread.
            item->moveToThread(mainThread);

            result.append(item);
        }

        return result;
    });

    // Set the future in the watcher to track completion.
    m_watcher.setFuture(future);
}

void ScheduledEvents::sortEvents(QList<ScheduledEvent*>& events, const QDateTime& localDateTime) {
    auto heuristic = [&localDateTime](const ScheduledEvent* a, const ScheduledEvent* b) -> bool {
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
