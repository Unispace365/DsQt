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

} // namespace dsqt::bridge
