#ifndef DSQMLTOUCHENGINENUMERICOUTPUT_H
#define DSQMLTOUCHENGINENUMERICOUTPUT_H

#include "dsQmlTouchEngineOutputBase.h"
#include <QVariant>
#include <QVariantList>

/**
 * DsQmlTouchEngineNumericOutput - QML component for reading numeric values from TouchEngine output links
 * Supports int, float/double, and boolean values, as well as arrays of values
 *
 * Example usage in QML:
 * DsTouchEngineNumericOutput {
 *     instanceId: myInstance.instanceIdString
 *     linkName: "op/speed_out"
 *     onValueChanged: console.log("Speed:", value)
 * }
 *
 * // For array values (e.g., xyz coordinates):
 * DsTouchEngineNumericOutput {
 *     instanceId: myInstance.instanceIdString
 *     linkName: "op/position_out"
 *     onValuesChanged: console.log("Position:", values[0], values[1], values[2])
 * }
 */
class DsQmlTouchEngineNumericOutput : public DsQmlTouchEngineOutputBase
{
    Q_OBJECT
    QML_NAMED_ELEMENT(DsTouchEngineNumericOutput)

public:
    enum OutputType {
        Integer,
        Float,
        Double,
        Boolean
    };
    Q_ENUM(OutputType)

    Q_PROPERTY(QVariant value READ value NOTIFY valueChanged)
    Q_PROPERTY(QVariantList values READ values NOTIFY valuesChanged)
    Q_PROPERTY(OutputType outputType READ outputType NOTIFY outputTypeChanged)
    Q_PROPERTY(int valueCount READ valueCount NOTIFY valueCountChanged)

public:
    explicit DsQmlTouchEngineNumericOutput(QObject *parent = nullptr);
    ~DsQmlTouchEngineNumericOutput() override;

    QVariant value() const { return m_value; }
    QVariantList values() const { return m_values; }
    OutputType outputType() const { return m_outputType; }
    int valueCount() const { return m_values.size(); }

    // Convenience getters for specific types
    Q_INVOKABLE int intValue() const { return m_value.toInt(); }
    Q_INVOKABLE float floatValue() const { return m_value.toFloat(); }
    Q_INVOKABLE double doubleValue() const { return m_value.toDouble(); }
    Q_INVOKABLE bool boolValue() const { return m_value.toBool(); }

signals:
    void valueChanged();
    void valuesChanged();
    void outputTypeChanged();
    void valueCountChanged();

protected:
    void readValue(TEInstance* teInstance) override;

private:
    QVariant m_value;
    QVariantList m_values;
    OutputType m_outputType = Double;
};

#endif // DSQMLTOUCHENGINENUMERICOUTPUT_H
