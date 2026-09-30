#ifndef DSBRIDGE_H
#define DSBRIDGE_H

#include "bridge/dsBridgeDatabase.h"
#include "model/dsContentModel.h"
#include "settings/dsSettings.h"

#include <QObject>
#include <QQmlEngine>


namespace dsqt::bridge {
// TODO: logging category for this class

class DsQmlBridge : public QObject {
    Q_OBJECT
    QML_SINGLETON
    QML_NAMED_ELEMENT(DsBridge)
    Q_PROPERTY(dsqt::model::ContentModel* content READ content NOTIFY contentChanged FINAL)

  public:
    static DsQmlBridge& instance() {
        static DsQmlBridge instance;
        return instance;
    }

    ~DsQmlBridge() = default;

    dsqt::model::ContentModel* content() const;

    static DsQmlBridge* create(QQmlEngine*, QJSEngine*) {
        auto& inst = instance();
        return &inst;
    }

    /**
     * @brief Returns the Bridge database content in a thread-safe manner.
     * @return Const reference to bridge::DatabaseContent.
     */
    const bridge::DatabaseContent& database() const { return m_database; }

    /**
     * @brief Get ContentModel by Id.
     * @param id The unique identifier of the content record.
     * @return Pointer to ContentModel if found, nullptr otherwise.
     */
    Q_INVOKABLE model::ContentModel* getRecordById(const QString& id) const {
        auto record = model::ContentModel::find(id);
        if (!record)
            return nullptr;
        else
            return record;
    }

    /// Returns tag ContentModels for a class UID or its exact CMS app_key, in tag catalog order.
    /// Class UIDs take precedence over app keys. Unknown or empty keys return an empty list.
    /// Use on the main thread, after bridgeUpdated; refresh the list on subsequent updates.
    Q_INVOKABLE QVariantList getTagsForClass(const QString& classGuidOrAppKey) const;

    /// Returns loaded ContentModels carrying the tag, in record order, without duplicates.
    /// Includes direct assignments and TAGS fields. Use on the main thread after bridgeUpdated.
    Q_INVOKABLE QVariantList getRecordsWithTag(const QString& tagGuid) const;

    /// Returns loaded ContentModels carrying any tag in the class, in record order.
    /// Accepts a class UID or exact CMS app_key. Use on the main thread after bridgeUpdated.
    Q_INVOKABLE QVariantList getRecordsWithTagClass(const QString& classGuidOrAppKey) const;

    /// Returns a record's distinct tag ContentModels in tag catalog order.
    /// Includes direct assignments and TAGS fields. Use on the main thread after bridgeUpdated.
    Q_INVOKABLE QVariantList getTagsForRecord(const QString& recordGuid) const;

    /**
     * @brief Get uid of platform from app_settings platform.id
     * @return QString uid of platform if found, empty QString otherwise.
     */
    Q_INVOKABLE QString getPlatformUid() const {
        auto platformId = Settings::find<QString>("app_settings", "platform.id", "");
        if (platformId.isEmpty()) {
            qDebug() << "Attempting to get platform uid but platform.id is not set in app_settings";
            return "";
        }
        for (const auto& platform : m_database.platforms()) {
            if (platform.value("uid").toString() == platformId) {
                return platform.value("uid").toString();
            }
        }
        qDebug() << "Attempting to get platform uid but no platform with id " << platformId << " found in database";
        return "";
    }

    /**
     * @brief Get uids of platforms
     * @return QStringList uid of platforms.
     */
    Q_INVOKABLE QStringList getPlatformUids() const {
        QStringList uids;
        for (const auto& platform : m_database.platforms()) {
            uids.append(platform.value("uid").toString());
        }
        return uids;
    }

    /**
     * @brief Get ContentModel of platform by id from app_settings platform.id
     * @return Pointer to ContentModel if found, nullptr otherwise.
     */
    Q_INVOKABLE model::ContentModel* getPlatformRecord() const { return getRecordById(getPlatformUid()); }

  signals:
    /**
     * @brief Emitted when the root content is updated. You should only listen to this on the main thread.
     */
    void contentChanged();

    /**
     * @brief Emitted when the root content is updated. Thread-safe version.
     */
    void databaseChanged();

    /**
     * @brief Emitted when the bridge has updated. In practice this is the same as databaseChanged, but with the
     * semantic meaning that the update is complete and all relevant properties have been updated.
     * You should listen to this on the main thread.
     */
    void bridgeUpdated();

  private:
    DsQmlBridge();

    friend class DsBridgeSqlQuery;
    void setContent(dsqt::model::ContentModel* newContent);
    void setDatabase(bridge::DatabaseContent&& database);

    dsqt::model::ContentModel* m_content = nullptr;
    bridge::DatabaseContent    m_database;
};

} // namespace dsqt::bridge

#endif // DSBRIDGE_H
