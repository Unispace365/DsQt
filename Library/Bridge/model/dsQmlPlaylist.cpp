#include "model/dsQmlPlaylist.h"
#include "bridge/dsBridge.h"
#include "core/dsEnvironment.h"
#include "core/dsQmlApplicationEngine.h"

#include <QFileInfo>
#include <QQmlContext>

namespace dsqt::model {

DsQmlPlaylist::DsQmlPlaylist(QObject* parent)
    : QObject(parent) {
    auto& bridge = bridge::DsBridge::instance();

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);

    // Listen to timer.
    connect(m_timer, &QTimer::timeout, this, [this] { setIndex(m_playlist_index + 1); });
    // Listen to content updates.
    connect(&bridge, &bridge::DsBridge::contentChanged, this, &DsQmlPlaylist::updateNow);
    // Restart playlist if mode changes.
    connect(this, &DsQmlPlaylist::modeChanged, this, &DsQmlPlaylist::updateNow);
    // Restart playlist if event changes.
    connect(this, &DsQmlPlaylist::eventChanged, this, &DsQmlPlaylist::updateNow);
}

void DsQmlPlaylist::setModes(const QVariantMap& modes) {
    if (!modes.contains(m_mode)) {
        setMode("");
    }
    if (modes != m_modes) {
        m_modes = modes;
        emit modesChanged();
    }
}

void DsQmlPlaylist::setTemplates(QVariantMap templates) {
    // Normalize values.
    for (QVariantMap::Iterator itr = templates.begin(); itr != templates.end();) {
        QString path = DsEnvironment::expandq(itr.value().toString());
        QUrl    url  = QUrl::fromUserInput(path);
        if (url.scheme() != "qrc" && url.scheme() != "file") {
            // TODO log not a file or resource
            itr = templates.erase(std::as_const(itr));
        } else if (url.scheme() == "file" && !QFileInfo::exists(url.toLocalFile())) {
            // TODO log file does not exist
            itr = templates.erase(std::as_const(itr));
        } else {
            itr->setValue(url.toString());
            ++itr;
        }
    }

    //
    if (templates != m_templates) {
        m_templates = templates;

        // for (auto [key, value] : m_templates.asKeyValueRange()) {
        //     if (value.isValid()) {
        //         // Flag existing component for deletion.
        //         if (auto itr = m_components.find(key); itr != m_components.end()) {
        //             itr.value()->deleteLater();
        //         }

        //         // Create new component.
        //         QQmlComponent* component = new QQmlComponent(qmlEngine(this), this);
        //         component->loadUrl(value.toString(), QQmlComponent::CompilationMode::Asynchronous);
        //         m_components.insert(key, component);
        //     }
        // }

        emit templatesChanged();
    }
}

void DsQmlPlaylist::setComponents(const QVariantMap& components) {
    m_components = components;
    emit componentsChanged();
}

void DsQmlPlaylist::setEvent(DsQmlEvent* event) {
    if (event != m_event) {
        m_event = event;

        // auto engine = DsQmlApplicationEngine::DefEngine();
        // if (m_event) {
        //     // No longer need to listen for content updates, as we will get them from the event.
        //     disconnect(engine, &DsQmlApplicationEngine::rootUpdated, this, &DsQmlPlaylist::updateNow);
        // } else {
        //     // Listen to content updates.
        //     connect(engine, &DsQmlApplicationEngine::rootUpdated, this, &DsQmlPlaylist::updateNow);
        // }

        emit eventChanged();
    }
}


void DsQmlPlaylist::updateNow() {
    // Only if we have a valid mode.
    if (!m_modes.contains(m_mode)) return;

    // Obtain mode.
    auto mode = m_modes.value(m_mode).toMap();

    // Obtain engine pointer.
    auto engine = DsQmlApplicationEngine::DefEngine();

    // Obtain platform playlist from engine.
    auto platformUid = engine->getAppSettings()->getOr<QString>("platform.id", "");
    auto platform    = model::ContentModel::find(platformUid);
    if (!platform) return;

    auto platformKey = mode.value("platformKey").toString();
    auto playlistUid = platform->getProperty<QString>(platformKey);

    // Use the event's playlist if available.
    if (m_event) {
        const auto& record = m_event->record();
        if (auto uid = record.value(mode.value("eventKey").toString()).toString(); !uid.isEmpty()) {
            playlistUid = uid;
        }
    }

    auto model = model::ContentModel::find(playlistUid);
    if (model != m_playlist_model) {
        // Force a playlist restart if this is a different playlist.
        if (model && m_playlist_model &&
            model->getProperty<QString>("uid") != m_playlist_model->getProperty<QString>("uid")) {
            m_model = nullptr;
        }

        // Keep track of playlist.
        m_playlist_model = model;

        if (!m_model || !m_playlist_model) {
            // Start the playlist now.
            setIndex(0);
        } else {
            // Adjust the playlist index if needed.
            auto children = m_playlist_model->getChildren();
            for (qsizetype i = 0; i < children.size(); ++i) {
                if (m_model == children.at(i)) {
                    m_playlist_index = i;
                    return;
                }
            }
            // Not a child, so reset the playlist.
            setIndex(0);
        }
    }
}

void DsQmlPlaylist::setIndex(qsizetype index) {
    // Stop the timer.
    m_timer->stop();

    // Always update index.
    m_playlist_index = index;

    // Obtain content model and check for changes.
    auto count = m_playlist_model ? m_playlist_model->getChildren().size() : 0;
    if (count > 0) {
        auto model      = m_playlist_model->getChild(index % count);
        bool qmlChanged = model != m_model;

        // Update content model now, so it can be used by new templates,
        // but don't emit signal yet, so existing templates won't be affected.
        m_model = model;

        // Get template QML and update source and component.
        QString  typeUid = m_model->getProperty<QString>("type_uid");
        QVariant value   = m_templates.value(typeUid);
        QString  source  = value.toString();

        if (!source.isEmpty() && source != m_source) {
            m_source = source;
            emit sourceChanged();
        }

        auto variant = m_components.value(typeUid);
        if (variant.canConvert<QQmlComponent*>()) {
            auto component = variant.value<QQmlComponent*>();
            if (component && component != m_component) {
                m_component = component;
                emit componentChanged();
            }
        }

        // Emit signal now.
        if (qmlChanged) emit modelChanged();

        // Always restart timer.
        m_timer->start(m_interval);
    } else {
        if (!m_source.isEmpty()) {
            m_source.clear();
            emit sourceChanged();
        }
        if (m_component) {
            m_component = nullptr;
            emit componentChanged();
        }
        if (m_model) {
            m_model = nullptr;
            emit modelChanged();
        }
    }
}

} // namespace dsqt::model
