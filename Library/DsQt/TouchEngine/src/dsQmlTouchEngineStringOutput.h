#ifndef DSQMLTOUCHENGINESTRINGOUTPUT_H
#define DSQMLTOUCHENGINESTRINGOUTPUT_H

#include "dsQmlTouchEngineOutputBase.h"
#include <QString>

/**
 * DsQmlTouchEngineStringOutput - QML component for reading string values from TouchEngine output links
 *
 * Example usage in QML:
 * DsTouchEngineStringOutput {
 *     instanceId: myInstance.instanceIdString
 *     linkName: "op/text_out"
 *     onValueChanged: console.log("Output text:", value)
 * }
 */
class DsQmlTouchEngineStringOutput : public DsQmlTouchEngineOutputBase
{
    Q_OBJECT
    QML_NAMED_ELEMENT(DsTouchEngineStringOutput)

    Q_PROPERTY(QString value READ value NOTIFY valueChanged)

public:
    explicit DsQmlTouchEngineStringOutput(QObject *parent = nullptr);
    ~DsQmlTouchEngineStringOutput() override;

    QString value() const { return m_value; }

signals:
    void valueChanged();

protected:
    void readValue(TEInstance* teInstance) override;

private:
    QString m_value;
};

#endif // DSQMLTOUCHENGINESTRINGOUTPUT_H
