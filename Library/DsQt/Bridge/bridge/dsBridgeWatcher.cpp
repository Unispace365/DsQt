#include "bridge/dsBridgeWatcher.h"

namespace dsqt::bridge {

DsBridgeWatcher::DsBridgeWatcher(const QFileInfo& dbPath, QObject* parent)
    : QObject(parent) {
    // Make sure the directory exists.
    QDir().mkdir(dbPath.absolutePath());

    m_database_file.setFile(QDir(dbPath.path()), "db.sqlite");
    m_notification_file.setFile(QDir(dbPath.path()), "sync_notification.txt");

    // Initialize watcher
    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, &DsBridgeWatcher::onChanged);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &DsBridgeWatcher::onChanged);

    // Start processing
    processFile();
}

void DsBridgeWatcher::onChanged(const QString& path) {
    m_database_file.refresh();
    m_notification_file.refresh();
    if (m_database_file.exists() && m_notification_file.exists())
        processFile();
    else
        m_last_modified = {};
}

void DsBridgeWatcher::processFile() {
    // Add directory and file to watcher if not already monitored
    if (!m_watcher->directories().contains(m_notification_file.dir().absolutePath())) {
        m_watcher->addPath(m_notification_file.dir().absolutePath());
        qDebug() << "Monitoring" << m_notification_file.dir();
    }
    if (!m_watcher->files().contains(m_notification_file.absoluteFilePath())) {
        m_watcher->addPath(m_notification_file.absoluteFilePath());
        qDebug() << "Monitoring" << m_notification_file;
    }

    // Check if the file exists. If not, warn the user.
    m_notification_file.refresh();
    if (!m_notification_file.exists()) {
        qDebug() << "Notification file" << m_notification_file << "missing. Make sure BridgeSync is running.";
        return;
    }

    // Check if the file has changed. If it has, notify listeners.
    if (m_last_modified != m_notification_file.lastModified()) {
        m_last_modified = m_notification_file.lastModified();

        // Signal listeners.
        qDebug() << "Database updated";
        emit databaseUpdated();
    }
}

} // namespace dsqt::bridge
