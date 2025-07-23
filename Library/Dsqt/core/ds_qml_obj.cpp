#include "ds_qml_obj.h"

Q_LOGGING_CATEGORY(lgQmlObj, "core.qmlobj");
Q_LOGGING_CATEGORY(lgQmlObjVerbose, "core.qmlobj.verbose");
namespace dsqt {

namespace {
    const QString DATE_TIME_FORMAT = "yyyy-MM-ddTHH:mm:ss";
}

DsQmlObj::DsQmlObj(int force, QObject* parent)
    : QObject{parent} {
}

DsQmlObj* DsQmlObj::create(QQmlEngine* qmlEngine, QJSEngine*) {
    auto obj     = new DsQmlObj(0);
    obj->mEngine = dynamic_cast<DSQmlApplicationEngine*>(qmlEngine);
    if (!obj->mEngine) {
        qWarning() << "Engine is not a DSQmlApplicationEngine or a subclass. $DS functionality will not be available";
    }
    obj->connect(obj->mEngine, &DSQmlApplicationEngine::rootUpdated, obj, [obj]() { obj->updatePlatform(); });
    obj->updatePlatform();
    return obj;
}

DSSettingsProxy* DsQmlObj::appSettings() const {
    if (!mEngine) return nullptr;
    return mEngine->getAppSettingsProxy();
}

DSEnvironmentQML* DsQmlObj::env() const {
    if (!mEngine) return nullptr;
    return mEngine->getEnvQml();
}

DSQmlApplicationEngine* DsQmlObj::engine() const {
    return mEngine;
}

model::QmlContentModel* DsQmlObj::platform() {
    return mPlatformQml;
}

void DsQmlObj::updatePlatform() {
    qDebug() << "Updating platform";
    auto platform = mEngine->getContentHelper()->getPlatform();
    if (platform == mPlatform) return;

    mPlatform    = platform.duplicate();
    mPlatformQml = mEngine->getReferenceMap()->value(mPlatform.getId());
    emit platformChanged();
}


model::QmlContentModel* DsQmlObj::getRecordById(QString id) const {
    if (id.isEmpty()) return nullptr;
    return mEngine->getReferenceMap()->value(id);
}

bool DsQmlObj::isEventNow(QString event_id, QString localDateTime) const {
    auto event = mEngine->getContentRoot().getChildByName("events").getChildById(event_id);
    return isEventNow(event, QDateTime::fromString(localDateTime, DATE_TIME_FORMAT));
}

bool DsQmlObj::isEventNow(model::QmlContentModel* model, QString localDateTime) const {
    auto event = mEngine->getContentRoot().getChildByName("events").getChildById(model->value("id").toString());
    return isEventNow(event, QDateTime::fromString(localDateTime, DATE_TIME_FORMAT));
}

bool DsQmlObj::isEventToday(QString event_id, QString localDateTime) const {
    auto event = mEngine->getContentRoot().getChildByName("events").getChildById(event_id);
    return isEventToday(event, QDateTime::fromString(localDateTime, DATE_TIME_FORMAT).date());
}

bool DsQmlObj::isEventToday(model::QmlContentModel* model, QString localDateTime) const {
    auto event = mEngine->getContentRoot().getChildByName("events").getChildById(model->value("id").toString());
    return isEventToday(event, QDateTime::fromString(localDateTime, DATE_TIME_FORMAT).date());
}

QVariantList DsQmlObj::getEventsForSpan(QString spanStart, QString spanEnd) {
    QVariantList retval;
    const auto   events = getEventsForSpan(QDateTime::fromString(spanStart, DATE_TIME_FORMAT),
                                           QDateTime::fromString(spanEnd, DATE_TIME_FORMAT));
    for (const auto& event : events) {
        const auto& eventId = event.getId();
        QVariant    eventVariant;
        eventVariant.setValue(mEngine->getReferenceMap()->value(eventId));
        retval.append(eventVariant);
    }
    return retval;
}

std::vector<model::ContentModelRef> DsQmlObj::getScheduledEvents() {
    auto platformHolder = mEngine->getContentRoot().getChildByName("platform");
    if (platformHolder.hasChildren()) {
        auto platform        = platformHolder.getChildren()[0];
        auto scheduledEvents = platform.getChildByName("scheduled_events");
        if (scheduledEvents.hasChildren()) return scheduledEvents.getChildren(); // Returns a copy.
    }
    return {};
}

std::vector<model::ContentModelRef> DsQmlObj::getEventsAtTime(QDateTime localDateTime) {
    auto events = getScheduledEvents();
    filterEvents(events, localDateTime);
    return events;
}

std::vector<model::ContentModelRef> DsQmlObj::getEventsAtDate(QDate localDate) {
    auto events = getScheduledEvents();
    filterEvents(events, localDate);
    return events;
}

std::vector<model::ContentModelRef> DsQmlObj::getEventsForSpan(QDateTime spanStart, QDateTime spanEnd) {
    auto events = getScheduledEvents();
    filterEvents(events, spanStart, spanEnd);
    return events;
}

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
    if (localDateTime.time() < startTime || localDateTime.time() > endTime) {
        qCDebug(lgQmlObj) << "Event happens outside the current time: " << event.getName() << "(" << event.getId()
                          << ")";
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

    qCDebug(lgQmlObj) << "Event not scheduled for the current weekday: " << event.getName() << "(" << event.getId()
                      << ")";
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
    static auto heuristic = [localDateTime](const model::ContentModelRef& a, const model::ContentModelRef& b) -> bool {
        // Active Event trumps Inactive event
        const auto isActiveA = isEventNow(a, localDateTime);
        const auto isActiveB = isEventNow(b, localDateTime);
        if (isActiveA != isActiveB) return isActiveA;

        // Recently Started trumps Previously started
        const auto startA      = a.getPropertyTime("start_time");
        const auto startB      = b.getPropertyTime("start_time");
        const auto sinceStartA = startA.secsTo(localDateTime.time());
        const auto sinceStartB = startB.secsTo(localDateTime.time());
        if (glm::sign(sinceStartA) > 0 && glm::sign(sinceStartB) > 0)
            return sinceStartB > sinceStartA;
        else if (glm::sign(sinceStartA) > 0)
            return true;

        return false;
    };

    // Sort from highest priority to lowest priority.
    std::stable_sort(events.begin(), events.end(), heuristic);
}

} // namespace dsqt
