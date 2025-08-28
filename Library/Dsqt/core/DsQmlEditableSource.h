#include "dsEnvironment.h"
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QObject>
#include <QQmlEngine>
#include <QRandomGenerator>
#include <QUrl>

namespace dsqt {

class DsQmlEditableSource : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(DsEditableSource)
    Q_PROPERTY(QString source READ source WRITE setSource NOTIFY sourceChanged)

  public:
    explicit DsQmlEditableSource(QObject* parent = nullptr)
        : QObject(parent) {
        // Get notified of file changes.
        connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this](const QString& path) {
            if (path == m_source) emit sourceChanged();
        });
        // Each time the source file is changed, add it to the watcher if it's local and exists.
        // This is required because some editors delete and recreate the file, causing the watcher to lose track.
        connect(this, &DsQmlEditableSource::sourceChanged, this, [this] {
            qInfo() << "Watching file: " << m_source;
            m_watcher.addPath(m_source);
        });
    }

    QString source() const {
        // Trick the listener into reloading by appending a random query parameter.
        quint32 randQuery = QRandomGenerator::global()->generate();
        return QString("file:%1?%2").arg(m_source).arg(randQuery);
    }

    void setSource(QString source) {
        QUrl url = QUrl::fromLocalFile(DsEnvironment::expandq(source));

        QFileInfo fi(url.toLocalFile());
        source = fi.absoluteFilePath();

        if (fi.exists() && source != m_source) {
            // Remove old path from watcher if it was being watched.
            if (!m_source.isEmpty() && m_watcher.files().contains(m_source)) {
                m_watcher.removePath(m_source);
            }

            m_source = source;

            emit sourceChanged();
        } else {
            m_source.clear();

            qWarning() << "Can't watch " << source << " as it is not a local file.";
        }
    }

  signals:
    void sourceChanged();

  private:
    QFileSystemWatcher m_watcher;
    QString            m_source;
};

} // namespace dsqt
