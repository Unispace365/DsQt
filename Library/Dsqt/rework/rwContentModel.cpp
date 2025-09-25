#include "rework/rwContentModel.h"

#include <iostream>

namespace dsqt::model {

QHash<QString, ContentModel*>& ContentLookup::get(Qt::HANDLE threadId) {
    QMutexLocker lock(&s_mutex);
    if (!s_lookup.contains(threadId)) s_lookup.insertOrAssign(threadId, ContentModelHash{});
    return s_lookup[threadId];
}

void ContentLookup::destroy(Qt::HANDLE threadId) {
    QMutexLocker lock(&s_mutex);
    if (s_lookup.contains(threadId)) {
        auto& records = s_lookup[threadId];
        for (const auto& [uid, record] : records.asKeyValueRange()) {
            record->deleteLater();
        }
        records.clear();
    }
}

} // namespace dsqt::model

std::ostream& operator<<(std::ostream& os, const dsqt::model::ContentModel* o) {
    const auto name = o->value("record_name").toString().toStdString();
    const auto uid  = o->value("uid").toString().toStdString();
    return os << name << " (" << uid << ")";
}
