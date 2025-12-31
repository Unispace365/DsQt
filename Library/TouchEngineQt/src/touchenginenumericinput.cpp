#include "touchenginenumericinput.h"
#include "touchengineinstance.h"
#include <TouchEngine/TouchEngine.h>
#include <QDebug>
#include <limits>

TouchEngineNumericInput::TouchEngineNumericInput(QObject *parent)
    : TouchEngineInputBase(parent)
{
    m_value = 0.0;
}

TouchEngineNumericInput::~TouchEngineNumericInput()
{
}

void TouchEngineNumericInput::setValue(const QVariant &value)
{
    QVariant newValue = value;
    
    // Convert to appropriate type based on inputType
    switch (m_inputType) {
        case Integer:
            newValue = value.toInt();
            break;
        case Float:
            newValue = value.toFloat();
            break;
        case Double:
            newValue = value.toDouble();
            break;
        case Boolean:
            newValue = value.toBool();
            break;
    }
    
    if (m_clampValue && m_inputType != Boolean) {
        newValue = clampedValue();
    }
    
    if (m_value != newValue) {
        m_value = newValue;
        emit valueChanged();
        
        if (autoUpdate() && getInstance()) {
            updateValue();
        }
    }
}

void TouchEngineNumericInput::setInputType(InputType type)
{
    if (m_inputType != type) {
        m_inputType = type;
        
        // Convert existing value to new type
        switch (type) {
            case Integer:
                m_value = m_value.toInt();
                break;
            case Float:
                m_value = m_value.toFloat();
                break;
            case Double:
                m_value = m_value.toDouble();
                break;
            case Boolean:
                m_value = m_value.toBool();
                break;
        }
        
        emit inputTypeChanged();
        emit valueChanged();
        
        if (autoUpdate() && getInstance()) {
            updateValue();
        }
    }
}

void TouchEngineNumericInput::setMinValue(double min)
{
    if (!qFuzzyCompare(m_minValue, min)) {
        m_minValue = min;
        emit minValueChanged();
        
        if (m_clampValue) {
            setValue(clampedValue());
        }
    }
}

void TouchEngineNumericInput::setMaxValue(double max)
{
    if (!qFuzzyCompare(m_maxValue, max)) {
        m_maxValue = max;
        emit maxValueChanged();
        
        if (m_clampValue) {
            setValue(clampedValue());
        }
    }
}

void TouchEngineNumericInput::setClampValue(bool clamp)
{
    if (m_clampValue != clamp) {
        m_clampValue = clamp;
        emit clampValueChanged();
        
        if (clamp) {
            setValue(clampedValue());
        }
    }
}

void TouchEngineNumericInput::setIntValue(int value)
{
    setInputType(Integer);
    setValue(value);
}

void TouchEngineNumericInput::setFloatValue(float value)
{
    setInputType(Float);
    setValue(value);
}

void TouchEngineNumericInput::setDoubleValue(double value)
{
    setInputType(Double);
    setValue(value);
}

void TouchEngineNumericInput::setBoolValue(bool value)
{
    setInputType(Boolean);
    setValue(value);
}



void TouchEngineNumericInput::applyValue(TEInstance* teInstance)
{
    if (!getInstance() || linkName().isEmpty()) {
        return;
    }
    
    // Get the TouchEngine instance handle
    if (!teInstance) {
        emit errorOccurred("TouchEngine instance not initialized");
        return;
    }
    
    QByteArray linkNameUtf8 = linkName().toUtf8();
    TEResult result = TEResultSuccess;
    
    switch (m_inputType) {
        case Integer:
            {
                int32_t intValue = m_value.toInt();
                result = TEInstanceLinkSetIntValue(teInstance, 
                                                   linkNameUtf8.constData(), 
                                                   &intValue, 1);
            }
            break;
            
        case Float:
            {
                float floatValue = m_value.toFloat();
                double doubleValue = static_cast<double>(floatValue);
                result = TEInstanceLinkSetDoubleValue(teInstance, 
                                                      linkNameUtf8.constData(), 
                                                      &doubleValue, 1);
            }
            break;
            
        case Double:
            {
                double doubleValue = m_value.toDouble();
                result = TEInstanceLinkSetDoubleValue(teInstance, 
                                                      linkNameUtf8.constData(), 
                                                      &doubleValue, 1);
            }
            break;
            
        case Boolean:
            {
                bool boolValue = m_value.toBool();
                result = TEInstanceLinkSetBooleanValue(teInstance, 
                                                       linkNameUtf8.constData(), 
                                                       boolValue);
            }
            break;
    }
    
    if (result != TEResultSuccess) {
        QString errorMsg = QString("Failed to set numeric value on link '%1': %2")
                          .arg(linkName())
                          .arg(result);
        qWarning() << errorMsg;
        emit errorOccurred(errorMsg);
    }
}

QVariant TouchEngineNumericInput::clampedValue() const
{
    if (m_inputType == Boolean) {
        return m_value;
    }
    
    double val = m_value.toDouble();
    val = qBound(m_minValue, val, m_maxValue);
    
    switch (m_inputType) {
        case Integer:
            return static_cast<int>(val);
        case Float:
            return static_cast<float>(val);
        case Double:
            return val;
        default:
            return val;
    }
}
