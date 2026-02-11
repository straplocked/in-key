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
        bool isMuted = false;
        float transitionInDuration = 0.0f;
        float transitionInShape = 0.0f;
        PitchEnvelope pitchEnvelope;
        float fadeInMs = 0.0f;
        float fadeOutMs = 0.0f;
        float sibilantGainDB = 0.0f;
        float vibratoDepthScale = 1.0f;
        float driftScale = 1.0f;

        // Centralized helpers to avoid field-by-field copies everywhere
        static EditState fromNote (const Note& note)
        {
            EditState s;
            s.pitchShiftSemitones = note.pitchShiftSemitones;
            s.formantShiftSemitones = note.formantShiftSemitones;
            s.timeOffset = note.timeOffset;
            s.durationScale = note.durationScale;
            s.gainDB = note.gainDB;
            s.pitchDriftScale = note.pitchDriftScale;
            s.isMuted = note.isMuted;
            s.transitionInDuration = note.transitionInDuration;
            s.transitionInShape = note.transitionInShape;
            s.pitchEnvelope = note.pitchEnvelope;
            s.fadeInMs = note.fadeInMs;
            s.fadeOutMs = note.fadeOutMs;
            s.sibilantGainDB = note.sibilantGainDB;
            s.vibratoDepthScale = note.vibratoDepthScale;
            s.driftScale = note.driftScale;
            return s;
        }

        void applyToNote (Note& note) const
        {
            note.pitchShiftSemitones = pitchShiftSemitones;
            note.formantShiftSemitones = formantShiftSemitones;
            note.timeOffset = timeOffset;
            note.durationScale = durationScale;
            note.gainDB = gainDB;
            note.pitchDriftScale = pitchDriftScale;
            note.isMuted = isMuted;
            note.transitionInDuration = transitionInDuration;
            note.transitionInShape = transitionInShape;
            note.pitchEnvelope = pitchEnvelope;
            note.fadeInMs = fadeInMs;
            note.fadeOutMs = fadeOutMs;
            note.sibilantGainDB = sibilantGainDB;
            note.vibratoDepthScale = vibratoDepthScale;
            note.driftScale = driftScale;
        }
    };

    NoteEditAction (NoteList& noteList,
                    int noteId,
                    EditState before,
                    EditState after,
                    std::function<void()> onChange)
        : notes (noteList), id (noteId),
          oldState (std::move (before)), newState (std::move (after)),
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
                state.applyToNote (note);
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
                    snap.state.applyToNote (note);
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

// Action for deleting notes from the timeline
class NoteDeleteAction : public juce::UndoableAction
{
public:
    NoteDeleteAction (NoteList& noteList,
                      std::vector<Note> deletedNotes,
                      std::function<void()> onChange)
        : notes (noteList),
          removed (std::move (deletedNotes)),
          onChangeCallback (std::move (onChange))
    {
    }

    bool perform() override
    {
        for (const auto& r : removed)
            notes.erase (std::remove_if (notes.begin(), notes.end(),
                [&r] (const Note& n) { return n.id == r.id; }), notes.end());

        if (onChangeCallback)
            onChangeCallback();
        return true;
    }

    bool undo() override
    {
        for (const auto& r : removed)
            notes.push_back (r);

        std::sort (notes.begin(), notes.end(),
            [] (const Note& a, const Note& b) { return a.startTime < b.startTime; });

        if (onChangeCallback)
            onChangeCallback();
        return true;
    }

    int getSizeInUnits() override { return static_cast<int> (removed.size()); }

private:
    NoteList& notes;
    std::vector<Note> removed;
    std::function<void()> onChangeCallback;
};
