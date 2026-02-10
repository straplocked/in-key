#include "PianoRollComponent.h"
#include "InKeyLookAndFeel.h"
#include <cmath>
#include <algorithm>

PianoRollComponent::PianoRollComponent()
{
    setWantsKeyboardFocus (true);
}

void PianoRollComponent::setData (const NoteList& newNotes, const PitchCurve& newPitchCurve,
                                   double audioDuration)
{
    notes = newNotes;
    pitchCurve = newPitchCurve;
    totalDuration = audioDuration;
    selectedNoteIds.clear();
    playheadTime = -1.0;
    undoManager.clearUndoHistory();

    // Track next note ID for split/merge
    nextNoteId = 1;
    for (const auto& note : notes)
    {
        if (note.id >= nextNoteId)
            nextNoteId = note.id + 1;
    }

    // Auto-fit view to the data
    viewStartTime = 0.0;
    viewDuration = juce::jmax (1.0, totalDuration);

    if (! notes.empty())
    {
        int minMidi = 127;
        int maxMidi = 0;
        for (const auto& note : notes)
        {
            if (note.midiNote < minMidi) minMidi = note.midiNote;
            if (note.midiNote > maxMidi) maxMidi = note.midiNote;
        }

        int padding = 6;
        viewBottomMidi = juce::jmax (0, minMidi - padding);
        viewTopMidi = juce::jmin (127, maxMidi + padding);

        if (viewTopMidi - viewBottomMidi < 24)
        {
            int center = (viewTopMidi + viewBottomMidi) / 2;
            viewBottomMidi = juce::jmax (0, center - 12);
            viewTopMidi = juce::jmin (127, center + 12);
        }
    }

    repaint();
}

void PianoRollComponent::clear()
{
    notes.clear();
    pitchCurve.clear();
    totalDuration = 0.0;
    selectedNoteIds.clear();
    playheadTime = -1.0;
    undoManager.clearUndoHistory();
    repaint();
}

void PianoRollComponent::setPlayheadPosition (double timeSeconds)
{
    playheadTime = timeSeconds;

    // Auto-scroll to keep playhead visible during playback
    if (playheadTime >= 0.0)
    {
        double scrollThreshold = viewStartTime + viewDuration * 0.8;
        if (playheadTime > scrollThreshold)
            viewStartTime = playheadTime - viewDuration * 0.2;
        else if (playheadTime < viewStartTime)
            viewStartTime = juce::jmax (0.0, playheadTime);
    }

    repaint();
}

void PianoRollComponent::zoomToFit()
{
    if (totalDuration <= 0.0)
        return;

    viewStartTime = 0.0;
    viewDuration = juce::jmax (1.0, totalDuration);

    if (! notes.empty())
    {
        int minMidi = 127;
        int maxMidi = 0;
        for (const auto& note : notes)
        {
            if (note.midiNote < minMidi) minMidi = note.midiNote;
            if (note.midiNote > maxMidi) maxMidi = note.midiNote;
        }

        int padding = 6;
        viewBottomMidi = juce::jmax (0, minMidi - padding);
        viewTopMidi = juce::jmin (127, maxMidi + padding);

        if (viewTopMidi - viewBottomMidi < 24)
        {
            int center = (viewTopMidi + viewBottomMidi) / 2;
            viewBottomMidi = juce::jmax (0, center - 12);
            viewTopMidi = juce::jmin (127, center + 12);
        }
    }

    repaint();
}

void PianoRollComponent::editNoteProperty (int noteId, float pitchShift, float formantShift,
                                            double timeOffset, double durationScale,
                                            float gainDB, float pitchDriftScale)
{
    for (auto& note : notes)
    {
        if (note.id == noteId)
        {
            NoteEditAction::EditState before;
            before.pitchShiftSemitones = note.pitchShiftSemitones;
            before.formantShiftSemitones = note.formantShiftSemitones;
            before.timeOffset = note.timeOffset;
            before.durationScale = note.durationScale;
            before.gainDB = note.gainDB;
            before.pitchDriftScale = note.pitchDriftScale;

            NoteEditAction::EditState after;
            after.pitchShiftSemitones = pitchShift;
            after.formantShiftSemitones = formantShift;
            after.timeOffset = timeOffset;
            after.durationScale = durationScale;
            after.gainDB = gainDB;
            after.pitchDriftScale = pitchDriftScale;

            undoManager.perform (
                new NoteEditAction (notes, noteId, before, after,
                                     [this]() { repaint(); }),
                "Edit Note");

            listeners.call ([&note] (Listener& l) { l.noteEdited (note); });
            listeners.call ([] (Listener& l) { l.notesChanged(); });
            return;
        }
    }
}

void PianoRollComponent::quantizePitch()
{
    // Default: chromatic, full strength
    auto chromatic = QuantizationEngine::chromaticScale (0);
    quantizePitchToScale (1.0f, chromatic);
}

void PianoRollComponent::quantizePitchToScale (float strength, const ScaleDefinition& scale)
{
    if (notes.empty() || strength <= 0.0f)
        return;

    std::vector<BatchNoteEditAction::NoteSnapshot> beforeStates;
    std::vector<BatchNoteEditAction::NoteSnapshot> afterStates;

    for (const auto& note : notes)
    {
        // If multi-select is active, only quantize selected notes
        if (! selectedNoteIds.empty() && selectedNoteIds.count (note.id) == 0)
            continue;

        BatchNoteEditAction::NoteSnapshot before;
        before.id = note.id;
        before.state.pitchShiftSemitones = note.pitchShiftSemitones;
        before.state.formantShiftSemitones = note.formantShiftSemitones;
        before.state.timeOffset = note.timeOffset;
        before.state.durationScale = note.durationScale;
        before.state.gainDB = note.gainDB;
        before.state.pitchDriftScale = note.pitchDriftScale;
        beforeStates.push_back (before);

        // Calculate correction using the QuantizationEngine
        float correction = QuantizationEngine::calculateCorrection (
            note.originalPitch, strength, scale);

        BatchNoteEditAction::NoteSnapshot after = before;
        after.state.pitchShiftSemitones = correction;
        afterStates.push_back (after);
    }

    if (! beforeStates.empty())
    {
        undoManager.perform (
            new BatchNoteEditAction (notes, beforeStates, afterStates,
                                      [this]() { repaint(); }),
            "Quantize Pitch");
        listeners.call ([] (Listener& l) { l.notesChanged(); });
    }
}

void PianoRollComponent::resetAllEdits()
{
    if (notes.empty())
        return;

    std::vector<BatchNoteEditAction::NoteSnapshot> beforeStates;
    std::vector<BatchNoteEditAction::NoteSnapshot> afterStates;

    for (const auto& note : notes)
    {
        BatchNoteEditAction::NoteSnapshot before;
        before.id = note.id;
        before.state.pitchShiftSemitones = note.pitchShiftSemitones;
        before.state.formantShiftSemitones = note.formantShiftSemitones;
        before.state.timeOffset = note.timeOffset;
        before.state.durationScale = note.durationScale;
        before.state.gainDB = note.gainDB;
        before.state.pitchDriftScale = note.pitchDriftScale;
        beforeStates.push_back (before);

        BatchNoteEditAction::NoteSnapshot after;
        after.id = note.id;
        // EditState defaults are all zeros/1.0 which is what we want
        afterStates.push_back (after);
    }

    undoManager.perform (
        new BatchNoteEditAction (notes, beforeStates, afterStates,
                                  [this]() { repaint(); }),
        "Reset All Edits");
    listeners.call ([] (Listener& l) { l.notesChanged(); });
}

// --- Selection ---

int PianoRollComponent::getSelectedNoteId() const
{
    if (selectedNoteIds.empty())
        return -1;
    return *selectedNoteIds.begin();
}

void PianoRollComponent::selectAllNotes()
{
    selectedNoteIds.clear();
    for (const auto& note : notes)
        selectedNoteIds.insert (note.id);
    notifySelectionChanged();
    repaint();
}

void PianoRollComponent::deselectAll()
{
    selectedNoteIds.clear();
    notifySelectionChanged();
    repaint();
}

void PianoRollComponent::notifySelectionChanged()
{
    const Note* firstNote = getFirstSelectedNote();
    listeners.call ([firstNote] (Listener& l) { l.noteSelected (firstNote); });
    listeners.call ([this] (Listener& l) { l.selectionChanged (selectedNoteIds); });
}

const Note* PianoRollComponent::getFirstSelectedNote() const
{
    if (selectedNoteIds.empty())
        return nullptr;

    int firstId = *selectedNoteIds.begin();
    for (const auto& note : notes)
    {
        if (note.id == firstId)
            return &note;
    }
    return nullptr;
}

// --- Paint ---

void PianoRollComponent::paint (juce::Graphics& g)
{
    // Gradient background
    juce::ColourGradient bgGradient (
        InKeyColours::backgroundTop.brighter (0.05f), 0.0f, 0.0f,
        InKeyColours::backgroundTop,                  0.0f, static_cast<float> (getHeight()),
        false);
    g.setGradientFill (bgGradient);
    g.fillAll();

    if (totalDuration <= 0.0)
    {
        g.setColour (InKeyColours::textDim);
        g.setFont (juce::Font (14.0f));
        g.drawText ("No audio analyzed", getLocalBounds().toFloat(), juce::Justification::centred);
        return;
    }

    drawGrid (g);
    drawPianoKeys (g);
    drawNotes (g);
    drawLasso (g);
    drawPlayhead (g);

    // Time ruler at top
    auto gridArea = getGridArea();
    g.setColour (InKeyColours::pianoKeySidebar);
    g.fillRect (gridArea.getX(), 0.0f, gridArea.getWidth(), headerHeight);

    g.setColour (InKeyColours::textSecondary);
    g.setFont (juce::Font (10.0f));

    double timeStep = viewDuration / 10.0;
    if (timeStep > 1.0) timeStep = std::floor (timeStep);
    else if (timeStep > 0.5) timeStep = 0.5;
    else if (timeStep > 0.25) timeStep = 0.25;
    else timeStep = 0.1;

    double t = std::ceil (viewStartTime / timeStep) * timeStep;
    while (t < viewStartTime + viewDuration)
    {
        float x = timeToX (t);
        g.drawText (juce::String (t, 1) + "s",
                    juce::Rectangle<float> (x - 20.0f, 2.0f, 40.0f, headerHeight - 4.0f),
                    juce::Justification::centred);
        t += timeStep;
    }
}

void PianoRollComponent::resized()
{
}

// --- Mouse handling ---

void PianoRollComponent::mouseDown (const juce::MouseEvent& event)
{
    auto gridArea = getGridArea();

    // Panning: middle-click or Alt+click
    if (event.mods.isMiddleButtonDown() || event.mods.isAltDown())
    {
        isPanning = true;
        panStartPos = event.position;
        panStartTime = viewStartTime;
        panStartBottomMidi = viewBottomMidi;
        return;
    }

    // Right-click context menu
    if (event.mods.isPopupMenu())
    {
        auto* rightClickNote = noteAtPoint (event.position);

        // If right-clicking on a note that isn't selected, select it
        if (rightClickNote != nullptr && ! selectedNoteIds.count (rightClickNote->id))
        {
            selectedNoteIds.clear();
            selectedNoteIds.insert (rightClickNote->id);
            notifySelectionChanged();
            repaint();
        }

        juce::PopupMenu menu;
        menu.addItem (1, "Select All", true);
        menu.addSeparator();
        menu.addItem (2, "Quantize Selected", ! selectedNoteIds.empty());
        menu.addItem (3, "Reset Edits", ! selectedNoteIds.empty());
        menu.addSeparator();

        bool canSplit = (rightClickNote != nullptr && selectedNoteIds.size() == 1);
        menu.addItem (4, "Split Note Here", canSplit);

        // Check if selected notes are adjacent for merge
        bool canMerge = false;
        if (selectedNoteIds.size() >= 2)
        {
            std::vector<const Note*> sel;
            for (const auto& n : notes)
                if (selectedNoteIds.count (n.id))
                    sel.push_back (&n);

            std::sort (sel.begin(), sel.end(),
                [] (const Note* a, const Note* b) { return a->startTime < b->startTime; });

            canMerge = true;
            for (size_t i = 1; i < sel.size(); ++i)
            {
                if (sel[i]->startTime - sel[i - 1]->endTime() > 0.05)
                {
                    canMerge = false;
                    break;
                }
            }
        }
        menu.addItem (5, "Merge Notes", canMerge);

        double clickTime = xToTime (event.position.x);

        menu.showMenuAsync (juce::PopupMenu::Options(),
            [this, rightClickNote, clickTime] (int result)
            {
                switch (result)
                {
                    case 1: selectAllNotes(); break;
                    case 2: quantizePitch(); repaint(); break;
                    case 3:
                    {
                        // Reset edits on selected notes
                        std::vector<BatchNoteEditAction::NoteSnapshot> befores, afters;
                        for (const auto& note : notes)
                        {
                            if (selectedNoteIds.count (note.id) && note.isEdited())
                            {
                                BatchNoteEditAction::NoteSnapshot b;
                                b.id = note.id;
                                b.state.pitchShiftSemitones = note.pitchShiftSemitones;
                                b.state.formantShiftSemitones = note.formantShiftSemitones;
                                b.state.timeOffset = note.timeOffset;
                                b.state.durationScale = note.durationScale;
                                b.state.gainDB = note.gainDB;
                                b.state.pitchDriftScale = note.pitchDriftScale;
                                befores.push_back (b);

                                BatchNoteEditAction::NoteSnapshot a;
                                a.id = note.id;
                                afters.push_back (a);
                            }
                        }
                        if (! befores.empty())
                        {
                            undoManager.perform (
                                new BatchNoteEditAction (notes, befores, afters,
                                    [this]() { repaint(); }),
                                "Reset Selected");
                            repaint();
                            notifySelectionChanged();
                        }
                        break;
                    }
                    case 4:
                        if (rightClickNote != nullptr)
                            splitNoteAtTime (rightClickNote->id, clickTime);
                        break;
                    case 5:
                        mergeSelectedNotes();
                        break;
                    default: break;
                }
            });
        return;
    }

    if (gridArea.contains (event.position))
    {
        auto* clickedNote = noteAtPoint (event.position);

        if (clickedNote != nullptr)
        {
            bool isShiftOrCmd = event.mods.isShiftDown() || event.mods.isCommandDown();

            if (isShiftOrCmd)
            {
                // Toggle this note in/out of selection
                if (selectedNoteIds.count (clickedNote->id))
                    selectedNoteIds.erase (clickedNote->id);
                else
                    selectedNoteIds.insert (clickedNote->id);
            }
            else
            {
                // If clicking an already-selected note, keep the multi-selection for dragging
                if (! selectedNoteIds.count (clickedNote->id))
                {
                    selectedNoteIds.clear();
                    selectedNoteIds.insert (clickedNote->id);
                }
            }

            notifySelectionChanged();

            // Set up drag
            dragMode = edgeAtPoint (*clickedNote, event.position);
            dragNoteId = clickedNote->id;
            dragStartX = event.position.x;
            dragStartY = event.position.y;
            dragOriginalPitchShift = clickedNote->pitchShiftSemitones;
            dragOriginalTimeOffset = clickedNote->timeOffset;
            dragOriginalDurationScale = clickedNote->durationScale;

            // Store state before drag for undo (single note for resize)
            dragBeforeState.pitchShiftSemitones = clickedNote->pitchShiftSemitones;
            dragBeforeState.formantShiftSemitones = clickedNote->formantShiftSemitones;
            dragBeforeState.timeOffset = clickedNote->timeOffset;
            dragBeforeState.durationScale = clickedNote->durationScale;
            dragBeforeState.gainDB = clickedNote->gainDB;
            dragBeforeState.pitchDriftScale = clickedNote->pitchDriftScale;

            // Store all selected notes' state for multi-drag
            if (dragMode == DragMode::MoveNote && selectedNoteIds.size() > 1)
            {
                multiDragBeforeStates.clear();
                for (const auto& note : notes)
                {
                    if (selectedNoteIds.count (note.id))
                    {
                        NoteEditAction::EditState state;
                        state.pitchShiftSemitones = note.pitchShiftSemitones;
                        state.formantShiftSemitones = note.formantShiftSemitones;
                        state.timeOffset = note.timeOffset;
                        state.durationScale = note.durationScale;
                        state.gainDB = note.gainDB;
                        state.pitchDriftScale = note.pitchDriftScale;
                        multiDragBeforeStates[note.id] = state;
                    }
                }
            }
        }
        else
        {
            // Clicked empty space — start lasso or deselect
            bool isShiftOrCmd = event.mods.isShiftDown() || event.mods.isCommandDown();

            if (! isShiftOrCmd)
                selectedNoteIds.clear();

            isLassoing = true;
            lassoStart = event.position;
            lassoRect = {};
            preLassoSelection = selectedNoteIds;

            dragMode = DragMode::None;
            notifySelectionChanged();
        }
        repaint();
    }
}

void PianoRollComponent::mouseUp (const juce::MouseEvent& /*event*/)
{
    if (isLassoing)
    {
        isLassoing = false;
        lassoRect = {};
        repaint();
    }

    if (dragMode != DragMode::None && dragNoteId >= 0)
    {
        if (dragMode == DragMode::MoveNote && selectedNoteIds.size() > 1)
        {
            // Record multi-drag undo
            recordMultiDragUndo();
        }
        else
        {
            // Record single-note undo
            recordDragUndo (dragNoteId);

            for (auto& note : notes)
            {
                if (note.id == dragNoteId)
                {
                    listeners.call ([&note] (Listener& l) { l.noteEdited (note); });
                    break;
                }
            }
        }

        // Notify that notes have changed (triggers re-render for DAW sync)
        listeners.call ([] (Listener& l) { l.notesChanged(); });
    }

    dragMode = DragMode::None;
    dragNoteId = -1;
    isPanning = false;
    multiDragBeforeStates.clear();
}

void PianoRollComponent::mouseMove (const juce::MouseEvent& event)
{
    auto* hoveredNote = noteAtPoint (event.position);
    if (hoveredNote != nullptr)
    {
        auto edge = edgeAtPoint (*hoveredNote, event.position);
        if (edge == DragMode::ResizeLeft || edge == DragMode::ResizeRight)
            setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
        else
            setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    }
    else
    {
        setMouseCursor (juce::MouseCursor::NormalCursor);
    }
}

void PianoRollComponent::mouseDrag (const juce::MouseEvent& event)
{
    if (isPanning)
    {
        auto gridArea = getGridArea();
        float dx = event.position.x - panStartPos.x;
        float dy = event.position.y - panStartPos.y;

        double timeDelta = -(static_cast<double> (dx) / gridArea.getWidth()) * viewDuration;
        viewStartTime = juce::jmax (0.0, panStartTime + timeDelta);

        int midiRange = viewTopMidi - viewBottomMidi;
        int midiDelta = static_cast<int> (dy / noteHeight());
        viewBottomMidi = juce::jlimit (0, 127 - midiRange, panStartBottomMidi + midiDelta);
        viewTopMidi = viewBottomMidi + midiRange;

        repaint();
        return;
    }

    // Lasso selection
    if (isLassoing)
    {
        float x1 = juce::jmin (lassoStart.x, event.position.x);
        float y1 = juce::jmin (lassoStart.y, event.position.y);
        float x2 = juce::jmax (lassoStart.x, event.position.x);
        float y2 = juce::jmax (lassoStart.y, event.position.y);
        lassoRect = { x1, y1, x2 - x1, y2 - y1 };

        // Find all notes intersecting the lasso
        selectedNoteIds = preLassoSelection;
        for (const auto& note : notes)
        {
            auto noteRect = getNoteRect (note);
            if (lassoRect.intersects (noteRect))
                selectedNoteIds.insert (note.id);
        }

        notifySelectionChanged();
        repaint();
        return;
    }

    if (dragMode == DragMode::None || dragNoteId < 0)
        return;

    float dy = event.position.y - dragStartY;
    float dx = event.position.x - dragStartX;

    // Multi-note move
    if (dragMode == DragMode::MoveNote && selectedNoteIds.size() > 1)
    {
        float semitonesPerPixel = static_cast<float> (viewTopMidi - viewBottomMidi) / getGridArea().getHeight();
        float semitonesDelta = -dy * semitonesPerPixel;
        double timeDelta = static_cast<double> (dx) / getGridArea().getWidth() * viewDuration;

        // Fine pitch: Shift held = cent-level, otherwise snap to semitones
        if (! event.mods.isShiftDown())
            semitonesDelta = std::round (semitonesDelta);
        else
            semitonesDelta = std::round (semitonesDelta * 100.0f) / 100.0f;

        for (auto& note : notes)
        {
            auto it = multiDragBeforeStates.find (note.id);
            if (it == multiDragBeforeStates.end())
                continue;

            float newPitchShift = it->second.pitchShiftSemitones + semitonesDelta;
            newPitchShift = juce::jlimit (-24.0f, 24.0f, newPitchShift);
            note.pitchShiftSemitones = newPitchShift;

            double newTimeOffset = it->second.timeOffset + timeDelta;
            newTimeOffset = juce::jmax (-note.startTime, newTimeOffset);
            note.timeOffset = newTimeOffset;
        }

        repaint();
        return;
    }

    // Single-note drag (or single-note resize)
    for (auto& note : notes)
    {
        if (note.id != dragNoteId)
            continue;

        if (dragMode == DragMode::MoveNote)
        {
            // Vertical: pitch shift
            float semitonesPerPixel = static_cast<float> (viewTopMidi - viewBottomMidi) / getGridArea().getHeight();
            float semitonesDelta = -dy * semitonesPerPixel;

            // Fine pitch: Shift held = cent-level, otherwise snap to semitones
            if (! event.mods.isShiftDown())
                semitonesDelta = std::round (semitonesDelta);
            else
                semitonesDelta = std::round (semitonesDelta * 100.0f) / 100.0f;

            float newPitchShift = dragOriginalPitchShift + semitonesDelta;
            newPitchShift = juce::jlimit (-24.0f, 24.0f, newPitchShift);
            note.pitchShiftSemitones = newPitchShift;

            // Horizontal: time offset
            double timeDelta = static_cast<double> (dx) / getGridArea().getWidth() * viewDuration;
            double newTimeOffset = dragOriginalTimeOffset + timeDelta;
            newTimeOffset = juce::jmax (-note.startTime, newTimeOffset);
            note.timeOffset = newTimeOffset;
        }
        else if (dragMode == DragMode::ResizeLeft)
        {
            double timeDelta = static_cast<double> (dx) / getGridArea().getWidth() * viewDuration;
            double newTimeOffset = dragOriginalTimeOffset + timeDelta;
            newTimeOffset = juce::jmax (-note.startTime, newTimeOffset);

            double origEditedEnd = note.startTime + dragOriginalTimeOffset + note.duration * dragOriginalDurationScale;
            double newEditedStart = note.startTime + newTimeOffset;
            double newEditedDuration = origEditedEnd - newEditedStart;

            if (newEditedDuration > 0.01 && note.duration > 0.0)
            {
                note.timeOffset = newTimeOffset;
                note.durationScale = newEditedDuration / note.duration;
            }
        }
        else if (dragMode == DragMode::ResizeRight)
        {
            double timeDelta = static_cast<double> (dx) / getGridArea().getWidth() * viewDuration;
            double newEditedDuration = note.duration * dragOriginalDurationScale + timeDelta;

            if (newEditedDuration > 0.01 && note.duration > 0.0)
                note.durationScale = newEditedDuration / note.duration;
        }

        break;
    }

    repaint();
}

void PianoRollComponent::mouseWheelMove (const juce::MouseEvent& event,
                                          const juce::MouseWheelDetails& wheel)
{
    bool cmdDown = event.mods.isCommandDown();
    bool shiftDown = event.mods.isShiftDown();

    if (cmdDown && shiftDown)
    {
        // Cmd+Shift+scroll: zoom pitch (vertical zoom)
        int midiRange = viewTopMidi - viewBottomMidi;
        int delta = wheel.deltaY > 0 ? -2 : 2;
        int newRange = juce::jlimit (12, 96, midiRange + delta);

        int center = (viewTopMidi + viewBottomMidi) / 2;
        viewBottomMidi = juce::jmax (0, center - newRange / 2);
        viewTopMidi = juce::jmin (127, viewBottomMidi + newRange);
    }
    else if (cmdDown)
    {
        // Cmd+scroll: zoom time (horizontal zoom at cursor)
        auto gridArea = getGridArea();
        double zoomFactor = wheel.deltaY > 0 ? 0.85 : 1.18;
        double newDuration = juce::jlimit (0.5, juce::jmax (1.0, totalDuration * 1.5),
                                            viewDuration * zoomFactor);

        if (gridArea.getWidth() > 0)
        {
            double mouseTime = xToTime (event.position.x);
            double mouseRatio = (event.position.x - gridArea.getX()) / gridArea.getWidth();
            viewStartTime = mouseTime - mouseRatio * newDuration;
            viewStartTime = juce::jmax (0.0, viewStartTime);
        }

        viewDuration = newDuration;
    }
    else
    {
        // No modifier: scroll/pan
        // deltaX → pan time (trackpad horizontal swipe)
        // deltaY → pan pitch (vertical scroll)
        // Shift+deltaY → pan time (mouse wheel shortcut for horizontal scroll)

        float scrollSpeed = 6.0f;

        if (shiftDown || std::abs (wheel.deltaX) > std::abs (wheel.deltaY))
        {
            // Horizontal scroll (time)
            float dx = shiftDown ? wheel.deltaY : wheel.deltaX;
            double timeDelta = -static_cast<double> (dx) * scrollSpeed * viewDuration / 10.0;
            viewStartTime = juce::jmax (0.0, viewStartTime + timeDelta);
        }
        else
        {
            // Vertical scroll (pitch)
            int midiRange = viewTopMidi - viewBottomMidi;
            int midiDelta = static_cast<int> (wheel.deltaY * scrollSpeed);
            viewBottomMidi = juce::jlimit (0, 127 - midiRange, viewBottomMidi + midiDelta);
            viewTopMidi = viewBottomMidi + midiRange;
        }

        // Also handle trackpad diagonal scrolling (both axes simultaneously)
        if (! shiftDown && std::abs (wheel.deltaX) > 0.001f && std::abs (wheel.deltaY) > 0.001f)
        {
            double timeDelta = -static_cast<double> (wheel.deltaX) * scrollSpeed * viewDuration / 10.0;
            viewStartTime = juce::jmax (0.0, viewStartTime + timeDelta);
        }
    }

    repaint();
}

void PianoRollComponent::mouseMagnify (const juce::MouseEvent& event, float scaleFactor)
{
    // Pinch-to-zoom: zoom time axis at cursor position
    auto gridArea = getGridArea();
    double zoomFactor = 1.0 / static_cast<double> (scaleFactor);
    double newDuration = juce::jlimit (0.5, juce::jmax (1.0, totalDuration * 1.5),
                                        viewDuration * zoomFactor);

    if (gridArea.getWidth() > 0)
    {
        double mouseTime = xToTime (event.position.x);
        double mouseRatio = (event.position.x - gridArea.getX()) / gridArea.getWidth();
        viewStartTime = mouseTime - mouseRatio * newDuration;
        viewStartTime = juce::jmax (0.0, viewStartTime);
    }

    viewDuration = newDuration;
    repaint();
}

bool PianoRollComponent::keyPressed (const juce::KeyPress& key)
{
    // Cmd+Z = Undo
    if (key == juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0))
    {
        if (undoManager.canUndo())
        {
            undoManager.undo();
            repaint();
            listeners.call ([] (Listener& l) { l.notesChanged(); });
            return true;
        }
    }

    // Cmd+Shift+Z = Redo
    if (key == juce::KeyPress ('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0))
    {
        if (undoManager.canRedo())
        {
            undoManager.redo();
            repaint();
            listeners.call ([] (Listener& l) { l.notesChanged(); });
            return true;
        }
    }

    // Cmd+A = Select all
    if (key == juce::KeyPress ('a', juce::ModifierKeys::commandModifier, 0))
    {
        selectAllNotes();
        return true;
    }

    // Delete/Backspace = reset selected notes' edits
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (! selectedNoteIds.empty())
        {
            std::vector<BatchNoteEditAction::NoteSnapshot> beforeStates;
            std::vector<BatchNoteEditAction::NoteSnapshot> afterStates;

            for (const auto& note : notes)
            {
                if (selectedNoteIds.count (note.id) && note.isEdited())
                {
                    BatchNoteEditAction::NoteSnapshot before;
                    before.id = note.id;
                    before.state.pitchShiftSemitones = note.pitchShiftSemitones;
                    before.state.formantShiftSemitones = note.formantShiftSemitones;
                    before.state.timeOffset = note.timeOffset;
                    before.state.durationScale = note.durationScale;
                    before.state.gainDB = note.gainDB;
                    before.state.pitchDriftScale = note.pitchDriftScale;
                    beforeStates.push_back (before);

                    BatchNoteEditAction::NoteSnapshot after;
                    after.id = note.id;
                    afterStates.push_back (after);
                }
            }

            if (! beforeStates.empty())
            {
                undoManager.perform (
                    new BatchNoteEditAction (notes, beforeStates, afterStates,
                                              [this]() { repaint(); }),
                    "Reset Selected");
                repaint();
                listeners.call ([] (Listener& l) { l.notesChanged(); });
                return true;
            }
        }
    }

    return false;
}

// --- Coordinate conversion ---

float PianoRollComponent::timeToX (double time) const
{
    auto gridArea = getGridArea();
    double fraction = (time - viewStartTime) / viewDuration;
    return gridArea.getX() + static_cast<float> (fraction) * gridArea.getWidth();
}

double PianoRollComponent::xToTime (float x) const
{
    auto gridArea = getGridArea();
    double fraction = static_cast<double> (x - gridArea.getX()) / gridArea.getWidth();
    return viewStartTime + fraction * viewDuration;
}

float PianoRollComponent::midiToY (int midi) const
{
    auto gridArea = getGridArea();
    float fraction = static_cast<float> (midi - viewBottomMidi) /
                     static_cast<float> (viewTopMidi - viewBottomMidi);
    return gridArea.getBottom() - fraction * gridArea.getHeight();
}

float PianoRollComponent::midiToY (float midiFloat) const
{
    auto gridArea = getGridArea();
    float fraction = (midiFloat - static_cast<float> (viewBottomMidi)) /
                     static_cast<float> (viewTopMidi - viewBottomMidi);
    return gridArea.getBottom() - fraction * gridArea.getHeight();
}

int PianoRollComponent::yToMidi (float y) const
{
    auto gridArea = getGridArea();
    float fraction = (gridArea.getBottom() - y) / gridArea.getHeight();
    return viewBottomMidi + static_cast<int> (fraction * (viewTopMidi - viewBottomMidi));
}

float PianoRollComponent::noteHeight() const
{
    auto gridArea = getGridArea();
    return gridArea.getHeight() / static_cast<float> (viewTopMidi - viewBottomMidi);
}

juce::Rectangle<float> PianoRollComponent::getGridArea() const
{
    return juce::Rectangle<float> (pianoKeyWidth, headerHeight,
                                    static_cast<float> (getWidth()) - pianoKeyWidth,
                                    static_cast<float> (getHeight()) - headerHeight);
}

// --- Note rect helper (uses edited positions + sub-semitone positioning) ---

juce::Rectangle<float> PianoRollComponent::getNoteRect (const Note& note) const
{
    // Use float MIDI for sub-semitone positioning
    float displayMidiFloat = static_cast<float> (note.midiNote) + note.pitchShiftSemitones;
    displayMidiFloat = juce::jlimit (0.0f, 127.0f, displayMidiFloat);

    float x = timeToX (note.editedStartTime());
    float w = timeToX (note.editedStartTime() + note.editedDuration()) - x;
    float y = midiToY (displayMidiFloat + 1.0f);
    float h = noteHeight();

    w = juce::jmax (2.0f, w);
    return { x, y, w, h };
}

// --- Edge detection ---

PianoRollComponent::DragMode PianoRollComponent::edgeAtPoint (const Note& note, juce::Point<float> point) const
{
    auto rect = getNoteRect (note);

    bool yInRange = point.y >= rect.getY() && point.y <= rect.getBottom();

    if (std::abs (point.x - rect.getX()) < edgeThreshold && yInRange)
        return DragMode::ResizeLeft;

    if (std::abs (point.x - rect.getRight()) < edgeThreshold && yInRange)
        return DragMode::ResizeRight;

    return DragMode::MoveNote;
}

// --- Undo recording ---

void PianoRollComponent::recordDragUndo (int noteId)
{
    for (const auto& note : notes)
    {
        if (note.id == noteId)
        {
            NoteEditAction::EditState afterState;
            afterState.pitchShiftSemitones = note.pitchShiftSemitones;
            afterState.formantShiftSemitones = note.formantShiftSemitones;
            afterState.timeOffset = note.timeOffset;
            afterState.durationScale = note.durationScale;
            afterState.gainDB = note.gainDB;
            afterState.pitchDriftScale = note.pitchDriftScale;

            // Only record if something actually changed
            if (std::abs (afterState.pitchShiftSemitones - dragBeforeState.pitchShiftSemitones) > 0.001f
                || std::abs (afterState.formantShiftSemitones - dragBeforeState.formantShiftSemitones) > 0.001f
                || std::abs (afterState.timeOffset - dragBeforeState.timeOffset) > 0.0001
                || std::abs (afterState.durationScale - dragBeforeState.durationScale) > 0.0001
                || std::abs (afterState.gainDB - dragBeforeState.gainDB) > 0.01f)
            {
                undoManager.perform (
                    new NoteEditAction (notes, noteId, dragBeforeState, afterState,
                                         [this]() { repaint(); }),
                    "Drag Edit");
            }
            break;
        }
    }
}

void PianoRollComponent::recordMultiDragUndo()
{
    std::vector<BatchNoteEditAction::NoteSnapshot> beforeStates;
    std::vector<BatchNoteEditAction::NoteSnapshot> afterStates;

    bool anyChanged = false;

    for (const auto& note : notes)
    {
        auto it = multiDragBeforeStates.find (note.id);
        if (it == multiDragBeforeStates.end())
            continue;

        BatchNoteEditAction::NoteSnapshot before;
        before.id = note.id;
        before.state = it->second;
        beforeStates.push_back (before);

        BatchNoteEditAction::NoteSnapshot after;
        after.id = note.id;
        after.state.pitchShiftSemitones = note.pitchShiftSemitones;
        after.state.formantShiftSemitones = note.formantShiftSemitones;
        after.state.timeOffset = note.timeOffset;
        after.state.durationScale = note.durationScale;
        after.state.gainDB = note.gainDB;
        after.state.pitchDriftScale = note.pitchDriftScale;
        afterStates.push_back (after);

        if (std::abs (before.state.pitchShiftSemitones - after.state.pitchShiftSemitones) > 0.001f
            || std::abs (before.state.timeOffset - after.state.timeOffset) > 0.0001)
            anyChanged = true;
    }

    if (anyChanged && ! beforeStates.empty())
    {
        undoManager.perform (
            new BatchNoteEditAction (notes, beforeStates, afterStates,
                                      [this]() { repaint(); }),
            "Multi Drag Edit");
    }
}

// --- Drawing ---

void PianoRollComponent::drawGrid (juce::Graphics& g)
{
    auto gridArea = getGridArea();

    for (int midi = viewBottomMidi; midi <= viewTopMidi; ++midi)
    {
        float y = midiToY (midi);
        int noteInOctave = midi % 12;

        bool isBlackKey = (noteInOctave == 1 || noteInOctave == 3 || noteInOctave == 6 ||
                           noteInOctave == 8 || noteInOctave == 10);

        if (isBlackKey)
        {
            g.setColour (InKeyColours::blackKeyBg);
            g.fillRect (gridArea.getX(), y, gridArea.getWidth(), noteHeight());
        }

        // Scale shading: tint out-of-scale rows
        if (currentScale.degrees.size() == 12)
        {
            int degreeIndex = ((noteInOctave - currentScale.rootNote) + 12) % 12;
            if (! currentScale.degrees[static_cast<size_t> (degreeIndex)])
            {
                g.setColour (juce::Colour (0x08ff4466));  // subtle red tint for out-of-scale
                g.fillRect (gridArea.getX(), y, gridArea.getWidth(), noteHeight());
            }
        }

        if (noteInOctave == 0)
            g.setColour (InKeyColours::gridLineOctave);
        else
            g.setColour (InKeyColours::gridLine);

        g.drawHorizontalLine (static_cast<int> (y), gridArea.getX(), gridArea.getRight());
    }

    double timeStep = viewDuration / 10.0;
    if (timeStep > 1.0) timeStep = std::floor (timeStep);
    else if (timeStep > 0.5) timeStep = 0.5;
    else if (timeStep > 0.25) timeStep = 0.25;
    else timeStep = 0.1;

    g.setColour (InKeyColours::gridLine);
    double t = std::ceil (viewStartTime / timeStep) * timeStep;
    while (t < viewStartTime + viewDuration)
    {
        float x = timeToX (t);
        g.drawVerticalLine (static_cast<int> (x), gridArea.getY(), gridArea.getBottom());
        t += timeStep;
    }
}

void PianoRollComponent::drawPianoKeys (juce::Graphics& g)
{
    // Gradient sidebar
    juce::ColourGradient keyGradient (
        InKeyColours::pianoKeySidebar.brighter (0.08f), 0.0f, headerHeight,
        InKeyColours::pianoKeySidebar,                   pianoKeyWidth, headerHeight,
        false);
    g.setGradientFill (keyGradient);
    g.fillRect (0.0f, headerHeight, pianoKeyWidth, static_cast<float> (getHeight()) - headerHeight);

    for (int midi = viewBottomMidi; midi <= viewTopMidi; ++midi)
    {
        float y = midiToY (midi);
        float h = noteHeight();
        int noteInOctave = midi % 12;

        bool isBlackKey = (noteInOctave == 1 || noteInOctave == 3 || noteInOctave == 6 ||
                           noteInOctave == 8 || noteInOctave == 10);

        if (isBlackKey)
        {
            g.setColour (InKeyColours::blackKeyBg);
            g.fillRect (0.0f, y, pianoKeyWidth * 0.65f, h);
        }

        if (noteInOctave == 0)
        {
            g.setColour (InKeyColours::textSecondary);
            g.setFont (juce::Font (10.0f));
            juce::String label = "C" + juce::String (Note::midiToOctave (midi));
            g.drawText (label,
                        juce::Rectangle<float> (2.0f, y, pianoKeyWidth - 4.0f, h),
                        juce::Justification::centredLeft);
        }

        g.setColour (InKeyColours::gridLine);
        g.drawHorizontalLine (static_cast<int> (y), 0.0f, pianoKeyWidth);
    }

    g.setColour (InKeyColours::glassBorder);
    g.drawVerticalLine (static_cast<int> (pianoKeyWidth), headerHeight,
                         static_cast<float> (getHeight()));
}

void PianoRollComponent::drawNotes (juce::Graphics& g)
{
    float h = noteHeight();

    for (const auto& note : notes)
    {
        auto rect = getNoteRect (note);

        // Skip notes outside the view
        double editedEnd = note.editedStartTime() + note.editedDuration();
        if (editedEnd < viewStartTime || note.editedStartTime() > viewStartTime + viewDuration)
            continue;

        float displayMidiFloat = static_cast<float> (note.midiNote) + note.pitchShiftSemitones;
        int displayMidi = static_cast<int> (std::round (displayMidiFloat));
        displayMidi = juce::jlimit (0, 127, displayMidi);

        if (displayMidiFloat < static_cast<float> (viewBottomMidi) - 1.0f
            || displayMidiFloat > static_cast<float> (viewTopMidi) + 1.0f)
            continue;

        bool isSelected = selectedNoteIds.count (note.id) > 0;
        bool edited = note.isEdited();

        // Chromatic rainbow colour
        auto baseColour = InKeyColours::getNoteColour (displayMidi, note.confidence);

        if (edited && ! isSelected)
            baseColour = baseColour.withSaturation (
                juce::jmin (1.0f, baseColour.getSaturation() + 0.1f));

        auto fillColour = baseColour.withAlpha (0.85f);
        auto borderColour = isSelected ? InKeyColours::selectedGlow
                                       : baseColour.brighter (0.3f).withAlpha (0.6f);

        // Note body
        g.setColour (fillColour);
        g.fillRoundedRectangle (rect, 3.0f);

        // Draw pitch curve inside note (Melodyne-style)
        drawNotePitchCurve (g, note, rect);

        // Border
        g.setColour (borderColour);
        g.drawRoundedRectangle (rect, 3.0f, isSelected ? 2.0f : 1.0f);

        // Selected glow
        if (isSelected)
        {
            g.setColour (juce::Colours::white.withAlpha (0.15f));
            g.drawRoundedRectangle (rect.expanded (2.0f), 5.0f, 2.0f);

            // Resize handles
            g.setColour (juce::Colours::white.withAlpha (0.5f));
            g.fillRect (rect.getX(), rect.getY(), 3.0f, rect.getHeight());
            g.fillRect (rect.getRight() - 3.0f, rect.getY(), 3.0f, rect.getHeight());
        }

        // Ghost of original position (dashed outline)
        if (edited)
        {
            float origX = timeToX (note.startTime);
            float origW = timeToX (note.endTime()) - origX;
            float origY = midiToY (note.midiNote + 1);
            origW = juce::jmax (2.0f, origW);

            auto ghostRect = juce::Rectangle<float> (origX, origY, origW, h);

            juce::Path dashPath;
            dashPath.addRoundedRectangle (ghostRect, 2.0f);

            float dashLengths[] = { 4.0f, 3.0f };
            juce::PathStrokeType strokeType (1.0f);
            juce::Path dashedPath;
            strokeType.createDashedStroke (dashedPath, dashPath, dashLengths, 2);

            g.setColour (juce::Colour (0x40ffffff));
            g.fillPath (dashedPath);
        }

        // Volume indicator bar at bottom of note
        if (std::abs (note.gainDB) > 0.1f && rect.getWidth() > 6.0f)
        {
            float barHeight = 3.0f;
            auto barRect = juce::Rectangle<float> (rect.getX() + 2.0f, rect.getBottom() - barHeight - 1.0f,
                                                    rect.getWidth() - 4.0f, barHeight);
            auto barColour = note.gainDB > 0.0f ? juce::Colour (0x8044cc66) : juce::Colour (0x80ff4466);
            float fraction = juce::jlimit (0.0f, 1.0f, std::abs (note.gainDB) / 24.0f);
            g.setColour (barColour);
            g.fillRect (barRect.withWidth (barRect.getWidth() * fraction));
        }

        // Note name label
        if (rect.getWidth() > 30.0f && h > 10.0f)
        {
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.setFont (juce::Font (juce::jmin (h - 2.0f, 11.0f)));

            juce::String noteName = juce::String (Note::midiToNoteName (displayMidi))
                                    + juce::String (Note::midiToOctave (displayMidi));

            if (std::abs (note.pitchShiftSemitones) > 0.01f)
            {
                int wholeSemitones = static_cast<int> (std::round (note.pitchShiftSemitones));
                float cents = (note.pitchShiftSemitones - static_cast<float> (wholeSemitones)) * 100.0f;

                if (wholeSemitones != 0)
                    noteName += (wholeSemitones > 0 ? "+" : "") + juce::String (wholeSemitones);

                if (std::abs (cents) > 0.5f)
                    noteName += juce::String (cents >= 0 ? "+" : "") + juce::String (static_cast<int> (cents)) + "c";
            }

            g.drawText (noteName,
                        juce::Rectangle<float> (rect.getX() + 3.0f, rect.getY(),
                                                 rect.getWidth() - 6.0f, h),
                        juce::Justification::centredLeft);
        }
    }
}

void PianoRollComponent::drawNotePitchCurve (juce::Graphics& g, const Note& note,
                                               const juce::Rectangle<float>& rect)
{
    if (pitchCurve.empty() || rect.getWidth() < 8.0f)
        return;

    // Binary search for the start of this note's frames
    auto it = std::lower_bound (pitchCurve.begin(), pitchCurve.end(), note.startTime,
        [] (const PitchFrame& f, double t) { return f.time < t; });

    // Compute average MIDI pitch within this note for drift scaling
    float avgMidi = 0.0f;
    int avgCount = 0;
    if (std::abs (note.pitchDriftScale - 1.0f) > 0.01f)
    {
        for (auto jt = it; jt != pitchCurve.end() && jt->time <= note.endTime(); ++jt)
        {
            if (jt->frequency > 0.0f && jt->confidence >= 0.3f)
            {
                avgMidi += 69.0f + 12.0f * std::log2 (jt->frequency / 440.0f);
                avgCount++;
            }
        }
        if (avgCount > 0)
            avgMidi /= static_cast<float> (avgCount);
    }

    g.saveState();
    g.reduceClipRegion (rect.toNearestIntEdges());

    juce::Path curvePath;
    bool pathStarted = false;

    for (; it != pitchCurve.end() && it->time <= note.endTime(); ++it)
    {
        const auto& frame = *it;

        if (frame.frequency <= 0.0f || frame.confidence < 0.3f)
        {
            pathStarted = false;
            continue;
        }

        float frameMidiFloat = 69.0f + 12.0f * std::log2 (frame.frequency / 440.0f);

        // Apply pitch drift scaling: scale deviations from average
        if (std::abs (note.pitchDriftScale - 1.0f) > 0.01f && avgCount > 0)
        {
            float deviation = frameMidiFloat - avgMidi;
            frameMidiFloat = avgMidi + deviation * note.pitchDriftScale;
        }

        // Apply the note's pitch shift for display
        float shiftedMidi = frameMidiFloat + note.pitchShiftSemitones;

        // Apply time offset for display
        float x = timeToX (frame.time + note.timeOffset);
        float y = midiToY (shiftedMidi);

        if (! pathStarted)
        {
            curvePath.startNewSubPath (x, y);
            pathStarted = true;
        }
        else
        {
            curvePath.lineTo (x, y);
        }
    }

    if (! curvePath.isEmpty())
    {
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.strokePath (curvePath, juce::PathStrokeType (1.5f));
    }

    g.restoreState();
}

void PianoRollComponent::drawPitchCurve (juce::Graphics& g)
{
    // Global pitch curve overlay — drawn at reduced opacity since curves are now inside notes
    if (pitchCurve.empty())
        return;

    bool inSegment = false;
    float lastX = 0.0f;
    float lastY = 0.0f;

    for (size_t i = 0; i < pitchCurve.size(); ++i)
    {
        const auto& frame = pitchCurve[i];

        if (frame.frequency <= 0.0f || frame.confidence < 0.3f)
        {
            inSegment = false;
            continue;
        }

        if (frame.time < viewStartTime || frame.time > viewStartTime + viewDuration)
        {
            inSegment = false;
            continue;
        }

        float midiFloat = 69.0f + 12.0f * std::log2 (frame.frequency / 440.0f);

        float x = timeToX (frame.time);
        float y = midiToY (midiFloat);

        g.setColour (juce::Colours::white.withAlpha (0.12f));

        if (inSegment)
            g.drawLine (lastX, lastY, x, y, 1.0f);

        lastX = x;
        lastY = y;
        inSegment = true;
    }
}

void PianoRollComponent::drawLasso (juce::Graphics& g)
{
    if (! isLassoing || lassoRect.isEmpty())
        return;

    // Semi-transparent fill
    g.setColour (juce::Colour (0x18ffffff));
    g.fillRect (lassoRect);

    // Dashed border
    g.setColour (InKeyColours::selectedGlow.withAlpha (0.5f));
    juce::Path borderPath;
    borderPath.addRectangle (lassoRect);

    float dashLengths[] = { 6.0f, 4.0f };
    juce::PathStrokeType strokeType (1.0f);
    juce::Path dashedBorder;
    strokeType.createDashedStroke (dashedBorder, borderPath, dashLengths, 2);
    g.fillPath (dashedBorder);
}

void PianoRollComponent::drawPlayhead (juce::Graphics& g)
{
    if (playheadTime < 0.0 || playheadTime < viewStartTime
        || playheadTime > viewStartTime + viewDuration)
        return;

    auto gridArea = getGridArea();
    float x = timeToX (playheadTime);

    // Soft glow behind playhead
    float glowWidth = 12.0f;
    juce::ColourGradient glowLeft (
        InKeyColours::playheadColour.withAlpha (0.25f), x, 0.0f,
        juce::Colours::transparentBlack,                x - glowWidth * 0.5f, 0.0f,
        false);
    g.setGradientFill (glowLeft);
    g.fillRect (x - glowWidth * 0.5f, gridArea.getY(), glowWidth * 0.5f, gridArea.getHeight());

    juce::ColourGradient glowRight (
        InKeyColours::playheadColour.withAlpha (0.25f), x, 0.0f,
        juce::Colours::transparentBlack,                x + glowWidth * 0.5f, 0.0f,
        false);
    g.setGradientFill (glowRight);
    g.fillRect (x, gridArea.getY(), glowWidth * 0.5f, gridArea.getHeight());

    // Core line
    g.setColour (InKeyColours::playheadColour);
    g.fillRect (x - 0.5f, gridArea.getY(), 1.5f, gridArea.getHeight());

    // Top triangle indicator
    juce::Path triangle;
    triangle.addTriangle (x - 5.0f, gridArea.getY(),
                          x + 5.0f, gridArea.getY(),
                          x, gridArea.getY() + 6.0f);
    g.fillPath (triangle);
}

// --- Hit testing (uses edited positions) ---

Note* PianoRollComponent::noteAtPoint (juce::Point<float> point)
{
    for (auto& note : notes)
    {
        auto rect = getNoteRect (note);
        if (rect.contains (point))
            return &note;
    }

    return nullptr;
}

const Note* PianoRollComponent::noteAtPoint (juce::Point<float> point) const
{
    for (const auto& note : notes)
    {
        auto rect = getNoteRect (note);
        if (rect.contains (point))
            return &note;
    }

    return nullptr;
}

// --- Note splitting and merging ---

void PianoRollComponent::splitNoteAtTime (int noteId, double splitTime)
{
    for (const auto& note : notes)
    {
        if (note.id != noteId)
            continue;

        // splitTime must be within the note's original time range
        if (splitTime <= note.startTime + 0.02 || splitTime >= note.endTime() - 0.02)
            return;

        Note first;
        first.id = nextNoteId++;
        first.startTime = note.startTime;
        first.duration = splitTime - note.startTime;
        first.originalPitch = computeAveragePitch (note.startTime, splitTime);
        first.midiNote = Note::frequencyToMidi (first.originalPitch);
        first.confidence = note.confidence;

        Note second;
        second.id = nextNoteId++;
        second.startTime = splitTime;
        second.duration = note.endTime() - splitTime;
        second.originalPitch = computeAveragePitch (splitTime, note.endTime());
        second.midiNote = Note::frequencyToMidi (second.originalPitch);
        second.confidence = note.confidence;

        undoManager.perform (
            new NoteSplitAction (notes, note, first, second,
                                 [this]() { repaint(); }),
            "Split Note");

        // Select the two new notes
        selectedNoteIds.clear();
        selectedNoteIds.insert (first.id);
        selectedNoteIds.insert (second.id);
        notifySelectionChanged();
        listeners.call ([] (Listener& l) { l.notesChanged(); });
        break;
    }
}

void PianoRollComponent::mergeSelectedNotes()
{
    if (selectedNoteIds.size() < 2)
        return;

    // Collect selected notes sorted by start time
    std::vector<Note> selectedNotes;
    for (const auto& note : notes)
    {
        if (selectedNoteIds.count (note.id))
            selectedNotes.push_back (note);
    }

    std::sort (selectedNotes.begin(), selectedNotes.end(),
        [] (const Note& a, const Note& b) { return a.startTime < b.startTime; });

    // Check adjacency (within 50ms gap tolerance)
    for (size_t i = 1; i < selectedNotes.size(); ++i)
    {
        double gap = selectedNotes[i].startTime - selectedNotes[i - 1].endTime();
        if (gap > 0.05)
            return;  // Notes are not adjacent
    }

    // Merge all selected into one note
    // For simplicity, merge pairs from the end
    // But the simple approach: merge all at once by creating one note spanning the full range
    Note merged;
    merged.id = nextNoteId++;
    merged.startTime = selectedNotes.front().startTime;
    double endTime = selectedNotes.back().endTime();
    merged.duration = endTime - merged.startTime;
    merged.confidence = 0.0f;

    // Weighted average pitch
    double totalWeight = 0.0;
    double weightedPitch = 0.0;
    for (const auto& n : selectedNotes)
    {
        weightedPitch += static_cast<double> (n.originalPitch) * n.duration;
        totalWeight += n.duration;
        merged.confidence += n.confidence * static_cast<float> (n.duration);
    }
    merged.originalPitch = totalWeight > 0.0 ? static_cast<float> (weightedPitch / totalWeight) : 0.0f;
    merged.confidence = totalWeight > 0.0f ? merged.confidence / static_cast<float> (totalWeight) : 0.0f;
    merged.midiNote = Note::frequencyToMidi (merged.originalPitch);

    // For merge with 2 notes, use the dedicated MergeAction
    if (selectedNotes.size() == 2)
    {
        undoManager.perform (
            new NoteMergeAction (notes, selectedNotes[0], selectedNotes[1], merged,
                                  [this]() { repaint(); }),
            "Merge Notes");
    }
    else
    {
        // For 3+ notes, we do sequential pair merges isn't clean with undo.
        // Simpler: manually remove all selected, add merged, with a custom undo
        // For now, just merge the first two — user can merge iteratively
        undoManager.perform (
            new NoteMergeAction (notes, selectedNotes[0], selectedNotes[1], merged,
                                  [this]() { repaint(); }),
            "Merge Notes");
    }

    selectedNoteIds.clear();
    selectedNoteIds.insert (merged.id);
    notifySelectionChanged();
    listeners.call ([] (Listener& l) { l.notesChanged(); });
}

float PianoRollComponent::computeAveragePitch (double startT, double endT) const
{
    float sum = 0.0f;
    int count = 0;
    for (const auto& frame : pitchCurve)
    {
        if (frame.time >= startT && frame.time < endT
            && frame.frequency > 0.0f && frame.confidence >= 0.3f)
        {
            sum += frame.frequency;
            count++;
        }
    }
    return count > 0 ? sum / static_cast<float> (count) : 0.0f;
}

void PianoRollComponent::addListener (Listener* listener)
{
    listeners.add (listener);
}

void PianoRollComponent::removeListener (Listener* listener)
{
    listeners.remove (listener);
}
