#include "dsQmlPlaylistControl.h"

#include <dsQmlApplicationEngine.h>
namespace dsqt::ui {
DsQmlPlaylistControl::DsQmlPlaylistControl() {
    // create a convience variable for the engine
    mEngine         = DsQmlApplicationEngine::DefEngine(); // create a timer and connect its timout to next();
    mAdvancingTimer = new QTimer(this);
    connect(mAdvancingTimer, &QTimer::timeout, this, &DsQmlPlaylistControl::next);

    // handle model and template map changes
    // processTemplateMap will call handleModelOrTemplateChange
    connect(this, &DsQmlPlaylistControl::modelChanged, this, &DsQmlPlaylistControl::handleModelOrTemplateChange);
    connect(this, &DsQmlPlaylistControl::templateMapChanged, this, &DsQmlPlaylistControl::processTemplateMap);
}


int DsQmlPlaylistControl::intervalDefault() const {
    return mIntervalDefault;
}

void DsQmlPlaylistControl::setIntervalDefault(int newIntervalDefault) {
    if (mIntervalDefault == newIntervalDefault) return;
    mIntervalDefault = newIntervalDefault;
    emit intervalDefaultChanged();
}

int DsQmlPlaylistControl::interval() const {
    return mInterval;
}

void DsQmlPlaylistControl::setInterval(int newInterval) {
    if (mInterval == newInterval) return;
    mInterval = newInterval;
    emit intervalChanged();
}

QString DsQmlPlaylistControl::userKey() const {
    return mUserKey;
}

void DsQmlPlaylistControl::setUserKey(const QString& newUserKey) {
    if (mUserKey == newUserKey) return;
    mUserKey = newUserKey;
    emit userKeyChanged();
}

dsqt::model::ContentModel* DsQmlPlaylistControl::model() const {
    return mModel;
}

void DsQmlPlaylistControl::setModel(dsqt::model::ContentModel* newModel) {
    if (mModel == newModel) return;

    if (m_updateMode == UpdateMode::Immediate) {
        mModel = newModel;
        emit modelChanged();
    } else {
        // if we are not in immediate mode, we will set the model later
        mNextModel = newModel;
        emit nextModelChanged();
    }
}

QVariantMap DsQmlPlaylistControl::templateMap() const {
    return mTemplateMap;
}

void DsQmlPlaylistControl::setTemplateMap(const QVariantMap& newTemplateMap) {
    if (mTemplateMap == newTemplateMap) return;
    mTemplateMap = newTemplateMap;
    emit templateMapChanged();
}

void DsQmlPlaylistControl::next() {
    if (m_updateMode == UpdateMode::NextInterval) {
        // if we are not in immediate mode, we need to set the model first
        updateModel();
    }

    if (!mModel) {
        return;
    }

    auto tempIndex = m_playlistIndex;
    // advance the index
    if (tempIndex + 1 < mModel->getChildren().size()) {
        tempIndex++;
    } else {
        tempIndex = 0;
    }
    m_playlistIndex = tempIndex;
    auto didAdvance = loadTemplate(tempIndex);
    if (didAdvance) {
        emit currentItemChanged();
        emit navigatedNext();
    } else {
        QMetaObject::invokeMethod(this, [this]() { next(); }, Qt::QueuedConnection); // try again in the next event loop
    }
}

void DsQmlPlaylistControl::prev() {
    if (m_updateMode == UpdateMode::NextInterval) {
        // if we are not in immediate mode, we need to set the model first
        updateModel();
    }

    if (!mModel) {
        return;
    }

    auto tempIndex = m_playlistIndex;
    // advance the index
    if (tempIndex - 1 >= 0) {
        tempIndex--;
    } else {
        tempIndex = mModel->getChildren().size() - 1;
    }
    m_playlistIndex = tempIndex;
    auto didAdvance = loadTemplate(tempIndex);
    if (didAdvance) {
        emit currentItemChanged();
        emit navigatedPrev();
    } else {
        QMetaObject::invokeMethod(this, [this]() { prev(); }, Qt::QueuedConnection); // try again in the next event loop
    }
}

void DsQmlPlaylistControl::processTemplateMap() {
    for (auto [key, value] : mTemplateComponents.asKeyValueRange()) {
        value->deleteLater();
    }
    mTemplateComponents.clear();
    if (mTemplateMap.isEmpty()) {
        return;
    }

    for (auto [key, value] : mTemplateMap.asKeyValueRange()) {
        QUrl url;
        if (value.isValid()) {
            url = QUrl::fromUserInput(value.toString());
            if (url.scheme() != "qrc" && url.scheme() != "file") {
                // log invalid url
                continue;
            }
        }
        QQmlComponent* component = new QQmlComponent(qmlEngine(this), this);

        component->loadUrl(url, QQmlComponent::CompilationMode::Asynchronous);
        mTemplateComponents.insert(key, component);
    }
    qInfo() << "Processed " << mTemplateComponents.size() << " Templates to the map";
    handleModelOrTemplateChange();
}

void DsQmlPlaylistControl::handleModelOrTemplateChange() {
    if (m_updateMode == UpdateMode::Immediate) {
        m_playlistIndex = -1;
        mAdvancingTimer->stop();
    }
    if (!mModel) return;

    QMetaObject::invokeMethod(this, [this]() { next(); }, Qt::QueuedConnection);
}

bool DsQmlPlaylistControl::loadTemplate(int index) {
    if (!mModel) {
        return false;
    }
    mAdvancingTimer->stop();

    auto    modelItem = mModel->getChild(index);
    QString typeId    = modelItem->getProperty<QString>("type_uid");
    if (mTemplateComponents.contains(typeId)) {
        qInfo() << "found template for " << typeId;
        // find the interval in the modelItem via intervalKey
        auto interval = mInterval;

        if (modelItem->contains(m_intervalKey)) interval = modelItem->getProperty<int>(m_intervalKey);

        if (mInterval != mNoInterval) {
            if (mInterval == 0) {
                interval = mIntervalDefault;
            }
            if (interval == 0) {
                interval = 5000; // if we are still 0 set it to 5 seconds
            }
            setInterval(interval);

            mAdvancingTimer->setInterval(interval);
            mAdvancingTimer->setSingleShot(true);
            mAdvancingTimer->start();
        }
        m_currentModelItem    = modelItem;
        auto comp             = mTemplateComponents[typeId];
        m_currentTemplateComp = comp;
        return true;
    }
    return false;
}

QString DsQmlPlaylistControl::intervalKey() const {
    return m_intervalKey;
}

void DsQmlPlaylistControl::setIntervalKey(const QString& newIntervalKey) {
    if (m_intervalKey == newIntervalKey) return;
    m_intervalKey = newIntervalKey;
    emit intervalKeyChanged();
}

QQmlComponent* DsQmlPlaylistControl::currentTemplateComp() const {
    return m_currentTemplateComp;
}


model::ContentModel* DsQmlPlaylistControl::currentModelItem() const {
    return m_currentModelItem;
}

DsQmlPlaylistControl::UpdateMode DsQmlPlaylistControl::updateMode() const {
    return m_updateMode;
}

void DsQmlPlaylistControl::setUpdateMode(const dsqt::ui::DsQmlPlaylistControl::UpdateMode& newUpdateMode) {
    if (m_updateMode == newUpdateMode) return;
    m_updateMode = newUpdateMode;
    emit updateModeChanged();
}


void DsQmlPlaylistControl::updateModel(bool force) {
    if (!force && m_updateMode != UpdateMode::Immediate) {
        qWarning() << "Update model called but not in immediate mode, ignoring";
        return;
    }

    if (mNextModel != nullptr && mModel != mNextModel) {
        mModel     = mNextModel;
        mNextModel = nullptr;
        emit modelChanged();
        emit nextModelChanged();
    } else {
        qWarning() << "No next model to update to";
    }
}

model::ContentModel* DsQmlPlaylistControl::nextModel() const {
    return mNextModel;
}

bool DsQmlPlaylistControl::active() const {
    return m_active;
}

void DsQmlPlaylistControl::setActive(bool newActive) {
    if (m_active == newActive) return;
    m_active = newActive;
    if (m_active) {
        // start the timer if we are active
        next();
    } else {
        // stop the timer if we are not active
        mAdvancingTimer->stop();
    }
    emit activeChanged();
}

int DsQmlPlaylistControl::playlistIndex() const {
    return m_playlistIndex;
}

void DsQmlPlaylistControl::setPlaylistIndex(int newPlaylistIndex) {
    if (m_playlistIndex == newPlaylistIndex) return;
    m_playlistIndex = newPlaylistIndex;
    emit playlistIndexChanged();
}

} // namespace dsqt::ui
