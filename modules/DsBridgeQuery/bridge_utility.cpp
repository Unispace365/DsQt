#include "bridge_utility.h"
#include <QDateTime>
namespace dsqt::bridge {
BridgeUtility::BridgeUtility(QObject *parent, model::ContentModelRef contentRoot)
    : QObject(parent)
    , m_contentRoot(contentRoot)
{}

bool BridgeUtility::isEventNow(model::ContentModelRef event, QDateTime ldt)
{
    int tzd = 0;

    /// ---------- Check the effective dates
    auto startDate = QDateTime::fromString(event.getPropertyQString("start_date"), "yyyy-MM-dd");
    auto endDate = QDateTime::fromString(event.getPropertyQString("end_date"), "yyyy-MM-dd");

    if (!startDate.isValid()) {
        qWarning() << "Couldn't parse the start date for an event!";
        return false;
    }
    if (!endDate.isValid()) {
        qWarning() << "Couldn't parse the end date for an event!";
        return false;
    }

    //do we need to manipulate the end date?
    if (ldt.date() < startDate.date() || ldt.date() > endDate.date()) {
        qWarning() << "Event happens outside the current date: " << event.getPropertyString("name");
        return false;
    }

    /// ---------- Check the effective times of day
    if (!event.getPropertyString("start_time").isEmpty()) {
        auto startTime = QDateTime::fromString(event.getPropertyQString("start_time"), "HH:mm:ss");
        auto endTime = QDateTime::fromString(event.getPropertyQString("end_time"), "HH:mm:ss");
        if (!startTime.isValid()) {
            qWarning() << "Couldn't parse the start time for an event!";
            return false;
        }
        if (!endTime.isValid()) {
            qWarning() << "Couldn't parse the end time for an event!";
            return false;
        }

        int daySeconds = ldt.time().msecsSinceStartOfDay() / 1000;

        int startDaySeconds = startTime.time().msecsSinceStartOfDay() / 1000;
        int endDaySeconds = endTime.time().msecsSinceStartOfDay() / 1000;

        if (daySeconds < startDaySeconds || daySeconds > endDaySeconds) {
            qWarning() << "Event happens outside the current time: "
                       << event.getPropertyString("name");
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
    Q_UNIMPLEMENTED();
    auto retval = QVariantList();
    return retval;
}

std::vector<model::ContentModelRef> BridgeUtility::getEventsForSpan(QDateTime start, QDateTime end)
{
    Q_UNIMPLEMENTED();
    auto retval = std::vector<model::ContentModelRef>();
    return retval;
}

QQmlPropertyMap *BridgeUtility::platform() const
{
    QQmlPropertyMap *outmap = nullptr;
    auto platformHolder = m_contentRoot.getChildByName("platform");
    if (platformHolder.hasChildren()) {
        outmap = platformHolder.getChildren()[0].getMap(nullptr);
    }
    if (outmap) {
        QQmlEngine::setObjectOwnership(outmap, QQmlEngine::JavaScriptOwnership);
    }
    return outmap;
}

void BridgeUtility::setRoot(model::ContentModelRef root)
{
    m_contentRoot = root;
}

} // namespace dsqt::bridge
