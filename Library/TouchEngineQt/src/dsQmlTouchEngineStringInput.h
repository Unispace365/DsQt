#ifndef DSQMLTOUCHENGINESTRINGINPUT_H
#define DSQMLTOUCHENGINESTRINGINPUT_H

#include "dsQmlTouchEngineInputBase.h"
#include <QString>

/**
 * DsQmlTouchEngineStringInput - QML component for setting string values on TouchEngine links
 *
 * Example usage in QML:
 * DsTouchEngineStringInput {
 *     instanceId: myInstance.instanceIdString
 *     linkName: "text"
 *     value: "Hello TouchEngine"
 * }
 */
class DsQmlTouchEngineStringInput : public DsQmlTouchEngineInputBase
{
    Q_OBJECT
    QML_NAMED_ELEMENT(DsTouchEngineStringInput)

    Q_PROPERTY(QString value READ value WRITE setValue NOTIFY valueChanged)

public:
    explicit DsQmlTouchEngineStringInput(QObject *parent = nullptr);
    ~DsQmlTouchEngineStringInput() override;

    QString value() const { return m_value; }
    void setValue(const QString &value);

signals:
    void valueChanged();

protected:
    void applyValue(TEInstance* teInstance) override;

private:
    QString m_value;
};

#endif // DSQMLTOUCHENGINESTRINGINPUT_H
