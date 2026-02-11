// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 InKey contributors
// This file is part of InKey. See LICENSE for details.

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "../Data/NoteData.h"
#include "../Data/PitchData.h"

// Renders edited audio from the original captured audio + note edits.
// Uses Rubber Band for pitch shifting, time stretching, and formant control.
class AudioEngine
{
public:
    AudioEngine();
    ~AudioEngine();

    // Render the full edited audio from original + note edits.
    // Returns a mono audio buffer with the result.
    juce::AudioBuffer<float> render (const juce::AudioBuffer<float>& originalAudio,
                                      double sampleRate,
                                      const NoteList& notes,
                                      const PitchCurve& pitchCurve = {});

    // Playback state
    void startPlayback (const juce::AudioBuffer<float>& renderedAudio, double sampleRate);
    void startPlaybackFromSample (const juce::AudioBuffer<float>& renderedAudio, double sampleRate, int startSample);
    void stopPlayback();
    bool isPlaying() const { return playing.load(); }

    // Called from processBlock — fills output buffer with playback audio.
    // Returns true if still playing, false if playback ended.
    bool fillBuffer (juce::AudioBuffer<float>& outputBuffer, int numSamples);

    double getPlaybackPosition() const;

    // DAW-synced playback: read from rendered buffer at a specific DAW position
    bool fillBufferAtPosition (juce::AudioBuffer<float>& outputBuffer, int numSamples,
                               int64_t dawPositionInSamples, int64_t captureStartSample);

    // Double-buffered rendered audio for DAW transport sync
    juce::AudioBuffer<float> renderedBuffers[2];
    std::atomic<int> activeBufferIndex { 0 };
    std::atomic<bool> hasRenderedAudio { false };

    // Store a new rendered buffer (called from background render thread)
    void swapRenderedBuffer (juce::AudioBuffer<float>&& newBuffer);

private:
    // Process a note segment: pitch shift, time stretch, and/or formant shift.
    // Returns processed audio buffer.
    juce::AudioBuffer<float> processNote (const float* input, int numSamples,
                                           double sampleRate, float pitchSemitones,
                                           float formantSemitones, double timeRatio);

    // Process a note with drift scaling (sub-segment processing)
    juce::AudioBuffer<float> processNoteWithDrift (const float* input, int numSamples,
                                                     double sampleRate, const Note& note,
                                                     const PitchCurve& pitchCurve);

    std::atomic<bool> playing { false };
    juce::AudioBuffer<float> playbackBuffer;
    std::atomic<int> playbackPosition { 0 };
    double playbackSampleRate = 44100.0;

    // Playback ramp state (prevents pops at play/stop boundaries)
    static constexpr int kRampSamples = 256; // ~5ms at 44100 Hz
    std::atomic<int> rampUpRemaining { 0 };

    // DAW transport ramp state
    std::atomic<bool> wasPlayingLastBlock { false };

    // Double-buffer swap crossfade state
    static constexpr int kSwapCrossfadeSamples = 128; // ~3ms at 44100 Hz
    std::atomic<int> lastUsedBufferIndex { 0 };
    std::atomic<int> swapCrossfadeRemaining { 0 };
};
