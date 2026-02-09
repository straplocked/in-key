#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
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
    void stopPlayback();

    AudioCapture& getAudioCapture() { return audioCapture; }
    PitchAnalyzer& getPitchAnalyzer() { return pitchAnalyzer; }
    AudioEngine& getAudioEngine() { return audioEngine; }

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InKeyProcessor)
};
