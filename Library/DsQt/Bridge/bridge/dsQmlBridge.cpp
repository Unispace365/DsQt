#include "bridge/dsQmlBridge.h"

#include <QCoreApplication.h>

namespace dsqt::bridge {

DsQmlBridge::DsQmlBridge()
    : QObject(nullptr) {
    // Let qml know we are handling the destruction of this object despite it being a QObject. Falure to do so results
    // in a crash upon exit from a 'double free' error.
    QJSEngine::setObjectOwnership(this, QJSEngine::CppOwnership);
    m_content = model::ContentModel::createNamed("Bridge");
    //connect database changed to bridge updated, since for now they are effectively the same thing.
    //This is to provide a more specific signal that can be listened to on the main thread to know when Bridge
    //has finished updating and all relevant properties have been updated.
    connect(this, &DsQmlBridge::databaseChanged, this, &DsQmlBridge::bridgeUpdated);
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

//we are straying from the usual pattern of checking if we have the same database before setting it because we want to
// make sure that any changes to the database are reflected in the UI.
// If we check for the same database, then we might miss updates to the database that are made in place.
void DsQmlBridge::setDatabase(DatabaseContent&& database) {
    m_database = std::move(database);
    emit databaseChanged();
}

} // namespace dsqt::bridge
