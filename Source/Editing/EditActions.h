#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Data/NoteData.h"
#include <functional>

// Stores before/after edit state for a single note.
class NoteEditAction : public juce::UndoableAction
{
public:
    struct EditState
    {
        float pitchShiftSemitones = 0.0f;
        float formantShiftSemitones = 0.0f;
        double timeOffset = 0.0;
        double durationScale = 1.0;
        float gainDB = 0.0f;
        float pitchDriftScale = 1.0f;
    };

    NoteEditAction (NoteList& noteList,
                    int noteId,
                    EditState before,
                    EditState after,
                    std::function<void()> onChange)
        : notes (noteList), id (noteId),
          oldState (before), newState (after),
          onChangeCallback (std::move (onChange))
    {
    }

    bool perform() override
    {
        applyState (newState);
        return true;
    }

    bool undo() override
    {
        applyState (oldState);
        return true;
    }

    int getSizeInUnits() override { return 1; }

private:
    void applyState (const EditState& state)
    {
        for (auto& note : notes)
        {
            if (note.id == id)
            {
                note.pitchShiftSemitones = state.pitchShiftSemitones;
                note.formantShiftSemitones = state.formantShiftSemitones;
                note.timeOffset = state.timeOffset;
                note.durationScale = state.durationScale;
                note.gainDB = state.gainDB;
                note.pitchDriftScale = state.pitchDriftScale;
                break;
            }
        }

        if (onChangeCallback)
            onChangeCallback();
    }

    NoteList& notes;
    int id;
    EditState oldState;
    EditState newState;
    std::function<void()> onChangeCallback;
};

// Batch action for quantizing/resetting multiple notes at once
class BatchNoteEditAction : public juce::UndoableAction
{
public:
    struct NoteSnapshot
    {
        int id;
        NoteEditAction::EditState state;
    };

    BatchNoteEditAction (NoteList& noteList,
                         std::vector<NoteSnapshot> before,
                         std::vector<NoteSnapshot> after,
                         std::function<void()> onChange)
        : notes (noteList),
          oldStates (std::move (before)),
          newStates (std::move (after)),
          onChangeCallback (std::move (onChange))
    {
    }

    bool perform() override
    {
        applyStates (newStates);
        return true;
    }

    bool undo() override
    {
        applyStates (oldStates);
        return true;
    }

    int getSizeInUnits() override { return static_cast<int> (newStates.size()); }

private:
    void applyStates (const std::vector<NoteSnapshot>& states)
    {
        for (const auto& snap : states)
        {
            for (auto& note : notes)
            {
                if (note.id == snap.id)
                {
                    note.pitchShiftSemitones = snap.state.pitchShiftSemitones;
                    note.formantShiftSemitones = snap.state.formantShiftSemitones;
                    note.timeOffset = snap.state.timeOffset;
                    note.durationScale = snap.state.durationScale;
                    note.gainDB = snap.state.gainDB;
                    note.pitchDriftScale = snap.state.pitchDriftScale;
                    break;
                }
            }
        }

        if (onChangeCallback)
            onChangeCallback();
    }

    NoteList& notes;
    std::vector<NoteSnapshot> oldStates;
    std::vector<NoteSnapshot> newStates;
    std::function<void()> onChangeCallback;
};

// Action for splitting a note into two
class NoteSplitAction : public juce::UndoableAction
{
public:
    NoteSplitAction (NoteList& noteList,
                     Note original,
                     Note firstHalf,
                     Note secondHalf,
                     std::function<void()> onChange)
        : notes (noteList),
          originalNote (std::move (original)),
          firstHalfNote (std::move (firstHalf)),
          secondHalfNote (std::move (secondHalf)),
          onChangeCallback (std::move (onChange))
    {
    }

    bool perform() override
    {
        // Remove the original note
        notes.erase (std::remove_if (notes.begin(), notes.end(),
            [this] (const Note& n) { return n.id == originalNote.id; }),
            notes.end());

        // Insert the two halves
        notes.push_back (firstHalfNote);
        notes.push_back (secondHalfNote);

        // Sort by start time
        std::sort (notes.begin(), notes.end(),
            [] (const Note& a, const Note& b) { return a.startTime < b.startTime; });

        if (onChangeCallback)
            onChangeCallback();
        return true;
    }

    bool undo() override
    {
        // Remove the two halves
        notes.erase (std::remove_if (notes.begin(), notes.end(),
            [this] (const Note& n) {
                return n.id == firstHalfNote.id || n.id == secondHalfNote.id;
            }),
            notes.end());

        // Restore the original
        notes.push_back (originalNote);

        std::sort (notes.begin(), notes.end(),
            [] (const Note& a, const Note& b) { return a.startTime < b.startTime; });

        if (onChangeCallback)
            onChangeCallback();
        return true;
    }

    int getSizeInUnits() override { return 2; }

private:
    NoteList& notes;
    Note originalNote;
    Note firstHalfNote;
    Note secondHalfNote;
    std::function<void()> onChangeCallback;
};

// Action for merging two adjacent notes into one
class NoteMergeAction : public juce::UndoableAction
{
public:
    NoteMergeAction (NoteList& noteList,
                     Note first,
                     Note second,
                     Note merged,
                     std::function<void()> onChange)
        : notes (noteList),
          firstOriginal (std::move (first)),
          secondOriginal (std::move (second)),
          mergedNote (std::move (merged)),
          onChangeCallback (std::move (onChange))
    {
    }

    bool perform() override
    {
        // Remove both original notes
        notes.erase (std::remove_if (notes.begin(), notes.end(),
            [this] (const Note& n) {
                return n.id == firstOriginal.id || n.id == secondOriginal.id;
            }),
            notes.end());

        // Insert merged note
        notes.push_back (mergedNote);

        std::sort (notes.begin(), notes.end(),
            [] (const Note& a, const Note& b) { return a.startTime < b.startTime; });

        if (onChangeCallback)
            onChangeCallback();
        return true;
    }

    bool undo() override
    {
        // Remove merged note
        notes.erase (std::remove_if (notes.begin(), notes.end(),
            [this] (const Note& n) { return n.id == mergedNote.id; }),
            notes.end());

        // Restore both originals
        notes.push_back (firstOriginal);
        notes.push_back (secondOriginal);

        std::sort (notes.begin(), notes.end(),
            [] (const Note& a, const Note& b) { return a.startTime < b.startTime; });

        if (onChangeCallback)
            onChangeCallback();
        return true;
    }

    int getSizeInUnits() override { return 2; }

private:
    NoteList& notes;
    Note firstOriginal;
    Note secondOriginal;
    Note mergedNote;
    std::function<void()> onChangeCallback;
};
