#include "bridge/dsQmlBridge.h"

#include <QCoreApplication.h>

namespace dsqt::bridge {

DsQmlBridge::DsQmlBridge()
    : QObject(nullptr) {
    // Let qml know we are handling the destruction of this object despite it being a QObject. Falure to do so results
    // in a crash upon exit from a 'double free' error.
    QJSEngine::setObjectOwnership(this, QJSEngine::CppOwnership);
    m_content = model::ContentModel::createNamed("Bridge");
}

model::ContentModel* DsQmlBridge::content() const {
    // Should only be called from main thread! Use database() to obtain thread-safe data model.
    bool isMainThread = QThread::currentThread() == QCoreApplication::instance()->thread();
    Q_ASSERT(isMainThread);

    return m_content;
}

void DsQmlBridge::setContent(model::ContentModel* newContent) {
    if (m_content == newContent || !newContent) return;
    m_content = newContent;
    emit contentChanged();
}

void DsQmlBridge::setDatabase(DatabaseContent&& database) {
    m_database = std::move(database);
    emit databaseChanged();
}

} // namespace dsqt::bridge
