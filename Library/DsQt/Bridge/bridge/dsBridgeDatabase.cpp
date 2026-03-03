#include "bridge/dsBridgeDatabase.h"
#include "settings/dsSettings.h"

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

DatabaseRecord DatabaseContent::getCurrentEvent(const QDateTime& localDateTime) const {
    DatabaseRecordList records = events();
    filterEvents(records, localDateTime);
    sortEvents(records, localDateTime);
    if (!records.isEmpty()) return records.front();
    return {};
}

} // namespace dsqt::bridge
