// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 InKey contributors
// This file is part of InKey. See LICENSE for details.

#include "PluginEditor.h"
#include "Analysis/KeyDetector.h"
#include "Data/MidiExporter.h"
#if JucePlugin_Enable_ARA
 #include "ARA/InKeyPlaybackRenderer.h"
#endif

InKeyEditor::InKeyEditor (InKeyProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      waveformComponent (p.getAudioCapture()),
      presetPanel (p.getPresetManager())
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
            notesChanged();
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
            notesChanged();
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

    // Correct Pitch button (intelligent, non-linear correction)
    correctPitchButton.setButtonText ("Correct");
    correctPitchButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff5577cc));
    correctPitchButton.onClick = [this]()
    {
        float strength = static_cast<float> (quantizeStrengthSlider.getValue()) / 100.0f;
        auto scale = getCurrentScale();
        pianoRollComponent.intelligentCorrectPitch (strength, scale);
        pianoRollComponent.repaint();
        updatePropertiesFromSelection();
    };
    addChildComponent (correctPitchButton);

    // Auto Level button
    levelButton.setButtonText ("Level");
    levelButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff44aa66));
    levelButton.onClick = [this]()
    {
        int activeTake = processorRef.getActiveTakeIndex();
        if (activeTake < 0 || activeTake >= processorRef.getNumTakes())
            return;

        auto& take = processorRef.getTakes()[static_cast<size_t> (activeTake)];
        float strength = static_cast<float> (quantizeStrengthSlider.getValue()) / 100.0f;
        pianoRollComponent.autoLevelNotes (take.audio.getReadPointer (0),
                                            take.audio.getNumSamples(),
                                            take.sampleRate, strength);
        pianoRollComponent.repaint();
        updatePropertiesFromSelection();
    };
    addChildComponent (levelButton);

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
    scaleTypeSelector.addItem ("Custom", 9);
    scaleTypeSelector.setSelectedId (1, juce::dontSendNotification);
    scaleTypeSelector.onChange = [this]()
    {
        pianoRollComponent.setCurrentScale (getCurrentScale());
        resized(); // re-layout to show/hide custom scale buttons
    };
    addChildComponent (scaleTypeSelector);

    // Custom scale toggle buttons (12 pitch classes)
    {
        const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        for (int i = 0; i < 12; ++i)
        {
            customScaleButtons[i].setButtonText (noteNames[i]);
            customScaleButtons[i].setClickingTogglesState (true);
            customScaleButtons[i].setToggleState (true, juce::dontSendNotification); // all on by default
            customScaleButtons[i].setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff4488cc));
            customScaleButtons[i].onClick = [this]()
            {
                pianoRollComponent.setCurrentScale (getCurrentScale());
            };
            addChildComponent (customScaleButtons[i]);
        }
    }

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

    // A/B toggle button
    abToggleButton.setButtonText ("A/B");
    abToggleButton.setClickingTogglesState (true);
    abToggleButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff555566));
    abToggleButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffcc8844));
    abToggleButton.onClick = [this]()
    {
        processorRef.setBypassEdits (abToggleButton.getToggleState());
    };
    addChildComponent (abToggleButton);

    // Detect Key button
    detectKeyButton.setButtonText ("Detect");
    detectKeyButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff446688));
    detectKeyButton.onClick = [this]()
    {
        auto result = KeyDetector::detect (pianoRollComponent.getNotes());
        scaleRootSelector.setSelectedId (result.rootNote + 1, juce::sendNotification);
        scaleTypeSelector.setSelectedId (result.scaleTypeIndex + 1, juce::sendNotification);
    };
    addChildComponent (detectKeyButton);

    // Settings button (detection settings dialog)
    settingsButton.setButtonText ("Settings");
    settingsButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff666688));
    settingsButton.onClick = [this]()
    {
        auto* dialog = new DetectionSettingsDialog (
            processorRef.getPitchDetectorSettings(),
            processorRef.getNoteSegmenter().getSettings());
        dialog->onReanalyze = [this] (PitchDetector::Settings pitchSettings,
                                        NoteSegmenter::Settings segSettings)
        {
            processorRef.getPitchDetectorSettings() = pitchSettings;
            processorRef.getNoteSegmenter().setSettings (segSettings);
            processorRef.reanalyzeActiveTake();
        };

        juce::DialogWindow::LaunchOptions options;
        options.content.setOwned (dialog);
        options.dialogTitle = "Detection Settings";
        options.dialogBackgroundColour = juce::Colour (0xff2a2a3a);
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = false;
        options.resizable = false;
        options.launchAsync();
    };
    addChildComponent (settingsButton);

    // Tuning (A4 reference) controls
    tuningLabel.setText ("A4", juce::dontSendNotification);
    tuningLabel.setColour (juce::Label::textColourId, InKeyColours::textSecondary);
    tuningLabel.setFont (juce::Font (11.0f));
    tuningLabel.setJustificationType (juce::Justification::centredRight);
    addChildComponent (tuningLabel);

    tuningSlider.setRange (415.0, 465.0, 0.1);
    tuningSlider.setValue (440.0, juce::dontSendNotification);
    tuningSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    tuningSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 45, 20);
    tuningSlider.setTextValueSuffix (" Hz");
    tuningSlider.setColour (juce::Slider::thumbColourId, juce::Colour (0xff888888));
    tuningSlider.setTooltip ("A4 reference frequency (default 440 Hz)");
    tuningSlider.onValueChange = [this]() {
        processorRef.setA4Reference (static_cast<float> (tuningSlider.getValue()));
    };
    addChildComponent (tuningSlider);

    // Time quantization controls
    timeQuantizeButton.setButtonText ("Time Q");
    timeQuantizeButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff7755aa));
    timeQuantizeButton.onClick = [this]()
    {
        double bpm = bpmSlider.getValue();
        int subdivision = 4; // default quarter note
        switch (subdivisionSelector.getSelectedId())
        {
            case 1: subdivision = 1; break;   // Whole
            case 2: subdivision = 2; break;   // Half
            case 3: subdivision = 4; break;   // Quarter
            case 4: subdivision = 8; break;   // Eighth
            case 5: subdivision = 16; break;  // Sixteenth
            default: subdivision = 4; break;
        }
        float strength = static_cast<float> (quantizeStrengthSlider.getValue()) / 100.0f;

        // Get capture start time offset for grid alignment
        double captureStartTime = 0.0;
        int activeTake = processorRef.getActiveTakeIndex();
        if (activeTake >= 0 && activeTake < processorRef.getNumTakes())
        {
            auto& take = processorRef.getTakes()[static_cast<size_t> (activeTake)];
            captureStartTime = static_cast<double> (take.captureStartSampleInDAW) / take.sampleRate;
        }

        pianoRollComponent.quantizeTime (bpm, subdivision, strength, captureStartTime);
        pianoRollComponent.repaint();
        updatePropertiesFromSelection();
    };
    addChildComponent (timeQuantizeButton);

    // BPM label
    bpmLabel.setText ("BPM", juce::dontSendNotification);
    bpmLabel.setColour (juce::Label::textColourId, InKeyColours::textSecondary);
    bpmLabel.setFont (juce::Font (11.0f));
    bpmLabel.setJustificationType (juce::Justification::centredRight);
    addChildComponent (bpmLabel);

    // BPM slider
    bpmSlider.setRange (20.0, 300.0, 0.1);
    bpmSlider.setValue (120.0, juce::dontSendNotification);
    bpmSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    bpmSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 45, 20);
    bpmSlider.setColour (juce::Slider::thumbColourId, juce::Colour (0xff7755aa));
    bpmSlider.setTooltip ("Beats per minute for time quantization grid");
    addChildComponent (bpmSlider);

    // Subdivision selector
    subdivisionSelector.addItem ("Whole", 1);
    subdivisionSelector.addItem ("Half", 2);
    subdivisionSelector.addItem ("Quarter", 3);
    subdivisionSelector.addItem ("Eighth", 4);
    subdivisionSelector.addItem ("16th", 5);
    subdivisionSelector.setSelectedId (3, juce::dontSendNotification); // Default to quarter note
    subdivisionSelector.setTooltip ("Beat grid subdivision for time quantization");
    addChildComponent (subdivisionSelector);

    // Preset panel
    presetPanel.onGatherSettings = [this]() { return gatherCurrentSettings(); };
    presetPanel.onApplyPreset = [this](const PresetData& preset) { applyPreset (preset); };
    addChildComponent (presetPanel);

    // Try to read BPM from DAW transport
    if (auto* playHead = processorRef.getPlayHead())
    {
        if (auto pos = playHead->getPosition())
        {
            if (auto bpm = pos->getBpm())
                bpmSlider.setValue (*bpm, juce::dontSendNotification);
        }
    }

    // MIDI Export button
    exportMidiButton.setButtonText ("MIDI");
    exportMidiButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff448866));
    exportMidiButton.onClick = [this]()
    {
        auto chooser = std::make_shared<juce::FileChooser> (
            "Export MIDI", juce::File::getSpecialLocation (juce::File::userDesktopDirectory),
            "*.mid");

        chooser->launchAsync (juce::FileBrowserComponent::saveMode
                               | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser] (const juce::FileChooser& fc)
            {
                auto result = fc.getResult();
                if (result != juce::File())
                {
                    auto file = result.withFileExtension ("mid");
                    bool ok = MidiExporter::exportToFile (file, pianoRollComponent.getNotes());
                    statusLabel.setText (ok ? "MIDI exported: " + file.getFileName()
                                            : "MIDI export failed",
                                          juce::dontSendNotification);
                }
            });
    };
    addChildComponent (exportMidiButton);

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
                                                 float gainDB, float vibratoDepthScale,
                                                 float driftScale, float sibilantGainDB,
                                                 float fadeInMs, float fadeOutMs)
    {
        pianoRollComponent.editNoteProperty (noteId, pitchShift, formantShift,
                                              timeOffset, durationScale, gainDB,
                                              vibratoDepthScale, driftScale, sibilantGainDB,
                                              fadeInMs, fadeOutMs);
        pianoRollComponent.repaint();
    };

    propertiesPanel.onMultiPropertyChanged = [this] (float pitchDelta, float formantDelta,
                                                      float gainDelta, float vibratoDelta,
                                                      float driftDelta, float sibGainDelta,
                                                      float fadeInDelta, float fadeOutDelta)
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
                float newVibrato = juce::jlimit (0.0f, 2.0f,
                                                   note.vibratoDepthScale + vibratoDelta);
                float newDrift = juce::jlimit (0.0f, 1.0f,
                                                note.driftScale + driftDelta);
                float newSibGain = juce::jlimit (-12.0f, 6.0f,
                                                    note.sibilantGainDB + sibGainDelta);
                float newFadeIn = juce::jlimit (0.0f, 100.0f,
                                                  note.fadeInMs + fadeInDelta);
                float newFadeOut = juce::jlimit (0.0f, 100.0f,
                                                   note.fadeOutMs + fadeOutDelta);
                pianoRollComponent.editNoteProperty (note.id, newPitch, newFormant,
                                                      note.timeOffset, note.durationScale,
                                                      newGain, newVibrato, newDrift,
                                                      newSibGain, newFadeIn, newFadeOut);
            }
        }
        pianoRollComponent.repaint();
    };

    processorRef.getPitchAnalyzer().addListener (this);

    // Restore tuning slider from processor state
    tuningSlider.setValue (static_cast<double> (processorRef.getA4Reference()), juce::dontSendNotification);

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

    // ARA mode: hide capture controls and listen for analysis
#if JucePlugin_Enable_ARA
    if (processorRef.isInARAMode())
    {
        captureButton.setVisible (false);

        if (auto* dc = processorRef.getARADocumentController())
            dc->addListener (this);
    }
#endif

    // Tooltips
    captureButton.setTooltip ("Start/stop audio capture (R)");
    playButton.setTooltip ("Play/stop edited audio (Space)");
    undoButton.setTooltip ("Undo last edit (Cmd+Z)");
    redoButton.setTooltip ("Redo last edit (Cmd+Shift+Z)");
    quantizeButton.setTooltip ("Snap notes to scale (Q)");
    correctPitchButton.setTooltip ("Intelligent pitch correction — far-off notes get more correction");
    levelButton.setTooltip ("Auto-level note volumes to a uniform level");
    resetButton.setTooltip ("Reset all edits to original");
    abToggleButton.setTooltip ("Toggle A/B comparison (B)");
    alignButton.setTooltip ("Align active take to reference using DTW");
    detectKeyButton.setTooltip ("Auto-detect musical key");
    exportMidiButton.setTooltip ("Export notes as MIDI file");
    settingsButton.setTooltip ("Adjust pitch detection and note segmentation settings");
    scaleRootSelector.setTooltip ("Scale root note");
    scaleTypeSelector.setTooltip ("Scale type");
    quantizeStrengthSlider.setTooltip ("Quantize strength (0-100%)");
    timeQuantizeButton.setTooltip ("Snap note start times to beat grid (T)");

    setResizable (true, true);
    setResizeLimits (700, 500, 1800, 1200);
    setSize (900, 700);
    startTimer (50);
}

InKeyEditor::~InKeyEditor()
{
    setLookAndFeel (nullptr);
    stopTimer();

#if JucePlugin_Enable_ARA
    if (auto* dc = processorRef.getARADocumentController())
        dc->removeListener (this);
#endif

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
    correctPitchButton.setBounds (topBar.removeFromLeft (80).reduced (10, 10));
    levelButton.setBounds (topBar.removeFromLeft (70).reduced (10, 10));
    resetButton.setBounds (topBar.removeFromLeft (80).reduced (10, 10));
    abToggleButton.setBounds (topBar.removeFromLeft (55).reduced (10, 10));
    exportMidiButton.setBounds (topBar.removeFromLeft (60).reduced (10, 10));

    statusLabel.setBounds (topBar.reduced (10, 0));

    // Top bar row 2: scale/quantize controls (only visible in EDITING with piano roll)
    auto scaleBar = area.removeFromTop (32);
    presetPanel.setBounds (scaleBar.removeFromLeft (200).reduced (4, 4));
    scaleBar.removeFromLeft (4);
    scaleRootSelector.setBounds (scaleBar.removeFromLeft (65).reduced (4, 4));
    scaleTypeSelector.setBounds (scaleBar.removeFromLeft (120).reduced (4, 4));
    scaleBar.removeFromLeft (8);
    quantizeStrengthSlider.setBounds (scaleBar.removeFromLeft (150).reduced (4, 4));
    scaleBar.removeFromLeft (4);
    detectKeyButton.setBounds (scaleBar.removeFromLeft (65).reduced (4, 4));
    settingsButton.setBounds (scaleBar.removeFromLeft (75).reduced (4, 4));
    scaleBar.removeFromLeft (8);
    tuningLabel.setBounds (scaleBar.removeFromLeft (22).reduced (4, 4));
    tuningSlider.setBounds (scaleBar.removeFromLeft (130).reduced (4, 4));
    scaleBar.removeFromLeft (8);
    timeQuantizeButton.setBounds (scaleBar.removeFromLeft (65).reduced (4, 4));
    bpmLabel.setBounds (scaleBar.removeFromLeft (28).reduced (4, 4));
    bpmSlider.setBounds (scaleBar.removeFromLeft (120).reduced (4, 4));
    subdivisionSelector.setBounds (scaleBar.removeFromLeft (80).reduced (4, 4));

    // Optional row 3: custom scale note toggles (only when "Custom" is selected)
    bool isCustomScale = (scaleTypeSelector.getSelectedId() == 9);
    if (isCustomScale && showPianoRoll)
    {
        auto customRow = area.removeFromTop (28);
        customRow.removeFromLeft (260);
        for (int i = 0; i < 12; ++i)
        {
            int btnWidth = (i == 1 || i == 3 || i == 6 || i == 8 || i == 10) ? 28 : 24; // sharps slightly wider
            customScaleButtons[i].setBounds (customRow.removeFromLeft (btnWidth).reduced (1, 2));
        }
    }
    else
    {
        for (int i = 0; i < 12; ++i)
            customScaleButtons[i].setBounds (0, 0, 0, 0);
    }

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
#if JucePlugin_Enable_ARA
    if (processorRef.isInARAMode())
    {
        // ARA mode: no capture states to manage
        // Update DAW playhead tracking if editing
        if (showPianoRoll)
        {
            if (processorRef.dawIsPlaying.load())
            {
                double playPos = processorRef.dawPlayheadInCapture.load();
                pianoRollComponent.setPlayheadPosition (playPos);
            }
            else
            {
                pianoRollComponent.setPlayheadPosition (-1.0);
            }
        }
        return;
    }
#endif

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
        correctPitchButton.setVisible (false);
        levelButton.setVisible (false);
        resetButton.setVisible (false);
        abToggleButton.setVisible (false);
        detectKeyButton.setVisible (false);
        exportMidiButton.setVisible (false);
        settingsButton.setVisible (false);
        propertiesPanel.setVisible (false);
        scaleRootSelector.setVisible (false);
        scaleTypeSelector.setVisible (false);
        quantizeStrengthSlider.setVisible (false);
        tuningLabel.setVisible (false);
        tuningSlider.setVisible (false);
        timeQuantizeButton.setVisible (false);
        bpmLabel.setVisible (false);
        bpmSlider.setVisible (false);
        subdivisionSelector.setVisible (false);
        presetPanel.setVisible (false);
        for (auto& btn : customScaleButtons) btn.setVisible (false);

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
        correctPitchButton.setVisible (showPianoRoll);
        levelButton.setVisible (showPianoRoll);
        resetButton.setVisible (showPianoRoll);
        abToggleButton.setVisible (showPianoRoll);
        detectKeyButton.setVisible (showPianoRoll);
        exportMidiButton.setVisible (showPianoRoll);
        settingsButton.setVisible (showPianoRoll);
        propertiesPanel.setVisible (showPianoRoll);
        scaleRootSelector.setVisible (showPianoRoll);
        scaleTypeSelector.setVisible (showPianoRoll);
        quantizeStrengthSlider.setVisible (showPianoRoll);
        tuningLabel.setVisible (showPianoRoll);
        tuningSlider.setVisible (showPianoRoll);
        timeQuantizeButton.setVisible (showPianoRoll);
        bpmLabel.setVisible (showPianoRoll);
        bpmSlider.setVisible (showPianoRoll);
        subdivisionSelector.setVisible (showPianoRoll);
        presetPanel.setVisible (showPianoRoll);
        {
            bool customVis = showPianoRoll && (scaleTypeSelector.getSelectedId() == 9);
            for (auto& btn : customScaleButtons) btn.setVisible (customVis);
        }

        // Try to update BPM from DAW transport if still at default
        if (showPianoRoll)
        {
            if (auto* playHead = processorRef.getPlayHead())
            {
                if (auto pos = playHead->getPosition())
                {
                    if (auto bpm = pos->getBpm())
                    {
                        if (*bpm > 0.0 && std::abs (bpmSlider.getValue() - 120.0) < 0.01)
                            bpmSlider.setValue (*bpm, juce::dontSendNotification);
                    }
                }
            }
        }

        // Show take selector and align button when there are 2+ takes
        bool multiTake = processorRef.getNumTakes() > 1;
        takeSelector.setVisible (showPianoRoll && multiTake);
        alignButton.setVisible (showPianoRoll && multiTake
                                 && processorRef.getActiveTakeIndex() != processorRef.getReferenceTakeIndex());

        // Show playhead if DAW transport is playing through our region
        if (processorRef.dawIsPlaying.load())
        {
            double playPos = processorRef.dawPlayheadInCapture.load();
            pianoRollComponent.setPlayheadPosition (playPos);
        }
        else
        {
            pianoRollComponent.setPlayheadPosition (-1.0);
        }

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
        correctPitchButton.setVisible (false);
        levelButton.setVisible (false);
        resetButton.setVisible (false);
        abToggleButton.setVisible (false);
        detectKeyButton.setVisible (false);
        exportMidiButton.setVisible (false);
        settingsButton.setVisible (false);
        alignButton.setVisible (false);
        scaleRootSelector.setVisible (false);
        scaleTypeSelector.setVisible (false);
        quantizeStrengthSlider.setVisible (false);
        tuningLabel.setVisible (false);
        tuningSlider.setVisible (false);
        timeQuantizeButton.setVisible (false);
        bpmLabel.setVisible (false);
        bpmSlider.setVisible (false);
        subdivisionSelector.setVisible (false);
        presetPanel.setVisible (false);
        for (auto& btn : customScaleButtons) btn.setVisible (false);

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
        correctPitchButton.setVisible (false);
        levelButton.setVisible (false);
        resetButton.setVisible (false);
        abToggleButton.setVisible (false);
        detectKeyButton.setVisible (false);
        exportMidiButton.setVisible (false);
        settingsButton.setVisible (false);
        propertiesPanel.setVisible (false);
        scaleRootSelector.setVisible (false);
        scaleTypeSelector.setVisible (false);
        quantizeStrengthSlider.setVisible (false);
        tuningLabel.setVisible (false);
        tuningSlider.setVisible (false);
        timeQuantizeButton.setVisible (false);
        bpmLabel.setVisible (false);
        bpmSlider.setVisible (false);
        subdivisionSelector.setVisible (false);
        presetPanel.setVisible (false);
        for (auto& btn : customScaleButtons) btn.setVisible (false);

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

#if JucePlugin_Enable_ARA
void InKeyEditor::araAnalysisComplete (juce::ARAAudioSource* source)
{
    auto* dc = processorRef.getARADocumentController();
    if (dc == nullptr)
        return;

    auto* analysis = dc->getAnalysis (source);
    if (analysis == nullptr)
        return;

    // Load analysis results into the piano roll
    showPianoRoll = true;
    double duration = static_cast<double> (analysis->numSamples) / analysis->sampleRate;
    pianoRollComponent.setData (analysis->notes, analysis->pitchCurve, duration);
    pianoRollComponent.setVisible (true);
    propertiesPanel.setVisible (true);
    playButton.setVisible (true);
    undoButton.setVisible (true);
    redoButton.setVisible (true);
    quantizeButton.setVisible (true);
    correctPitchButton.setVisible (true);
    levelButton.setVisible (true);
    resetButton.setVisible (true);
    abToggleButton.setVisible (true);
    detectKeyButton.setVisible (true);
    exportMidiButton.setVisible (true);
    settingsButton.setVisible (true);
    scaleRootSelector.setVisible (true);
    scaleTypeSelector.setVisible (true);
    quantizeStrengthSlider.setVisible (true);
    tuningLabel.setVisible (true);
    tuningSlider.setVisible (true);
    timeQuantizeButton.setVisible (true);
    bpmLabel.setVisible (true);
    bpmSlider.setVisible (true);
    subdivisionSelector.setVisible (true);
    waveformComponent.setVisible (false);
    pitchCurveComponent.setVisible (false);

    statusLabel.setText ("ARA: " + juce::String (analysis->notes.size()) + " notes detected",
                         juce::dontSendNotification);
    resized();
}

void InKeyEditor::araAnalysisProgress (juce::ARAAudioSource* /*source*/, float progress)
{
    statusLabel.setText ("Analyzing... " + juce::String (static_cast<int> (progress * 100.0f)) + "%",
                         juce::dontSendNotification);
}
#endif

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

void InKeyEditor::noteAuditionRequested (const Note& note)
{
    // Skip audition if DAW is playing or not in editing state
    if (processorRef.dawIsPlaying.load() || processorRef.getState() != PluginState::EDITING)
        return;

    int activeTake = processorRef.getActiveTakeIndex();
    if (activeTake < 0 || activeTake >= processorRef.getNumTakes())
        return;

    auto& take = processorRef.getTakes()[static_cast<size_t> (activeTake)];

    // Render just this one note
    NoteList singleNote;
    singleNote.push_back (note);

    auto rendered = processorRef.getAudioEngine().render (take.audio, take.sampleRate,
                                                           singleNote, take.pitchCurve);

    if (rendered.getNumSamples() > 0)
    {
        // Trim to just the note's region
        int startSample = static_cast<int> (note.editedStartTime() * take.sampleRate);
        int endSample = static_cast<int> ((note.editedStartTime() + note.editedDuration()) * take.sampleRate);
        startSample = juce::jlimit (0, rendered.getNumSamples(), startSample);
        endSample = juce::jlimit (startSample, rendered.getNumSamples(), endSample);
        int length = endSample - startSample;

        if (length > 0)
        {
            juce::AudioBuffer<float> trimmed (1, length);
            trimmed.copyFrom (0, 0, rendered, 0, startSample, length);
            processorRef.startPreviewPlayback (trimmed, take.sampleRate);
        }
    }
}

void InKeyEditor::playFromPositionRequested (double timeInSeconds)
{
    if (processorRef.getState() == PluginState::EDITING && showPianoRoll)
    {
        processorRef.startPlaybackFromPosition (pianoRollComponent.getNotes(), timeInSeconds);
    }
}

void InKeyEditor::notesChanged()
{
#if JucePlugin_Enable_ARA
    if (processorRef.isInARAMode())
    {
        // ARA mode: update the analysis data with edited notes and re-render
        if (auto* dc = processorRef.getARADocumentController())
        {
            if (auto* araController = dc->getDocumentController())
            {
                // Find the first active ARA source with analysis and update its notes
                if (auto* doc = araController->getDocument<juce::ARADocument>())
                {
                    for (auto* source : doc->getAudioSources())
                    {
                        auto* analysis = dc->getAnalysis (source);
                        if (analysis != nullptr && analysis->analysisComplete.load())
                        {
                            analysis->notes = pianoRollComponent.getNotes();
                            break;
                        }
                    }
                }

                // Signal all playback renderers to re-render
                for (auto* renderer : araController->getPlaybackRenderers<InKeyPlaybackRenderer>())
                {
                    renderer->notifyEditsChanged();
                }
            }
        }
        return;
    }
#endif

    // Save edited notes to active take and request background re-render
    if (processorRef.getActiveTakeIndex() >= 0)
        processorRef.saveActiveTakeNotes (pianoRollComponent.getNotes());

    processorRef.requestRender();
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
        processorRef.requestRender();
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

        processorRef.requestRender();
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
        case 9:
        {
            // Custom scale: read toggle states
            ScaleDefinition custom;
            custom.name = "Custom";
            custom.rootNote = root;
            custom.degrees.resize (12, false);
            for (int i = 0; i < 12; ++i)
                custom.degrees[static_cast<size_t> (i)] = customScaleButtons[i].getToggleState();
            return custom;
        }
        default: return QuantizationEngine::chromaticScale (root);
    }
}

PresetData InKeyEditor::gatherCurrentSettings()
{
    PresetData data;

    // Pitch detection
    const auto& pitchSettings = processorRef.getPitchDetectorSettings();
    data.yinThreshold = pitchSettings.threshold;
    data.minFrequency = pitchSettings.minFrequency;
    data.maxFrequency = pitchSettings.maxFrequency;

    // Note segmentation
    const auto& segSettings = processorRef.getNoteSegmenter().getSettings();
    data.confidenceThreshold = segSettings.confidenceThreshold;
    data.minNoteDuration = segSettings.minNoteDuration;

    // Scale
    data.scaleRootNote = scaleRootSelector.getSelectedId() - 1;
    if (data.scaleRootNote < 0) data.scaleRootNote = 0;
    data.scaleTypeId = scaleTypeSelector.getSelectedId();

    // Custom scale degrees
    for (int i = 0; i < 12; ++i)
        data.customScaleDegrees[i] = customScaleButtons[i].getToggleState();

    // Quantization
    data.quantizeStrength = static_cast<float> (quantizeStrengthSlider.getValue());

    // Time quantization
    data.bpm = bpmSlider.getValue();
    data.subdivisionId = subdivisionSelector.getSelectedId();

    // Tuning
    data.a4Reference = static_cast<float> (tuningSlider.getValue());

    return data;
}

void InKeyEditor::applyPreset (const PresetData& preset)
{
    // Pitch detection
    auto& pitchSettings = processorRef.getPitchDetectorSettings();
    pitchSettings.threshold = preset.yinThreshold;
    pitchSettings.minFrequency = preset.minFrequency;
    pitchSettings.maxFrequency = preset.maxFrequency;

    // Note segmentation
    NoteSegmenter::Settings segSettings;
    segSettings.confidenceThreshold = preset.confidenceThreshold;
    segSettings.minNoteDuration = preset.minNoteDuration;
    processorRef.getNoteSegmenter().setSettings (segSettings);

    // Scale
    scaleRootSelector.setSelectedId (preset.scaleRootNote + 1, juce::dontSendNotification);
    scaleTypeSelector.setSelectedId (preset.scaleTypeId, juce::dontSendNotification);

    // Custom scale degrees
    for (int i = 0; i < 12; ++i)
        customScaleButtons[i].setToggleState (preset.customScaleDegrees[i], juce::dontSendNotification);

    // Quantization
    quantizeStrengthSlider.setValue (static_cast<double> (preset.quantizeStrength), juce::dontSendNotification);

    // Time quantization
    bpmSlider.setValue (preset.bpm, juce::dontSendNotification);
    subdivisionSelector.setSelectedId (preset.subdivisionId, juce::dontSendNotification);

    // Tuning
    tuningSlider.setValue (static_cast<double> (preset.a4Reference), juce::dontSendNotification);
    processorRef.setA4Reference (preset.a4Reference);

    // Update scale in piano roll
    pianoRollComponent.setCurrentScale (getCurrentScale());

    resized();
    repaint();
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

    // T = Quantize time (using current BPM/subdivision/strength settings)
    if (key == juce::KeyPress ('t') && showPianoRoll && state == PluginState::EDITING)
    {
        timeQuantizeButton.triggerClick();
        return true;
    }

    // F = Zoom to fit
    if (key == juce::KeyPress ('f') && showPianoRoll)
    {
        pianoRollComponent.zoomToFit();
        return true;
    }

    // B = Toggle A/B comparison
    if (key == juce::KeyPress ('b') && showPianoRoll && state == PluginState::EDITING)
    {
        abToggleButton.setToggleState (! abToggleButton.getToggleState(), juce::sendNotification);
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
