// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 InKey contributors
// This file is part of InKey. See LICENSE for details.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../Analysis/PitchAnalyzer.h"
#include "../Analysis/NoteSegmenter.h"
#include "../Data/NoteData.h"
#include "../Data/PitchData.h"

// Per-audio-source analysis results, attached to ARAAudioSource
struct ARASourceAnalysis
{
    PitchCurve pitchCurve;
    NoteList notes;
    double sampleRate = 0.0;
    int64_t numSamples = 0;
    std::atomic<bool> analysisComplete { false };
    std::atomic<float> analysisProgress { 0.0f };
};

class InKeyDocumentController : public juce::ARADocumentControllerSpecialisation,
                                private PitchAnalyzer::Listener
{
public:
    using juce::ARADocumentControllerSpecialisation::ARADocumentControllerSpecialisation;

    // Access analysis results for an audio source
    ARASourceAnalysis* getAnalysis (juce::ARAAudioSource* source);

    // Listener for UI updates when analysis completes
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void araAnalysisComplete (juce::ARAAudioSource* source) = 0;
        virtual void araAnalysisProgress (juce::ARAAudioSource* source, float progress) = 0;
    };
    void addListener (Listener* l);
    void removeListener (Listener* l);

protected:
    // ARADocumentControllerSpecialisation overrides
    juce::ARAPlaybackRenderer* doCreatePlaybackRenderer() override;
    juce::ARAEditorRenderer*   doCreateEditorRenderer()   override;
    juce::ARAEditorView*       doCreateEditorView()       override;

    // Called when the host provides or updates audio sources
    void didEnableAudioSourceSamplesAccess (juce::ARAAudioSource* source, bool enable) noexcept override;

    // Persistence
    bool doRestoreObjectsFromStream (juce::ARAInputStream& input,
                                     const juce::ARARestoreObjectsFilter* filter) override;
    bool doStoreObjectsToStream (juce::ARAOutputStream& output,
                                 const juce::ARAStoreObjectsFilter* filter) override;

private:
    // PitchAnalyzer::Listener
    void analysisComplete() override;
    void analysisProgress (float progress) override;

    void analyzeSource (juce::ARAAudioSource* source);

    // Map from ARAAudioSource persistent ID to analysis data
    std::map<std::string, std::unique_ptr<ARASourceAnalysis>> analysisMap;
    PitchAnalyzer pitchAnalyzer;
    NoteSegmenter noteSegmenter;
    juce::ARAAudioSource* currentlyAnalyzingSource = nullptr;

    // Temporary buffer for sibilant detection (cleared after analysis)
    juce::AudioBuffer<float> analysisAudioBuffer;

    juce::ListenerList<Listener> listeners;
};
