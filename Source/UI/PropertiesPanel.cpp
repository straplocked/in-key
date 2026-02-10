#include "PropertiesPanel.h"
#include "InKeyLookAndFeel.h"

PropertiesPanel::PropertiesPanel()
{
    // Note name label
    noteLabel.setColour (juce::Label::textColourId, InKeyColours::textPrimary);
    noteLabel.setFont (juce::Font (14.0f));
    noteLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (noteLabel);

    // Pitch shift slider — now supports cent-level precision
    pitchLabel.setText ("Pitch", juce::dontSendNotification);
    pitchLabel.setColour (juce::Label::textColourId, InKeyColours::textSecondary);
    pitchLabel.setFont (juce::Font (11.0f));
    pitchLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (pitchLabel);

    pitchShiftSlider.setRange (-24.0, 24.0, 0.01);
    pitchShiftSlider.setValue (0.0, juce::dontSendNotification);
    pitchShiftSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    pitchShiftSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 20);
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

    // Volume slider
    gainLabel.setText ("Vol", juce::dontSendNotification);
    gainLabel.setColour (juce::Label::textColourId, InKeyColours::textSecondary);
    gainLabel.setFont (juce::Font (11.0f));
    gainLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (gainLabel);

    gainSlider.setRange (-24.0, 12.0, 0.1);
    gainSlider.setValue (0.0, juce::dontSendNotification);
    gainSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    gainSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 45, 20);
    gainSlider.setColour (juce::Slider::thumbColourId, juce::Colour (0xff4caf50));
    gainSlider.setTextValueSuffix (" dB");
    gainSlider.addListener (this);
    addAndMakeVisible (gainSlider);

    // Drift (vibrato) slider
    driftLabel.setText ("Drift", juce::dontSendNotification);
    driftLabel.setColour (juce::Label::textColourId, InKeyColours::textSecondary);
    driftLabel.setFont (juce::Font (11.0f));
    driftLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (driftLabel);

    driftSlider.setRange (0.0, 2.0, 0.01);
    driftSlider.setValue (1.0, juce::dontSendNotification);
    driftSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    driftSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 35, 20);
    driftSlider.setColour (juce::Slider::thumbColourId, juce::Colour (0xffff9800));
    driftSlider.addListener (this);
    addAndMakeVisible (driftSlider);
}

void PropertiesPanel::setSelectedNote (const Note* note)
{
    updatingFromNote = true;
    selectionCount = (note != nullptr) ? 1 : 0;

    if (note == nullptr)
    {
        currentNoteId = -1;
        noteLabel.setText ("", juce::dontSendNotification);
        pitchShiftSlider.setValue (0.0, juce::dontSendNotification);
        formantShiftSlider.setValue (0.0, juce::dontSendNotification);
        gainSlider.setValue (0.0, juce::dontSendNotification);
        driftSlider.setValue (1.0, juce::dontSendNotification);
        pitchShiftSlider.setEnabled (false);
        formantShiftSlider.setEnabled (false);
        gainSlider.setEnabled (false);
        driftSlider.setEnabled (false);
        prevPitchValue = 0.0f;
        prevFormantValue = 0.0f;
        prevGainValue = 0.0f;
        prevDriftValue = 1.0f;
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
        gainSlider.setValue (note->gainDB, juce::dontSendNotification);
        driftSlider.setValue (note->pitchDriftScale, juce::dontSendNotification);
        pitchShiftSlider.setEnabled (true);
        formantShiftSlider.setEnabled (true);
        gainSlider.setEnabled (true);
        driftSlider.setEnabled (true);
        prevPitchValue = note->pitchShiftSemitones;
        prevFormantValue = note->formantShiftSemitones;
        prevGainValue = note->gainDB;
        prevDriftValue = note->pitchDriftScale;
    }

    updatingFromNote = false;
}

void PropertiesPanel::setSelectedNotes (const std::vector<const Note*>& selectedNotes)
{
    if (selectedNotes.empty())
    {
        setSelectedNote (nullptr);
        return;
    }

    if (selectedNotes.size() == 1)
    {
        setSelectedNote (selectedNotes[0]);
        return;
    }

    updatingFromNote = true;
    selectionCount = static_cast<int> (selectedNotes.size());
    currentNoteId = selectedNotes[0]->id;

    noteLabel.setText (juce::String (selectionCount) + " notes selected", juce::dontSendNotification);

    // Show average values
    float avgPitch = 0.0f;
    float avgFormant = 0.0f;
    float avgGain = 0.0f;
    float avgDrift = 0.0f;
    for (const auto* note : selectedNotes)
    {
        avgPitch += note->pitchShiftSemitones;
        avgFormant += note->formantShiftSemitones;
        avgGain += note->gainDB;
        avgDrift += note->pitchDriftScale;
    }
    avgPitch /= static_cast<float> (selectionCount);
    avgFormant /= static_cast<float> (selectionCount);
    avgGain /= static_cast<float> (selectionCount);
    avgDrift /= static_cast<float> (selectionCount);

    pitchShiftSlider.setValue (avgPitch, juce::dontSendNotification);
    formantShiftSlider.setValue (avgFormant, juce::dontSendNotification);
    gainSlider.setValue (avgGain, juce::dontSendNotification);
    driftSlider.setValue (avgDrift, juce::dontSendNotification);
    pitchShiftSlider.setEnabled (true);
    formantShiftSlider.setEnabled (true);
    gainSlider.setEnabled (true);
    driftSlider.setEnabled (true);
    prevPitchValue = avgPitch;
    prevFormantValue = avgFormant;
    prevGainValue = avgGain;
    prevDriftValue = avgDrift;

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

    auto formantArea = area.removeFromLeft (220);
    formantLabel.setBounds (formantArea.removeFromLeft (55));
    formantShiftSlider.setBounds (formantArea);

    area.removeFromLeft (10);

    auto gainArea = area.removeFromLeft (170);
    gainLabel.setBounds (gainArea.removeFromLeft (28));
    gainSlider.setBounds (gainArea);

    area.removeFromLeft (6);

    auto driftArea = area.removeFromLeft (150);
    driftLabel.setBounds (driftArea.removeFromLeft (32));
    driftSlider.setBounds (driftArea);
}

void PropertiesPanel::sliderValueChanged (juce::Slider* slider)
{
    if (updatingFromNote || currentNoteId < 0)
        return;

    if (slider == &pitchShiftSlider || slider == &formantShiftSlider
        || slider == &gainSlider || slider == &driftSlider)
    {
        float pitchShift = static_cast<float> (pitchShiftSlider.getValue());
        float formantShift = static_cast<float> (formantShiftSlider.getValue());
        float gain = static_cast<float> (gainSlider.getValue());
        float drift = static_cast<float> (driftSlider.getValue());

        if (selectionCount > 1 && onMultiPropertyChanged)
        {
            // Multi-note mode: apply delta
            float pitchDelta = pitchShift - prevPitchValue;
            float formantDelta = formantShift - prevFormantValue;
            float gainDelta = gain - prevGainValue;
            float driftDelta = drift - prevDriftValue;
            prevPitchValue = pitchShift;
            prevFormantValue = formantShift;
            prevGainValue = gain;
            prevDriftValue = drift;

            if (std::abs (pitchDelta) > 0.001f || std::abs (formantDelta) > 0.001f
                || std::abs (gainDelta) > 0.001f || std::abs (driftDelta) > 0.001f)
                onMultiPropertyChanged (pitchDelta, formantDelta, gainDelta, driftDelta);
        }
        else if (onPropertyChanged)
        {
            onPropertyChanged (currentNoteId, pitchShift, formantShift,
                               currentTimeOffset, currentDurationScale, gain, drift);
        }
    }
}
