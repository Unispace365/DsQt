#include "ds_qml_obj.h"

Q_LOGGING_CATEGORY(lgQmlObj, "core.qmlobj");
Q_LOGGING_CATEGORY(lgQmlObjVerbose, "core.qmlobj.verbose");
namespace dsqt {

DsQmlObj::DsQmlObj(int force,QObject *parent)
    : QObject{parent}
{}

DsQmlObj *DsQmlObj::create(QQmlEngine *qmlEngine, QJSEngine *)
{
    auto obj = new DsQmlObj(0);
    obj->mEngine = dynamic_cast<DSQmlApplicationEngine*>(qmlEngine);
    if(obj->mEngine == nullptr)
    {
        qWarning() << "Engine is not a DSQmlApplicationEngine or a subclass. $DS functionality will not be available";
    }
    obj->connect(obj->mEngine,&DSQmlApplicationEngine::rootUpdated,obj,[obj](){
        obj->updatePlatform();
    });
    obj->updatePlatform();
    return obj;
}

DSSettingsProxy* DsQmlObj::appSettings() const
{
    if (!mEngine) return nullptr;
    return mEngine->getAppSettingsProxy();
}

DSEnvironmentQML* DsQmlObj::env() const
{
    if (!mEngine) return nullptr;
    return mEngine->getEnvQml();
}

DSQmlApplicationEngine *DsQmlObj::engine() const
{
    if (!mEngine) return nullptr;
    return mEngine;
}



model::QmlContentModel *DsQmlObj::platform()
{
    return m_platform_qml;
}

void DsQmlObj::updatePlatform()
{
    qDebug()<<"updating platform";
    auto platform = mEngine->getContentHelper()->getPlatform();

    auto newPlatform = platform.duplicate();
    if(newPlatform == m_platform) return;
    m_platform = newPlatform;
    auto rm = mEngine->getReferenceMap();
    m_platform_qml = rm->value(m_platform.getId());
    emit platformChanged();
}


model::QmlContentModel *DsQmlObj::getRecordById(QString id) const
{
    if (id.isEmpty()) {
        //qCWarning(lgBrUt) << "getRecordById: invalid id(empty)!";
        return nullptr;
    }

    auto retval = mEngine->getReferenceMap()->value(id);
    return retval;
}

bool DsQmlObj::isEventNow(model::ContentModelRef event, QDateTime ldt)
{
    int tzd = 0;

    /// ---------- Check the effective dates
    auto startDate = QDateTime::fromString(event.getPropertyString("start_date"), "yyyy-MM-dd");
    auto endDate = QDateTime::fromString(event.getPropertyString("end_date"), "yyyy-MM-dd");

    if (!startDate.isValid()) {
        qCWarning(lgQmlObj) << "Couldn't parse the start date for an event!";
        qCWarning(lgQmlObjVerbose) << "Start Date of event: "
                                 << event.getPropertyString("start_date");
        return false;
    }
    if (!endDate.isValid()) {
        qCWarning(lgQmlObj) << "Couldn't parse the end date for an event!";
        qCWarning(lgQmlObjVerbose) << "End Date of event:" << event.getPropertyString("end_date");
        return false;
    }

    //do we need to manipulate the end date?
    if (ldt.date() < startDate.date() || ldt.date() > endDate.date()) {
        qCWarning(lgQmlObjVerbose) << "Event happens outside the current date: " << event.getName()
        << "(" << event.getId() << ")";
        return false;
    }

    /// ---------- Check the effective times of day
    if (!event.getPropertyString("start_time").isEmpty()) {
        auto startTime = QDateTime::fromString(event.getPropertyString("start_time"), "HH:mm:ss");
        auto endTime = QDateTime::fromString(event.getPropertyString("end_time"), "HH:mm:ss");
        if (!startTime.isValid()) {
            qCWarning(lgQmlObj) << "Couldn't parse the start time for an event!";
            qCWarning(lgQmlObjVerbose)
                << "Start Time of event: " << event.getPropertyString("start_time");
            return false;
        }
        if (!endTime.isValid()) {
            qCWarning(lgQmlObj) << "Couldn't parse the end time for an event!";
            qCWarning(lgQmlObjVerbose)
                << "End Time of event: " << event.getPropertyString("end_time");
            return false;
        }

        int daySeconds = ldt.time().msecsSinceStartOfDay() / 1000;

        int startDaySeconds = startTime.time().msecsSinceStartOfDay() / 1000;
        int endDaySeconds = endTime.time().msecsSinceStartOfDay() / 1000;

        if (daySeconds < startDaySeconds || daySeconds > endDaySeconds) {
            qCWarning(lgQmlObj) << "Event happens outside the current time: " << event.getName()
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

bool DsQmlObj::isEventNow(model::QmlContentModel* model, QString ldt)
{
    auto event = mEngine->getContentRoot().getChildByName("events").getChildById(model->value("id").toString());
    QDateTime ldtObj = QDateTime::fromString(ldt, "yyyy-MM-ddTHH:mm:ss");
    return isEventNow(event, ldtObj);
}

bool DsQmlObj::isEventNow(QString id, QString ldt)
{
    auto event = mEngine->getContentRoot().getChildByName("events").getChildById(id);
    QDateTime ldtObj = QDateTime::fromString(ldt, "yyyy-MM-ddTHH:mm:ss");
    return isEventNow(event, ldtObj);
}

QVariantList DsQmlObj::getEventsForSpan(QString start, QString end)
{
    auto retval = QVariantList();
    QDateTime startObj = QDateTime::fromString(start, "yyyy-MM-ddTHH:mm:ss");
    QDateTime endObj = QDateTime::fromString(end, "yyyy-MM-ddTHH:mm:ss");
    auto events = getEventsForSpan(startObj, endObj);
    for (auto &event : events) {
        QVariant eventVariant;
        auto eventId = event.getId();
        eventVariant.setValue(mEngine->getReferenceMap()->value(eventId));
        retval.append(eventVariant);
    }
    return retval;
}

std::vector<model::ContentModelRef> DsQmlObj::getEventsForSpan(QDateTime startObj,
                                                                    QDateTime endObj)
{
    auto retval = std::vector<model::ContentModelRef>();
    auto platformHolder = mEngine->getContentRoot().getChildByName("platform");
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

}
