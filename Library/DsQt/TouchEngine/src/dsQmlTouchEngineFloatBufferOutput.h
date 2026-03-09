#ifndef DSQMLTOUCHENGINEFLOATBUFFEROUTPUT_H
#define DSQMLTOUCHENGINEFLOATBUFFEROUTPUT_H

#include "dsQmlTouchEngineOutputBase.h"
#include <QVariantList>
#include <QStringList>
#include <QVector>

/**
 * DsQmlTouchEngineFloatBufferOutput - QML component for reading float buffer data from TouchEngine output links
 * Used for audio data, time-series data, or any multi-channel float array data
 *
 * Example usage in QML:
 * DsTouchEngineFloatBufferOutput {
 *     instanceId: myInstance.instanceIdString
 *     linkName: "op/audio_out"
 *     onDataChanged: {
 *         console.log("Channels:", channelCount, "Samples:", sampleCount)
 *         // Access first sample of first channel
 *         console.log("First sample:", getSample(0, 0))
 *     }
 * }
 */
class DsQmlTouchEngineFloatBufferOutput : public DsQmlTouchEngineOutputBase
{
    Q_OBJECT
    QML_NAMED_ELEMENT(DsTouchEngineFloatBufferOutput)

    Q_PROPERTY(int channelCount READ channelCount NOTIFY channelCountChanged)
    Q_PROPERTY(int sampleCount READ sampleCount NOTIFY sampleCountChanged)
    Q_PROPERTY(int capacity READ capacity NOTIFY capacityChanged)
    Q_PROPERTY(QStringList channelNames READ channelNames NOTIFY channelNamesChanged)
    Q_PROPERTY(double sampleRate READ sampleRate NOTIFY sampleRateChanged)
    Q_PROPERTY(bool isTimeDependant READ isTimeDependant NOTIFY isTimeDependantChanged)

public:
    explicit DsQmlTouchEngineFloatBufferOutput(QObject *parent = nullptr);
    ~DsQmlTouchEngineFloatBufferOutput() override;

    int channelCount() const { return m_channelCount; }
    int sampleCount() const { return m_sampleCount; }
    int capacity() const { return m_capacity; }
    QStringList channelNames() const { return m_channelNames; }
    double sampleRate() const { return m_sampleRate; }
    bool isTimeDependant() const { return m_isTimeDependant; }

    // Get a specific sample value
    Q_INVOKABLE float getSample(int channel, int sampleIndex) const;

    // Get all samples for a channel as a QVariantList (for QML)
    Q_INVOKABLE QVariantList getChannelData(int channel) const;

    //Get a specific sample value by channel name
    Q_INVOKABLE float getSampleByName(const QString& channelName, int sampleIndex) const;

    // Get all samples for a channel by name
    Q_INVOKABLE QVariantList getChannelDataByName(const QString& channelName) const;

    // Get the first sample from each channel (useful for single-value outputs)
    Q_INVOKABLE QVariantList getFirstSamples() const;

signals:
    void channelCountChanged();
    void sampleCountChanged();
    void capacityChanged();
    void channelNamesChanged();
    void sampleRateChanged();
    void isTimeDependantChanged();
    void dataChanged();

protected:
    void readValue(TEInstance* teInstance) override;

private:
    int m_channelCount = 0;
    int m_sampleCount = 0;
    int m_capacity = 0;
    QStringList m_channelNames;
    double m_sampleRate = 44100.0;
    bool m_isTimeDependant = false;

    // Buffer data stored per channel
    QVector<QVector<float>> m_bufferData;
};

#endif // DSQMLTOUCHENGINEFLOATBUFFEROUTPUT_H
