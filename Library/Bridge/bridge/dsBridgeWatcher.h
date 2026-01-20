#ifndef DSBRIDGEWATCHER_H
#define DSBRIDGEWATCHER_H

#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QTextStream>
#include <QTimer>

namespace dsqt::bridge {

/**
 * @class DsBridgeWatcher
 * @brief Monitors a notification file for changes indicating database updates.
 *
 * This class uses a QFileSystemWatcher to monitor a notification file and emits
 * a signal when changes are detected, signaling that the associated database has been updated.
 */
class DsBridgeWatcher : public QObject {
    Q_OBJECT
  public:
    /**
     * @brief Constructs a DsBridgeWatcher instance.
     * @param dbPath The QFileInfo representing the path to the database directory.
     * @param parent Optional parent QObject for ownership.
     */
    DsBridgeWatcher(const QFileInfo& dbPath, QObject* parent = nullptr);

  signals:
    /**
     * @brief Signal emitted when the database has been updated.
     *
     * This signal is triggered when changes are detected in the monitored notification file,
     * indicating that the database content may have changed.
     */
    void databaseUpdated();

  private slots:
    /**
     * @brief Slot called when a monitored file or directory changes.
     * @param path The path of the file or directory that changed.
     *
     * This slot refreshes the file infos and checks if processing is needed.
     */
    void onChanged(const QString& path);

    /**
     * @brief Processes the notification file to detect changes.
     *
     * This method adds paths to the watcher if necessary, checks for file existence,
     * and emits the databaseUpdated signal if the file has been modified.
     */
    void processFile();

  private:
    QFileInfo           m_database_file;
    QFileInfo           m_notification_file;
    QDateTime           m_last_modified;
    QFileSystemWatcher* m_watcher;
};

} // namespace dsqt::bridge

#endif
