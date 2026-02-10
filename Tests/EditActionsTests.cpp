#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "Editing/EditActions.h"

using Catch::Matchers::WithinAbs;

// Helper: create a note with given properties
static Note makeNote (int id, double startTime, double duration, float pitch)
{
    Note note;
    note.id = id;
    note.startTime = startTime;
    note.duration = duration;
    note.originalPitch = pitch;
    note.midiNote = Note::frequencyToMidi (pitch);
    note.confidence = 0.9f;
    return note;
}

// ============================================================
// NoteEditAction
// ============================================================
TEST_CASE ("NoteEditAction perform applies new state", "[EditActions]")
{
    NoteList notes = { makeNote (1, 0.0, 0.5, 440.0f) };

    NoteEditAction::EditState before;
    NoteEditAction::EditState after;
    after.pitchShiftSemitones = 5.0f;

    bool callbackCalled = false;
    NoteEditAction action (notes, 1, before, after, [&]() { callbackCalled = true; });

    action.perform();

    REQUIRE_THAT (notes[0].pitchShiftSemitones, WithinAbs (5.0f, 0.001f));
    REQUIRE (callbackCalled);
}

TEST_CASE ("NoteEditAction undo restores old state", "[EditActions]")
{
    NoteList notes = { makeNote (1, 0.0, 0.5, 440.0f) };

    NoteEditAction::EditState before;
    before.pitchShiftSemitones = 0.0f;

    NoteEditAction::EditState after;
    after.pitchShiftSemitones = 5.0f;

    NoteEditAction action (notes, 1, before, after, nullptr);

    action.perform();
    REQUIRE_THAT (notes[0].pitchShiftSemitones, WithinAbs (5.0f, 0.001f));

    action.undo();
    REQUIRE_THAT (notes[0].pitchShiftSemitones, WithinAbs (0.0f, 0.001f));
}

TEST_CASE ("NoteEditAction applies all edit fields", "[EditActions]")
{
    NoteList notes = { makeNote (1, 0.0, 0.5, 440.0f) };

    NoteEditAction::EditState before;
    NoteEditAction::EditState after;
    after.pitchShiftSemitones = 3.0f;
    after.formantShiftSemitones = -2.0f;
    after.timeOffset = 0.1;
    after.durationScale = 1.5;
    after.gainDB = -6.0f;
    after.pitchDriftScale = 0.5f;

    NoteEditAction action (notes, 1, before, after, nullptr);
    action.perform();

    REQUIRE_THAT (notes[0].pitchShiftSemitones, WithinAbs (3.0f, 0.001f));
    REQUIRE_THAT (notes[0].formantShiftSemitones, WithinAbs (-2.0f, 0.001f));
    REQUIRE_THAT (notes[0].timeOffset, WithinAbs (0.1, 0.001));
    REQUIRE_THAT (notes[0].durationScale, WithinAbs (1.5, 0.001));
    REQUIRE_THAT (notes[0].gainDB, WithinAbs (-6.0f, 0.001f));
    REQUIRE_THAT (notes[0].pitchDriftScale, WithinAbs (0.5f, 0.001f));
}

TEST_CASE ("NoteEditAction only modifies the targeted note", "[EditActions]")
{
    NoteList notes = {
        makeNote (1, 0.0, 0.3, 440.0f),
        makeNote (2, 0.5, 0.3, 330.0f)
    };

    NoteEditAction::EditState before;
    NoteEditAction::EditState after;
    after.pitchShiftSemitones = 7.0f;

    NoteEditAction action (notes, 1, before, after, nullptr);
    action.perform();

    REQUIRE_THAT (notes[0].pitchShiftSemitones, WithinAbs (7.0f, 0.001f));
    REQUIRE_THAT (notes[1].pitchShiftSemitones, WithinAbs (0.0f, 0.001f)); // Unchanged
}

// ============================================================
// BatchNoteEditAction
// ============================================================
TEST_CASE ("BatchNoteEditAction perform applies states to all notes", "[EditActions]")
{
    NoteList notes = {
        makeNote (1, 0.0, 0.3, 440.0f),
        makeNote (2, 0.5, 0.3, 330.0f)
    };

    std::vector<BatchNoteEditAction::NoteSnapshot> before = {
        { 1, {} },
        { 2, {} }
    };

    NoteEditAction::EditState state1;
    state1.pitchShiftSemitones = 2.0f;
    NoteEditAction::EditState state2;
    state2.pitchShiftSemitones = -3.0f;

    std::vector<BatchNoteEditAction::NoteSnapshot> after = {
        { 1, state1 },
        { 2, state2 }
    };

    bool callbackCalled = false;
    BatchNoteEditAction action (notes, before, after, [&]() { callbackCalled = true; });

    action.perform();

    REQUIRE_THAT (notes[0].pitchShiftSemitones, WithinAbs (2.0f, 0.001f));
    REQUIRE_THAT (notes[1].pitchShiftSemitones, WithinAbs (-3.0f, 0.001f));
    REQUIRE (callbackCalled);
}

TEST_CASE ("BatchNoteEditAction undo restores all original states", "[EditActions]")
{
    NoteList notes = {
        makeNote (1, 0.0, 0.3, 440.0f),
        makeNote (2, 0.5, 0.3, 330.0f)
    };

    NoteEditAction::EditState origState1;
    origState1.pitchShiftSemitones = 1.0f;
    NoteEditAction::EditState origState2;
    origState2.pitchShiftSemitones = -1.0f;

    notes[0].pitchShiftSemitones = 1.0f;
    notes[1].pitchShiftSemitones = -1.0f;

    std::vector<BatchNoteEditAction::NoteSnapshot> before = {
        { 1, origState1 },
        { 2, origState2 }
    };

    NoteEditAction::EditState newState1;
    newState1.pitchShiftSemitones = 5.0f;
    NoteEditAction::EditState newState2;
    newState2.pitchShiftSemitones = 5.0f;

    std::vector<BatchNoteEditAction::NoteSnapshot> after = {
        { 1, newState1 },
        { 2, newState2 }
    };

    BatchNoteEditAction action (notes, before, after, nullptr);

    action.perform();
    REQUIRE_THAT (notes[0].pitchShiftSemitones, WithinAbs (5.0f, 0.001f));
    REQUIRE_THAT (notes[1].pitchShiftSemitones, WithinAbs (5.0f, 0.001f));

    action.undo();
    REQUIRE_THAT (notes[0].pitchShiftSemitones, WithinAbs (1.0f, 0.001f));
    REQUIRE_THAT (notes[1].pitchShiftSemitones, WithinAbs (-1.0f, 0.001f));
}

// ============================================================
// NoteSplitAction
// ============================================================
TEST_CASE ("NoteSplitAction perform replaces note with two halves", "[EditActions]")
{
    NoteList notes = { makeNote (1, 0.0, 1.0, 440.0f) };

    Note firstHalf = makeNote (2, 0.0, 0.5, 440.0f);
    Note secondHalf = makeNote (3, 0.5, 0.5, 440.0f);

    bool callbackCalled = false;
    NoteSplitAction action (notes, notes[0], firstHalf, secondHalf,
                            [&]() { callbackCalled = true; });

    action.perform();

    REQUIRE (notes.size() == 2);
    REQUIRE (notes[0].id == 2);
    REQUIRE (notes[1].id == 3);
    REQUIRE_THAT (notes[0].startTime, WithinAbs (0.0, 0.001));
    REQUIRE_THAT (notes[1].startTime, WithinAbs (0.5, 0.001));
    REQUIRE (callbackCalled);
}

TEST_CASE ("NoteSplitAction undo restores original note", "[EditActions]")
{
    NoteList notes = { makeNote (1, 0.0, 1.0, 440.0f) };
    Note original = notes[0];

    Note firstHalf = makeNote (2, 0.0, 0.5, 440.0f);
    Note secondHalf = makeNote (3, 0.5, 0.5, 440.0f);

    NoteSplitAction action (notes, original, firstHalf, secondHalf, nullptr);

    action.perform();
    REQUIRE (notes.size() == 2);

    action.undo();
    REQUIRE (notes.size() == 1);
    REQUIRE (notes[0].id == 1);
    REQUIRE_THAT (notes[0].duration, WithinAbs (1.0, 0.001));
}

TEST_CASE ("NoteSplitAction keeps notes sorted by start time", "[EditActions]")
{
    NoteList notes = {
        makeNote (1, 0.0, 0.3, 440.0f),
        makeNote (2, 0.5, 1.0, 330.0f),
        makeNote (3, 2.0, 0.5, 220.0f)
    };

    Note original = notes[1]; // middle note
    Note firstHalf = makeNote (4, 0.5, 0.5, 330.0f);
    Note secondHalf = makeNote (5, 1.0, 0.5, 330.0f);

    NoteSplitAction action (notes, original, firstHalf, secondHalf, nullptr);
    action.perform();

    REQUIRE (notes.size() == 4);

    // Verify sorted order
    for (size_t i = 1; i < notes.size(); ++i)
        REQUIRE (notes[i].startTime >= notes[i - 1].startTime);
}

// ============================================================
// NoteMergeAction
// ============================================================
TEST_CASE ("NoteMergeAction perform replaces two notes with merged", "[EditActions]")
{
    NoteList notes = {
        makeNote (1, 0.0, 0.5, 440.0f),
        makeNote (2, 0.5, 0.5, 445.0f)
    };

    Note merged = makeNote (3, 0.0, 1.0, 442.5f);

    bool callbackCalled = false;
    NoteMergeAction action (notes, notes[0], notes[1], merged,
                            [&]() { callbackCalled = true; });

    action.perform();

    REQUIRE (notes.size() == 1);
    REQUIRE (notes[0].id == 3);
    REQUIRE_THAT (notes[0].duration, WithinAbs (1.0, 0.001));
    REQUIRE (callbackCalled);
}

TEST_CASE ("NoteMergeAction undo restores both original notes", "[EditActions]")
{
    Note first = makeNote (1, 0.0, 0.5, 440.0f);
    Note second = makeNote (2, 0.5, 0.5, 445.0f);
    NoteList notes = { first, second };

    Note merged = makeNote (3, 0.0, 1.0, 442.5f);

    NoteMergeAction action (notes, first, second, merged, nullptr);

    action.perform();
    REQUIRE (notes.size() == 1);

    action.undo();
    REQUIRE (notes.size() == 2);
    REQUIRE (notes[0].id == 1);
    REQUIRE (notes[1].id == 2);
}

// ============================================================
// getSizeInUnits
// ============================================================
TEST_CASE ("NoteEditAction getSizeInUnits returns 1", "[EditActions]")
{
    NoteList notes = { makeNote (1, 0.0, 0.5, 440.0f) };
    NoteEditAction action (notes, 1, {}, {}, nullptr);
    REQUIRE (action.getSizeInUnits() == 1);
}

TEST_CASE ("BatchNoteEditAction getSizeInUnits returns number of notes", "[EditActions]")
{
    NoteList notes = {
        makeNote (1, 0.0, 0.3, 440.0f),
        makeNote (2, 0.5, 0.3, 330.0f)
    };

    NoteEditAction::EditState s;
    std::vector<BatchNoteEditAction::NoteSnapshot> before = { { 1, s }, { 2, s } };
    std::vector<BatchNoteEditAction::NoteSnapshot> after = { { 1, s }, { 2, s } };

    BatchNoteEditAction action (notes, before, after, nullptr);
    REQUIRE (action.getSizeInUnits() == 2);
}

TEST_CASE ("NoteSplitAction getSizeInUnits returns 2", "[EditActions]")
{
    NoteList notes = { makeNote (1, 0.0, 1.0, 440.0f) };
    NoteSplitAction action (notes, notes[0], makeNote (2, 0.0, 0.5, 440.0f),
                            makeNote (3, 0.5, 0.5, 440.0f), nullptr);
    REQUIRE (action.getSizeInUnits() == 2);
}

TEST_CASE ("NoteMergeAction getSizeInUnits returns 2", "[EditActions]")
{
    NoteList notes = { makeNote (1, 0.0, 0.5, 440.0f), makeNote (2, 0.5, 0.5, 440.0f) };
    NoteMergeAction action (notes, notes[0], notes[1], makeNote (3, 0.0, 1.0, 440.0f), nullptr);
    REQUIRE (action.getSizeInUnits() == 2);
}
