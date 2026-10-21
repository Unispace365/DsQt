#include "bridge/dsQmlBridge.h"

#include <QCoreApplication.h>
#include <QSet>

namespace dsqt::bridge {

namespace {

QStringList tagUidsForClass(const DatabaseContent& database, const QString& classGuidOrAppKey) {
    if (classGuidOrAppKey.isEmpty()) return {};

    // Prefer an actual class UID if an app key happens to have the same spelling.
    bool matchesUid = false;
    for (const auto& tagUid : database.tagUids()) {
        if (database.find(tagUid).value("tag_class_uid").toString() == classGuidOrAppKey) {
            matchesUid = true;
            break;
        }
    }

    const auto key = matchesUid ? "tag_class_uid" : "tag_class_app_key";
    QStringList result;
    for (const auto& tagUid : database.tagUids()) {
        if (database.find(tagUid).value(key).toString() == classGuidOrAppKey) result.append(tagUid);
    }
    return result;
}

void appendContentModel(QVariantList& result, const QString& uid) {
    if (auto* record = model::ContentModel::find(uid)) {
        result.append(QVariant::fromValue(static_cast<QObject*>(record)));
    }
}

QVariantList recordsWithTags(const DatabaseContent& database, const QStringList& tagUids) {
    if (tagUids.isEmpty()) return {};

    const QSet<QString> wanted(tagUids.cbegin(), tagUids.cend());
    QVariantList result;
    for (const auto& recordUid : database.recordUids()) {
        for (const auto& tagUid : database.tagUidsForRecord(recordUid)) {
            if (wanted.contains(tagUid)) {
                appendContentModel(result, recordUid);
                break;
            }
        }
    }
    return result;
}

} // namespace

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

QVariantList DsQmlBridge::getTagsForClass(const QString& classGuidOrAppKey) const {
    Q_ASSERT(QThread::currentThread() == thread());
    QVariantList result;
    for (const auto& uid : tagUidsForClass(m_database, classGuidOrAppKey)) appendContentModel(result, uid);
    return result;
}

QVariantList DsQmlBridge::getRecordsWithTag(const QString& tagGuid) const {
    Q_ASSERT(QThread::currentThread() == thread());
    if (tagGuid.isEmpty() || !m_database.tagUids().contains(tagGuid)) return {};
    return recordsWithTags(m_database, {tagGuid});
}

QVariantList DsQmlBridge::getRecordsWithTagClass(const QString& classGuidOrAppKey) const {
    Q_ASSERT(QThread::currentThread() == thread());
    return recordsWithTags(m_database, tagUidsForClass(m_database, classGuidOrAppKey));
}

QVariantList DsQmlBridge::getTagsForRecord(const QString& recordGuid) const {
    Q_ASSERT(QThread::currentThread() == thread());
    const auto assigned = m_database.tagUidsForRecord(recordGuid);
    QVariantList result;
    for (const auto& uid : m_database.tagUids()) {
        if (assigned.contains(uid)) appendContentModel(result, uid);
    }
    return result;
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
