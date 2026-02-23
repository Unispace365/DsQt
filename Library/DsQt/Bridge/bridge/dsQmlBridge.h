#ifndef DSBRIDGE_H
#define DSBRIDGE_H

#include "bridge/dsBridgeDatabase.h"
#include "model/dsContentModel.h"
#include "settings/dsSettings.h"

#include <QObject>
#include <QQmlEngine>


namespace dsqt::bridge {
//TODO: logging category for this class

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

    void setContent(dsqt::model::ContentModel* newContent);

    static DsQmlBridge* create(QQmlEngine *, QJSEngine *){
        auto& inst = instance();
        return &inst;
    }

    /**
     * @brief Returns the Bridge database content in a thread-safe manner.
     * @return Const reference to bridge::DatabaseContent.
     */
    const bridge::DatabaseContent& database() const { return m_database; }

    /**
     * @brief setDatabase
     * @param database
     */
    void setDatabase(bridge::DatabaseContent&& database) {
        m_database = std::move(database);
        emit databaseChanged();
        emit bridgeUpdated();
    }

    /**
     * @brief Get ContentModel by Id.
     * @param id The unique identifier of the content record.
     * @return Pointer to ContentModel if found, nullptr otherwise.
     */
    Q_INVOKABLE model::ContentModel* getRecordById(const QString& id) const {
        auto record = model::ContentModel::find(id);
        if (!record) return nullptr;
        else return record;
    }

    /**
     * @brief Get uid of platform from app_settings platform.id
     * @return QString uid of platform if found, empty QString otherwise.
     */
    Q_INVOKABLE QString getPlatformUid() const {
        auto platformId = DsSettings::getSettings("app_settings")->getOr<QString>("platform.id", "");
        if (platformId.isEmpty()) {
            qDebug()<<"Attempting to get platform uid but platform.id is not set in app_settings";
            return "";
        }
        for (const auto& platform : m_database.platforms()) {
            if (platform.value("uid").toString() == platformId) {
                return platform.value("uid").toString();
            }
        }
        qDebug()<<"Attempting to get platform uid but no platform with id "<<platformId<<" found in database";
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
    Q_INVOKABLE model::ContentModel* getPlatformRecord() const {
        return getRecordById(getPlatformUid());
    }



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
     * @brief Emitted when the bridge is updated. This is a general signal that can be used to indicate any update to the bridge, including content and database changes. It is thread-safe.
     */
    void bridgeUpdated();

  private:
    DsQmlBridge();

    dsqt::model::ContentModel*    m_content = nullptr;
    bridge::DatabaseContent m_database;
};

} // namespace dsqt::bridge

#endif // DSBRIDGE_H
