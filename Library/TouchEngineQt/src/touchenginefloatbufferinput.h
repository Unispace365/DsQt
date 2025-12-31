#ifndef TOUCHENGINEFLOATBUFFERINPUT_H
#define TOUCHENGINEFLOATBUFFERINPUT_H

#include "touchengineinputbase.h"
#include <QVariantList>
#include <QVector>

/**
 * TouchEngineFloatBufferInput - QML component for setting float buffer values on TouchEngine links
 * Used for audio data, multi-channel numeric data, or time-series data
 * 
 * Example usage in QML:
 * TouchEngineFloatBufferInput {
 *     instanceId: myInstance.instanceIdString
 *     linkName: "audio_input"
 *     channelCount: 2
 *     capacity: 512
 *     
 *     // Set data for all channels at once
 *     setChannelData(0, audioLeftChannel)
 *     setChannelData(1, audioRightChannel)
 * }
 */
class TouchEngineFloatBufferInput : public TouchEngineInputBase
{
    Q_OBJECT
    QML_NAMED_ELEMENT(TouchEngineFloatBufferInput)
    
    Q_PROPERTY(int channelCount READ channelCount WRITE setChannelCount NOTIFY channelCountChanged)
    Q_PROPERTY(int capacity READ capacity WRITE setCapacity NOTIFY capacityChanged)
    Q_PROPERTY(int sampleCount READ sampleCount NOTIFY sampleCountChanged)
    Q_PROPERTY(QStringList channelNames READ channelNames WRITE setChannelNames NOTIFY channelNamesChanged)
    Q_PROPERTY(bool isTimeDependant READ isTimeDependant WRITE setIsTimeDependant NOTIFY isTimeDependantChanged)
    Q_PROPERTY(double sampleRate READ sampleRate WRITE setSampleRate NOTIFY sampleRateChanged)
    
public:
    explicit TouchEngineFloatBufferInput(QObject *parent = nullptr);
    ~TouchEngineFloatBufferInput() override;
    
    int channelCount() const { return m_channelCount; }
    void setChannelCount(int count);
    
    int capacity() const { return m_capacity; }
    void setCapacity(int capacity);
    
    int sampleCount() const { return m_sampleCount; }
    
    QStringList channelNames() const { return m_channelNames; }
    void setChannelNames(const QStringList &names);
    
    bool isTimeDependant() const { return m_isTimeDependant; }
    void setIsTimeDependant(bool timeDependant);
    
    double sampleRate() const { return m_sampleRate; }
    void setSampleRate(double rate);
    
    // Methods for setting data
    Q_INVOKABLE void setChannelData(int channel, const QVariantList &data);
    Q_INVOKABLE void setSample(int channel, int sampleIndex, float value);
    Q_INVOKABLE float getSample(int channel, int sampleIndex) const;
    
    // Set all channels with same value (useful for testing)
    Q_INVOKABLE void fillAllChannels(float value);
    
    // Clear all data
    Q_INVOKABLE void clear();
    
    // Generate test signals
    Q_INVOKABLE void generateSineWaveAll(double frequency, double amplitude = 1.0, double phase = 0.0);
    Q_INVOKABLE void generateSineWave(const QVariant& channel, double frequency, double amplitude = 1.0, double phase = 0.0);
    void generateSineWave_(int channel, double frequency, double amplitude = 1.0, double phase = 0.0);


    Q_INVOKABLE void generateWhiteNoiseAll(double amplitude = 1.0);
    Q_INVOKABLE void generateWhiteNoise(const QVariant& channel, double amplitude = 1.0);
    void generateWhiteNoise_(int channel, double amplitude = 1.0);
    
    Q_INVOKABLE void pushChannelData(int channel,const QVariant &data);


signals:
    void channelCountChanged();
    void capacityChanged();
    void sampleCountChanged();
    void channelNamesChanged();
    void isTimeDependantChanged();
    void sampleRateChanged();
    void dataChanged();
    
protected:
    void applyValue(TEInstance* teInstance) override;
private:
    int m_channelCount = 1;
    int m_capacity = 1;
    int m_sampleCount = 0;
    QStringList m_channelNames;
    bool m_isTimeDependant = false;
    double m_sampleRate = 44100.0;
    
    // Store data as vector of vectors (channel -> samples)
    QVector<QVector<float>> m_bufferData;
    
    void resizeBufferData();
    bool m_markDirty;
    void baseGenerateWhiteNoise_(int channel, double amplitude);
    void baseGenerateSineWave_(int channel, double frequency, double amplitude, double phase);
};

#endif // TOUCHENGINEFLOATBUFFERINPUT_H
