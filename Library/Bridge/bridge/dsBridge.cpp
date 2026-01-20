#include "bridge/dsBridge.h"

#include <qcoreapplication.h>

namespace dsqt::bridge {

DsBridge::DsBridge() {
    m_content = model::ContentModel::createNamed("Bridge");
}

model::ContentModel* DsBridge::content() const {
    // Should only be called from main thread! Use database() to obtain thread-safe data model.
    bool isMainThread = QThread::currentThread() == QCoreApplication::instance()->thread();
    Q_ASSERT(isMainThread);

    return m_content;
}

void DsBridge::setContent(model::ContentModel* newContent) {
    if (m_content == newContent || !newContent) return;
    m_content = newContent;
    emit contentChanged();
}

} // namespace dsqt::bridge
