#ifndef DSQMLPLAYLIST_H
#define DSQMLPLAYLIST_H

#include "model/dsContentModel.h"
#include "model/dsQmlContentModel.h"
#include "model/dsQmlEventSchedule.h"
#include "rework/rwContentModel.h"

#include <QColor>
#include <QDateTime>
#include <QFutureWatcher>
#include <QObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlListProperty>
#include <QtConcurrentRun>

Q_DECLARE_LOGGING_CATEGORY(lgPlaylist)
Q_DECLARE_LOGGING_CATEGORY(lgPlaylistVerbose)
namespace dsqt::model {

class DsQmlPlaylist : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(DsPlaylist)
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(QVariantMap modes READ modes WRITE setModes NOTIFY modesChanged)
    Q_PROPERTY(QVariantMap templates READ templates WRITE setTemplates NOTIFY templatesChanged)
    Q_PROPERTY(QVariantMap components READ components WRITE setComponents NOTIFY componentsChanged)
    Q_PROPERTY(QString source READ source NOTIFY sourceChanged)
    Q_PROPERTY(QQmlComponent* component READ component NOTIFY componentChanged)
    Q_PROPERTY(rework::RwContentModel* model READ model NOTIFY modelChanged)
    Q_PROPERTY(DsQmlEvent* event READ event WRITE setEvent NOTIFY eventChanged)
    Q_PROPERTY(int interval READ interval WRITE setInterval NOTIFY intervalChanged FINAL)

  public:
    explicit DsQmlPlaylist(QObject* parent = nullptr);

    const QString& mode() const { return m_mode; }
    void           setMode(const QString& mode) {
        if (mode != m_mode) {
            m_mode = mode;
            emit modeChanged();
        }
    }

    const QVariantMap& modes() const { return m_modes; };
    void               setModes(const QVariantMap& modes);

    const QVariantMap& templates() const { return m_templates; };
    void               setTemplates(QVariantMap templates);

    const QVariantMap& components() const { return m_components; };
    void               setComponents(const QVariantMap& components);

    const QString&          source() const { return m_source; }
    QQmlComponent*          component() const { return m_component; }
    rework::RwContentModel* model() const { return m_model; }

    int  interval() const { return m_interval; }
    void setInterval(int interval) {
        if (interval != m_interval) {
            m_interval = interval;
            emit intervalChanged();
        }
    }

    DsQmlEvent* event() const { return m_event; }
    void        setEvent(DsQmlEvent* event);

  signals:
    void modeChanged();
    void modesChanged();
    void templatesChanged();
    void componentsChanged();
    void sourceChanged();
    void componentChanged();
    void modelChanged();
    void scheduleChanged();
    void eventChanged();
    void intervalChanged();

  private:
    void updateNow();
    void setIndex(qsizetype index);

    QString                 m_mode;
    QVariantMap             m_modes;
    QVariantMap             m_templates;
    QVariantMap             m_components;
    QString                 m_source;
    QQmlComponent*          m_component      = nullptr; //
    rework::RwContentModel* m_model          = nullptr; // Current slide.
    DsQmlEvent*             m_event          = nullptr; // Current event.
    QTimer*                 m_timer          = nullptr; //
    rework::RwContentModel* m_playlist_model = nullptr; // Current playlist.
    qsizetype               m_playlist_index = 0;       //
    int                     m_interval       = 60000;   // In milliseconds.
    rework::RwContentModel* m_bridge         = nullptr; // Pointer to bridge content instance.
};

} // namespace dsqt::model

#endif
