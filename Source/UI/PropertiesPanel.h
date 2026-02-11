#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Data/NoteData.h"
#include <vector>

class PropertiesPanel : public juce::Component,
                        private juce::Slider::Listener,
                        private juce::TextEditor::Listener
{
public:
    PropertiesPanel();

    // Update display for a single note (nullptr = no selection)
    void setSelectedNote (const Note* note);

    // Update display for multiple selected notes
    void setSelectedNotes (const std::vector<const Note*>& notes);

    // Callback when a property slider changes
    std::function<void (int noteId, float pitchShift, float formantShift,
                        double timeOffset, double durationScale, float gainDB,
                        float vibratoDepthScale, float driftScale,
                        float sibilantGainDB, float fadeInMs, float fadeOutMs)> onPropertyChanged;

    // Multi-note callback: provides the delta to apply to all selected notes
    std::function<void (float pitchDelta, float formantDelta, float gainDelta,
                        float vibratoDelta, float driftDelta, float sibilantGainDelta,
                        float fadeInDelta, float fadeOutDelta)> onMultiPropertyChanged;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void sliderValueChanged (juce::Slider* slider) override;
    void textEditorReturnKeyPressed (juce::TextEditor& editor) override;
    void textEditorFocusLost (juce::TextEditor& editor) override;

    // Parse pitch input: note name ("C4", "Bb3") or relative shift ("+2", "-1.5st")
    // Returns the target pitch shift in semitones, or NaN if invalid.
    float parsePitchInput (const juce::String& text, int currentMidiNote) const;

    juce::Label noteLabel;
    juce::TextEditor pitchInputField;
    juce::Slider pitchShiftSlider;
    juce::Slider formantShiftSlider;
    juce::Slider gainSlider;
    juce::Slider vibratoSlider;
    juce::Slider driftSlider;
    juce::Slider sibilantGainSlider;
    juce::Slider fadeInSlider;
    juce::Slider fadeOutSlider;
    juce::Label pitchLabel;
    juce::Label formantLabel;
    juce::Label gainLabel;
    juce::Label vibratoLabel;
    juce::Label driftLabel;
    juce::Label sibilantGainLabel;
    juce::Label fadeInLabel;
    juce::Label fadeOutLabel;

    int currentNoteId = -1;
    int currentMidiNote = 60;
    int selectionCount = 0;
    bool updatingFromNote = false;

    // Current note state for callback
    double currentTimeOffset = 0.0;
    double currentDurationScale = 1.0;

    // Track previous slider values for delta calculation in multi-select mode
    float prevPitchValue = 0.0f;
    float prevFormantValue = 0.0f;
    float prevGainValue = 0.0f;
    float prevVibratoValue = 1.0f;
    float prevDriftValue = 1.0f;
    float prevSibilantGainValue = 0.0f;
    float prevFadeInValue = 0.0f;
    float prevFadeOutValue = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PropertiesPanel)
};
