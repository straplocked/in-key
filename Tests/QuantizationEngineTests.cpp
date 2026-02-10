#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "Editing/QuantizationEngine.h"

using Catch::Matchers::WithinAbs;

// ============================================================
// Scale definitions
// ============================================================
TEST_CASE ("Chromatic scale has all 12 degrees", "[QuantizationEngine]")
{
    auto scale = QuantizationEngine::chromaticScale (0);

    REQUIRE (scale.degrees.size() == 12);
    for (int i = 0; i < 12; ++i)
        REQUIRE (scale.degrees[static_cast<size_t> (i)] == true);
}

TEST_CASE ("C Major scale has correct degrees", "[QuantizationEngine]")
{
    auto scale = QuantizationEngine::majorScale (0); // C major

    // C D E F G A B = degrees 0,2,4,5,7,9,11
    REQUIRE (scale.degrees[0] == true);   // C
    REQUIRE (scale.degrees[1] == false);  // C#
    REQUIRE (scale.degrees[2] == true);   // D
    REQUIRE (scale.degrees[3] == false);  // D#
    REQUIRE (scale.degrees[4] == true);   // E
    REQUIRE (scale.degrees[5] == true);   // F
    REQUIRE (scale.degrees[6] == false);  // F#
    REQUIRE (scale.degrees[7] == true);   // G
    REQUIRE (scale.degrees[8] == false);  // G#
    REQUIRE (scale.degrees[9] == true);   // A
    REQUIRE (scale.degrees[10] == false); // A#
    REQUIRE (scale.degrees[11] == true);  // B
}

TEST_CASE ("Natural minor scale has correct degrees", "[QuantizationEngine]")
{
    auto scale = QuantizationEngine::naturalMinorScale (0); // C minor

    // C D Eb F G Ab Bb = degrees 0,2,3,5,7,8,10
    REQUIRE (scale.degrees[0] == true);   // C
    REQUIRE (scale.degrees[2] == true);   // D
    REQUIRE (scale.degrees[3] == true);   // Eb
    REQUIRE (scale.degrees[5] == true);   // F
    REQUIRE (scale.degrees[7] == true);   // G
    REQUIRE (scale.degrees[8] == true);   // Ab
    REQUIRE (scale.degrees[10] == true);  // Bb
}

TEST_CASE ("Pentatonic scale has 5 degrees", "[QuantizationEngine]")
{
    auto scale = QuantizationEngine::pentatonicScale (0);

    int count = 0;
    for (auto d : scale.degrees)
        if (d) count++;

    REQUIRE (count == 5);
}

TEST_CASE ("Blues scale has 6 degrees", "[QuantizationEngine]")
{
    auto scale = QuantizationEngine::bluesScale (0);

    int count = 0;
    for (auto d : scale.degrees)
        if (d) count++;

    REQUIRE (count == 6);
}

// ============================================================
// nearestScaleDegree
// ============================================================
TEST_CASE ("nearestScaleDegree snaps to exact scale tone", "[QuantizationEngine]")
{
    auto scale = QuantizationEngine::majorScale (0); // C major

    // C4 (MIDI 60) is in scale → should stay
    REQUIRE (QuantizationEngine::nearestScaleDegree (60.0f, scale) == 60);

    // E4 (MIDI 64) is in scale → should stay
    REQUIRE (QuantizationEngine::nearestScaleDegree (64.0f, scale) == 64);
}

TEST_CASE ("nearestScaleDegree snaps out-of-scale notes to nearest scale tone", "[QuantizationEngine]")
{
    auto scale = QuantizationEngine::majorScale (0); // C major

    // C#4 (MIDI 61) is NOT in C major → should snap to C (60) or D (62)
    int result = QuantizationEngine::nearestScaleDegree (61.0f, scale);
    REQUIRE ((result == 60 || result == 62));

    // F#4 (MIDI 66) is NOT in C major → should snap to F (65) or G (67)
    result = QuantizationEngine::nearestScaleDegree (66.0f, scale);
    REQUIRE ((result == 65 || result == 67));
}

TEST_CASE ("nearestScaleDegree handles fractional MIDI values", "[QuantizationEngine]")
{
    auto scale = QuantizationEngine::majorScale (0);

    // 60.3 (between C and C#) → should snap to C (60)
    REQUIRE (QuantizationEngine::nearestScaleDegree (60.3f, scale) == 60);

    // 60.7 (between C and C#) → rounds to 61 (C#), not in scale, snaps to C(60) or D(62)
    int result = QuantizationEngine::nearestScaleDegree (60.7f, scale);
    REQUIRE ((result == 60 || result == 62));
}

// ============================================================
// calculateCorrection
// ============================================================
TEST_CASE ("calculateCorrection returns 0 for zero strength", "[QuantizationEngine]")
{
    auto scale = QuantizationEngine::majorScale (0);

    float correction = QuantizationEngine::calculateCorrection (440.0f, 0.0f, scale);

    REQUIRE_THAT (correction, WithinAbs (0.0f, 0.001f));
}

TEST_CASE ("calculateCorrection returns 0 for zero frequency", "[QuantizationEngine]")
{
    auto scale = QuantizationEngine::majorScale (0);

    float correction = QuantizationEngine::calculateCorrection (0.0f, 1.0f, scale);

    REQUIRE_THAT (correction, WithinAbs (0.0f, 0.001f));
}

TEST_CASE ("calculateCorrection with full strength snaps to scale", "[QuantizationEngine]")
{
    auto scale = QuantizationEngine::majorScale (0); // C major

    // A4 (440 Hz, MIDI 69) IS in C major → no correction needed
    float correction = QuantizationEngine::calculateCorrection (440.0f, 1.0f, scale);
    REQUIRE_THAT (correction, WithinAbs (0.0f, 0.1f));
}

TEST_CASE ("calculateCorrection with half strength gives partial correction", "[QuantizationEngine]")
{
    auto scale = QuantizationEngine::majorScale (0);

    // Create a pitch exactly between C and C# (MIDI 60.5)
    float pitchHz = 440.0f * std::pow (2.0f, (60.5f - 69.0f) / 12.0f);

    float fullCorrection = QuantizationEngine::calculateCorrection (pitchHz, 1.0f, scale);
    float halfCorrection = QuantizationEngine::calculateCorrection (pitchHz, 0.5f, scale);

    REQUIRE_THAT (halfCorrection, WithinAbs (fullCorrection * 0.5f, 0.01f));
}

TEST_CASE ("calculateCorrection for chromatic scale always returns near-zero", "[QuantizationEngine]")
{
    auto scale = QuantizationEngine::chromaticScale (0);

    // Any note is in the chromatic scale — correction should be minimal
    float correction = QuantizationEngine::calculateCorrection (440.0f, 1.0f, scale);
    REQUIRE_THAT (correction, WithinAbs (0.0f, 0.5f)); // Within half semitone
}

// ============================================================
// rootNoteName
// ============================================================
TEST_CASE ("rootNoteName returns correct names", "[QuantizationEngine]")
{
    REQUIRE (std::string (QuantizationEngine::rootNoteName (0)) == "C");
    REQUIRE (std::string (QuantizationEngine::rootNoteName (1)) == "C#");
    REQUIRE (std::string (QuantizationEngine::rootNoteName (9)) == "A");
    REQUIRE (std::string (QuantizationEngine::rootNoteName (11)) == "B");
}

// ============================================================
// Different root notes
// ============================================================
TEST_CASE ("Major scale with different root shifts correctly", "[QuantizationEngine]")
{
    auto gMajor = QuantizationEngine::majorScale (7); // G major

    REQUIRE (gMajor.rootNote == 7);

    // G major: G A B C D E F#
    // Degrees relative to root: 0,2,4,5,7,9,11
    REQUIRE (gMajor.degrees[0] == true);   // root (G)
    REQUIRE (gMajor.degrees[2] == true);   // whole step (A)
    REQUIRE (gMajor.degrees[4] == true);   // whole step (B)
    REQUIRE (gMajor.degrees[5] == true);   // half step (C)
    REQUIRE (gMajor.degrees[7] == true);   // whole step (D)
    REQUIRE (gMajor.degrees[9] == true);   // whole step (E)
    REQUIRE (gMajor.degrees[11] == true);  // whole step (F#)
}

TEST_CASE ("All scale factories produce 12 degrees", "[QuantizationEngine]")
{
    auto scales = {
        QuantizationEngine::chromaticScale (0),
        QuantizationEngine::majorScale (0),
        QuantizationEngine::naturalMinorScale (0),
        QuantizationEngine::harmonicMinorScale (0),
        QuantizationEngine::pentatonicScale (0),
        QuantizationEngine::bluesScale (0),
        QuantizationEngine::dorianScale (0),
        QuantizationEngine::mixolydianScale (0)
    };

    for (const auto& scale : scales)
    {
        INFO ("Scale: " << scale.name);
        REQUIRE (scale.degrees.size() == 12);
    }
}
