#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "Analysis/NoteSegmenter.h"

using Catch::Matchers::WithinAbs;

// Helper: create a pitch curve with a steady tone at the given frequency
static PitchCurve createSteadyTone (float freqHz, double startTime, double duration,
                                     double frameInterval = 0.005, float confidence = 0.9f)
{
    PitchCurve curve;
    for (double t = startTime; t < startTime + duration; t += frameInterval)
    {
        PitchFrame frame;
        frame.time = t;
        frame.frequency = freqHz;
        frame.confidence = confidence;
        curve.push_back (frame);
    }
    return curve;
}

// Helper: create unvoiced frames
static PitchCurve createSilence (double startTime, double duration, double frameInterval = 0.005)
{
    PitchCurve curve;
    for (double t = startTime; t < startTime + duration; t += frameInterval)
    {
        PitchFrame frame;
        frame.time = t;
        frame.frequency = 0.0f;
        frame.confidence = 0.0f;
        curve.push_back (frame);
    }
    return curve;
}

// Helper: concatenate two pitch curves
static PitchCurve concat (const PitchCurve& a, const PitchCurve& b)
{
    PitchCurve result = a;
    result.insert (result.end(), b.begin(), b.end());
    return result;
}

// ============================================================
// Basic segmentation
// ============================================================
TEST_CASE ("NoteSegmenter returns empty for empty pitch curve", "[NoteSegmenter]")
{
    NoteSegmenter segmenter;
    PitchCurve empty;

    auto notes = segmenter.segment (empty);

    REQUIRE (notes.empty());
}

TEST_CASE ("NoteSegmenter detects single continuous note", "[NoteSegmenter]")
{
    NoteSegmenter segmenter;
    auto curve = createSteadyTone (440.0f, 0.0, 0.5);

    auto notes = segmenter.segment (curve);

    REQUIRE (notes.size() == 1);
    REQUIRE_THAT (notes[0].originalPitch, WithinAbs (440.0f, 1.0f));
    REQUIRE (notes[0].midiNote == 69); // A4
    REQUIRE (notes[0].confidence > 0.8f);
}

TEST_CASE ("NoteSegmenter detects two notes separated by silence", "[NoteSegmenter]")
{
    NoteSegmenter segmenter;

    auto tone1 = createSteadyTone (440.0f, 0.0, 0.2);
    auto gap = createSilence (0.2, 0.1);
    auto tone2 = createSteadyTone (330.0f, 0.3, 0.2);

    auto curve = concat (concat (tone1, gap), tone2);

    auto notes = segmenter.segment (curve);

    REQUIRE (notes.size() == 2);
    REQUIRE_THAT (notes[0].originalPitch, WithinAbs (440.0f, 1.0f));
    REQUIRE_THAT (notes[1].originalPitch, WithinAbs (330.0f, 1.0f));
}

TEST_CASE ("NoteSegmenter splits on large pitch jump", "[NoteSegmenter]")
{
    NoteSegmenter segmenter;

    // Two tones at very different pitches, no gap between them
    auto tone1 = createSteadyTone (220.0f, 0.0, 0.2);
    auto tone2 = createSteadyTone (440.0f, 0.2, 0.2); // Octave jump

    auto curve = concat (tone1, tone2);

    auto notes = segmenter.segment (curve);

    REQUIRE (notes.size() == 2);
    REQUIRE_THAT (notes[0].originalPitch, WithinAbs (220.0f, 2.0f));
    REQUIRE_THAT (notes[1].originalPitch, WithinAbs (440.0f, 2.0f));
}

// ============================================================
// Minimum duration filtering
// ============================================================
TEST_CASE ("NoteSegmenter filters notes shorter than minimum duration", "[NoteSegmenter]")
{
    NoteSegmenter segmenter;

    // Very short tone (20ms) — below default 50ms minimum
    auto shortTone = createSteadyTone (440.0f, 0.0, 0.02);

    auto notes = segmenter.segment (shortTone);

    REQUIRE (notes.empty());
}

TEST_CASE ("NoteSegmenter keeps notes at minimum duration", "[NoteSegmenter]")
{
    NoteSegmenter segmenter;

    // Tone at exactly 60ms (above 50ms minimum)
    auto tone = createSteadyTone (440.0f, 0.0, 0.06);

    auto notes = segmenter.segment (tone);

    REQUIRE (notes.size() == 1);
}

// ============================================================
// Confidence threshold
// ============================================================
TEST_CASE ("NoteSegmenter ignores low confidence frames", "[NoteSegmenter]")
{
    NoteSegmenter segmenter;

    // Create tone with very low confidence (below default 0.3 threshold)
    auto curve = createSteadyTone (440.0f, 0.0, 0.5, 0.005, 0.1f);

    auto notes = segmenter.segment (curve);

    REQUIRE (notes.empty());
}

// ============================================================
// Note properties
// ============================================================
TEST_CASE ("NoteSegmenter assigns sequential IDs", "[NoteSegmenter]")
{
    NoteSegmenter segmenter;

    auto tone1 = createSteadyTone (440.0f, 0.0, 0.2);
    auto gap = createSilence (0.2, 0.1);
    auto tone2 = createSteadyTone (330.0f, 0.3, 0.2);

    auto curve = concat (concat (tone1, gap), tone2);

    auto notes = segmenter.segment (curve);

    REQUIRE (notes.size() == 2);
    REQUIRE (notes[0].id == 1);
    REQUIRE (notes[1].id == 2);
}

TEST_CASE ("NoteSegmenter note start time matches first voiced frame", "[NoteSegmenter]")
{
    NoteSegmenter segmenter;

    auto silence = createSilence (0.0, 0.1);
    auto tone = createSteadyTone (440.0f, 0.1, 0.3);

    auto curve = concat (silence, tone);

    auto notes = segmenter.segment (curve);

    REQUIRE (notes.size() == 1);
    REQUIRE_THAT (notes[0].startTime, WithinAbs (0.1, 0.01));
}

// ============================================================
// Gap bridging
// ============================================================
TEST_CASE ("NoteSegmenter bridges short gaps within a note", "[NoteSegmenter]")
{
    NoteSegmenter segmenter;

    // Note with a tiny 10ms unvoiced gap (below 30ms threshold)
    auto part1 = createSteadyTone (440.0f, 0.0, 0.15);
    auto gap = createSilence (0.15, 0.01);
    auto part2 = createSteadyTone (440.0f, 0.16, 0.15);

    auto curve = concat (concat (part1, gap), part2);

    auto notes = segmenter.segment (curve);

    // Should be ONE note (gap was bridged)
    REQUIRE (notes.size() == 1);
}

// ============================================================
// Settings customization
// ============================================================
TEST_CASE ("NoteSegmenter respects custom minimum duration", "[NoteSegmenter]")
{
    NoteSegmenter segmenter;
    NoteSegmenter::Settings settings;
    settings.minNoteDuration = 0.2; // 200ms minimum
    segmenter.setSettings (settings);

    // 100ms note — below custom minimum
    auto tone = createSteadyTone (440.0f, 0.0, 0.1);

    auto notes = segmenter.segment (tone);

    REQUIRE (notes.empty());
}

TEST_CASE ("NoteSegmenter MIDI note matches frequency", "[NoteSegmenter]")
{
    NoteSegmenter segmenter;

    auto curve = createSteadyTone (261.63f, 0.0, 0.5); // C4

    auto notes = segmenter.segment (curve);

    REQUIRE (notes.size() == 1);
    REQUIRE (notes[0].midiNote == 60); // C4
}
