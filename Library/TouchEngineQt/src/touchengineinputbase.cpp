#include "touchengineinputbase.h"
#include "touchengineinstance.h"
#include "touchenginemanager.h"
#include <QDebug>

TouchEngineInputBase::TouchEngineInputBase(QObject *parent)
    : QObject(parent)
{
}

TouchEngineInputBase::~TouchEngineInputBase()
{
    disconnectFromInstance();
}

void TouchEngineInputBase::setInstanceId(const QString &id)
{
    if (m_instanceId != id) {
        disconnectFromInstance();
        m_instanceId = id;
        connectToInstance();
        emit instanceIdChanged();
    }
}

void TouchEngineInputBase::setLinkName(const QString &name)
{
    if (m_linkName != name) {
        m_linkName = name;
        emit linkNameChanged();
        
        if (m_autoUpdate && m_instance) {
            updateValue();
        }
    }
}

void TouchEngineInputBase::setAutoUpdate(bool auto_update)
{
    if (m_autoUpdate != auto_update) {
        m_autoUpdate = auto_update;
        updateValue();
        emit autoUpdateChanged();
    }
}

void TouchEngineInputBase::updateValue()
{
    if (!getInstance() || linkName().isEmpty()) {
        return;
    }

    m_isDirty = true;
}

void TouchEngineInputBase::handleCanUpdateLinks(TEInstance *teInstance)
{
    if(m_isDirty) {
        applyValue(teInstance);
        m_isDirty = false;
    }
}

void TouchEngineInputBase::connectToInstance()
{
    if (!m_instanceId.isEmpty()) {
        m_instance = TouchEngineManager::inst()->getInstance(m_instanceId);
        
        if (m_instance) {
            // Connect to instance signals if needed
            connect(m_instance, &TouchEngineInstance::destroyed,
                    this, &TouchEngineInputBase::disconnectFromInstance);

            connect(m_instance, &TouchEngineInstance::canUpdateLinks,
                    this, [this](TEInstance *teInstance) {
                        handleCanUpdateLinks(teInstance);
                    },Qt::QueuedConnection);
            connect(m_instance, &TouchEngineInstance::linksChanged,this, [this]() {
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

void TouchEngineInputBase::disconnectFromInstance()
{
    if (m_instance) {
        disconnect(m_instance, nullptr, this, nullptr);
        m_instance = nullptr;
        emit isConnectedChanged();
    }
}
