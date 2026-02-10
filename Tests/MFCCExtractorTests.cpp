#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "Alignment/MFCCExtractor.h"
#include <cmath>
#include <vector>

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

// ============================================================
// Basic extraction
// ============================================================
TEST_CASE ("MFCCExtractor returns empty for short buffer", "[MFCCExtractor]")
{
    MFCCExtractor extractor;
    std::vector<float> shortAudio (100, 0.0f);

    auto result = extractor.extract (shortAudio.data(), static_cast<int> (shortAudio.size()), 44100.0);

    REQUIRE (result.empty());
}

TEST_CASE ("MFCCExtractor returns correct number of frames", "[MFCCExtractor]")
{
    MFCCExtractor extractor;
    auto settings = extractor.getSettings();

    auto audio = generateSine (440.0f, 44100.0, 1.0);

    auto result = extractor.extract (audio.data(), static_cast<int> (audio.size()), 44100.0);

    int expectedFrames = (static_cast<int> (audio.size()) - settings.frameSize) / settings.hopSize + 1;
    REQUIRE (static_cast<int> (result.size()) == expectedFrames);
}

TEST_CASE ("MFCCExtractor each frame has correct number of coefficients", "[MFCCExtractor]")
{
    MFCCExtractor extractor;
    auto settings = extractor.getSettings();

    auto audio = generateSine (440.0f, 44100.0, 0.5);

    auto result = extractor.extract (audio.data(), static_cast<int> (audio.size()), 44100.0);

    REQUIRE_FALSE (result.empty());
    for (const auto& frame : result)
    {
        REQUIRE (static_cast<int> (frame.size()) == settings.numCoefficients);
    }
}

TEST_CASE ("MFCCExtractor produces non-zero coefficients for tonal audio", "[MFCCExtractor]")
{
    MFCCExtractor extractor;

    auto audio = generateSine (440.0f, 44100.0, 0.5);

    auto result = extractor.extract (audio.data(), static_cast<int> (audio.size()), 44100.0);

    REQUIRE_FALSE (result.empty());

    // The first coefficient (energy) should be non-zero for a sine wave
    bool hasNonZero = false;
    for (const auto& frame : result)
    {
        if (std::abs (frame[0]) > 0.001f)
        {
            hasNonZero = true;
            break;
        }
    }
    REQUIRE (hasNonZero);
}

TEST_CASE ("MFCCExtractor produces different features for different frequencies", "[MFCCExtractor]")
{
    MFCCExtractor extractor;

    auto audio440 = generateSine (440.0f, 44100.0, 0.5);
    auto audio110 = generateSine (110.0f, 44100.0, 0.5);

    auto mfcc440 = extractor.extract (audio440.data(), static_cast<int> (audio440.size()), 44100.0);
    auto mfcc110 = extractor.extract (audio110.data(), static_cast<int> (audio110.size()), 44100.0);

    REQUIRE_FALSE (mfcc440.empty());
    REQUIRE_FALSE (mfcc110.empty());

    // Compare a mid-sequence frame — coefficients should differ
    size_t midIdx = std::min (mfcc440.size(), mfcc110.size()) / 2;
    float totalDiff = 0.0f;
    for (size_t c = 0; c < mfcc440[midIdx].size(); ++c)
        totalDiff += std::abs (mfcc440[midIdx][c] - mfcc110[midIdx][c]);

    REQUIRE (totalDiff > 0.1f);
}

TEST_CASE ("MFCCExtractor produces consistent features for identical signals", "[MFCCExtractor]")
{
    MFCCExtractor extractor;

    auto audio1 = generateSine (440.0f, 44100.0, 0.5);
    auto audio2 = generateSine (440.0f, 44100.0, 0.5);

    auto mfcc1 = extractor.extract (audio1.data(), static_cast<int> (audio1.size()), 44100.0);
    auto mfcc2 = extractor.extract (audio2.data(), static_cast<int> (audio2.size()), 44100.0);

    REQUIRE (mfcc1.size() == mfcc2.size());

    // Identical input → identical output
    for (size_t f = 0; f < mfcc1.size(); ++f)
    {
        for (size_t c = 0; c < mfcc1[f].size(); ++c)
        {
            REQUIRE_THAT (mfcc1[f][c], Catch::Matchers::WithinAbs (mfcc2[f][c], 0.0001f));
        }
    }
}
