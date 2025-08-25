#include "model/dsQmlEventSchedule.h"
#include "core/dsQmlApplicationEngine.h"
#include "core/dsQmlObj.h"
#include "ui/dsQmlClock.h"

#include <bitset>

namespace dsqt::model {

DsQmlEventSchedule::DsQmlEventSchedule(QObject* parent)
    : DsQmlEventSchedule("", parent) {
}

DsQmlEventSchedule::DsQmlEventSchedule(const QString& type_name, QObject* parent)
    : QObject(parent)
    , m_type_name(type_name) {
    auto engine = DsQmlApplicationEngine::DefEngine();

    // Listen to content updates.
    connect(engine, &DsQmlApplicationEngine::rootUpdated, this, &DsQmlEventSchedule::updateNow,
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

bool DsQmlEventSchedule::isEventNow(const model::ContentModelRef& event, QDateTime localDateTime) {
    if (!isEventToday(event, localDateTime.date())) return false;

    const auto startTime = event.getPropertyTime("start_time");
    const auto endTime   = event.getPropertyTime("end_time");
    if (!startTime.isValid()) {
        qCWarning(lgQmlObj) << "Couldn't parse the start time for an event of type "
                            << event.getPropertyString("type_key");
        qCWarning(lgQmlObjVerbose) << "Start Time of event: " << event.getPropertyString("start_time");
        return false;
    }
    if (!endTime.isValid()) {
        qCWarning(lgQmlObj) << "Couldn't parse the end time for an event of type "
                            << event.getPropertyString("type_key");
        qCWarning(lgQmlObjVerbose) << "End Time of event: " << event.getPropertyString("end_time");
        return false;
    }
    if (localDateTime.time() < startTime || localDateTime.time() >= endTime) {
        qCDebug(lgQmlObjVerbose) << "Event happens outside the current time: " << event.getName() << "("
                                 << event.getId() << ")";
        return false;
    }

    return true;
}

bool DsQmlEventSchedule::isEventToday(const model::ContentModelRef& event, QDate localDate) {
    const auto startDate = event.getPropertyDate("start_date");
    const auto endDate   = event.getPropertyDate("end_date");
    if (!startDate.isValid()) {
        qCWarning(lgQmlObj) << "Couldn't parse the start date for an event of type "
                            << event.getPropertyString("type_key");
        qCWarning(lgQmlObjVerbose) << "Start Date of event: " << event.getPropertyString("start_date");
        return false;
    }
    if (!endDate.isValid()) {
        qCWarning(lgQmlObj) << "Couldn't parse the end date for an event of type "
                            << event.getPropertyString("type_key");
        qCWarning(lgQmlObjVerbose) << "End Date of event:" << event.getPropertyString("end_date");
        return false;
    }

    if (localDate < startDate || localDate > endDate) {
        qCWarning(lgQmlObjVerbose) << "Event happens outside the current date: " << event.getName() << "("
                                   << event.getId() << ")";
        return false;
    }

    const auto dayNumber =
        localDate.dayOfWeek() % 7; // Returns the weekday (0 to 6, where 0 = Sunday, 1 = Monday, ..., 6 = Saturday).
    const int dayFlag       = 0x1 << dayNumber;
    const int effectiveDays = event.getPropertyInt("effective_days"); // Flags for week days.
    if (effectiveDays & dayFlag) {
        return true;
    }

    qCDebug(lgQmlObjVerbose) << "Event not scheduled for the current weekday: " << event.getName() << "("
                             << event.getId() << ")";
    return false;
}

bool DsQmlEventSchedule::isEventWithinSpan(const model::ContentModelRef& event, QDateTime spanStart,
                                           QDateTime spanEnd) {
    if (spanStart > spanEnd) return false; // Invalid span.
    auto eventStart = event.getPropertyDateTime("start_date", "start_time");
    if (!eventStart.isValid()) return false;
    auto eventEnd = event.getPropertyDateTime("end_date", "end_time");
    if (!eventEnd.isValid()) return false;
    if (eventEnd < eventStart) return false; // Invalid value.
    return (spanEnd >= eventStart && eventEnd >= spanStart);
}

size_t DsQmlEventSchedule::filterEvents(std::vector<model::ContentModelRef>& events, const QString& typeName) {
    size_t count = events.size();
    auto empty = typeName.isEmpty();
    if (!empty) {
        events.erase(std::remove_if(events.begin(), events.end(),
                                    [&](const model::ContentModelRef& item) {
                                        const auto type = item.getPropertyString("type_name");
                                        return type != typeName;
                                    }),
                     events.end());
    }
    return count - events.size();
}

size_t DsQmlEventSchedule::filterEvents(std::vector<model::ContentModelRef>& events, QDateTime localDateTime) {
    size_t count = events.size();
    events.erase(std::remove_if(events.begin(), events.end(),
                                [&](const model::ContentModelRef& item) { return !isEventNow(item, localDateTime); }),
                 events.end());
    return count - events.size();
}

size_t DsQmlEventSchedule::filterEvents(std::vector<model::ContentModelRef>& events, QDate localDate) {
    size_t count = events.size();
    events.erase(std::remove_if(events.begin(), events.end(),
                                [&](const model::ContentModelRef& item) { return !isEventToday(item, localDate); }),
                 events.end());
    return count - events.size();
}

size_t DsQmlEventSchedule::filterEvents(std::vector<model::ContentModelRef>& events, QDateTime spanStart,
                                        QDateTime spanEnd) {
    size_t count = events.size();
    events.erase(std::remove_if(
                     events.begin(), events.end(),
                     [&](const model::ContentModelRef& item) { return !isEventWithinSpan(item, spanStart, spanEnd); }),
                 events.end());
    return count - events.size();
}

void DsQmlEventSchedule::sortEvents(std::vector<model::ContentModelRef>& events, QDateTime localDateTime) {
    auto heuristic = [&localDateTime](const model::ContentModelRef& a, const model::ContentModelRef& b) -> bool {
        // Active Event trumps Inactive event
        const auto isActiveA = isEventNow(a, localDateTime);
        const auto isActiveB = isEventNow(b, localDateTime);
        if (isActiveA != isActiveB) return isActiveA;

        // Recently Started trumps Previously started
        const auto startA      = a.getPropertyTime("start_time");
        const auto startB      = b.getPropertyTime("start_time");
        const auto sinceStartA = startA.secsTo(localDateTime.time());
        const auto sinceStartB = startB.secsTo(localDateTime.time());
        if (sinceStartA == sinceStartB) { // Starting at the same time.
            const auto durationA = startA.secsTo(a.getPropertyTime("end_time"));
            const auto durationB = startB.secsTo(b.getPropertyTime("end_time"));
            if (durationA == durationB)                                             // Same duration:
                return std::bitset<8>(a.getPropertyInt("effective_days")).count() < // Fewer days has higher priority.
                       std::bitset<8>(b.getPropertyInt("effective_days")).count();
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

void DsQmlEventSchedule::updateNow() {
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
        auto engine = DsQmlApplicationEngine::DefEngine();

        // Obtain all events from engine.
        auto content   = engine->getContentRoot();
        auto allEvents = content.getChildByName("all_events");
        auto events    = allEvents.getChildren();

        // Only keep events of a specific type.
        DsQmlEventSchedule::filterEvents(events, m_type_name);
        // Only keep today's events.
        DsQmlEventSchedule::filterEvents(events, localDateTime.date());
        // Sort them by priority.
        DsQmlEventSchedule::sortEvents(events, localDateTime);

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
