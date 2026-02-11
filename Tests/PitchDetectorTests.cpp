// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 InKey contributors
// This file is part of InKey. See LICENSE for details.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "Analysis/PitchDetector.h"
#include <cmath>
#include <vector>

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Helper: generate a mono sine wave
static std::vector<float> generateSine (float frequencyHz, double sampleRate, double durationSeconds)
{
    int numSamples = static_cast<int> (sampleRate * durationSeconds);
    std::vector<float> audio (static_cast<size_t> (numSamples));
    for (int i = 0; i < numSamples; ++i)
    {
        double phase = 2.0 * M_PI * frequencyHz * i / sampleRate;
        audio[static_cast<size_t> (i)] = static_cast<float> (std::sin (phase)) * 0.8f;
    }
    return audio;
}

// Helper: generate silence
static std::vector<float> generateSilence (double sampleRate, double durationSeconds)
{
    int numSamples = static_cast<int> (sampleRate * durationSeconds);
    return std::vector<float> (static_cast<size_t> (numSamples), 0.0f);
}

// ============================================================
// Single frame analysis
// ============================================================
TEST_CASE ("PitchDetector detects 440 Hz sine in single frame", "[PitchDetector]")
{
    PitchDetector detector;
    auto audio = generateSine (440.0f, 44100.0, 0.1);

    float confidence = 0.0f;
    float freq = detector.analyzeFrame (audio.data(), 2048, 44100.0, confidence);

    REQUIRE_THAT (freq, WithinAbs (440.0f, 5.0f));
    REQUIRE (confidence > 0.8f);
}

TEST_CASE ("PitchDetector detects 220 Hz sine in single frame", "[PitchDetector]")
{
    PitchDetector detector;
    auto audio = generateSine (220.0f, 44100.0, 0.1);

    float confidence = 0.0f;
    float freq = detector.analyzeFrame (audio.data(), 2048, 44100.0, confidence);

    REQUIRE_THAT (freq, WithinAbs (220.0f, 5.0f));
    REQUIRE (confidence > 0.8f);
}

TEST_CASE ("PitchDetector returns 0 for silence", "[PitchDetector]")
{
    PitchDetector detector;
    auto audio = generateSilence (44100.0, 0.1);

    float confidence = 0.0f;
    float freq = detector.analyzeFrame (audio.data(), 2048, 44100.0, confidence);

    REQUIRE (freq == 0.0f);
    REQUIRE (confidence < 0.3f);
}

// ============================================================
// Full analysis
// ============================================================
TEST_CASE ("PitchDetector analyze returns correct number of frames", "[PitchDetector]")
{
    PitchDetector detector;
    auto audio = generateSine (440.0f, 44100.0, 0.5); // 0.5 seconds

    auto result = detector.analyze (audio.data(), static_cast<int> (audio.size()), 44100.0);

    int expectedFrames = (static_cast<int> (audio.size()) - 2048) / 256 + 1;
    REQUIRE (static_cast<int> (result.size()) == expectedFrames);
}

TEST_CASE ("PitchDetector analyze of pure tone gives consistent pitch", "[PitchDetector]")
{
    PitchDetector detector;
    auto audio = generateSine (440.0f, 44100.0, 0.5);

    auto result = detector.analyze (audio.data(), static_cast<int> (audio.size()), 44100.0);

    REQUIRE (result.size() > 10);

    // Skip first few frames (onset transient) and check pitch consistency
    int voicedCount = 0;
    for (size_t i = 5; i < result.size(); ++i)
    {
        if (result[i].frequency > 0.0f)
        {
            REQUIRE_THAT (result[i].frequency, WithinAbs (440.0f, 10.0f));
            voicedCount++;
        }
    }

    // Most frames should be detected as voiced
    REQUIRE (voicedCount > static_cast<int> (result.size()) / 2);
}

TEST_CASE ("PitchDetector returns empty for buffer smaller than frame size", "[PitchDetector]")
{
    PitchDetector detector;
    std::vector<float> shortAudio (100, 0.0f);

    auto result = detector.analyze (shortAudio.data(), static_cast<int> (shortAudio.size()), 44100.0);

    REQUIRE (result.empty());
}

TEST_CASE ("PitchDetector frame timestamps are correct", "[PitchDetector]")
{
    PitchDetector detector;
    auto audio = generateSine (440.0f, 44100.0, 0.5);

    auto result = detector.analyze (audio.data(), static_cast<int> (audio.size()), 44100.0);

    REQUIRE (result.size() > 2);

    // First frame should be at time 0
    REQUIRE_THAT (result[0].time, WithinAbs (0.0, 0.001));

    // Second frame should be at hopSize / sampleRate
    double expectedHopTime = 256.0 / 44100.0;
    REQUIRE_THAT (result[1].time, WithinAbs (expectedHopTime, 0.0001));
}

// ============================================================
// Settings
// ============================================================
TEST_CASE ("PitchDetector filters out frequencies outside range", "[PitchDetector]")
{
    PitchDetector detector;
    PitchDetector::Settings settings;
    settings.minFrequency = 200.0f;
    settings.maxFrequency = 400.0f;
    detector.setSettings (settings);

    // 440 Hz is above the max range
    auto audio = generateSine (440.0f, 44100.0, 0.1);

    float confidence = 0.0f;
    float freq = detector.analyzeFrame (audio.data(), 2048, 44100.0, confidence);

    REQUIRE (freq == 0.0f);
}

TEST_CASE ("PitchDetector detects pitch within configured range", "[PitchDetector]")
{
    PitchDetector detector;
    PitchDetector::Settings settings;
    settings.minFrequency = 200.0f;
    settings.maxFrequency = 500.0f;
    detector.setSettings (settings);

    auto audio = generateSine (300.0f, 44100.0, 0.1);

    float confidence = 0.0f;
    float freq = detector.analyzeFrame (audio.data(), 2048, 44100.0, confidence);

    REQUIRE_THAT (freq, WithinAbs (300.0f, 10.0f));
    REQUIRE (confidence > 0.5f);
}

TEST_CASE ("PitchDetector detects different pitches accurately", "[PitchDetector]")
{
    PitchDetector detector;

    float testFrequencies[] = { 100.0f, 200.0f, 330.0f, 440.0f, 660.0f };

    for (float targetFreq : testFrequencies)
    {
        auto audio = generateSine (targetFreq, 44100.0, 0.1);

        float confidence = 0.0f;
        float freq = detector.analyzeFrame (audio.data(), 2048, 44100.0, confidence);

        INFO ("Testing frequency: " << targetFreq << " Hz");
        REQUIRE_THAT (freq, WithinAbs (targetFreq, 15.0f));
    }
}
