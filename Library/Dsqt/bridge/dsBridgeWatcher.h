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

class DsBridgeWatcher : public QObject {
    Q_OBJECT
  public:
    DsBridgeWatcher(const QFileInfo& dbPath, QObject* parent = nullptr);

  signals:
    void databaseUpdated();

  private slots:
    void onChanged(const QString& path);

    void processFile();

  private:
    QFileInfo           m_database_file;
    QFileInfo           m_notification_file;
    QDateTime           m_last_modified;
    QFileSystemWatcher* m_watcher;
};

} // namespace dsqt::bridge

#endif
