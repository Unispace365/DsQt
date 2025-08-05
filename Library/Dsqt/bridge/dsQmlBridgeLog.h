#ifndef DSQMLBRIDGELOG_H
#define DSQMLBRIDGELOG_H

#include <QAbstractListModel>
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QQmlEngine>
#include <QStringList>
#include <QTimer>

namespace dsqt::bridge {

class DsQmlBridgeLog : public QAbstractListModel {
    Q_OBJECT
    QML_NAMED_ELEMENT(DsBridgeLog)
    Q_PROPERTY(QString file READ file WRITE setFile NOTIFY fileChanged)

  public:
    DsQmlBridgeLog(QObject* parent = nullptr)
        : QAbstractListModel(parent) {
        // Create file system watcher.
        // m_watcher = new QFileSystemWatcher(this);
        // connect(m_watcher, &QFileSystemWatcher::fileChanged, this, &DsQmlBridgeLog::updateLog);
        // connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &DsQmlBridgeLog::updateLog);

        // Files still open may not always properly trigger file system notifications.
        // Let's also add a timer to poll for changes.
        m_timer = new QTimer(this);
        m_timer->setInterval(500);
        connect(m_timer, &QTimer::timeout, this, [this]() { updateLog(m_path); });
    }

    // Returns the path to the log file being monitored.
    QString file() const { return m_path; }
    // Sets the path to the log file being monitored.
    void setFile(const QString& path) {
        if (path == m_path) return;
        m_path = path;

        // Make sure the file is watched for changes.
        if (!m_path.isEmpty()) {
            // if (!m_watcher->files().isEmpty()) m_watcher->removePaths(m_watcher->files());
            // if (!m_watcher->directories().isEmpty()) m_watcher->removePaths(m_watcher->directories());
            loadInitialLog();

            // QFileInfo info(m_path);
            // m_watcher->addPath(m_path);
            // m_watcher->addPath(info.dir().absolutePath());

            m_timer->start();
        } else {
            m_timer->stop();
        }

        emit fileChanged();
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        if (parent.isValid()) return 0;
        return m_lines.size();
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || index.row() >= m_lines.size()) return QVariant();
        if (role == Qt::DisplayRole) return m_lines.at(index.row());
        return QVariant();
    }

  signals:
    void fileChanged();
    void contentUpdated();

  private slots:
    void updateLog(const QString& path) {
        if (path != m_path) return;

        // Try to open the log file.
        QFile file(m_path);
        if (!file.open(QIODevice::ReadOnly)) return;

        qint64 currentSize = file.size();
        if (currentSize < m_lastPosition) {
            // File contents were changed and may have been deleted. Reload the file.
            file.close();
            loadInitialLog();
            return;
        } else if (currentSize == m_lastPosition) {
            // No changes detected.
            file.close();
            return;
        }

        // Reading file from last known position.
        file.seek(m_lastPosition);
        QByteArray newData = file.read(currentSize - m_lastPosition);
        file.close();

        m_lastPosition = currentSize;

        // Convert to UTF8.
        QString newText = QString::fromUtf8(newData);
        if (newText.isEmpty()) return;

        // Split into lines.
        QStringList newLines = newText.split('\n');
        if (!newText.endsWith('\n') && !newLines.isEmpty()) {
            // Last line is incomplete; adjust m_lastPosition to its start.
            m_lastPosition -= newLines.last().size();
            newLines.removeLast(); // Remove the partial line.
        } else if (newText.endsWith('\n') && !newLines.isEmpty()) {
            // Remove empty line caused by trailing newline.
            newLines.removeLast();
        }

        if (!newLines.isEmpty()) {
            // Add the new lines.
            beginInsertRows(QModelIndex(), rowCount(), rowCount() + newLines.size() - 1);
            m_lines.append(newLines);
            endInsertRows();

            // Remove oldest content if necessary.
            trimLines();

            emit contentUpdated();
        }
    }

  private:
    void loadInitialLog() {
        QFile file(m_path);
        if (!file.open(QIODevice::ReadOnly)) {
            beginResetModel();
            m_lines.clear();
            endResetModel();
            m_lastPosition = 0;
        } else {
            qint64 startPos = findStartPosition(file);
            file.seek(startPos);

            // Read file.
            QByteArray data = file.read(m_lastPosition - startPos);
            QString    text = QString::fromUtf8(data);
            file.close();

            // Split into lines.
            QStringList lines = text.split('\n');
            if (!text.endsWith('\n') && !lines.isEmpty()) {
                // Last line is incomplete; adjust m_lastPosition to its start.
                m_lastPosition -= lines.last().size();
                lines.removeLast(); // Remove the partial line.
            } else if (text.endsWith('\n') && !lines.isEmpty()) {
                // Remove empty line caused by trailing newline.
                lines.removeLast();
            }

            // Update content.
            beginResetModel();
            m_lines = lines;
            endResetModel();
        }

        emit contentUpdated();
    }

    qint64 findStartPosition(QFile& file) {
        // Obtain file size.
        m_lastPosition = file.size();
        if (m_lastPosition == 0) return 0;

        // Read in chunks of 4kB each.
        const int chunkSize = 4096;

        // Find the position of the line to start with, based on the maximum number of lines allowed.
        qsizetype linesFound = 0;
        qint64    pos        = m_lastPosition;
        while (pos > 0 && linesFound < MAX_LINES) {
            qint64 readPos = qMax(qint64(0), pos - chunkSize);
            qint64 bytes   = pos - readPos;
            file.seek(readPos);

            QByteArray chunk = file.read(bytes);
            if (chunk.isEmpty()) break;

            for (int i = chunk.size() - 1; i >= 0; --i) {
                if (chunk.at(i) == '\n') {
                    ++linesFound;
                    if (linesFound == MAX_LINES) {
                        return readPos + i + 1;
                    }
                }
            }
            pos = readPos;
        }
        return 0;
    }

    // Removes oldest lines if line count exceeds MAX_LINES.
    void trimLines() {
        if (m_lines.size() <= MAX_LINES) return;

        qsizetype excess = m_lines.size() - MAX_LINES;
        beginRemoveRows(QModelIndex(), 0, excess - 1);
        m_lines.remove(0, excess);
        endRemoveRows();
    }


  private:
    QString     m_path;
    QStringList m_lines;
    // QFileSystemWatcher* m_watcher      = nullptr;
    QTimer*         m_timer        = nullptr;
    qint64          m_lastPosition = 0;
    const qsizetype MAX_LINES      = 500;
};


} // namespace dsqt::bridge

#endif
