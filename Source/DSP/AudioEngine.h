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
    void stopPlayback();
    bool isPlaying() const { return playing.load(); }

    // Called from processBlock — fills output buffer with playback audio.
    // Returns true if still playing, false if playback ended.
    bool fillBuffer (juce::AudioBuffer<float>& outputBuffer, int numSamples);

    double getPlaybackPosition() const;

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
};
