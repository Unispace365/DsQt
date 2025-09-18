#include "rework/rwContentModel.h"

namespace dsqt::rework {

QHash<QString, RwContentModel*>& RwContentLookup::get(Qt::HANDLE threadId) {
    QMutexLocker lock(&s_mutex);
    if (!s_lookup.contains(threadId)) s_lookup.insertOrAssign(threadId, QHash<QString, RwContentModel*>{});
    return s_lookup[threadId];
}

void RwContentLookup::destroy(Qt::HANDLE threadId) {
    QMutexLocker lock(&s_mutex);
    if (s_lookup.contains(threadId)) {
        auto& records = s_lookup[threadId];
        for (const auto& [uid, record] : records.asKeyValueRange()) {
            record->deleteLater();
        }
        records.clear();
    }
}

} // namespace dsqt::rework
