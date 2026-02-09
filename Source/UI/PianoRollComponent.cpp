#include "PianoRollComponent.h"
#include "InKeyLookAndFeel.h"
#include <cmath>

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
    selectedNoteId = -1;
    playheadTime = -1.0;
    undoManager.clearUndoHistory();

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
    selectedNoteId = -1;
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
                                            double timeOffset, double durationScale)
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

            NoteEditAction::EditState after;
            after.pitchShiftSemitones = pitchShift;
            after.formantShiftSemitones = formantShift;
            after.timeOffset = timeOffset;
            after.durationScale = durationScale;

            undoManager.perform (
                new NoteEditAction (notes, noteId, before, after,
                                     [this]() { repaint(); }),
                "Edit Note");

            listeners.call ([&note] (Listener& l) { l.noteEdited (note); });
            return;
        }
    }
}

void PianoRollComponent::quantizePitch()
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
        beforeStates.push_back (before);

        // Calculate the correction needed to snap to nearest semitone
        float exactMidi = 69.0f + 12.0f * std::log2 (note.originalPitch / 440.0f);
        float correction = std::round (exactMidi) - exactMidi;

        BatchNoteEditAction::NoteSnapshot after = before;
        after.state.pitchShiftSemitones = correction;
        afterStates.push_back (after);
    }

    undoManager.perform (
        new BatchNoteEditAction (notes, beforeStates, afterStates,
                                  [this]() { repaint(); }),
        "Quantize Pitch");
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
        beforeStates.push_back (before);

        BatchNoteEditAction::NoteSnapshot after;
        after.id = note.id;
        after.state.pitchShiftSemitones = 0.0f;
        after.state.formantShiftSemitones = 0.0f;
        after.state.timeOffset = 0.0;
        after.state.durationScale = 1.0;
        afterStates.push_back (after);
    }

    undoManager.perform (
        new BatchNoteEditAction (notes, beforeStates, afterStates,
                                  [this]() { repaint(); }),
        "Reset All Edits");
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
    drawPitchCurve (g);
    drawNotes (g);
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

    if (event.mods.isMiddleButtonDown() || event.mods.isAltDown())
    {
        isPanning = true;
        panStartPos = event.position;
        panStartTime = viewStartTime;
        panStartBottomMidi = viewBottomMidi;
        return;
    }

    if (gridArea.contains (event.position))
    {
        auto* clickedNote = noteAtPoint (event.position);
        if (clickedNote != nullptr)
        {
            selectedNoteId = clickedNote->id;
            listeners.call ([clickedNote] (Listener& l) { l.noteSelected (clickedNote); });

            // Determine drag mode based on click position
            dragMode = edgeAtPoint (*clickedNote, event.position);
            dragNoteId = clickedNote->id;
            dragStartX = event.position.x;
            dragStartY = event.position.y;
            dragOriginalPitchShift = clickedNote->pitchShiftSemitones;
            dragOriginalTimeOffset = clickedNote->timeOffset;
            dragOriginalDurationScale = clickedNote->durationScale;

            // Store state before drag for undo
            dragBeforeState.pitchShiftSemitones = clickedNote->pitchShiftSemitones;
            dragBeforeState.formantShiftSemitones = clickedNote->formantShiftSemitones;
            dragBeforeState.timeOffset = clickedNote->timeOffset;
            dragBeforeState.durationScale = clickedNote->durationScale;
        }
        else
        {
            selectedNoteId = -1;
            dragMode = DragMode::None;
            listeners.call ([] (Listener& l) { l.noteSelected (nullptr); });
        }
        repaint();
    }
}

void PianoRollComponent::mouseUp (const juce::MouseEvent& /*event*/)
{
    if (dragMode != DragMode::None && dragNoteId >= 0)
    {
        // Record undo action if the note actually changed
        recordDragUndo (dragNoteId);

        // Notify listener about the edit
        for (auto& note : notes)
        {
            if (note.id == dragNoteId)
            {
                listeners.call ([&note] (Listener& l) { l.noteEdited (note); });
                break;
            }
        }
    }

    dragMode = DragMode::None;
    dragNoteId = -1;
    isPanning = false;
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

    if (dragMode == DragMode::None || dragNoteId < 0)
        return;

    float dy = event.position.y - dragStartY;
    float dx = event.position.x - dragStartX;

    for (auto& note : notes)
    {
        if (note.id != dragNoteId)
            continue;

        if (dragMode == DragMode::MoveNote)
        {
            // Vertical: pitch shift
            float semitonesPerPixel = static_cast<float> (viewTopMidi - viewBottomMidi) / getGridArea().getHeight();
            float semitonesDelta = -dy * semitonesPerPixel;
            float newPitchShift = dragOriginalPitchShift + semitonesDelta;
            newPitchShift = std::round (newPitchShift);
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
            // Dragging left edge: changes start time and duration, keeps right edge fixed
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
            // Dragging right edge: changes duration only
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
    if (event.mods.isCommandDown())
    {
        int midiRange = viewTopMidi - viewBottomMidi;
        int delta = wheel.deltaY > 0 ? -2 : 2;
        int newRange = juce::jlimit (12, 96, midiRange + delta);

        int center = (viewTopMidi + viewBottomMidi) / 2;
        viewBottomMidi = juce::jmax (0, center - newRange / 2);
        viewTopMidi = juce::jmin (127, viewBottomMidi + newRange);
    }
    else
    {
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

    repaint();
}

bool PianoRollComponent::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0))
    {
        if (undoManager.canUndo())
        {
            undoManager.undo();
            repaint();
            return true;
        }
    }

    if (key == juce::KeyPress ('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0))
    {
        if (undoManager.canRedo())
        {
            undoManager.redo();
            repaint();
            return true;
        }
    }

    // Delete/Backspace resets selected note's edits
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (selectedNoteId >= 0)
        {
            for (const auto& note : notes)
            {
                if (note.id == selectedNoteId && note.isEdited())
                {
                    editNoteProperty (note.id, 0.0f, 0.0f, 0.0, 1.0);
                    repaint();
                    return true;
                }
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

// --- Note rect helper (uses edited positions) ---

juce::Rectangle<float> PianoRollComponent::getNoteRect (const Note& note) const
{
    int displayMidi = note.midiNote + static_cast<int> (std::round (note.pitchShiftSemitones));
    displayMidi = juce::jlimit (0, 127, displayMidi);

    float x = timeToX (note.editedStartTime());
    float w = timeToX (note.editedStartTime() + note.editedDuration()) - x;
    float y = midiToY (displayMidi + 1);
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

            // Only record if something actually changed
            if (std::abs (afterState.pitchShiftSemitones - dragBeforeState.pitchShiftSemitones) > 0.001f
                || std::abs (afterState.formantShiftSemitones - dragBeforeState.formantShiftSemitones) > 0.001f
                || std::abs (afterState.timeOffset - dragBeforeState.timeOffset) > 0.0001
                || std::abs (afterState.durationScale - dragBeforeState.durationScale) > 0.0001)
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

        int displayMidi = note.midiNote + static_cast<int> (std::round (note.pitchShiftSemitones));
        displayMidi = juce::jlimit (0, 127, displayMidi);

        if (displayMidi < viewBottomMidi || displayMidi > viewTopMidi)
            continue;

        bool isSelected = (note.id == selectedNoteId);
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

        // Note name label
        if (rect.getWidth() > 30.0f && h > 10.0f)
        {
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.setFont (juce::Font (juce::jmin (h - 2.0f, 11.0f)));

            juce::String noteName = juce::String (Note::midiToNoteName (displayMidi))
                                    + juce::String (Note::midiToOctave (displayMidi));

            if (std::abs (note.pitchShiftSemitones) > 0.01f)
            {
                int shift = static_cast<int> (std::round (note.pitchShiftSemitones));
                noteName += (shift > 0 ? "+" : "") + juce::String (shift);
            }

            g.drawText (noteName,
                        juce::Rectangle<float> (rect.getX() + 3.0f, rect.getY(),
                                                 rect.getWidth() - 6.0f, h),
                        juce::Justification::centredLeft);
        }
    }
}

void PianoRollComponent::drawPitchCurve (juce::Graphics& g)
{
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

        g.setColour (juce::Colours::white.withAlpha (0.35f));

        if (inSegment)
            g.drawLine (lastX, lastY, x, y, 1.0f);

        lastX = x;
        lastY = y;
        inSegment = true;
    }
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

void PianoRollComponent::addListener (Listener* listener)
{
    listeners.add (listener);
}

void PianoRollComponent::removeListener (Listener* listener)
{
    listeners.remove (listener);
}
