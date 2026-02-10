#include "PluginEditor.h"

InKeyEditor::InKeyEditor (InKeyProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      waveformComponent (p.getAudioCapture())
{
    setLookAndFeel (&lookAndFeel);

    // Capture button
    captureButton.setButtonText ("Capture");
    captureButton.setColour (juce::TextButton::buttonColourId, InKeyColours::accentCapture);
    captureButton.onClick = [this]()
    {
        auto state = processorRef.getState();
        if (state == PluginState::IDLE || state == PluginState::EDITING)
        {
            // Save current take's notes before recapture
            if (showPianoRoll && processorRef.getActiveTakeIndex() >= 0)
                processorRef.saveActiveTakeNotes (pianoRollComponent.getNotes());

            showPianoRoll = false;
            pianoRollComponent.clear();
            pitchCurveComponent.clear();
            pianoRollComponent.setVisible (false);
            propertiesPanel.setVisible (false);
            waveformComponent.setVisible (true);
            pitchCurveComponent.setVisible (true);
            processorRef.startCapture();
        }
        else if (state == PluginState::CAPTURING)
        {
            processorRef.stopCapture();
        }
    };
    addAndMakeVisible (captureButton);

    // Play button
    playButton.setButtonText ("Play");
    playButton.setColour (juce::TextButton::buttonColourId, InKeyColours::accentPlay);
    playButton.onClick = [this]()
    {
        auto state = processorRef.getState();
        if (state == PluginState::EDITING)
        {
            processorRef.startPlayback (pianoRollComponent.getNotes());
        }
        else if (state == PluginState::PLAYING)
        {
            processorRef.stopPlayback();
        }
    };
    addChildComponent (playButton);

    // Take selector
    takeSelector.onChange = [this]()
    {
        int selectedIndex = takeSelector.getSelectedId() - 1; // IDs are 1-based
        if (selectedIndex >= 0 && selectedIndex != processorRef.getActiveTakeIndex())
            switchToTake (selectedIndex);
    };
    addChildComponent (takeSelector);

    // Align button
    alignButton.setButtonText ("Align");
    alignButton.setColour (juce::TextButton::buttonColourId, InKeyColours::accentAlign);
    alignButton.onClick = [this]() { performAlignment(); };
    addChildComponent (alignButton);

    // Undo button
    undoButton.setButtonText ("Undo");
    undoButton.setColour (juce::TextButton::buttonColourId, InKeyColours::accentUndo);
    undoButton.onClick = [this]()
    {
        if (pianoRollComponent.canUndo())
        {
            pianoRollComponent.getUndoManager().undo();
            pianoRollComponent.repaint();
            updatePropertiesFromSelection();
        }
    };
    addChildComponent (undoButton);

    // Redo button
    redoButton.setButtonText ("Redo");
    redoButton.setColour (juce::TextButton::buttonColourId, InKeyColours::accentRedo);
    redoButton.onClick = [this]()
    {
        if (pianoRollComponent.canRedo())
        {
            pianoRollComponent.getUndoManager().redo();
            pianoRollComponent.repaint();
            updatePropertiesFromSelection();
        }
    };
    addChildComponent (redoButton);

    // Quantize button
    quantizeButton.setButtonText ("Quantize");
    quantizeButton.setColour (juce::TextButton::buttonColourId, InKeyColours::accentQuantize);
    quantizeButton.onClick = [this]()
    {
        float strength = static_cast<float> (quantizeStrengthSlider.getValue()) / 100.0f;
        auto scale = getCurrentScale();
        pianoRollComponent.quantizePitchToScale (strength, scale);
        pianoRollComponent.repaint();
        updatePropertiesFromSelection();
    };
    addChildComponent (quantizeButton);

    // Scale root selector (C, C#, D, ...)
    for (int i = 0; i < 12; ++i)
        scaleRootSelector.addItem (QuantizationEngine::rootNoteName (i), i + 1);
    scaleRootSelector.setSelectedId (1, juce::dontSendNotification); // C
    scaleRootSelector.onChange = [this]()
    {
        pianoRollComponent.setCurrentScale (getCurrentScale());
    };
    addChildComponent (scaleRootSelector);

    // Scale type selector
    scaleTypeSelector.addItem ("Chromatic", 1);
    scaleTypeSelector.addItem ("Major", 2);
    scaleTypeSelector.addItem ("Natural Minor", 3);
    scaleTypeSelector.addItem ("Harmonic Minor", 4);
    scaleTypeSelector.addItem ("Pentatonic", 5);
    scaleTypeSelector.addItem ("Blues", 6);
    scaleTypeSelector.addItem ("Dorian", 7);
    scaleTypeSelector.addItem ("Mixolydian", 8);
    scaleTypeSelector.setSelectedId (1, juce::dontSendNotification);
    scaleTypeSelector.onChange = [this]()
    {
        pianoRollComponent.setCurrentScale (getCurrentScale());
    };
    addChildComponent (scaleTypeSelector);

    // Quantize strength slider (0-100%)
    quantizeStrengthSlider.setRange (0.0, 100.0, 1.0);
    quantizeStrengthSlider.setValue (100.0, juce::dontSendNotification);
    quantizeStrengthSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    quantizeStrengthSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 35, 20);
    quantizeStrengthSlider.setTextValueSuffix ("%");
    quantizeStrengthSlider.setColour (juce::Slider::thumbColourId, InKeyColours::accentQuantize);
    addChildComponent (quantizeStrengthSlider);

    // Reset button
    resetButton.setButtonText ("Reset");
    resetButton.setColour (juce::TextButton::buttonColourId, InKeyColours::accentReset);
    resetButton.onClick = [this]()
    {
        pianoRollComponent.resetAllEdits();
        pianoRollComponent.repaint();
        updatePropertiesFromSelection();
    };
    addChildComponent (resetButton);

    // Status label
    statusLabel.setColour (juce::Label::textColourId, InKeyColours::textSecondary);
    statusLabel.setFont (juce::Font (13.0f));
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (statusLabel);

    // Waveform + pitch curve
    addAndMakeVisible (waveformComponent);
    addAndMakeVisible (pitchCurveComponent);

    // Piano roll
    addChildComponent (pianoRollComponent);
    pianoRollComponent.addListener (this);

    // Properties panel
    addChildComponent (propertiesPanel);
    propertiesPanel.onPropertyChanged = [this] (int noteId, float pitchShift, float formantShift,
                                                 double timeOffset, double durationScale,
                                                 float gainDB, float pitchDriftScale)
    {
        pianoRollComponent.editNoteProperty (noteId, pitchShift, formantShift,
                                              timeOffset, durationScale, gainDB, pitchDriftScale);
        pianoRollComponent.repaint();
    };

    propertiesPanel.onMultiPropertyChanged = [this] (float pitchDelta, float formantDelta,
                                                      float gainDelta, float driftDelta)
    {
        // Apply delta to all selected notes
        const auto& selectedIds = pianoRollComponent.getSelectedNoteIds();
        if (selectedIds.empty())
            return;

        for (const auto& note : pianoRollComponent.getNotes())
        {
            if (selectedIds.count (note.id))
            {
                float newPitch = juce::jlimit (-24.0f, 24.0f,
                                                note.pitchShiftSemitones + pitchDelta);
                float newFormant = juce::jlimit (-12.0f, 12.0f,
                                                  note.formantShiftSemitones + formantDelta);
                float newGain = juce::jlimit (-24.0f, 12.0f,
                                               note.gainDB + gainDelta);
                float newDrift = juce::jlimit (0.0f, 2.0f,
                                                note.pitchDriftScale + driftDelta);
                pianoRollComponent.editNoteProperty (note.id, newPitch, newFormant,
                                                      note.timeOffset, note.durationScale,
                                                      newGain, newDrift);
            }
        }
        pianoRollComponent.repaint();
    };

    processorRef.getPitchAnalyzer().addListener (this);

    // Restore state if editor re-opened with existing takes
    if (processorRef.getState() == PluginState::EDITING
        && processorRef.getNumTakes() > 0)
    {
        showPianoRoll = true;
        auto* activeTake = processorRef.switchToTake (processorRef.getActiveTakeIndex());
        if (activeTake != nullptr)
        {
            pianoRollComponent.setData (activeTake->notes, activeTake->pitchCurve, activeTake->duration);
            pianoRollComponent.setVisible (true);
            propertiesPanel.setVisible (true);
            playButton.setVisible (true);
            undoButton.setVisible (true);
            redoButton.setVisible (true);
            quantizeButton.setVisible (true);
            resetButton.setVisible (true);
            waveformComponent.setVisible (false);
            pitchCurveComponent.setVisible (false);

            updateTakeSelector();
        }
    }

    // Tooltips
    captureButton.setTooltip ("Start/stop audio capture (R)");
    playButton.setTooltip ("Play/stop edited audio (Space)");
    undoButton.setTooltip ("Undo last edit (Cmd+Z)");
    redoButton.setTooltip ("Redo last edit (Cmd+Shift+Z)");
    quantizeButton.setTooltip ("Snap notes to scale (Q)");
    resetButton.setTooltip ("Reset all edits to original");
    alignButton.setTooltip ("Align active take to reference using DTW");
    scaleRootSelector.setTooltip ("Scale root note");
    scaleTypeSelector.setTooltip ("Scale type");
    quantizeStrengthSlider.setTooltip ("Quantize strength (0-100%)");

    setResizable (true, true);
    setResizeLimits (700, 500, 1800, 1200);
    setSize (900, 700);
    startTimer (50);
}

InKeyEditor::~InKeyEditor()
{
    setLookAndFeel (nullptr);
    stopTimer();
    processorRef.getPitchAnalyzer().removeListener (this);
    pianoRollComponent.removeListener (this);

    // Save current take's notes on editor close
    if (showPianoRoll && processorRef.getActiveTakeIndex() >= 0)
        processorRef.saveActiveTakeNotes (pianoRollComponent.getNotes());
}

void InKeyEditor::paint (juce::Graphics& g)
{
    // Background gradient
    juce::ColourGradient bgGradient (
        InKeyColours::backgroundTop,    0.0f, 0.0f,
        InKeyColours::backgroundBottom, 0.0f, static_cast<float> (getHeight()),
        false);
    g.setGradientFill (bgGradient);
    g.fillAll();

    // Glass toolbar panel
    auto topBar = getLocalBounds().removeFromTop (50).toFloat();
    InKeyLookAndFeel::drawGlassPanel (g, topBar, 0.0f, true);

    // Glass scale bar (second row)
    if (scaleRootSelector.isVisible())
    {
        auto scaleBar = getLocalBounds().toFloat();
        scaleBar.removeFromTop (50.0f);
        scaleBar = scaleBar.removeFromTop (32.0f);
        InKeyLookAndFeel::drawGlassPanel (g, scaleBar, 0.0f, false);
    }

    // Glass properties panel
    if (propertiesPanel.isVisible())
    {
        auto bottomBar = getLocalBounds().removeFromBottom (44).toFloat();
        InKeyLookAndFeel::drawGlassPanel (g, bottomBar, 0.0f, true);
    }

    // Title
    g.setColour (InKeyColours::textPrimary);
    g.setFont (juce::Font (28.0f).boldened());
    g.drawText ("InKey", 20, 10, 200, 40, juce::Justification::centredLeft);

    g.setColour (InKeyColours::textDim);
    g.setFont (juce::Font (13.0f));
    g.drawText ("Vocal Pitch Editor", 90, 18, 200, 24, juce::Justification::centredLeft);
}

void InKeyEditor::resized()
{
    auto area = getLocalBounds();

    // Top bar row 1: main controls
    auto topBar = area.removeFromTop (50);
    topBar.removeFromLeft (260);
    captureButton.setBounds (topBar.removeFromLeft (110).reduced (10, 10));
    playButton.setBounds (topBar.removeFromLeft (90).reduced (10, 10));

    topBar.removeFromLeft (8);
    takeSelector.setBounds (topBar.removeFromLeft (120).reduced (10, 10));
    alignButton.setBounds (topBar.removeFromLeft (80).reduced (10, 10));

    topBar.removeFromLeft (8);
    undoButton.setBounds (topBar.removeFromLeft (70).reduced (10, 10));
    redoButton.setBounds (topBar.removeFromLeft (70).reduced (10, 10));
    quantizeButton.setBounds (topBar.removeFromLeft (90).reduced (10, 10));
    resetButton.setBounds (topBar.removeFromLeft (80).reduced (10, 10));

    statusLabel.setBounds (topBar.reduced (10, 0));

    // Top bar row 2: scale/quantize controls (only visible in EDITING with piano roll)
    auto scaleBar = area.removeFromTop (32);
    scaleBar.removeFromLeft (260);
    scaleRootSelector.setBounds (scaleBar.removeFromLeft (65).reduced (4, 4));
    scaleTypeSelector.setBounds (scaleBar.removeFromLeft (120).reduced (4, 4));
    scaleBar.removeFromLeft (8);
    quantizeStrengthSlider.setBounds (scaleBar.removeFromLeft (150).reduced (4, 4));

    // Properties panel at bottom
    static constexpr int propertiesHeight = 44;
    auto propertiesArea = area.removeFromBottom (propertiesHeight);
    propertiesPanel.setBounds (propertiesArea);

    // Display area
    area.removeFromTop (2);
    auto displayArea = area.reduced (10, 4);

    waveformComponent.setBounds (displayArea);
    pitchCurveComponent.setBounds (displayArea);
    pianoRollComponent.setBounds (displayArea);
}

void InKeyEditor::timerCallback()
{
    auto state = processorRef.getState();

    if (state == PluginState::CAPTURING)
    {
        captureButton.setButtonText ("Stop");
        captureButton.setColour (juce::TextButton::buttonColourId, InKeyColours::accentCapture.brighter (0.2f));
        captureButton.setEnabled (true);
        playButton.setVisible (false);
        takeSelector.setVisible (false);
        alignButton.setVisible (false);
        undoButton.setVisible (false);
        redoButton.setVisible (false);
        quantizeButton.setVisible (false);
        resetButton.setVisible (false);
        propertiesPanel.setVisible (false);
        scaleRootSelector.setVisible (false);
        scaleTypeSelector.setVisible (false);
        quantizeStrengthSlider.setVisible (false);

        auto duration = processorRef.getAudioCapture().getCapturedDurationSeconds();
        statusLabel.setText ("Recording... " + juce::String (duration, 1) + "s",
                             juce::dontSendNotification);
    }
    else if (state == PluginState::ANALYZING)
    {
        captureButton.setButtonText ("Capture");
        captureButton.setColour (juce::TextButton::buttonColourId, InKeyColours::accentCapture);
        captureButton.setEnabled (false);
        playButton.setVisible (false);

        auto progress = processorRef.getPitchAnalyzer().getProgress();
        statusLabel.setText ("Analyzing... " + juce::String (static_cast<int> (progress * 100.0f)) + "%",
                             juce::dontSendNotification);
    }
    else if (state == PluginState::EDITING)
    {
        captureButton.setButtonText (processorRef.getNumTakes() > 0 ? "New Take" : "Capture");
        captureButton.setColour (juce::TextButton::buttonColourId, InKeyColours::accentCapture);
        captureButton.setEnabled (true);

        playButton.setButtonText ("Play");
        playButton.setColour (juce::TextButton::buttonColourId, InKeyColours::accentPlay);
        playButton.setVisible (showPianoRoll);
        undoButton.setVisible (showPianoRoll);
        redoButton.setVisible (showPianoRoll);
        quantizeButton.setVisible (showPianoRoll);
        resetButton.setVisible (showPianoRoll);
        propertiesPanel.setVisible (showPianoRoll);
        scaleRootSelector.setVisible (showPianoRoll);
        scaleTypeSelector.setVisible (showPianoRoll);
        quantizeStrengthSlider.setVisible (showPianoRoll);

        // Show take selector and align button when there are 2+ takes
        bool multiTake = processorRef.getNumTakes() > 1;
        takeSelector.setVisible (showPianoRoll && multiTake);
        alignButton.setVisible (showPianoRoll && multiTake
                                 && processorRef.getActiveTakeIndex() != processorRef.getReferenceTakeIndex());

        pianoRollComponent.setPlayheadPosition (-1.0);

        auto numNotes = pianoRollComponent.getNotes().size();
        int takeNum = processorRef.getActiveTakeIndex() + 1;
        int totalTakes = processorRef.getNumTakes();

        juce::String info;
        if (totalTakes > 1)
            info = "Take " + juce::String (takeNum) + "/" + juce::String (totalTakes) + " — ";

        auto& activeTakes = processorRef.getTakes();
        if (processorRef.getActiveTakeIndex() >= 0
            && processorRef.getActiveTakeIndex() < static_cast<int> (activeTakes.size()))
        {
            auto duration = activeTakes[static_cast<size_t> (processorRef.getActiveTakeIndex())].duration;
            info += juce::String (duration, 1) + "s — ";
        }

        info += juce::String (numNotes) + " notes";

        // Show selection info
        const auto& selectedIds = pianoRollComponent.getSelectedNoteIds();
        if (selectedIds.size() > 1)
        {
            info += " | " + juce::String (static_cast<int> (selectedIds.size())) + " selected";
        }
        else if (selectedIds.size() == 1)
        {
            int selectedId = *selectedIds.begin();
            for (const auto& note : pianoRollComponent.getNotes())
            {
                if (note.id == selectedId)
                {
                    int displayMidi = note.midiNote + static_cast<int> (std::round (note.pitchShiftSemitones));
                    info += " | " + juce::String (Note::midiToNoteName (displayMidi))
                            + juce::String (Note::midiToOctave (displayMidi));

                    if (std::abs (note.pitchShiftSemitones) > 0.01f)
                    {
                        int wholeSt = static_cast<int> (std::round (note.pitchShiftSemitones));
                        float cents = (note.pitchShiftSemitones - static_cast<float> (wholeSt)) * 100.0f;
                        juce::String shiftStr = (wholeSt > 0 ? "+" : "") + juce::String (wholeSt) + " st";
                        if (std::abs (cents) > 0.5f)
                            shiftStr += " " + juce::String (static_cast<int> (cents)) + "c";
                        info += " (" + shiftStr + ")";
                    }
                    else
                    {
                        info += " (" + juce::String (note.originalPitch, 1) + " Hz)";
                    }
                    break;
                }
            }
        }

        statusLabel.setText (info, juce::dontSendNotification);
    }
    else if (state == PluginState::PLAYING)
    {
        captureButton.setEnabled (false);
        playButton.setButtonText ("Stop");
        playButton.setColour (juce::TextButton::buttonColourId, InKeyColours::accentCapture);
        playButton.setVisible (true);
        undoButton.setVisible (false);
        redoButton.setVisible (false);
        quantizeButton.setVisible (false);
        resetButton.setVisible (false);
        alignButton.setVisible (false);
        scaleRootSelector.setVisible (false);
        scaleTypeSelector.setVisible (false);
        quantizeStrengthSlider.setVisible (false);

        double pos = processorRef.getAudioEngine().getPlaybackPosition();
        pianoRollComponent.setPlayheadPosition (pos);

        statusLabel.setText ("Playing... " + juce::String (pos, 1) + "s",
                             juce::dontSendNotification);
    }
    else
    {
        captureButton.setButtonText ("Capture");
        captureButton.setColour (juce::TextButton::buttonColourId, InKeyColours::accentCapture);
        captureButton.setEnabled (true);
        playButton.setVisible (false);
        takeSelector.setVisible (false);
        alignButton.setVisible (false);
        undoButton.setVisible (false);
        redoButton.setVisible (false);
        quantizeButton.setVisible (false);
        resetButton.setVisible (false);
        propertiesPanel.setVisible (false);
        scaleRootSelector.setVisible (false);
        scaleTypeSelector.setVisible (false);
        quantizeStrengthSlider.setVisible (false);

        statusLabel.setText ("Ready — press Capture to record", juce::dontSendNotification);
    }
}

void InKeyEditor::analysisComplete()
{
    showPianoRoll = true;

    int activeIdx = processorRef.getActiveTakeIndex();
    if (activeIdx >= 0 && activeIdx < processorRef.getNumTakes())
    {
        auto& take = processorRef.getTakes()[static_cast<size_t> (activeIdx)];
        pianoRollComponent.setData (take.notes, take.pitchCurve, take.duration);
    }

    pianoRollComponent.setVisible (true);
    propertiesPanel.setVisible (true);
    playButton.setVisible (true);
    undoButton.setVisible (true);
    redoButton.setVisible (true);
    quantizeButton.setVisible (true);
    resetButton.setVisible (true);
    waveformComponent.setVisible (false);
    pitchCurveComponent.setVisible (false);

    updateTakeSelector();
}

void InKeyEditor::analysisProgress (float /*progress*/)
{
}

void InKeyEditor::noteSelected (const Note* note)
{
    // Called for single selection or first selected note
    // Full selection handling is in selectionChanged
    (void) note;
}

void InKeyEditor::selectionChanged (const std::set<int>& selectedIds)
{
    updatePropertiesFromSelection();
    (void) selectedIds;
}

void InKeyEditor::noteEdited (const Note& note)
{
    updatePropertiesFromSelection();
    (void) note;
}

void InKeyEditor::updatePropertiesFromSelection()
{
    const auto& selectedIds = pianoRollComponent.getSelectedNoteIds();

    if (selectedIds.empty())
    {
        propertiesPanel.setSelectedNote (nullptr);
        return;
    }

    if (selectedIds.size() == 1)
    {
        int selectedId = *selectedIds.begin();
        for (const auto& note : pianoRollComponent.getNotes())
        {
            if (note.id == selectedId)
            {
                propertiesPanel.setSelectedNote (&note);
                return;
            }
        }
        propertiesPanel.setSelectedNote (nullptr);
        return;
    }

    // Multiple selection
    std::vector<const Note*> selectedNotes;
    for (const auto& note : pianoRollComponent.getNotes())
    {
        if (selectedIds.count (note.id))
            selectedNotes.push_back (&note);
    }
    propertiesPanel.setSelectedNotes (selectedNotes);
}

void InKeyEditor::updateTakeSelector()
{
    takeSelector.clear (juce::dontSendNotification);

    for (int i = 0; i < processorRef.getNumTakes(); ++i)
    {
        juce::String name = "Take " + juce::String (i + 1);
        if (i == processorRef.getReferenceTakeIndex())
            name += " (ref)";

        takeSelector.addItem (name, i + 1); // ComboBox IDs are 1-based
    }

    takeSelector.setSelectedId (processorRef.getActiveTakeIndex() + 1, juce::dontSendNotification);
}

void InKeyEditor::switchToTake (int index)
{
    // Save current take's notes
    if (processorRef.getActiveTakeIndex() >= 0)
        processorRef.saveActiveTakeNotes (pianoRollComponent.getNotes());

    auto* take = processorRef.switchToTake (index);
    if (take != nullptr)
    {
        pianoRollComponent.setData (take->notes, take->pitchCurve, take->duration);
        pianoRollComponent.repaint();
        propertiesPanel.setSelectedNote (nullptr);
        updateTakeSelector();
    }
}

void InKeyEditor::performAlignment()
{
    // Save current notes first
    processorRef.saveActiveTakeNotes (pianoRollComponent.getNotes());

    bool success = processorRef.alignActiveTakeToReference();

    if (success)
    {
        // Reload aligned notes into the piano roll
        int activeIdx = processorRef.getActiveTakeIndex();
        if (activeIdx >= 0)
        {
            auto& take = processorRef.getTakes()[static_cast<size_t> (activeIdx)];
            pianoRollComponent.setData (take.notes, take.pitchCurve, take.duration);
            pianoRollComponent.repaint();
        }

        statusLabel.setText ("Alignment complete!", juce::dontSendNotification);
    }
    else
    {
        statusLabel.setText ("Alignment failed", juce::dontSendNotification);
    }
}

ScaleDefinition InKeyEditor::getCurrentScale() const
{
    int root = scaleRootSelector.getSelectedId() - 1;
    if (root < 0) root = 0;

    switch (scaleTypeSelector.getSelectedId())
    {
        case 2:  return QuantizationEngine::majorScale (root);
        case 3:  return QuantizationEngine::naturalMinorScale (root);
        case 4:  return QuantizationEngine::harmonicMinorScale (root);
        case 5:  return QuantizationEngine::pentatonicScale (root);
        case 6:  return QuantizationEngine::bluesScale (root);
        case 7:  return QuantizationEngine::dorianScale (root);
        case 8:  return QuantizationEngine::mixolydianScale (root);
        default: return QuantizationEngine::chromaticScale (root);
    }
}

bool InKeyEditor::keyPressed (const juce::KeyPress& key)
{
    auto state = processorRef.getState();

    // Space = Play / Stop
    if (key == juce::KeyPress::spaceKey)
    {
        if (state == PluginState::EDITING && showPianoRoll)
        {
            processorRef.startPlayback (pianoRollComponent.getNotes());
            return true;
        }
        else if (state == PluginState::PLAYING)
        {
            processorRef.stopPlayback();
            return true;
        }
    }

    // R = Capture / Stop capture
    if (key == juce::KeyPress ('r'))
    {
        if (state == PluginState::IDLE || state == PluginState::EDITING)
        {
            captureButton.triggerClick();
            return true;
        }
        else if (state == PluginState::CAPTURING)
        {
            captureButton.triggerClick();
            return true;
        }
    }

    // Q = Quantize pitch (using current scale/strength settings)
    if (key == juce::KeyPress ('q') && showPianoRoll && state == PluginState::EDITING)
    {
        float strength = static_cast<float> (quantizeStrengthSlider.getValue()) / 100.0f;
        auto scale = getCurrentScale();
        pianoRollComponent.quantizePitchToScale (strength, scale);
        pianoRollComponent.repaint();
        updatePropertiesFromSelection();
        return true;
    }

    // F = Zoom to fit
    if (key == juce::KeyPress ('f') && showPianoRoll)
    {
        pianoRollComponent.zoomToFit();
        return true;
    }

    // Escape = Stop playback
    if (key == juce::KeyPress::escapeKey)
    {
        if (state == PluginState::PLAYING)
        {
            processorRef.stopPlayback();
            return true;
        }
        return false;
    }

    return false;
}
