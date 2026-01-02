#include "dsQmlTouchEngineOutputBase.h"
#include "dsQmlTouchEngineInstance.h"
#include "dsQmlTouchEngineManager.h"
#include <QDebug>

DsQmlTouchEngineOutputBase::DsQmlTouchEngineOutputBase(QObject *parent)
    : QObject(parent)
{
}

DsQmlTouchEngineOutputBase::~DsQmlTouchEngineOutputBase()
{
    disconnectFromInstance();
}

void DsQmlTouchEngineOutputBase::setInstanceId(const QString &id)
{
    if (m_instanceId != id) {
        disconnectFromInstance();
        m_instanceId = id;
        connectToInstance();
        emit instanceIdChanged();
    }
}

void DsQmlTouchEngineOutputBase::setLinkName(const QString &name)
{
    if (m_linkName != name) {
        m_linkName = name;
        emit linkNameChanged();

        if (m_autoUpdate && m_instance) {
            fetchValue();
        }
    }
}

void DsQmlTouchEngineOutputBase::setAutoUpdate(bool auto_update)
{
    if (m_autoUpdate != auto_update) {
        m_autoUpdate = auto_update;
        emit autoUpdateChanged();

        if (m_autoUpdate && m_instance) {
            fetchValue();
        }
    }
}

void DsQmlTouchEngineOutputBase::fetchValue()
{
    if (!getInstance() || linkName().isEmpty()) {
        return;
    }

    DsQmlTouchEngineInstance* inst = getInstance();
    if (inst && inst->teInstance()) {
        readValue(inst->teInstance());
    }
}

void DsQmlTouchEngineOutputBase::handleFrameFinished()
{
    if (m_pendingRead || m_autoUpdate) {
        fetchValue();
        m_pendingRead = false;
    }
}

void DsQmlTouchEngineOutputBase::handleOutputLinkValueChanged(const QString& linkName)
{
    // Only read if this is our link
    if (linkName == m_linkName && m_autoUpdate) {
        fetchValue();
    }
}

void DsQmlTouchEngineOutputBase::connectToInstance()
{
    if (!m_instanceId.isEmpty()) {
        m_instance = DsQmlTouchEngineManager::inst()->getInstance(m_instanceId);

        if (m_instance) {
            // Connect to instance signals for output value changes
            connect(m_instance, &DsQmlTouchEngineInstance::destroyed,
                    this, &DsQmlTouchEngineOutputBase::disconnectFromInstance);

            // Listen for specific output link value changes
            connect(m_instance, &DsQmlTouchEngineInstance::outputLinkValueChanged,
                    this, &DsQmlTouchEngineOutputBase::handleOutputLinkValueChanged,
                    Qt::QueuedConnection);

            // Listen for frame completion to read output values
            connect(m_instance, &DsQmlTouchEngineInstance::frameFinished,
                    this, &DsQmlTouchEngineOutputBase::handleFrameFinished,
                    Qt::QueuedConnection);

            // Also listen for links changed to know when outputs might be available
            connect(m_instance, &DsQmlTouchEngineInstance::linksChanged,
                    this, [this]() {
                        if (m_autoUpdate) {
                            m_pendingRead = true;
                        }
                    });

            emit isConnectedChanged();

            // Fetch initial value if auto-update is enabled
            if (m_autoUpdate) {
                fetchValue();
            }
        } else {
            qWarning() << "Failed to connect to TouchEngine instance:" << m_instanceId;
            emit errorOccurred("Failed to connect to instance");
        }
    }
}

void DsQmlTouchEngineOutputBase::disconnectFromInstance()
{
    if (m_instance) {
        disconnect(m_instance, nullptr, this, nullptr);
        m_instance = nullptr;
        emit isConnectedChanged();
    }
}
