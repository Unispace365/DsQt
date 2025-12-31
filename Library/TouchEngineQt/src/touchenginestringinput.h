#ifndef TOUCHENGINESTRINGINPUT_H
#define TOUCHENGINESTRINGINPUT_H

#include "touchengineinputbase.h"
#include <QString>

/**
 * TouchEngineStringInput - QML component for setting string values on TouchEngine links
 * 
 * Example usage in QML:
 * TouchEngineStringInput {
 *     instanceId: myInstance.instanceIdString
 *     linkName: "text"
 *     value: "Hello TouchEngine"
 * }
 */
class TouchEngineStringInput : public TouchEngineInputBase
{
    Q_OBJECT
    QML_NAMED_ELEMENT(TouchEngineStringInput)
    
    Q_PROPERTY(QString value READ value WRITE setValue NOTIFY valueChanged)
    
public:
    explicit TouchEngineStringInput(QObject *parent = nullptr);
    ~TouchEngineStringInput() override;
    
    QString value() const { return m_value; }
    void setValue(const QString &value);
    
signals:
    void valueChanged();
    
protected:
    void applyValue(TEInstance* teInstance) override;
    
private:
    QString m_value;
};

#endif // TOUCHENGINESTRINGINPUT_H
