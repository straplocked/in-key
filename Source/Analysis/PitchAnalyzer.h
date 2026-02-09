#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_events/juce_events.h>
#include "../Data/PitchData.h"
#include "PitchDetector.h"

class PitchAnalyzer : private juce::Thread
{
public:
    PitchAnalyzer();
    ~PitchAnalyzer() override;

    // Start analysis on captured audio. Runs on background thread.
    void startAnalysis (const juce::AudioBuffer<float>& audio, double sampleRate);

    // Cancel a running analysis.
    void cancelAnalysis();

    // Progress (0.0 to 1.0), thread-safe.
    float getProgress() const { return progress.load(); }

    // True while analysis is running.
    bool isAnalyzing() const { return isThreadRunning(); }

    // Get results (only valid after analysis completes).
    const PitchCurve& getResult() const { return result; }

    // Listener for completion/progress
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void analysisComplete() = 0;
        virtual void analysisProgress (float progress) = 0;
    };

    void addListener (Listener* listener);
    void removeListener (Listener* listener);

    PitchDetector::Settings& getDetectorSettings() { return detector.getSettings(); }

private:
    void run() override;

    PitchDetector detector;
    PitchCurve result;

    // Audio to analyze (copied for thread safety)
    juce::AudioBuffer<float> audioToAnalyze;
    double analysisSampleRate = 44100.0;

    std::atomic<float> progress { 0.0f };

    juce::ListenerList<Listener> listeners;
};
