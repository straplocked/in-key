#pragma once

#include "PluginProcessor.h"
#include "UI/InKeyLookAndFeel.h"
#include "UI/WaveformComponent.h"
#include "UI/PitchCurveComponent.h"
#include "UI/PianoRollComponent.h"
#include "UI/PropertiesPanel.h"

class InKeyEditor : public juce::AudioProcessorEditor,
                    private juce::Timer,
                    private PitchAnalyzer::Listener,
                    private PianoRollComponent::Listener
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

    // PianoRollComponent::Listener
    void noteSelected (const Note* note) override;
    void noteEdited (const Note& note) override;

    // Update properties panel from current selection
    void updatePropertiesFromSelection();

    // Multi-take helpers
    void updateTakeSelector();
    void switchToTake (int index);
    void performAlignment();

    InKeyLookAndFeel lookAndFeel;
    InKeyProcessor& processorRef;

    juce::TextButton captureButton;
    juce::TextButton playButton;
    juce::TextButton undoButton;
    juce::TextButton redoButton;
    juce::TextButton quantizeButton;
    juce::TextButton resetButton;
    juce::TextButton alignButton;
    juce::ComboBox takeSelector;
    juce::Label statusLabel;

    WaveformComponent waveformComponent;
    PitchCurveComponent pitchCurveComponent;
    PianoRollComponent pianoRollComponent;
    PropertiesPanel propertiesPanel;

    bool showPianoRoll = false;

    juce::TooltipWindow tooltipWindow { this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InKeyEditor)
};
