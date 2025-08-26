#ifndef DSQMLEDITABLESOURCE_H
#define DSQMLEDITABLESOURCE_H

#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QObject>
#include <QQmlEngine>

namespace dsqt {

class DsQmlEditableSource : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(DsEditableSource)
    Q_PROPERTY(QString source READ source WRITE setSource NOTIFY sourceChanged)

  public:
    explicit DsQmlEditableSource(QObject* parent = nullptr)
        : QObject(parent) {
        //
        connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this](const QString& path) {
            if (path == m_source) {
                emit sourceChanged();
            }
        });
        // Each time the source file is changed, add it to the watcher. This is required, because some editors delete
        // and recreate the file, causing the file watcher to remove it from its list.
        connect(this, &DsQmlEditableSource::sourceChanged, this, [this] {
            if (QFileInfo::exists(m_source)) m_watcher.addPath(m_source);
        });
    }

    QString source() const {
        // Trick the listener into reloading the file by adding a random value to its path.
        return QString("file:%1?%2").arg(m_source).arg(rand());
    }

    void setSource(QString source) {
        // (normalize to absolute path)
        source = QFileInfo(source).canonicalFilePath();
        if (source == m_source) {
            return;
        }

        // Remove old path if watching
        if (!m_source.isEmpty()) {
            m_watcher.removePath(m_source);
        }

        // Set new source
        m_source = source;

        emit sourceChanged();
    }

  signals:
    void sourceChanged();

  private:
    QString            m_source;
    QFileSystemWatcher m_watcher;
};

} // namespace dsqt

#endif
