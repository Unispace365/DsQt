#include "dsQmlTouchEngineInputBase.h"
#include "dsQmlTouchEngineInstance.h"
#include "dsQmlTouchEngineManager.h"
#include <QDebug>

DsQmlTouchEngineInputBase::DsQmlTouchEngineInputBase(QObject *parent)
    : QObject(parent)
{
}

DsQmlTouchEngineInputBase::~DsQmlTouchEngineInputBase()
{
    disconnectFromInstance();
}

void DsQmlTouchEngineInputBase::setInstanceId(const QString &id)
{
    if (m_instanceId != id) {
        disconnectFromInstance();
        m_instanceId = id;
        connectToInstance();
        emit instanceIdChanged();
    }
}

void DsQmlTouchEngineInputBase::setLinkName(const QString &name)
{
    if (m_linkName != name) {
        m_linkName = name;
        emit linkNameChanged();

        if (m_autoUpdate && m_instance) {
            updateValue();
        }
    }
}

void DsQmlTouchEngineInputBase::setAutoUpdate(bool auto_update)
{
    if (m_autoUpdate != auto_update) {
        m_autoUpdate = auto_update;
        updateValue();
        emit autoUpdateChanged();
    }
}

void DsQmlTouchEngineInputBase::updateValue()
{
    if (!getInstance() || linkName().isEmpty()) {
        return;
    }

    m_isDirty = true;
}

void DsQmlTouchEngineInputBase::handleCanUpdateLinks(TEInstance *teInstance)
{
    if(m_isDirty) {
        applyValue(teInstance);
        m_isDirty = false;
    }
}

void DsQmlTouchEngineInputBase::connectToInstance()
{
    if (!m_instanceId.isEmpty()) {
        m_instance = DsQmlTouchEngineManager::inst()->getInstance(m_instanceId);

        if (m_instance) {
            // Connect to instance signals if needed
            connect(m_instance, &DsQmlTouchEngineInstance::destroyed,
                    this, &DsQmlTouchEngineInputBase::disconnectFromInstance);

            connect(m_instance, &DsQmlTouchEngineInstance::canUpdateLinks,
                    this, [this](TEInstance *teInstance) {
                        handleCanUpdateLinks(teInstance);
                    },Qt::QueuedConnection);
            connect(m_instance, &DsQmlTouchEngineInstance::linksChanged,this, [this]() {
                        m_isDirty = true;
                    });
            emit isConnectedChanged();

            // Apply initial value if auto-update is enabled
            if (m_autoUpdate) {
                updateValue();
            }
        } else {
            qWarning() << "Failed to connect to TouchEngine instance:" << m_instanceId;
            emit errorOccurred("Failed to connect to instance");
        }
    }
}

void DsQmlTouchEngineInputBase::disconnectFromInstance()
{
    if (m_instance) {
        disconnect(m_instance, nullptr, this, nullptr);
        m_instance = nullptr;
        emit isConnectedChanged();
    }
}
