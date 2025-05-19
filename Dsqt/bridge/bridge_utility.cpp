#include "bridge_utility.h"
#include <QDateTime>

Q_LOGGING_CATEGORY(lgBrUt, "bridgeSync.utility")
Q_LOGGING_CATEGORY(lgBrUtVerbose, "bridgeSync.utility.verbose")
namespace dsqt::bridge {
BridgeUtility::BridgeUtility(QObject *parent, model::ContentModelRef contentRoot)
    : QObject(parent)
    , m_contentRoot(contentRoot)
{}

bool BridgeUtility::isEventNow(model::ContentModelRef event, QDateTime ldt)
{
    int tzd = 0;

    /// ---------- Check the effective dates
    auto startDate = QDateTime::fromString(event.getPropertyString("start_date"), "yyyy-MM-dd");
    auto endDate = QDateTime::fromString(event.getPropertyString("end_date"), "yyyy-MM-dd");

    if (!startDate.isValid()) {
        qCWarning(lgBrUt) << "Couldn't parse the start date for an event!";
        qCWarning(lgBrUtVerbose) << "Start Date of event: "
                                 << event.getPropertyString("start_date");
        return false;
    }
    if (!endDate.isValid()) {
        qCWarning(lgBrUt) << "Couldn't parse the end date for an event!";
        qCWarning(lgBrUtVerbose) << "End Date of event:" << event.getPropertyString("end_date");
        return false;
    }

    //do we need to manipulate the end date?
    if (ldt.date() < startDate.date() || ldt.date() > endDate.date()) {
        qCWarning(lgBrUtVerbose) << "Event happens outside the current date: " << event.getName()
                                 << "(" << event.getId() << ")";
        return false;
    }

    /// ---------- Check the effective times of day
    if (!event.getPropertyString("start_time").isEmpty()) {
        auto startTime = QDateTime::fromString(event.getPropertyString("start_time"), "HH:mm:ss");
        auto endTime = QDateTime::fromString(event.getPropertyString("end_time"), "HH:mm:ss");
        if (!startTime.isValid()) {
            qCWarning(lgBrUt) << "Couldn't parse the start time for an event!";
            qCWarning(lgBrUtVerbose)
                << "Start Time of event: " << event.getPropertyString("start_time");
            return false;
        }
        if (!endTime.isValid()) {
            qCWarning(lgBrUt) << "Couldn't parse the end time for an event!";
            qCWarning(lgBrUtVerbose)
                << "End Time of event: " << event.getPropertyString("end_time");
            return false;
        }

        int daySeconds = ldt.time().msecsSinceStartOfDay() / 1000;

        int startDaySeconds = startTime.time().msecsSinceStartOfDay() / 1000;
        int endDaySeconds = endTime.time().msecsSinceStartOfDay() / 1000;

        if (daySeconds < startDaySeconds || daySeconds > endDaySeconds) {
            qCWarning(lgBrUt) << "Event happens outside the current time: " << event.getName()
                              << "(" << event.getId() << ")";
            return false;
        }
    }

    /// ---------- Check the effective days of the week
    static const int WEEK_SUN = 0b00000001;
    static const int WEEK_MON = 0b00000010;
    static const int WEEK_TUE = 0b00000100;
    static const int WEEK_WED = 0b00001000;
    static const int WEEK_THU = 0b00010000;
    static const int WEEK_FRI = 0b00100000;
    static const int WEEK_SAT = 0b01000000;
    static const int WEEK_ALL = 0b01111111;

    // Returns the weekday (0 to 6, where
    /// 0 = Sunday, 1 = Monday, ..., 6 = Saturday).
    auto dotw = ldt.date().dayOfWeek() - 1;
    int dayFlag = 0;

    if (dotw == 0)
        dayFlag = WEEK_SUN;
    if (dotw == 1)
        dayFlag = WEEK_MON;
    if (dotw == 2)
        dayFlag = WEEK_TUE;
    if (dotw == 3)
        dayFlag = WEEK_WED;
    if (dotw == 4)
        dayFlag = WEEK_THU;
    if (dotw == 5)
        dayFlag = WEEK_FRI;
    if (dotw == 6)
        dayFlag = WEEK_SAT;

    int effectiveDays = event.getPropertyInt("effective_days");
    if (effectiveDays == WEEK_ALL || effectiveDays & dayFlag) {
        return true;
    }

    return false;
}

bool BridgeUtility::isEventNow(QString id, QString ldt)
{
    auto event = m_contentRoot.getChildByName("events").getChildById(id);
    QDateTime ldtObj = QDateTime::fromString(ldt, "yyyy-MM-ddTHH:mm:ss");
    return isEventNow(event, ldtObj);
}

QVariantList BridgeUtility::getEventsForSpan(QString start, QString end)
{
    auto retval = QVariantList();
    QDateTime startObj = QDateTime::fromString(start, "yyyy-MM-ddTHH:mm:ss");
    QDateTime endObj = QDateTime::fromString(end, "yyyy-MM-ddTHH:mm:ss");
    auto events = getEventsForSpan(startObj, endObj);
    for (auto &event : events) {
        QVariant eventVariant;
        eventVariant.setValue(event.getQml(nullptr));
        retval.append(eventVariant);
    }
    return retval;
}

std::vector<model::ContentModelRef> BridgeUtility::getEventsForSpan(QDateTime startObj,
                                                                    QDateTime endObj)
{
    auto retval = std::vector<model::ContentModelRef>();
    auto platformHolder = m_contentRoot.getChildByName("platform");
    if (platformHolder.hasChildren()) {
        auto platform = platformHolder.getChildren()[0];
        auto events = platform.getChildByName("scheduled_events");
        if (events.hasChildren()) {
            auto eventsList = events.getChildren();

            for (auto event : eventsList) {
                if (isEventNow(event, startObj)) {
                    retval.push_back(event);
                    break;
                }
                if (isEventNow(event, endObj)) {
                    retval.push_back(event);
                    break;
                }
                //check if event is between startObj and endObj
                auto startDate = QDateTime::fromString(event.getPropertyString("start_date"),
                                                       "yyyy-MM-dd");
                auto endDate = QDateTime::fromString(event.getPropertyString("end_date"),
                                                     "yyyy-MM-dd");
                auto startTime = QDateTime::fromString(event.getPropertyString("start_time"),
                                                       "HH:mm:ss");
                auto endTime = QDateTime::fromString(event.getPropertyString("end_time"),
                                                     "HH:mm:ss");
                startDate.setTime(startTime.time());
                endDate.setTime(endTime.time());
                if (startDate.isValid() && endDate.isValid()) {
                    if (startDate.date() >= startObj.date() && startDate.date() <= endObj.date()
                        && endDate.date() >= startObj.date() && endDate.date() <= endObj.date()) {
                        retval.push_back(event);
                    }
                }
            }
            return retval;
        }
    }
    return retval;
}

model::QmlContentModel *BridgeUtility::platform() const
{
    model::QmlContentModel *outmap = nullptr;
    auto platformHolder = m_contentRoot.getChildByName("platform");
    if (platformHolder.hasChildren()) {
        outmap = platformHolder.getChildren()[0].getQml(nullptr);
    }
    if (outmap) {
        QQmlEngine::setObjectOwnership(outmap, QQmlEngine::JavaScriptOwnership);
    }
    return outmap;
}

model::QmlContentModel *BridgeUtility::getRecord(QString id, QString name) const
{
    if (id.isEmpty() && name.isEmpty()) {
        qCWarning(lgBrUt) << "Both ID and Name are empty!";
        return nullptr;
    }

    auto base = m_contentRoot;
    if (!id.isEmpty()) {
        base = m_contentRoot.getReference("all_records", id);
    }

    auto record = name.isEmpty()?base:model::ContentModelRef();
    if (!name.isEmpty()) {
        record = base.getChildByName(name);
    }
    auto retval = record.getQml(nullptr);
    if (retval) {
        QQmlEngine::setObjectOwnership(retval, QQmlEngine::JavaScriptOwnership);
    }
    return retval;
}

void BridgeUtility::setRoot(model::ContentModelRef root)
{
    m_contentRoot = root;
}

} // namespace dsqt::bridge
