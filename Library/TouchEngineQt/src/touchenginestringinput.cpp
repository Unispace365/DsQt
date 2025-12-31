#include "touchenginestringinput.h"
#include <TouchEngine/TouchEngine.h>
#include <QDebug>

TouchEngineStringInput::TouchEngineStringInput(QObject *parent)
    : TouchEngineInputBase(parent)
{
}

TouchEngineStringInput::~TouchEngineStringInput()
{
}

void TouchEngineStringInput::setValue(const QString &value)
{
    if (m_value != value) {
        m_value = value;
        emit valueChanged();

        if (autoUpdate() && getInstance()) {
            updateValue();
        }
    }
}



void TouchEngineStringInput::applyValue(TEInstance* teInstance)
{
    if (!getInstance() || linkName().isEmpty()) {
        return;
    }
    
    // Get the TouchEngine instance handle
    if (!teInstance) {
        emit errorOccurred("TouchEngine instance not initialized");
        return;
    }
    
    // Convert QString to UTF-8 for TouchEngine
    QByteArray linkNameUtf8 = linkName().toUtf8();
    QByteArray valueUtf8 = m_value.toUtf8();
    
    // Set the string value
    TEResult result = TEInstanceLinkSetStringValue(teInstance, 
                                                   linkNameUtf8.constData(), 
                                                   valueUtf8.constData());
    
    if (result != TEResultSuccess) {
        QString errorMsg = QString("Failed to set string value on link '%1': %2")
                          .arg(linkName())
                          .arg(result);
        qWarning() << errorMsg;
        emit errorOccurred(errorMsg);
    }
}
