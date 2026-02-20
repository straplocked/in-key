// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 InKey contributors
// This file is part of InKey. See LICENSE for details.

#pragma once

#include "PluginProcessor.h"
#include "Editing/QuantizationEngine.h"
#include "UI/InKeyLookAndFeel.h"
#include "UI/WaveformComponent.h"
#include "UI/PitchCurveComponent.h"
#include "UI/PianoRollComponent.h"
#include "UI/PropertiesPanel.h"
#include "UI/DetectionSettingsDialog.h"
#include "ARA/InKeyDocumentController.h"

class InKeyEditor : public juce::AudioProcessorEditor,
                    private juce::Timer,
                    private PitchAnalyzer::Listener,
                    private PianoRollComponent::Listener,
                    private InKeyDocumentController::Listener
{
public:
    explicit InKeyEditor (InKeyProcessor&);
    ~InKeyEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;

private:
    void timerCallback() override;

    // PitchAnalyzer::Listener
    void analysisComplete() override;
    void analysisProgress (float progress) override;

    // InKeyDocumentController::Listener (ARA mode)
    void araAnalysisComplete (juce::ARAAudioSource* source) override;
    void araAnalysisProgress (juce::ARAAudioSource* source, float progress) override;

    // PianoRollComponent::Listener
    void noteSelected (const Note* note) override;
    void selectionChanged (const std::set<int>& selectedIds) override;
    void noteEdited (const Note& note) override;
    void notesChanged() override;
    void noteAuditionRequested (const Note& note) override;
    void playFromPositionRequested (double timeInSeconds) override;

    // Update properties panel from current selection
    void updatePropertiesFromSelection();

    // Multi-take helpers
    void updateTakeSelector();
    void switchToTake (int index);
    void performAlignment();

    // Build current scale from selector state
    ScaleDefinition getCurrentScale() const;

    InKeyLookAndFeel lookAndFeel;
    InKeyProcessor& processorRef;

    juce::TextButton captureButton;
    juce::TextButton playButton;
    juce::TextButton undoButton;
    juce::TextButton redoButton;
    juce::TextButton quantizeButton;
    juce::TextButton correctPitchButton;
    juce::TextButton levelButton;
    juce::TextButton resetButton;
    juce::TextButton alignButton;
    juce::TextButton abToggleButton;
    juce::TextButton detectKeyButton;
    juce::TextButton exportMidiButton;
    juce::TextButton settingsButton;
    juce::ComboBox takeSelector;
    juce::ComboBox scaleRootSelector;
    juce::ComboBox scaleTypeSelector;
    juce::Slider quantizeStrengthSlider;
    juce::Slider tuningSlider;
    juce::Label tuningLabel;
    juce::Label statusLabel;

    // Time quantization controls
    juce::TextButton timeQuantizeButton;
    juce::Slider bpmSlider;
    juce::Label bpmLabel;
    juce::ComboBox subdivisionSelector;

    // Custom scale toggle buttons (one per pitch class)
    juce::TextButton customScaleButtons[12];

    WaveformComponent waveformComponent;
    PitchCurveComponent pitchCurveComponent;
    PianoRollComponent pianoRollComponent;
    PropertiesPanel propertiesPanel;

    bool showPianoRoll = false;

    juce::TooltipWindow tooltipWindow { this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InKeyEditor)
};
