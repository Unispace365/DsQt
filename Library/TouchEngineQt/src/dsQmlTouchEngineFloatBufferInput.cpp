#include "dsQmlTouchEngineFloatBufferInput.h"
#include <TouchEngine/TouchEngine.h>
#include <QDebug>
#include <QtMath>
#include <QRandomGenerator>

DsQmlTouchEngineFloatBufferInput::DsQmlTouchEngineFloatBufferInput(QObject *parent)
    : DsQmlTouchEngineInputBase(parent)
{
    resizeBufferData();
}

DsQmlTouchEngineFloatBufferInput::~DsQmlTouchEngineFloatBufferInput()
{
}

void DsQmlTouchEngineFloatBufferInput::setChannelCount(int count)
{
    if (count < 1) count = 1;
    
    if (m_channelCount != count) {
        m_channelCount = count;
        resizeBufferData();
        emit channelCountChanged();
        
        if (autoUpdate() && getInstance()) {
            updateValue();
        }
    }
}

void DsQmlTouchEngineFloatBufferInput::setCapacity(int capacity)
{
    if (capacity < 1) capacity = 1;
    
    if (m_capacity != capacity) {
        m_capacity = capacity;
        resizeBufferData();
        emit capacityChanged();
        
        if (autoUpdate() && getInstance()) {
            updateValue();
        }
    }
}

void DsQmlTouchEngineFloatBufferInput::setChannelNames(const QStringList &names)
{
    if (m_channelNames != names) {
        m_channelNames = names;
        emit channelNamesChanged();
        
        if (autoUpdate() && getInstance()) {
            updateValue();
        }
    }
}

void DsQmlTouchEngineFloatBufferInput::setIsTimeDependant(bool timeDependant)
{
    if (m_isTimeDependant != timeDependant) {
        m_isTimeDependant = timeDependant;
        emit isTimeDependantChanged();
        
        if (autoUpdate() && getInstance()) {
            updateValue();
        }
    }
}

void DsQmlTouchEngineFloatBufferInput::setSampleRate(double rate)
{
    if (!qFuzzyCompare(m_sampleRate, rate)) {
        m_sampleRate = rate;
        emit sampleRateChanged();
        
        if (autoUpdate() && getInstance()) {
            updateValue();
        }
    }
}

void DsQmlTouchEngineFloatBufferInput::setChannelData(int channel, const QVariantList &data)
{
    if (channel < 0 || channel >= m_channelCount) {
        qWarning() << "Channel index out of bounds:" << channel;
        return;
    }
    
    resizeBufferData();
    
    int sampleCount = qMin(data.size(), m_capacity);
    m_bufferData[channel].clear();
    
    for (int i = 0; i < sampleCount; ++i) {
        m_bufferData[channel].append(data[i].toFloat());
    }
    
    // Update sample count to maximum across all channels
    m_sampleCount = 0;
    for (const auto &channelData : m_bufferData) {
        m_sampleCount = qMax(m_sampleCount, channelData.size());
    }
    
    emit sampleCountChanged();
    emit dataChanged();
    
    if (autoUpdate() && getInstance()) {
        updateValue();
    }
}

void DsQmlTouchEngineFloatBufferInput::setSample(int channel, int sampleIndex, float value)
{
    if (channel < 0 || channel >= m_channelCount || sampleIndex < 0 || sampleIndex >= m_capacity) {
        qWarning() << "Index out of bounds - channel:" << channel << "sample:" << sampleIndex;
        return;
    }
    
    resizeBufferData();
    
    // Ensure channel has enough samples
    while (m_bufferData[channel].size() <= sampleIndex) {
        m_bufferData[channel].append(0.0f);
    }
    
    m_bufferData[channel][sampleIndex] = value;
    
    // Update sample count
    m_sampleCount = qMax(m_sampleCount, sampleIndex + 1);
    
    emit sampleCountChanged();
    emit dataChanged();
    
    if (autoUpdate() && getInstance()) {
        updateValue();
    }
}

float DsQmlTouchEngineFloatBufferInput::getSample(int channel, int sampleIndex) const
{
    if (channel < 0 || channel >= m_bufferData.size() || 
        sampleIndex < 0 || sampleIndex >= m_bufferData[channel].size()) {
        return 0.0f;
    }
    
    return m_bufferData[channel][sampleIndex];
}

void DsQmlTouchEngineFloatBufferInput::fillAllChannels(float value)
{
    resizeBufferData();
    
    for (int channel = 0; channel < m_channelCount; ++channel) {
        m_bufferData[channel].clear();
        m_bufferData[channel].append(value);
    }
    
    m_sampleCount = 1;
    
    emit sampleCountChanged();
    emit dataChanged();
    
    if (autoUpdate() && getInstance()) {
        updateValue();
    }
}

void DsQmlTouchEngineFloatBufferInput::clear()
{
    for (auto &channelData : m_bufferData) {
        channelData.clear();
    }
    
    m_sampleCount = 0;
    
    emit sampleCountChanged();
    emit dataChanged();
    
    if (autoUpdate() && getInstance()) {
        updateValue();
    }
}

void DsQmlTouchEngineFloatBufferInput::generateSineWaveAll(double frequency, double amplitude, double phase)
{
    resizeBufferData();
    

    
    for (int channel = 0; channel < m_channelCount; ++channel) {
        baseGenerateSineWave_(channel,frequency,amplitude,phase);
    }
    
    m_sampleCount = m_capacity;
    
    emit sampleCountChanged();
    emit dataChanged();
    
    if (autoUpdate() && getInstance()) {
        updateValue();
    }
}

void DsQmlTouchEngineFloatBufferInput::generateSineWave(const QVariant& channel,double frequency, double amplitude, double phase){
    auto chString = channel.toString();
    //do we have a channel named chString?
    int index = m_channelNames.indexOf(chString);
    if(index!=-1){
        generateSineWave_(index,frequency,amplitude,phase);
        return;
    }
    //check if channel is a number;
    bool ok = false;
    int chan = chString.toInt(&ok);
    if(ok){
        generateSineWave_(chan,frequency,amplitude,phase);
        return;
    }
    return;
}

void DsQmlTouchEngineFloatBufferInput::generateSineWave_(int channel,double frequency, double amplitude, double phase)
{
    resizeBufferData();

    baseGenerateSineWave_(channel,frequency,amplitude,phase);
    m_sampleCount = m_capacity;
    emit sampleCountChanged();
    emit dataChanged();
    if (autoUpdate() && getInstance()) {
        updateValue();
    }
}



void DsQmlTouchEngineFloatBufferInput::baseGenerateSineWave_(int channel,double frequency, double amplitude, double phase)
{
    double samplesPerCycle = m_sampleRate / frequency;

    m_bufferData[channel].clear();
    m_bufferData[channel].reserve(m_capacity);

    for (int i = 0; i < m_capacity; ++i) {
        double t = (2.0 * M_PI * i) / samplesPerCycle;
        float value = amplitude * sin(t + phase + (channel * M_PI / m_channelCount));
        m_bufferData[channel].append(value);
    }

}

void DsQmlTouchEngineFloatBufferInput::generateWhiteNoiseAll(double amplitude)
{
    resizeBufferData();
    
    for (int channel = 0; channel < m_channelCount; ++channel) {
        baseGenerateWhiteNoise_(channel,amplitude);
    }
    
    m_sampleCount = m_capacity;
    
    emit sampleCountChanged();
    emit dataChanged();
    
    if (autoUpdate() && getInstance()) {
        updateValue();
    }
}



void DsQmlTouchEngineFloatBufferInput::generateWhiteNoise(const QVariant& channel,double amplitude){
    auto chString = channel.toString();
    //do we have a channel named chString?
    int index = m_channelNames.indexOf(chString);
    if(index!=-1){
        generateWhiteNoise_(index,amplitude);
        return;
    }
    //check if channel is a number;
    bool ok = false;
    int chan = chString.toInt(&ok);
    if(ok){
        generateWhiteNoise_(chan,amplitude);
        return;
    }
    return;
}

void DsQmlTouchEngineFloatBufferInput::generateWhiteNoise_(int channel,double amplitude)
{
    resizeBufferData();

    baseGenerateWhiteNoise_(channel,amplitude);


    m_sampleCount = m_capacity;

    emit sampleCountChanged();
    emit dataChanged();

    if (autoUpdate() && getInstance()) {
        updateValue();
    }
}



void DsQmlTouchEngineFloatBufferInput::baseGenerateWhiteNoise_(int channel,double amplitude)
{


    QRandomGenerator *gen = QRandomGenerator::global();

    if (channel < 0 || channel >= m_channelCount) {
        qWarning() << "Channel index out of bounds:" << channel;
        return;
    }

    m_bufferData[channel].clear();
    m_bufferData[channel].reserve(m_capacity);

    for (int i = 0; i < m_capacity; ++i) {
        float value = amplitude * (2.0f * gen->generateDouble() - 1.0f);
        m_bufferData[channel].append(value);
    }
}

void DsQmlTouchEngineFloatBufferInput::pushChannelData(int channel, const QVariant &data)
{
    resizeBufferData();
    if (channel < 0 || channel >= m_channelCount) {
        qWarning() << "Channel index out of bounds:" << channel;
        return;
    }
    if(data.canConvert<float>()){
        m_bufferData[channel].push_front(data.toFloat());
        m_sampleCount++;
    } else if(data.canConvert<QList<QVariant>>()){
        QList<QVariant> dataList = data.toList();
        for(auto itr = dataList.rbegin();itr!=dataList.rend();++itr){
            m_bufferData[channel].push_front(itr->toFloat());
            m_sampleCount++;
        }
    }

    //clean up extra items beyond capacity and update sample count
    while(m_bufferData[channel].size()>m_capacity){
        m_bufferData[channel].pop_back();
    }

    m_sampleCount = m_bufferData[channel].size();

    emit sampleCountChanged();
    emit dataChanged();
}

void DsQmlTouchEngineFloatBufferInput::applyValue(TEInstance *teInstance)
{
    if (!getInstance() || linkName().isEmpty()) {
        return;
    }

    if(getInstance()->hasInputLink(linkName())==false){
        return;
    }
    
    // Get the TouchEngine instance handle
    //TEInstance* teInstance = getInstance()->;
    if (!teInstance) {
        emit errorOccurred("TouchEngine instance not initialized");
        return;
    }
    
    QByteArray linkNameUtf8 = linkName().toUtf8();
    
    // Try to get existing buffer to reuse (more efficient)
    TouchObject<TEFloatBuffer> buffer;
    TEResult result = TEInstanceLinkGetFloatBufferValue(teInstance,
                                                        linkNameUtf8.constData(),
                                                        TELinkValueCurrent,
                                                        buffer.take());
    
    if (result == TEResultSuccess && buffer) {
        // Check if we can reuse the existing buffer
        if (TEFloatBufferGetCapacity(buffer) < m_capacity ||
            TEFloatBufferGetChannelCount(buffer) != m_channelCount) {
            // Need a new buffer with different dimensions
            TouchObject<TEFloatBuffer> newBuffer;
            newBuffer.take(TEFloatBufferCreateCopy(buffer));
            buffer = newBuffer;
        }
    }
    
    if (!buffer) {
        // Create new buffer
        // Convert channel names to C strings if provided
        std::vector<const char*> channelNamePtrs;
        std::vector<QByteArray> channelNameBytes;
        
        if (!m_channelNames.isEmpty()) {
            for (const QString &name : m_channelNames) {
                channelNameBytes.push_back(name.toUtf8());
                channelNamePtrs.push_back(channelNameBytes.back().constData());
            }
        }
        
        // Create buffer with appropriate time base (-1 for non-time-dependent)
        int64_t timeValue = m_isTimeDependant ? 0 : -1;
        
        buffer.take(TEFloatBufferCreate(timeValue,
                                        m_channelCount,
                                        m_capacity,
                                        channelNamePtrs.empty() ? nullptr : channelNamePtrs.data()));
    }
    
    if (!buffer) {
        emit errorOccurred("Failed to create float buffer");
        return;
    }
    
    // Prepare data pointers for each channel
    std::vector<const float*> channelPointers;
    
    for (int channel = 0; channel < m_channelCount; ++channel) {
        if (channel < m_bufferData.size() && !m_bufferData[channel].isEmpty()) {
            channelPointers.push_back(m_bufferData[channel].constData());
        } else {
            // Provide null pointer for empty channels
            channelPointers.push_back(nullptr);
        }
    }
    
    // Set the values in the buffer
    if (!channelPointers.empty()) {
        TEFloatBufferSetValues(buffer, channelPointers.data(), m_sampleCount);
    }
    
    // Set the buffer value on the link
    result = TEInstanceLinkSetFloatBufferValue(teInstance,
                                               linkNameUtf8.constData(),
                                               buffer);

    if (result != TEResultSuccess) {
        QString errorMsg = QString("Failed to set float buffer value on link '%1': %2")
                          .arg(linkName())
                               .arg(std::string(TEResultGetDescription(result)));
        qWarning() << errorMsg;
        emit errorOccurred(errorMsg);
    }
}

void DsQmlTouchEngineFloatBufferInput::resizeBufferData()
{
    m_bufferData.resize(m_channelCount);
    
    for (int channel = 0; channel < m_channelCount; ++channel) {
        if (m_bufferData[channel].size() > m_capacity) {
            m_bufferData[channel].resize(m_capacity);
        }
    }
    
    // Update sample count
    m_sampleCount = 0;
    for (const auto &channelData : m_bufferData) {
        m_sampleCount = qMax(m_sampleCount, channelData.size());
    }
}
