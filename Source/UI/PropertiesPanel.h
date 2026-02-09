#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Data/NoteData.h"

class PropertiesPanel : public juce::Component,
                        private juce::Slider::Listener
{
public:
    PropertiesPanel();

    // Update display for the given note (nullptr = no selection)
    void setSelectedNote (const Note* note);

    // Callback when a property slider changes
    std::function<void (int noteId, float pitchShift, float formantShift,
                        double timeOffset, double durationScale)> onPropertyChanged;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void sliderValueChanged (juce::Slider* slider) override;

    juce::Label noteLabel;
    juce::Slider pitchShiftSlider;
    juce::Slider formantShiftSlider;
    juce::Label pitchLabel;
    juce::Label formantLabel;

    int currentNoteId = -1;
    bool updatingFromNote = false;

    // Current note state for callback
    double currentTimeOffset = 0.0;
    double currentDurationScale = 1.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PropertiesPanel)
};
