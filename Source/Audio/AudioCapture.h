#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

class AudioCapture : private juce::Timer
{
public:
    AudioCapture();
    ~AudioCapture() override;

    void prepare (double sampleRate, int maxBlockSize);
    void startCapture();
    void stopCapture();

    // Called from the audio thread — must be lock-free
    void writeAudio (const juce::AudioBuffer<float>& buffer, int numSamples);

    // Access captured audio (message thread only)
    const juce::AudioBuffer<float>& getCapturedAudio() const { return capturedAudio; }
    double getSampleRate() const { return currentSampleRate; }
    int getNumCapturedSamples() const { return totalCapturedSamples; }
    double getCapturedDurationSeconds() const;

    bool isCapturing() const { return capturing.load(); }

    // Listener for waveform updates
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void captureUpdated() = 0;
    };

    void addListener (Listener* listener);
    void removeListener (Listener* listener);

private:
    void timerCallback() override;
    void drainFifo();

    double currentSampleRate = 44100.0;
    int fifoSize = 0;

    // Lock-free FIFO for audio thread → message thread
    std::unique_ptr<juce::AbstractFifo> fifo;
    juce::AudioBuffer<float> fifoBuffer;

    // Captured audio storage (message thread only)
    juce::AudioBuffer<float> capturedAudio;
    int totalCapturedSamples = 0;

    std::atomic<bool> capturing { false };

    juce::ListenerList<Listener> listeners;

    static constexpr int maxCaptureDurationSeconds = 300; // 5 minutes
    static constexpr int timerIntervalMs = 50;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioCapture)
};
