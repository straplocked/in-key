#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include "PluginState.h"
#include "Audio/AudioCapture.h"
#include "Analysis/PitchAnalyzer.h"
#include "Analysis/NoteSegmenter.h"
#include "DSP/AudioEngine.h"
#include "Data/TakeData.h"

class InKeyProcessor : public juce::AudioProcessor,
                       private PitchAnalyzer::Listener
{
public:
    InKeyProcessor();
    ~InKeyProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // State machine
    PluginState getState() const { return currentState.load(); }
    void startCapture();
    void stopCapture();

    // Playback
    void startPlayback (const NoteList& editedNotes);
    void startPlaybackFromPosition (const NoteList& editedNotes, double timeInSeconds);
    void stopPlayback();

    // Preview playback for note audition (renders single note, plays it)
    void startPreviewPlayback (const juce::AudioBuffer<float>& audio, double sampleRate);

    // A/B comparison: bypass edits to hear original audio
    bool getBypassEdits() const { return bypassEdits.load(); }
    void setBypassEdits (bool bypass) { bypassEdits.store (bypass); }

    // Master tuning: A4 reference frequency (default 440 Hz)
    float getA4Reference() const { return a4Reference; }
    void setA4Reference (float freq) { a4Reference = freq; }

    AudioCapture& getAudioCapture() { return audioCapture; }
    PitchAnalyzer& getPitchAnalyzer() { return pitchAnalyzer; }
    AudioEngine& getAudioEngine() { return audioEngine; }
    PitchDetector::Settings& getPitchDetectorSettings() { return pitchAnalyzer.getDetectorSettings(); }
    NoteSegmenter& getNoteSegmenter() { return noteSegmenter; }

    // Re-analyze the active take with current settings
    void reanalyzeActiveTake();

    // Multi-take support
    const TakeList& getTakes() const { return takes; }
    int getActiveTakeIndex() const { return activeTakeIndex; }
    int getReferenceTakeIndex() const { return referenceTakeIndex; }
    int getNumTakes() const { return static_cast<int> (takes.size()); }

    // Save current edited notes back to the active take
    void saveActiveTakeNotes (const NoteList& editedNotes);

    // Switch the active take (returns the take's data)
    const Take* switchToTake (int index);

    // Set which take is the reference
    void setReferenceTake (int index);

    // Run DTW alignment on the active take against the reference
    bool alignActiveTakeToReference();

    // Legacy accessor (returns active take's notes)
    const NoteList& getSegmentedNotes() const;

    // DAW transport sync: request background re-render after note edits
    void requestRender();

    // DAW transport state for UI playhead
    std::atomic<double> dawPlayheadInCapture { -1.0 };
    std::atomic<bool> dawIsPlaying { false };

private:
    // PitchAnalyzer::Listener
    void analysisComplete() override;
    void analysisProgress (float progress) override;

    std::atomic<PluginState> currentState { PluginState::IDLE };
    AudioCapture audioCapture;
    PitchAnalyzer pitchAnalyzer;
    NoteSegmenter noteSegmenter;
    AudioEngine audioEngine;

    // Multi-take storage
    TakeList takes;
    int activeTakeIndex = -1;
    int referenceTakeIndex = 0;
    int nextTakeId = 1;

    // Fallback empty notes for legacy accessor
    NoteList emptyNotes;

    // A/B bypass
    std::atomic<bool> bypassEdits { false };

    // Master tuning
    float a4Reference = 440.0f;

    // DAW transport sync
    std::atomic<bool> captureJustStarted { false };
    int64_t captureStartInSamples = 0;

    // Background render
    std::atomic<bool> renderNeeded { false };
    std::atomic<bool> renderInProgress { false };
    void performRender();

    // Background render thread
    class RenderThread : public juce::Thread
    {
    public:
        RenderThread (InKeyProcessor& p) : Thread ("InKey Render"), processor (p) {}
        void run() override;
    private:
        InKeyProcessor& processor;
    };
    std::unique_ptr<RenderThread> renderThread;

    // Render debounce timer (fires on message thread)
    class RenderDebounceTimer : public juce::Timer
    {
    public:
        RenderDebounceTimer (InKeyProcessor& p) : processor (p) {}
        void timerCallback() override;
    private:
        InKeyProcessor& processor;
    };
    RenderDebounceTimer renderDebounceTimer { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InKeyProcessor)
};
