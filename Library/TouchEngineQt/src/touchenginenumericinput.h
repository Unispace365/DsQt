#ifndef TOUCHENGINENUMERICINPUT_H
#define TOUCHENGINENUMERICINPUT_H

#include "touchengineinputbase.h"
#include <QVariant>
#include <limits>

/**
 * TouchEngineNumericInput - QML component for setting numeric values on TouchEngine links
 * Supports int, float/double, and boolean values
 * 
 * Example usage in QML:
 * TouchEngineNumericInput {
 *     instanceId: myInstance.instanceIdString
 *     linkName: "speed"
 *     value: 0.5
 *     inputType: TouchEngineNumericInput.Float
 * }
 */
class TouchEngineNumericInput : public TouchEngineInputBase
{
    Q_OBJECT
    QML_NAMED_ELEMENT(TouchEngineNumericInput)
    
public:
    enum InputType {
        Integer,
        Float,
        Double,
        Boolean
    };
    Q_ENUM(InputType)
    
    Q_PROPERTY(QVariant value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(InputType inputType READ inputType WRITE setInputType NOTIFY inputTypeChanged)
    Q_PROPERTY(double minValue READ minValue WRITE setMinValue NOTIFY minValueChanged)
    Q_PROPERTY(double maxValue READ maxValue WRITE setMaxValue NOTIFY maxValueChanged)
    Q_PROPERTY(bool clampValue READ clampValue WRITE setClampValue NOTIFY clampValueChanged)
    
public:
    explicit TouchEngineNumericInput(QObject *parent = nullptr);
    ~TouchEngineNumericInput() override;
    
    QVariant value() const { return m_value; }
    void setValue(const QVariant &value);
    
    InputType inputType() const { return m_inputType; }
    void setInputType(InputType type);
    
    double minValue() const { return m_minValue; }
    void setMinValue(double min);
    
    double maxValue() const { return m_maxValue; }
    void setMaxValue(double max);
    
    bool clampValue() const { return m_clampValue; }
    void setClampValue(bool clamp);
    

    
    // Convenience setters for specific types
    Q_INVOKABLE void setIntValue(int value);
    Q_INVOKABLE void setFloatValue(float value);
    Q_INVOKABLE void setDoubleValue(double value);
    Q_INVOKABLE void setBoolValue(bool value);
    
signals:
    void valueChanged();
    void inputTypeChanged();
    void minValueChanged();
    void maxValueChanged();
    void clampValueChanged();
    
protected:
    void applyValue(TEInstance* teInstance) override;
    
private:
    QVariant m_value;
    InputType m_inputType = Float;
    double m_minValue = -std::numeric_limits<double>::max();
    double m_maxValue = std::numeric_limits<double>::max();
    bool m_clampValue = false;
    
    QVariant clampedValue() const;
};

#endif // TOUCHENGINENUMERICINPUT_H
