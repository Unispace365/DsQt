#ifndef DSQMLTEXTFILEMODEL_H
#define DSQMLTEXTFILEMODEL_H

#include <QAbstractListModel>
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QQmlEngine>
#include <QStringList>
#include <QTimer>

namespace dsqt::model {

/**
 * @class DsQmlTextFileModel
 * @brief A QML element for monitoring and displaying text file contents as a list model.
 *
 * This class provides a model for displaying the last lines of a text file in QML.
 * It monitors the file for changes using a timer and updates the model accordingly.
 */
class DsQmlTextFileModel : public QAbstractListModel {
    Q_OBJECT
    QML_NAMED_ELEMENT(DsTextFileModel)
    Q_PROPERTY(QString file READ file WRITE setFile NOTIFY fileChanged)
    Q_PROPERTY(
        qsizetype maximumLineCount READ maximumLineCount WRITE setMaximumLineCount NOTIFY maximumLineCountChanged)

  public:
    /**
     * @brief Constructs a DsQmlTextFileModel instance.
     * @param parent The parent QObject (optional).
     */
    DsQmlTextFileModel(QObject* parent = nullptr);

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

    qsizetype maximumLineCount() const { return m_max_line_count; }

    void setMaximumLineCount(qsizetype count) {
        if (m_max_line_count == count) return;
        m_max_line_count = count;
        loadInitialLog();
        emit maximumLineCountChanged();
    }

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
     * @brief Emitted when the maximum line count changes.
     */
    void maximumLineCountChanged();

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
     * Resets the model and loads the last lines from the file.
     */
    void loadInitialLog();

    /**
     * @brief Finds the starting position in the file for loading the last lines.
     * @param file The open QFile to read from.
     * @return The byte position to start reading from.
     */
    qint64 findStartPosition(QFile& file);

    /**
     * @brief Trims excess lines from the model if exceeding the maximum line count.
     */
    void trimLines();

  private:
    // QFileSystemWatcher* m_watcher      = nullptr;
    QString     m_path;
    QStringList m_lines;
    QTimer*     m_timer          = nullptr;
    qint64      m_lastPosition   = 0;
    qsizetype   m_max_line_count = 500;
};

} // namespace dsqt::bridge

#endif
