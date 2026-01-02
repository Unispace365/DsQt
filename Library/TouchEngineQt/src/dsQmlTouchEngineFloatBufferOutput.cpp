#include "dsQmlTouchEngineFloatBufferOutput.h"
#include "dsQmlTouchEngineInstance.h"
#include <TouchEngine/TouchEngine.h>
#include <TouchEngine/TouchObject.h>
#include <QDebug>

DsQmlTouchEngineFloatBufferOutput::DsQmlTouchEngineFloatBufferOutput(QObject *parent)
    : DsQmlTouchEngineOutputBase(parent)
{
}

DsQmlTouchEngineFloatBufferOutput::~DsQmlTouchEngineFloatBufferOutput()
{
}

float DsQmlTouchEngineFloatBufferOutput::getSample(int channel, int sampleIndex) const
{
    if (channel < 0 || channel >= m_bufferData.size() ||
        sampleIndex < 0 || sampleIndex >= m_bufferData[channel].size()) {
        return 0.0f;
    }

    return m_bufferData[channel][sampleIndex];
}

QVariantList DsQmlTouchEngineFloatBufferOutput::getChannelData(int channel) const
{
    QVariantList result;

    if (channel < 0 || channel >= m_bufferData.size()) {
        return result;
    }

    for (float value : m_bufferData[channel]) {
        result.append(value);
    }

    return result;
}

float DsQmlTouchEngineFloatBufferOutput::getSampleByName(const QString &channelName, int sampleIndex) const
{
    int index = m_channelNames.indexOf(channelName);
    if (index != -1) {
        return getSample(index, sampleIndex);
    }
    return 0;
}

QVariantList DsQmlTouchEngineFloatBufferOutput::getChannelDataByName(const QString& channelName) const
{
    int index = m_channelNames.indexOf(channelName);
    if (index != -1) {
        return getChannelData(index);
    }
    return QVariantList();
}

QVariantList DsQmlTouchEngineFloatBufferOutput::getFirstSamples() const
{
    QVariantList result;

    for (const auto& channelData : m_bufferData) {
        if (!channelData.isEmpty()) {
            result.append(channelData[0]);
        } else {
            result.append(0.0f);
        }
    }

    return result;
}

void DsQmlTouchEngineFloatBufferOutput::readValue(TEInstance* teInstance)
{
    if (!teInstance || linkName().isEmpty()) {
        return;
    }

    QByteArray linkNameUtf8 = linkName().toUtf8();

    // Get the float buffer value from the output link
    TouchObject<TEFloatBuffer> buffer;
    TEResult result = TEInstanceLinkGetFloatBufferValue(teInstance,
                                                         linkNameUtf8.constData(),
                                                         TELinkValueCurrent,
                                                         buffer.take());

    if (result != TEResultSuccess) {
        QString errorMsg = QString("Failed to get float buffer value from link '%1': %2")
                          .arg(linkName())
                          .arg(TEResultGetDescription(result));
        qWarning() << errorMsg;
        emit errorOccurred(errorMsg);
        return;
    }

    if (!buffer) {
        return;
    }

    // Get buffer properties
    int32_t newChannelCount = TEFloatBufferGetChannelCount(buffer);
    uint32_t newValueCount = TEFloatBufferGetValueCount(buffer);
    int32_t newCapacity = TEFloatBufferGetCapacity(buffer);

    // Get channel names
    QStringList newChannelNames;
    const char* const* channelNames = TEFloatBufferGetChannelNames(buffer);
    for (int32_t i = 0; i < newChannelCount; ++i) {
        if (channelNames && channelNames[i]) {
            newChannelNames.append(QString::fromUtf8(channelNames[i]));
        } else {
            newChannelNames.append(QString());
        }
    }

    // Check if time-dependent
    bool newIsTimeDependant = TEFloatBufferIsTimeDependent(buffer);

    // Get the data
    const float* const* data = TEFloatBufferGetValues(buffer);

    // Copy data to our internal buffer
    QVector<QVector<float>> newBufferData;
    newBufferData.resize(newChannelCount);

    if (data) {
        for (int32_t channel = 0; channel < newChannelCount; ++channel) {
            if (data[channel]) {
                newBufferData[channel].resize(newValueCount);
                for (uint32_t sample = 0; sample < newValueCount; ++sample) {
                    newBufferData[channel][sample] = data[channel][sample];
                }
            }
        }
    }

    // Update values and emit signals
    bool channelCountChanged = (newChannelCount != m_channelCount);
    bool sampleCountChanged = (static_cast<int>(newValueCount) != m_sampleCount);
    bool capacityChanged = (newCapacity != m_capacity);
    bool channelNamesChanged = (newChannelNames != m_channelNames);
    bool isTimeDependantChanged = (newIsTimeDependant != m_isTimeDependant);
    bool dataChanged = (newBufferData != m_bufferData);

    if (channelCountChanged) {
        m_channelCount = newChannelCount;
        emit this->channelCountChanged();
    }

    if (sampleCountChanged) {
        m_sampleCount = newValueCount;
        emit this->sampleCountChanged();
    }

    if (capacityChanged) {
        m_capacity = newCapacity;
        emit this->capacityChanged();
    }

    if (channelNamesChanged) {
        m_channelNames = newChannelNames;
        emit this->channelNamesChanged();
    }

    if (isTimeDependantChanged) {
        m_isTimeDependant = newIsTimeDependant;
        emit this->isTimeDependantChanged();
    }

    if (dataChanged) {
        m_bufferData = newBufferData;
        emit this->dataChanged();
        emit valueUpdated();
    }
}
