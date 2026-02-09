#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Data/NoteData.h"
#include <functional>

// Stores before/after edit state for a single note.
// The callback is used to apply changes and trigger repaints.
class NoteEditAction : public juce::UndoableAction
{
public:
    struct EditState
    {
        float pitchShiftSemitones = 0.0f;
        float formantShiftSemitones = 0.0f;
        double timeOffset = 0.0;
        double durationScale = 1.0;
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

// Batch action for quantizing all notes at once
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
