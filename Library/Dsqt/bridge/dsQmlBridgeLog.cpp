#include "bridge/dsQmlBridgeLog.h"

namespace dsqt::bridge {

DsQmlBridgeLog::DsQmlBridgeLog(QObject* parent)
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

void DsQmlBridgeLog::setFile(const QString& path) {
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

void DsQmlBridgeLog::updateLog(const QString& path) {
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

void DsQmlBridgeLog::loadInitialLog() {
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

qint64 DsQmlBridgeLog::findStartPosition(QFile& file) {
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

void DsQmlBridgeLog::trimLines() {
    if (m_lines.size() <= MAX_LINES) return;

    qsizetype excess = m_lines.size() - MAX_LINES;
    beginRemoveRows(QModelIndex(), 0, excess - 1);
    m_lines.remove(0, excess);
    endRemoveRows();
}

} // namespace dsqt::bridge
