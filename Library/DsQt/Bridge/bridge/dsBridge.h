#ifndef DSBRIDGE_H
#define DSBRIDGE_H

#include "bridge/dsBridgeDatabase.h"
#include "model/dsContentModel.h"

#include <QObject>
#include <QQmlEngine>

namespace dsqt::bridge {

class DsBridge : public QObject {
    Q_OBJECT
    QML_SINGLETON
    QML_NAMED_ELEMENT(DsBridge)
    Q_PROPERTY(model::ContentModel* content READ content WRITE setContent NOTIFY contentChanged FINAL)

  public:
    static DsBridge& instance() {
        static DsBridge instance;
        return instance;
    }

    ~DsBridge() = default;

    model::ContentModel* content() const;

    void setContent(model::ContentModel* newContent);

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

  private:
    DsBridge();

    model::ContentModel*    m_content = nullptr;
    bridge::DatabaseContent m_database;
};

} // namespace dsqt::bridge

#endif // DSBRIDGE_H
