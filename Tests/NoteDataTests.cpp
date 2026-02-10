#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "Data/NoteData.h"

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

// ============================================================
// frequencyToMidi
// ============================================================
TEST_CASE ("frequencyToMidi converts A4 (440 Hz) to MIDI 69", "[NoteData]")
{
    REQUIRE (Note::frequencyToMidi (440.0f) == 69);
}

TEST_CASE ("frequencyToMidi converts C4 (~261.63 Hz) to MIDI 60", "[NoteData]")
{
    REQUIRE (Note::frequencyToMidi (261.63f) == 60);
}

TEST_CASE ("frequencyToMidi converts C3 (~130.81 Hz) to MIDI 48", "[NoteData]")
{
    REQUIRE (Note::frequencyToMidi (130.81f) == 48);
}

TEST_CASE ("frequencyToMidi returns 0 for zero frequency", "[NoteData]")
{
    REQUIRE (Note::frequencyToMidi (0.0f) == 0);
}

TEST_CASE ("frequencyToMidi returns 0 for negative frequency", "[NoteData]")
{
    REQUIRE (Note::frequencyToMidi (-100.0f) == 0);
}

// ============================================================
// midiToFrequency
// ============================================================
TEST_CASE ("midiToFrequency converts MIDI 69 to 440 Hz", "[NoteData]")
{
    REQUIRE_THAT (Note::midiToFrequency (69), WithinAbs (440.0f, 0.01f));
}

TEST_CASE ("midiToFrequency converts MIDI 60 to ~261.63 Hz", "[NoteData]")
{
    REQUIRE_THAT (Note::midiToFrequency (60), WithinAbs (261.63f, 0.1f));
}

TEST_CASE ("midiToFrequency and frequencyToMidi are inverse operations", "[NoteData]")
{
    for (int midi = 24; midi <= 96; ++midi)
    {
        float freq = Note::midiToFrequency (midi);
        REQUIRE (Note::frequencyToMidi (freq) == midi);
    }
}

// ============================================================
// midiToNoteName
// ============================================================
TEST_CASE ("midiToNoteName returns correct note names", "[NoteData]")
{
    REQUIRE (std::string (Note::midiToNoteName (60)) == "C");
    REQUIRE (std::string (Note::midiToNoteName (61)) == "C#");
    REQUIRE (std::string (Note::midiToNoteName (62)) == "D");
    REQUIRE (std::string (Note::midiToNoteName (69)) == "A");
    REQUIRE (std::string (Note::midiToNoteName (72)) == "C");  // C5
}

// ============================================================
// midiToOctave
// ============================================================
TEST_CASE ("midiToOctave returns correct octaves", "[NoteData]")
{
    REQUIRE (Note::midiToOctave (60) == 4);   // C4
    REQUIRE (Note::midiToOctave (69) == 4);   // A4
    REQUIRE (Note::midiToOctave (72) == 5);   // C5
    REQUIRE (Note::midiToOctave (48) == 3);   // C3
    REQUIRE (Note::midiToOctave (36) == 2);   // C2
}

// ============================================================
// isEdited / resetEdits
// ============================================================
TEST_CASE ("Default note is not edited", "[NoteData]")
{
    Note note;
    REQUIRE_FALSE (note.isEdited());
}

TEST_CASE ("Note with pitch shift is edited", "[NoteData]")
{
    Note note;
    note.pitchShiftSemitones = 2.0f;
    REQUIRE (note.isEdited());
}

TEST_CASE ("Note with formant shift is edited", "[NoteData]")
{
    Note note;
    note.formantShiftSemitones = -3.0f;
    REQUIRE (note.isEdited());
}

TEST_CASE ("Note with time offset is edited", "[NoteData]")
{
    Note note;
    note.timeOffset = 0.5;
    REQUIRE (note.isEdited());
}

TEST_CASE ("Note with duration scale is edited", "[NoteData]")
{
    Note note;
    note.durationScale = 1.5;
    REQUIRE (note.isEdited());
}

TEST_CASE ("Note with gain change is edited", "[NoteData]")
{
    Note note;
    note.gainDB = -6.0f;
    REQUIRE (note.isEdited());
}

TEST_CASE ("Note with drift scale change is edited", "[NoteData]")
{
    Note note;
    note.pitchDriftScale = 0.5f;
    REQUIRE (note.isEdited());
}

TEST_CASE ("resetEdits clears all edit fields", "[NoteData]")
{
    Note note;
    note.pitchShiftSemitones = 5.0f;
    note.formantShiftSemitones = -2.0f;
    note.timeOffset = 0.3;
    note.durationScale = 2.0;
    note.gainDB = -12.0f;
    note.pitchDriftScale = 0.0f;

    REQUIRE (note.isEdited());
    note.resetEdits();
    REQUIRE_FALSE (note.isEdited());
}

// ============================================================
// Computed helpers
// ============================================================
TEST_CASE ("editedPitch applies pitch shift correctly", "[NoteData]")
{
    Note note;
    note.originalPitch = 440.0f;
    note.pitchShiftSemitones = 12.0f; // One octave up

    REQUIRE_THAT (note.editedPitch(), WithinAbs (880.0f, 0.1f));
}

TEST_CASE ("editedPitch with zero shift returns original", "[NoteData]")
{
    Note note;
    note.originalPitch = 440.0f;

    REQUIRE_THAT (note.editedPitch(), WithinAbs (440.0f, 0.01f));
}

TEST_CASE ("editedStartTime applies time offset", "[NoteData]")
{
    Note note;
    note.startTime = 1.0;
    note.timeOffset = 0.5;

    REQUIRE_THAT (note.editedStartTime(), WithinAbs (1.5, 0.001));
}

TEST_CASE ("editedDuration applies duration scale", "[NoteData]")
{
    Note note;
    note.duration = 0.5;
    note.durationScale = 2.0;

    REQUIRE_THAT (note.editedDuration(), WithinAbs (1.0, 0.001));
}

TEST_CASE ("endTime returns startTime + duration", "[NoteData]")
{
    Note note;
    note.startTime = 1.0;
    note.duration = 0.5;

    REQUIRE_THAT (note.endTime(), WithinAbs (1.5, 0.001));
}

// ============================================================
// Very small edits below threshold are NOT considered edited
// ============================================================
TEST_CASE ("Very small pitch shift below threshold is not considered edited", "[NoteData]")
{
    Note note;
    note.pitchShiftSemitones = 0.005f; // Below 0.01 threshold
    REQUIRE_FALSE (note.isEdited());
}
