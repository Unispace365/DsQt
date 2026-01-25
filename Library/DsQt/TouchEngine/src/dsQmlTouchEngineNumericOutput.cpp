#include "dsQmlTouchEngineNumericOutput.h"
#include "dsQmlTouchEngineInstance.h"
#include <TouchEngine/TouchEngine.h>
#include <TouchEngine/TouchObject.h>
#include <QDebug>

DsQmlTouchEngineNumericOutput::DsQmlTouchEngineNumericOutput(QObject *parent)
    : DsQmlTouchEngineOutputBase(parent)
{
}

DsQmlTouchEngineNumericOutput::~DsQmlTouchEngineNumericOutput()
{
}

void DsQmlTouchEngineNumericOutput::readValue(TEInstance* teInstance)
{
    if (!teInstance || linkName().isEmpty()) {
        return;
    }

    QByteArray linkNameUtf8 = linkName().toUtf8();

    // First, get link info to determine the type
    TouchObject<TELinkInfo> linkInfo;
    TEResult result = TEInstanceLinkGetInfo(teInstance, linkNameUtf8.constData(), linkInfo.take());

    if (result != TEResultSuccess) {
        QString errorMsg = QString("Failed to get link info for '%1': %2")
                          .arg(linkName())
                          .arg(TEResultGetDescription(result));
        qWarning() << errorMsg;
        emit errorOccurred(errorMsg);
        return;
    }

    QVariantList newValues;
    QVariant newValue;
    OutputType newType = m_outputType;

    switch (linkInfo->type) {
    case TELinkTypeDouble:
    {
        newType = Double;
        int32_t count = linkInfo->count;
        std::vector<double> values(count);
        result = TEInstanceLinkGetDoubleValue(teInstance,
                                               linkNameUtf8.constData(),
                                               TELinkValueCurrent,
                                               values.data(),
                                               count);
        if (result == TEResultSuccess) {
            for (int i = 0; i < count; ++i) {
                newValues.append(values[i]);
            }
            if (count > 0) {
                newValue = values[0];
            }
        }
        break;
    }

    case TELinkTypeInt:
    {
        newType = Integer;
        int32_t count = linkInfo->count;
        std::vector<int32_t> values(count);
        result = TEInstanceLinkGetIntValue(teInstance,
                                            linkNameUtf8.constData(),
                                            TELinkValueCurrent,
                                            values.data(),
                                            count);
        if (result == TEResultSuccess) {
            for (int i = 0; i < count; ++i) {
                newValues.append(values[i]);
            }
            if (count > 0) {
                newValue = values[0];
            }
        }
        break;
    }

    case TELinkTypeBoolean:
    {
        newType = Boolean;
        bool value = false;
        result = TEInstanceLinkGetBooleanValue(teInstance,
                                                linkNameUtf8.constData(),
                                                TELinkValueCurrent,
                                                &value);
        if (result == TEResultSuccess) {
            newValues.append(value);
            newValue = value;
        }
        break;
    }

    default:
        qWarning() << "Unsupported link type for numeric output:" << linkInfo->type;
        return;
    }

    if (result != TEResultSuccess) {
        QString errorMsg = QString("Failed to get numeric value from link '%1': %2")
                          .arg(linkName())
                          .arg(TEResultGetDescription(result));
        qWarning() << errorMsg;
        emit errorOccurred(errorMsg);
        return;
    }

    // Update values and emit signals
    bool typeChanged = (newType != m_outputType);
    bool valueChanged = (newValue != m_value);
    bool valuesChanged = (newValues != m_values);

    if (typeChanged) {
        m_outputType = newType;
        emit outputTypeChanged();
    }

    if (valuesChanged) {
        int oldCount = m_values.size();
        m_values = newValues;
        emit this->valuesChanged();
        if (newValues.size() != oldCount) {
            emit valueCountChanged();
        }
    }

    if (valueChanged) {
        m_value = newValue;
        emit this->valueChanged();
        emit valueUpdated();
    }
}
