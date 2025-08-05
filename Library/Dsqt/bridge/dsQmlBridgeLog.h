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

/**
 * @class DsQmlBridgeLog
 * @brief A QML bridge for monitoring and displaying log file contents as a list model.
 *
 * This class provides a model for displaying the last MAX_LINES lines of a log file in QML.
 * It monitors the file for changes using a timer and updates the model accordingly.
 */
class DsQmlBridgeLog : public QAbstractListModel {
    Q_OBJECT
    QML_NAMED_ELEMENT(DsBridgeLog)
    Q_PROPERTY(QString file READ file WRITE setFile NOTIFY fileChanged)

  public:
    /**
     * @brief Constructs a DsQmlBridgeLog instance.
     * @param parent The parent QObject (optional).
     */
    DsQmlBridgeLog(QObject* parent = nullptr);

    /**
     * @brief Returns the path to the log file being monitored.
     * @return The current log file path.
     */
    QString file() const { return m_path; }

    /**
     * @brief Sets the path to the log file to monitor.
     *
     * If the path changes, the model is reset, initial log content is loaded,
     * and monitoring starts via a timer.
     *
     * @param path The new log file path.
     */
    void setFile(const QString& path);

    /**
     * @brief Returns the number of rows (log lines) in the model.
     * @param parent The parent model index (unused).
     * @return The number of log lines.
     */
    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        if (parent.isValid()) return 0;
        return m_lines.size();
    }

    /**
     * @brief Returns data for the given index and role.
     * @param index The model index.
     * @param role The data role (only Qt::DisplayRole is supported).
     * @return The log line at the index or an invalid QVariant.
     */
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || index.row() >= m_lines.size()) return QVariant();
        if (role == Qt::DisplayRole) return m_lines.at(index.row());
        return QVariant();
    }

  signals:
    /**
     * @brief Emitted when the log file path changes.
     */
    void fileChanged();

    /**
     * @brief Emitted when the log content is updated.
     */
    void contentUpdated();

  private slots:
    /**
     * @brief Updates the log model with new content from the file.
     * @param path The file path (must match the current m_path).
     */
    void updateLog(const QString& path);

  private:
    /**
     * @brief Loads the initial log content into the model.
     *
     * Resets the model and loads the last MAX_LINES lines from the file.
     */
    void loadInitialLog();

    /**
     * @brief Finds the starting position in the file for loading the last MAX_LINES lines.
     * @param file The open QFile to read from.
     * @return The byte position to start reading from.
     */
    qint64 findStartPosition(QFile& file);

    /**
     * @brief Trims excess lines from the model if exceeding MAX_LINES.
     */
    void trimLines();

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
