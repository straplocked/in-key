#include "PropertiesPanel.h"
#include "InKeyLookAndFeel.h"

PropertiesPanel::PropertiesPanel()
{
    // Note name label
    noteLabel.setColour (juce::Label::textColourId, InKeyColours::textPrimary);
    noteLabel.setFont (juce::Font (14.0f));
    noteLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (noteLabel);

    // Pitch shift slider
    pitchLabel.setText ("Pitch", juce::dontSendNotification);
    pitchLabel.setColour (juce::Label::textColourId, InKeyColours::textSecondary);
    pitchLabel.setFont (juce::Font (11.0f));
    pitchLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (pitchLabel);

    pitchShiftSlider.setRange (-24.0, 24.0, 1.0);
    pitchShiftSlider.setValue (0.0, juce::dontSendNotification);
    pitchShiftSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    pitchShiftSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 40, 20);
    pitchShiftSlider.setColour (juce::Slider::thumbColourId, InKeyColours::accentUndo);
    pitchShiftSlider.setTextValueSuffix (" st");
    pitchShiftSlider.addListener (this);
    addAndMakeVisible (pitchShiftSlider);

    // Formant shift slider
    formantLabel.setText ("Formant", juce::dontSendNotification);
    formantLabel.setColour (juce::Label::textColourId, InKeyColours::textSecondary);
    formantLabel.setFont (juce::Font (11.0f));
    formantLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (formantLabel);

    formantShiftSlider.setRange (-12.0, 12.0, 0.5);
    formantShiftSlider.setValue (0.0, juce::dontSendNotification);
    formantShiftSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    formantShiftSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 40, 20);
    formantShiftSlider.setColour (juce::Slider::thumbColourId, InKeyColours::accentQuantize);
    formantShiftSlider.setTextValueSuffix (" st");
    formantShiftSlider.addListener (this);
    addAndMakeVisible (formantShiftSlider);
}

void PropertiesPanel::setSelectedNote (const Note* note)
{
    updatingFromNote = true;

    if (note == nullptr)
    {
        currentNoteId = -1;
        noteLabel.setText ("", juce::dontSendNotification);
        pitchShiftSlider.setValue (0.0, juce::dontSendNotification);
        formantShiftSlider.setValue (0.0, juce::dontSendNotification);
        pitchShiftSlider.setEnabled (false);
        formantShiftSlider.setEnabled (false);
    }
    else
    {
        currentNoteId = note->id;
        currentTimeOffset = note->timeOffset;
        currentDurationScale = note->durationScale;

        int displayMidi = note->midiNote + static_cast<int> (std::round (note->pitchShiftSemitones));
        juce::String info = juce::String (Note::midiToNoteName (displayMidi))
                            + juce::String (Note::midiToOctave (displayMidi))
                            + " (" + juce::String (note->originalPitch, 1) + " Hz)";
        noteLabel.setText (info, juce::dontSendNotification);

        pitchShiftSlider.setValue (note->pitchShiftSemitones, juce::dontSendNotification);
        formantShiftSlider.setValue (note->formantShiftSemitones, juce::dontSendNotification);
        pitchShiftSlider.setEnabled (true);
        formantShiftSlider.setEnabled (true);
    }

    updatingFromNote = false;
}

void PropertiesPanel::paint (juce::Graphics& g)
{
    InKeyLookAndFeel::drawGlassPanel (g, getLocalBounds().toFloat(), 0.0f, true);
}

void PropertiesPanel::resized()
{
    auto area = getLocalBounds().reduced (8, 4);

    noteLabel.setBounds (area.removeFromLeft (120));

    auto pitchArea = area.removeFromLeft (260);
    pitchLabel.setBounds (pitchArea.removeFromLeft (40));
    pitchShiftSlider.setBounds (pitchArea);

    area.removeFromLeft (10);

    auto formantArea = area.removeFromLeft (260);
    formantLabel.setBounds (formantArea.removeFromLeft (55));
    formantShiftSlider.setBounds (formantArea);
}

void PropertiesPanel::sliderValueChanged (juce::Slider* slider)
{
    if (updatingFromNote || currentNoteId < 0)
        return;

    if (onPropertyChanged)
    {
        float pitchShift = static_cast<float> (pitchShiftSlider.getValue());
        float formantShift = static_cast<float> (formantShiftSlider.getValue());

        // Check which slider changed
        if (slider == &pitchShiftSlider || slider == &formantShiftSlider)
        {
            onPropertyChanged (currentNoteId, pitchShift, formantShift,
                               currentTimeOffset, currentDurationScale);
        }
    }
}
