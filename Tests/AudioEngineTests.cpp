#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "DSP/AudioEngine.h"
#include <cmath>

using Catch::Matchers::WithinAbs;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Helper: create a mono AudioBuffer with a sine wave
static juce::AudioBuffer<float> createSineBuffer (float frequencyHz, double sampleRate, double durationSeconds)
{
    int numSamples = static_cast<int> (sampleRate * durationSeconds);
    juce::AudioBuffer<float> buffer (1, numSamples);
    float* data = buffer.getWritePointer (0);

    for (int i = 0; i < numSamples; ++i)
    {
        double phase = 2.0 * M_PI * frequencyHz * i / sampleRate;
        data[i] = static_cast<float> (std::sin (phase)) * 0.5f;
    }

    return buffer;
}

// ============================================================
// Render
// ============================================================
TEST_CASE ("AudioEngine render returns empty for empty audio", "[AudioEngine]")
{
    AudioEngine engine;
    juce::AudioBuffer<float> empty;
    NoteList notes;

    auto result = engine.render (empty, 44100.0, notes);

    REQUIRE (result.getNumSamples() == 0);
}

TEST_CASE ("AudioEngine render produces output with same length as input", "[AudioEngine]")
{
    AudioEngine engine;
    auto audio = createSineBuffer (440.0f, 44100.0, 1.0);

    Note note;
    note.id = 1;
    note.startTime = 0.0;
    note.duration = 1.0;
    note.originalPitch = 440.0f;
    note.midiNote = 69;
    NoteList notes = { note };

    auto result = engine.render (audio, 44100.0, notes);

    REQUIRE (result.getNumSamples() == audio.getNumSamples());
}

TEST_CASE ("AudioEngine render passes through unedited notes", "[AudioEngine]")
{
    AudioEngine engine;
    auto audio = createSineBuffer (440.0f, 44100.0, 0.5);

    Note note;
    note.id = 1;
    note.startTime = 0.0;
    note.duration = 0.5;
    note.originalPitch = 440.0f;
    note.midiNote = 69;
    // No edits — should pass through
    NoteList notes = { note };

    auto result = engine.render (audio, 44100.0, notes);

    REQUIRE (result.getNumSamples() == audio.getNumSamples());
    REQUIRE (result.getNumChannels() == 1);

    // Output should be non-silent in the note region (accounting for crossfade)
    const float* outData = result.getReadPointer (0);
    float maxVal = 0.0f;
    for (int i = 500; i < result.getNumSamples() - 500; ++i)
        maxVal = std::max (maxVal, std::abs (outData[i]));

    REQUIRE (maxVal > 0.1f);
}

TEST_CASE ("AudioEngine render applies gain correctly", "[AudioEngine]")
{
    AudioEngine engine;
    auto audio = createSineBuffer (440.0f, 44100.0, 0.5);

    Note note;
    note.id = 1;
    note.startTime = 0.0;
    note.duration = 0.5;
    note.originalPitch = 440.0f;
    note.midiNote = 69;
    note.gainDB = -20.0f; // Significant volume reduction

    NoteList notes = { note };

    auto result = engine.render (audio, 44100.0, notes);

    // Find max amplitude in the rendered output (skip crossfade regions)
    const float* outData = result.getReadPointer (0);
    float maxVal = 0.0f;
    int safeStart = static_cast<int> (0.01 * 44100.0);
    int safeEnd = result.getNumSamples() - safeStart;
    for (int i = safeStart; i < safeEnd; ++i)
        maxVal = std::max (maxVal, std::abs (outData[i]));

    // Original amplitude is 0.5, -20 dB → factor 0.1, so max should be ~0.05
    REQUIRE (maxVal < 0.15f);
    REQUIRE (maxVal > 0.01f);
}

// ============================================================
// Playback state
// ============================================================
TEST_CASE ("AudioEngine is not playing by default", "[AudioEngine]")
{
    AudioEngine engine;
    REQUIRE_FALSE (engine.isPlaying());
}

TEST_CASE ("AudioEngine starts and stops playback", "[AudioEngine]")
{
    AudioEngine engine;
    auto audio = createSineBuffer (440.0f, 44100.0, 0.5);

    engine.startPlayback (audio, 44100.0);
    REQUIRE (engine.isPlaying());

    engine.stopPlayback();
    REQUIRE_FALSE (engine.isPlaying());
}

TEST_CASE ("AudioEngine playback position starts at zero", "[AudioEngine]")
{
    AudioEngine engine;
    auto audio = createSineBuffer (440.0f, 44100.0, 0.5);

    engine.startPlayback (audio, 44100.0);
    REQUIRE_THAT (engine.getPlaybackPosition(), WithinAbs (0.0, 0.001));

    engine.stopPlayback();
}

// ============================================================
// fillBuffer
// ============================================================
TEST_CASE ("AudioEngine fillBuffer returns false when not playing", "[AudioEngine]")
{
    AudioEngine engine;
    juce::AudioBuffer<float> output (2, 512);

    REQUIRE_FALSE (engine.fillBuffer (output, 512));
}

TEST_CASE ("AudioEngine fillBuffer fills output during playback", "[AudioEngine]")
{
    AudioEngine engine;
    auto audio = createSineBuffer (440.0f, 44100.0, 0.5);

    engine.startPlayback (audio, 44100.0);

    juce::AudioBuffer<float> output (2, 512);
    output.clear();

    bool stillPlaying = engine.fillBuffer (output, 512);

    REQUIRE (stillPlaying);

    // Output should contain audio data
    float maxVal = 0.0f;
    for (int i = 0; i < 512; ++i)
        maxVal = std::max (maxVal, std::abs (output.getSample (0, i)));

    REQUIRE (maxVal > 0.01f);

    engine.stopPlayback();
}

TEST_CASE ("AudioEngine fillBuffer advances playback position", "[AudioEngine]")
{
    AudioEngine engine;
    auto audio = createSineBuffer (440.0f, 44100.0, 1.0);

    engine.startPlayback (audio, 44100.0);

    juce::AudioBuffer<float> output (1, 4410); // 0.1 seconds
    engine.fillBuffer (output, 4410);

    double pos = engine.getPlaybackPosition();
    REQUIRE_THAT (pos, WithinAbs (0.1, 0.01));

    engine.stopPlayback();
}

TEST_CASE ("AudioEngine fillBuffer stops at end of buffer", "[AudioEngine]")
{
    AudioEngine engine;

    // Very short audio
    juce::AudioBuffer<float> shortAudio (1, 256);
    shortAudio.clear();
    float* data = shortAudio.getWritePointer (0);
    for (int i = 0; i < 256; ++i)
        data[i] = 0.5f;

    engine.startPlayback (shortAudio, 44100.0);

    // Try to read more than available
    juce::AudioBuffer<float> output (1, 512);
    bool stillPlaying = engine.fillBuffer (output, 512);

    REQUIRE_FALSE (stillPlaying);
}
