#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Data/NoteData.h"
#include "../Data/PitchData.h"
#include "../Editing/EditActions.h"

class PianoRollComponent : public juce::Component
{
public:
    PianoRollComponent();

    void setData (const NoteList& notes, const PitchCurve& pitchCurve, double audioDuration);
    void clear();

    // Get the current (possibly edited) notes
    const NoteList& getNotes() const { return notes; }

    // Reset view to show all notes
    void zoomToFit();

    // Edit a note's properties (records undo action)
    void editNoteProperty (int noteId, float pitchShift, float formantShift,
                           double timeOffset, double durationScale);

    // Quantize all notes' pitch to nearest semitone
    void quantizePitch();

    // Reset all edits on all notes
    void resetAllEdits();

    // Undo/redo
    juce::UndoManager& getUndoManager() { return undoManager; }
    bool canUndo() const { return undoManager.canUndo(); }
    bool canRedo() const { return undoManager.canRedo(); }

    void paint (juce::Graphics& g) override;
    void resized() override;

    // Mouse interaction
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseMove (const juce::MouseEvent& event) override;
    void mouseWheelMove (const juce::MouseEvent& event,
                         const juce::MouseWheelDetails& wheel) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    bool keyPressed (const juce::KeyPress& key) override;

    // Set playback position for playhead drawing (in seconds)
    void setPlayheadPosition (double timeSeconds);

    // Selected note
    int getSelectedNoteId() const { return selectedNoteId; }

    // Listener for note edits and selection
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void noteSelected (const Note* note) = 0;
        virtual void noteEdited (const Note& note) = 0;
    };

    void addListener (Listener* listener);
    void removeListener (Listener* listener);

private:
    NoteList notes;
    PitchCurve pitchCurve;
    double totalDuration = 0.0;

    // View state
    double viewStartTime = 0.0;
    double viewDuration = 5.0;
    int viewBottomMidi = 48;
    int viewTopMidi = 84;
    int selectedNoteId = -1;

    // Panning
    bool isPanning = false;
    juce::Point<float> panStartPos;
    double panStartTime = 0.0;
    int panStartBottomMidi = 0;

    // Drag modes
    enum class DragMode { None, MoveNote, ResizeLeft, ResizeRight };
    DragMode dragMode = DragMode::None;
    int dragNoteId = -1;
    float dragStartX = 0.0f;
    float dragStartY = 0.0f;
    // Snapshot of note state before drag began
    float dragOriginalPitchShift = 0.0f;
    double dragOriginalTimeOffset = 0.0;
    double dragOriginalDurationScale = 1.0;
    NoteEditAction::EditState dragBeforeState {};

    // Edge detection threshold in pixels
    static constexpr float edgeThreshold = 6.0f;

    // Undo manager
    juce::UndoManager undoManager { 500 };

    // Playhead
    double playheadTime = -1.0;

    // Coordinate conversion
    float timeToX (double time) const;
    double xToTime (float x) const;
    float midiToY (int midi) const;
    float midiToY (float midiFloat) const;
    int yToMidi (float y) const;
    float noteHeight() const;

    // Drawing
    void drawGrid (juce::Graphics& g);
    void drawPianoKeys (juce::Graphics& g);
    void drawNotes (juce::Graphics& g);
    void drawPitchCurve (juce::Graphics& g);
    void drawPlayhead (juce::Graphics& g);

    // Hit testing — uses edited positions
    Note* noteAtPoint (juce::Point<float> point);
    const Note* noteAtPoint (juce::Point<float> point) const;

    // Check which edge of a note the point is near
    DragMode edgeAtPoint (const Note& note, juce::Point<float> point) const;

    // Get note display rect (using edited positions)
    juce::Rectangle<float> getNoteRect (const Note& note) const;

    // Record undo action from drag
    void recordDragUndo (int noteId);

    // Layout
    static constexpr float pianoKeyWidth = 50.0f;
    static constexpr float headerHeight = 24.0f;

    juce::Rectangle<float> getGridArea() const;

    juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoRollComponent)
};
