#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Data/NoteData.h"
#include <vector>

class PropertiesPanel : public juce::Component,
                        private juce::Slider::Listener
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
                        float pitchDriftScale)> onPropertyChanged;

    // Multi-note callback: provides the delta to apply to all selected notes
    std::function<void (float pitchDelta, float formantDelta, float gainDelta,
                        float driftDelta)> onMultiPropertyChanged;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void sliderValueChanged (juce::Slider* slider) override;

    juce::Label noteLabel;
    juce::Slider pitchShiftSlider;
    juce::Slider formantShiftSlider;
    juce::Slider gainSlider;
    juce::Slider driftSlider;
    juce::Label pitchLabel;
    juce::Label formantLabel;
    juce::Label gainLabel;
    juce::Label driftLabel;

    int currentNoteId = -1;
    int selectionCount = 0;
    bool updatingFromNote = false;

    // Current note state for callback
    double currentTimeOffset = 0.0;
    double currentDurationScale = 1.0;

    // Track previous slider values for delta calculation in multi-select mode
    float prevPitchValue = 0.0f;
    float prevFormantValue = 0.0f;
    float prevGainValue = 0.0f;
    float prevDriftValue = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PropertiesPanel)
};
