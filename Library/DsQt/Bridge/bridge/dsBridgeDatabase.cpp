#include "bridge/dsBridgeDatabase.h"
#include "settings/dsSettings.h"

#include <bitset>

namespace dsqt::bridge {

DatabaseRecord DatabaseContent::getPlatform() const {
    const QString& platformUid = DsSettings::getSettings("app_settings")->getOr<QString>("platform.id", "");
    return find(platformUid);
}

DatabaseRecord DatabaseContent::getPlatform(const QString& typeName) const {
    for (const QString& platformUid : m_platforms) {
        DatabaseRecord platform = find(platformUid);
        if (platform.value("type_name", "") == typeName) return platform;
    }
    return {};
}

void DatabaseContent::sortEvents(DatabaseRecordList& events, const QDateTime& localDateTime) {
    auto heuristic = [&localDateTime](const DatabaseRecord& a, const DatabaseRecord& b) -> bool {
        const auto startA = a.getStartDateTime();
        const auto startB = b.getStartDateTime();
        const auto endA   = a.getEndDateTime();
        const auto endB   = b.getEndDateTime();
        const auto daysA  = a.getEffectiveDays();
        const auto daysB  = b.getEffectiveDays();

        // Active Event trumps Inactive event
        const auto isActiveA = DatabaseRecord::isNow(localDateTime, startA, endA, daysA);
        const auto isActiveB = DatabaseRecord::isNow(localDateTime, startB, endB, daysB);
        if (isActiveA != isActiveB) return isActiveA;

        // Recently Started trumps Previously started
        const auto sinceStartA = startA.time().secsTo(localDateTime.time());
        const auto sinceStartB = startB.time().secsTo(localDateTime.time());
        if (sinceStartA == sinceStartB) { // Starting at the same time.
            const auto durationA = DatabaseRecord::durationInSeconds(startA, endA);
            const auto durationB = DatabaseRecord::durationInSeconds(startB, endB);
            if (durationA == durationB)                // Same duration:
                return std::bitset<8>(daysA).count() < // Fewer days has higher priority
                       std::bitset<8>(daysB).count();
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

void DatabaseContent::removeInactiveEvents(DatabaseRecordList& events, const QDateTime& localDateTime) {
    auto heuristic = [&localDateTime](const DatabaseRecord& record) -> bool {
        return !record.isNow(localDateTime);
    };

    events.removeIf(heuristic);
}

DatabaseRecord DatabaseContent::getCurrentEvent(const QDateTime& localDateTime) const {
    DatabaseRecordList records = events();
    removeInactiveEvents(records, localDateTime);
    sortEvents(records, localDateTime);
    if (!records.isEmpty()) return records.front();
    return {};
}

} // namespace dsqt::bridge
