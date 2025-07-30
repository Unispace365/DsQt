#include "dsQmlObj.h"

#include <bitset>

Q_LOGGING_CATEGORY(lgQmlObj, "core.qmlobj");
Q_LOGGING_CATEGORY(lgQmlObjVerbose, "core.qmlobj.verbose");
namespace dsqt {

namespace {
    const QString DATE_TIME_FORMAT = "yyyy-MM-ddTHH:mm:ss";
}

DsQmlObj::DsQmlObj(QQmlEngine* qmlEngine, QJSEngine* jsEngine, QObject* parent)
    : QObject{parent} {
    mEngine = dynamic_cast<DsQmlApplicationEngine*>(qmlEngine);
    if (mEngine == nullptr) {
        qWarning() << "Engine is not a DsQmlApplicationEngine or a subclass. $DS functionality will not be available";
    } else {
        connect(mEngine, &DsQmlApplicationEngine::rootUpdated, this, &DsQmlObj::updatePlatform);
    }
    updatePlatform();
}

DsQmlObj* DsQmlObj::create(QQmlEngine* qmlEngine, QJSEngine* jsEngine) {
    return new DsQmlObj(qmlEngine, jsEngine);
}

DsQmlSettingsProxy* DsQmlObj::appSettings() const
{
    if (!mEngine) return nullptr;
    return mEngine->getAppSettingsProxy();
}

DsQmlEnvironment* DsQmlObj::env() const
{
    if (!mEngine) return nullptr;
    return mEngine->getEnvQml();
}

DsQmlApplicationEngine *DsQmlObj::engine() const
{
    if (!mEngine) return nullptr;
    return mEngine;
}

model::DsQmlContentModel* DsQmlObj::platform() {
    return mPlatformQml;
}

void DsQmlObj::updatePlatform() {
    if (!mEngine) return;

    qDebug() << "Updating platform";

    auto platform = mEngine->getContentHelper()->getPlatform();
    if (platform != mPlatform) {
        mPlatform    = platform.duplicate();
        mPlatformQml = mEngine->getReferenceMap()->value(mPlatform.getId());
        emit platformChanged();
    }
}


model::DsQmlContentModel* DsQmlObj::getRecordById(QString id) const {
    if (id.isEmpty()) return nullptr;
    return mEngine->getReferenceMap()->value(id);
}

// bool DsQmlObj::isEventNow(QString event_id, QString localDateTime) const {
//     auto event = mEngine->getContentRoot().getChildByName("events").getChildById(event_id);
//     return isEventNow(event, QDateTime::fromString(localDateTime, DATE_TIME_FORMAT));
// }

// bool DsQmlObj::isEventNow(model::QmlContentModel* model, QString localDateTime) const {
//     auto event = mEngine->getContentRoot().getChildByName("events").getChildById(model->value("id").toString());
//     return isEventNow(event, QDateTime::fromString(localDateTime, DATE_TIME_FORMAT));
// }

// bool DsQmlObj::isEventToday(QString event_id, QString localDateTime) const {
//     auto event = mEngine->getContentRoot().getChildByName("events").getChildById(event_id);
//     return isEventToday(event, QDateTime::fromString(localDateTime, DATE_TIME_FORMAT).date());
// }

// bool DsQmlObj::isEventToday(model::QmlContentModel* model, QString localDateTime) const {
//     auto event = mEngine->getContentRoot().getChildByName("events").getChildById(model->value("id").toString());
//     return isEventToday(event, QDateTime::fromString(localDateTime, DATE_TIME_FORMAT).date());
// }

// QVariantList DsQmlObj::getEventsForSpan(QString spanStart, QString spanEnd) {
//     QVariantList retval;
//     const auto   events = getEventsForSpan(QDateTime::fromString(spanStart, DATE_TIME_FORMAT),
//                                            QDateTime::fromString(spanEnd, DATE_TIME_FORMAT));
//     for (const auto& event : events) {
//         const auto& eventId = event.getId();
//         QVariant    eventVariant;
//         eventVariant.setValue(mEngine->getReferenceMap()->value(eventId));
//         retval.append(eventVariant);
//     }
//     return retval;
// }

// std::vector<model::ContentModelRef> DsQmlObj::getScheduledEvents() {
//     auto platformHolder = mEngine->getContentRoot().getChildByName("platform");
//     if (platformHolder.hasChildren()) {
//         auto platform        = platformHolder.getChildren()[0];
//         auto scheduledEvents = platform.getChildByName("scheduled_events");
//         if (scheduledEvents.hasChildren()) return scheduledEvents.getChildren(); // Returns a copy.
//     }
//     return {};
// }

// std::vector<model::ContentModelRef> DsQmlObj::getEventsAtTime(QDateTime localDateTime) {
//     auto events = getScheduledEvents();
//     filterEvents(events, localDateTime);
//     return events;
// }

// std::vector<model::ContentModelRef> DsQmlObj::getEventsAtDate(QDate localDate) {
//     auto events = getScheduledEvents();
//     filterEvents(events, localDate);
//     return events;
// }

// std::vector<model::ContentModelRef> DsQmlObj::getEventsForSpan(QDateTime spanStart, QDateTime spanEnd) {
//     auto events = getScheduledEvents();
//     filterEvents(events, spanStart, spanEnd);
//     return events;
// }

bool DsQmlObj::isEventNow(const model::ContentModelRef& event, QDateTime localDateTime) {
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

bool DsQmlObj::isEventToday(const model::ContentModelRef& event, QDate localDate) {
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

bool DsQmlObj::isEventWithinSpan(const model::ContentModelRef& event, QDateTime spanStart, QDateTime spanEnd) {
    if (spanStart > spanEnd) return false; // Invalid span.
    auto eventStart = event.getPropertyDateTime("start_date", "start_time");
    if (!eventStart.isValid()) return false;
    auto eventEnd = event.getPropertyDateTime("end_date", "end_time");
    if (!eventEnd.isValid()) return false;
    if (eventEnd < eventStart) return false; // Invalid value.
    return (spanEnd >= eventStart && eventEnd >= spanStart);
}

size_t DsQmlObj::filterEvents(std::vector<model::ContentModelRef>& events, const QString& typeName) {
    size_t count = events.size();
    if (!typeName.isEmpty()) {
        events.erase(std::remove_if(events.begin(), events.end(),
                                    [&](const model::ContentModelRef& item) {
                                        const auto type = item.getPropertyString("type_name");
                                        return item.getPropertyString("type_name") != typeName;
                                    }),
                     events.end());
    }
    return count - events.size();
}

size_t DsQmlObj::filterEvents(std::vector<model::ContentModelRef>& events, QDateTime localDateTime) {
    size_t count = events.size();
    events.erase(std::remove_if(events.begin(), events.end(),
                                [&](const model::ContentModelRef& item) { return !isEventNow(item, localDateTime); }),
                 events.end());
    return count - events.size();
}

size_t DsQmlObj::filterEvents(std::vector<model::ContentModelRef>& events, QDate localDate) {
    size_t count = events.size();
    events.erase(std::remove_if(events.begin(), events.end(),
                                [&](const model::ContentModelRef& item) { return !isEventToday(item, localDate); }),
                 events.end());
    return count - events.size();
}

size_t DsQmlObj::filterEvents(std::vector<model::ContentModelRef>& events, QDateTime spanStart, QDateTime spanEnd) {
    size_t count = events.size();
    events.erase(std::remove_if(
                     events.begin(), events.end(),
                     [&](const model::ContentModelRef& item) { return !isEventWithinSpan(item, spanStart, spanEnd); }),
                 events.end());
    return count - events.size();
}

void DsQmlObj::sortEvents(std::vector<model::ContentModelRef>& events, QDateTime localDateTime) {
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

} // namespace dsqt
