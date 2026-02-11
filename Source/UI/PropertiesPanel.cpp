#include "PropertiesPanel.h"
#include "InKeyLookAndFeel.h"

PropertiesPanel::PropertiesPanel()
{
    // Note name label
    noteLabel.setColour (juce::Label::textColourId, InKeyColours::textPrimary);
    noteLabel.setFont (juce::Font (14.0f));
    noteLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (noteLabel);

    // Pitch input text field (accepts note names like "C4" or relative shifts like "+2")
    pitchInputField.setFont (juce::Font (12.0f));
    pitchInputField.setJustification (juce::Justification::centred);
    pitchInputField.setTextToShowWhenEmpty ("C4", juce::Colours::grey);
    pitchInputField.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff333344));
    pitchInputField.setColour (juce::TextEditor::textColourId, InKeyColours::textPrimary);
    pitchInputField.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff555566));
    pitchInputField.setColour (juce::TextEditor::focusedOutlineColourId, InKeyColours::accentPlay);
    pitchInputField.setTooltip ("Type note name (C4, Bb3) or shift (+2, -1.5st)");
    pitchInputField.addListener (this);
    addAndMakeVisible (pitchInputField);

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

    // Vibrato slider (scales periodic component)
    vibratoLabel.setText ("Vib", juce::dontSendNotification);
    vibratoLabel.setColour (juce::Label::textColourId, InKeyColours::textSecondary);
    vibratoLabel.setFont (juce::Font (11.0f));
    vibratoLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (vibratoLabel);

    vibratoSlider.setRange (0.0, 2.0, 0.01);
    vibratoSlider.setValue (1.0, juce::dontSendNotification);
    vibratoSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    vibratoSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 35, 20);
    vibratoSlider.setColour (juce::Slider::thumbColourId, juce::Colour (0xffff9800));
    vibratoSlider.addListener (this);
    addAndMakeVisible (vibratoSlider);

    // Drift slider (scales aperiodic pitch wandering)
    driftLabel.setText ("Drift", juce::dontSendNotification);
    driftLabel.setColour (juce::Label::textColourId, InKeyColours::textSecondary);
    driftLabel.setFont (juce::Font (11.0f));
    driftLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (driftLabel);

    driftSlider.setRange (0.0, 1.0, 0.01);
    driftSlider.setValue (1.0, juce::dontSendNotification);
    driftSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    driftSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 35, 20);
    driftSlider.setColour (juce::Slider::thumbColourId, juce::Colour (0xffcc7700));
    driftSlider.addListener (this);
    addAndMakeVisible (driftSlider);

    // Sibilant gain slider
    sibilantGainLabel.setText ("Sib", juce::dontSendNotification);
    sibilantGainLabel.setColour (juce::Label::textColourId, InKeyColours::textSecondary);
    sibilantGainLabel.setFont (juce::Font (11.0f));
    sibilantGainLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (sibilantGainLabel);

    sibilantGainSlider.setRange (-12.0, 6.0, 0.1);
    sibilantGainSlider.setValue (0.0, juce::dontSendNotification);
    sibilantGainSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    sibilantGainSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 40, 20);
    sibilantGainSlider.setColour (juce::Slider::thumbColourId, juce::Colour (0xff66aacc));
    sibilantGainSlider.setTextValueSuffix (" dB");
    sibilantGainSlider.addListener (this);
    addAndMakeVisible (sibilantGainSlider);

    // Fade In slider
    fadeInLabel.setText ("FdIn", juce::dontSendNotification);
    fadeInLabel.setColour (juce::Label::textColourId, InKeyColours::textSecondary);
    fadeInLabel.setFont (juce::Font (11.0f));
    fadeInLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (fadeInLabel);

    fadeInSlider.setRange (0.0, 100.0, 1.0);
    fadeInSlider.setValue (0.0, juce::dontSendNotification);
    fadeInSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    fadeInSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 35, 20);
    fadeInSlider.setColour (juce::Slider::thumbColourId, juce::Colour (0xff8888cc));
    fadeInSlider.setTextValueSuffix ("ms");
    fadeInSlider.addListener (this);
    addAndMakeVisible (fadeInSlider);

    // Fade Out slider
    fadeOutLabel.setText ("FdOut", juce::dontSendNotification);
    fadeOutLabel.setColour (juce::Label::textColourId, InKeyColours::textSecondary);
    fadeOutLabel.setFont (juce::Font (11.0f));
    fadeOutLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (fadeOutLabel);

    fadeOutSlider.setRange (0.0, 100.0, 1.0);
    fadeOutSlider.setValue (0.0, juce::dontSendNotification);
    fadeOutSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    fadeOutSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 35, 20);
    fadeOutSlider.setColour (juce::Slider::thumbColourId, juce::Colour (0xff8888cc));
    fadeOutSlider.setTextValueSuffix ("ms");
    fadeOutSlider.addListener (this);
    addAndMakeVisible (fadeOutSlider);
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
        vibratoSlider.setValue (1.0, juce::dontSendNotification);
        driftSlider.setValue (1.0, juce::dontSendNotification);
        sibilantGainSlider.setValue (0.0, juce::dontSendNotification);
        fadeInSlider.setValue (0.0, juce::dontSendNotification);
        fadeOutSlider.setValue (0.0, juce::dontSendNotification);
        pitchInputField.setText ("", juce::dontSendNotification);
        pitchInputField.setEnabled (false);
        pitchShiftSlider.setEnabled (false);
        formantShiftSlider.setEnabled (false);
        gainSlider.setEnabled (false);
        vibratoSlider.setEnabled (false);
        driftSlider.setEnabled (false);
        sibilantGainSlider.setEnabled (false);
        fadeInSlider.setEnabled (false);
        fadeOutSlider.setEnabled (false);
        prevPitchValue = 0.0f;
        prevFormantValue = 0.0f;
        prevGainValue = 0.0f;
        prevVibratoValue = 1.0f;
        prevDriftValue = 1.0f;
        prevSibilantGainValue = 0.0f;
        prevFadeInValue = 0.0f;
        prevFadeOutValue = 0.0f;
    }
    else
    {
        currentNoteId = note->id;
        currentMidiNote = note->midiNote;
        currentTimeOffset = note->timeOffset;
        currentDurationScale = note->durationScale;

        int displayMidi = note->midiNote + static_cast<int> (std::round (note->pitchShiftSemitones));
        juce::String info = juce::String (Note::midiToNoteName (displayMidi))
                            + juce::String (Note::midiToOctave (displayMidi))
                            + " (" + juce::String (note->originalPitch, 1) + " Hz)";
        noteLabel.setText (info, juce::dontSendNotification);

        // Update pitch input field with current target note name
        juce::String pitchText = juce::String (Note::midiToNoteName (displayMidi))
                                  + juce::String (Note::midiToOctave (displayMidi));
        float cents = (note->pitchShiftSemitones - std::round (note->pitchShiftSemitones)) * 100.0f;
        if (std::abs (cents) > 0.5f)
            pitchText += (cents > 0 ? "+" : "") + juce::String (static_cast<int> (cents)) + "c";
        pitchInputField.setText (pitchText, juce::dontSendNotification);
        pitchInputField.setEnabled (true);

        pitchShiftSlider.setValue (note->pitchShiftSemitones, juce::dontSendNotification);
        formantShiftSlider.setValue (note->formantShiftSemitones, juce::dontSendNotification);
        gainSlider.setValue (note->gainDB, juce::dontSendNotification);
        vibratoSlider.setValue (note->vibratoDepthScale, juce::dontSendNotification);
        driftSlider.setValue (note->driftScale, juce::dontSendNotification);
        sibilantGainSlider.setValue (note->sibilantGainDB, juce::dontSendNotification);
        fadeInSlider.setValue (note->fadeInMs, juce::dontSendNotification);
        fadeOutSlider.setValue (note->fadeOutMs, juce::dontSendNotification);
        pitchShiftSlider.setEnabled (true);
        formantShiftSlider.setEnabled (true);
        gainSlider.setEnabled (true);
        vibratoSlider.setEnabled (true);
        driftSlider.setEnabled (true);
        sibilantGainSlider.setEnabled (! note->sibilantRegions.empty());
        fadeInSlider.setEnabled (true);
        fadeOutSlider.setEnabled (true);
        prevPitchValue = note->pitchShiftSemitones;
        prevFormantValue = note->formantShiftSemitones;
        prevGainValue = note->gainDB;
        prevVibratoValue = note->vibratoDepthScale;
        prevDriftValue = note->driftScale;
        prevSibilantGainValue = note->sibilantGainDB;
        prevFadeInValue = note->fadeInMs;
        prevFadeOutValue = note->fadeOutMs;
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
    float avgVibrato = 0.0f;
    float avgDrift = 0.0f;
    float avgSibilantGain = 0.0f;
    float avgFadeIn = 0.0f;
    float avgFadeOut = 0.0f;
    bool anySibilants = false;
    for (const auto* note : selectedNotes)
    {
        avgPitch += note->pitchShiftSemitones;
        avgFormant += note->formantShiftSemitones;
        avgGain += note->gainDB;
        avgVibrato += note->vibratoDepthScale;
        avgDrift += note->driftScale;
        avgSibilantGain += note->sibilantGainDB;
        avgFadeIn += note->fadeInMs;
        avgFadeOut += note->fadeOutMs;
        if (! note->sibilantRegions.empty()) anySibilants = true;
    }
    avgPitch /= static_cast<float> (selectionCount);
    avgFormant /= static_cast<float> (selectionCount);
    avgGain /= static_cast<float> (selectionCount);
    avgVibrato /= static_cast<float> (selectionCount);
    avgDrift /= static_cast<float> (selectionCount);
    avgSibilantGain /= static_cast<float> (selectionCount);
    avgFadeIn /= static_cast<float> (selectionCount);
    avgFadeOut /= static_cast<float> (selectionCount);

    pitchShiftSlider.setValue (avgPitch, juce::dontSendNotification);
    formantShiftSlider.setValue (avgFormant, juce::dontSendNotification);
    gainSlider.setValue (avgGain, juce::dontSendNotification);
    vibratoSlider.setValue (avgVibrato, juce::dontSendNotification);
    driftSlider.setValue (avgDrift, juce::dontSendNotification);
    sibilantGainSlider.setValue (avgSibilantGain, juce::dontSendNotification);
    fadeInSlider.setValue (avgFadeIn, juce::dontSendNotification);
    fadeOutSlider.setValue (avgFadeOut, juce::dontSendNotification);
    pitchShiftSlider.setEnabled (true);
    formantShiftSlider.setEnabled (true);
    gainSlider.setEnabled (true);
    vibratoSlider.setEnabled (true);
    driftSlider.setEnabled (true);
    sibilantGainSlider.setEnabled (anySibilants);
    fadeInSlider.setEnabled (true);
    fadeOutSlider.setEnabled (true);
    prevPitchValue = avgPitch;
    prevFormantValue = avgFormant;
    prevGainValue = avgGain;
    prevVibratoValue = avgVibrato;
    prevDriftValue = avgDrift;
    prevSibilantGainValue = avgSibilantGain;
    prevFadeInValue = avgFadeIn;
    prevFadeOutValue = avgFadeOut;

    updatingFromNote = false;
}

void PropertiesPanel::paint (juce::Graphics& g)
{
    InKeyLookAndFeel::drawGlassPanel (g, getLocalBounds().toFloat(), 0.0f, true);
}

void PropertiesPanel::resized()
{
    auto area = getLocalBounds().reduced (8, 4);

    noteLabel.setBounds (area.removeFromLeft (90));
    pitchInputField.setBounds (area.removeFromLeft (55).reduced (2, 6));
    area.removeFromLeft (4);

    auto pitchArea = area.removeFromLeft (240);
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

    auto vibratoArea = area.removeFromLeft (115);
    vibratoLabel.setBounds (vibratoArea.removeFromLeft (24));
    vibratoSlider.setBounds (vibratoArea);

    area.removeFromLeft (2);

    auto driftArea = area.removeFromLeft (115);
    driftLabel.setBounds (driftArea.removeFromLeft (30));
    driftSlider.setBounds (driftArea);

    area.removeFromLeft (2);

    auto sibArea = area.removeFromLeft (130);
    sibilantGainLabel.setBounds (sibArea.removeFromLeft (24));
    sibilantGainSlider.setBounds (sibArea);

    area.removeFromLeft (2);

    auto fadeInArea = area.removeFromLeft (110);
    fadeInLabel.setBounds (fadeInArea.removeFromLeft (28));
    fadeInSlider.setBounds (fadeInArea);

    area.removeFromLeft (2);

    auto fadeOutArea = area.removeFromLeft (115);
    fadeOutLabel.setBounds (fadeOutArea.removeFromLeft (33));
    fadeOutSlider.setBounds (fadeOutArea);
}

void PropertiesPanel::sliderValueChanged (juce::Slider* slider)
{
    if (updatingFromNote || currentNoteId < 0)
        return;

    if (slider == &pitchShiftSlider || slider == &formantShiftSlider
        || slider == &gainSlider || slider == &vibratoSlider || slider == &driftSlider
        || slider == &sibilantGainSlider || slider == &fadeInSlider || slider == &fadeOutSlider)
    {
        float pitchShift = static_cast<float> (pitchShiftSlider.getValue());
        float formantShift = static_cast<float> (formantShiftSlider.getValue());
        float gain = static_cast<float> (gainSlider.getValue());
        float vibrato = static_cast<float> (vibratoSlider.getValue());
        float drift = static_cast<float> (driftSlider.getValue());
        float sibGain = static_cast<float> (sibilantGainSlider.getValue());
        float fadeIn = static_cast<float> (fadeInSlider.getValue());
        float fadeOut = static_cast<float> (fadeOutSlider.getValue());

        if (selectionCount > 1 && onMultiPropertyChanged)
        {
            // Multi-note mode: apply delta
            float pitchDelta = pitchShift - prevPitchValue;
            float formantDelta = formantShift - prevFormantValue;
            float gainDelta = gain - prevGainValue;
            float vibratoDelta = vibrato - prevVibratoValue;
            float driftDelta = drift - prevDriftValue;
            float sibGainDelta = sibGain - prevSibilantGainValue;
            float fadeInDelta = fadeIn - prevFadeInValue;
            float fadeOutDelta = fadeOut - prevFadeOutValue;
            prevPitchValue = pitchShift;
            prevFormantValue = formantShift;
            prevGainValue = gain;
            prevVibratoValue = vibrato;
            prevDriftValue = drift;
            prevSibilantGainValue = sibGain;
            prevFadeInValue = fadeIn;
            prevFadeOutValue = fadeOut;

            if (std::abs (pitchDelta) > 0.001f || std::abs (formantDelta) > 0.001f
                || std::abs (gainDelta) > 0.001f || std::abs (vibratoDelta) > 0.001f
                || std::abs (driftDelta) > 0.001f || std::abs (sibGainDelta) > 0.001f
                || std::abs (fadeInDelta) > 0.001f || std::abs (fadeOutDelta) > 0.001f)
                onMultiPropertyChanged (pitchDelta, formantDelta, gainDelta, vibratoDelta,
                                         driftDelta, sibGainDelta, fadeInDelta, fadeOutDelta);
        }
        else if (onPropertyChanged)
        {
            onPropertyChanged (currentNoteId, pitchShift, formantShift,
                               currentTimeOffset, currentDurationScale, gain,
                               vibrato, drift, sibGain, fadeIn, fadeOut);
        }
    }
}

void PropertiesPanel::textEditorReturnKeyPressed (juce::TextEditor& editor)
{
    if (&editor == &pitchInputField && currentNoteId >= 0 && selectionCount == 1)
    {
        float newPitch = parsePitchInput (editor.getText(), currentMidiNote);
        if (! std::isnan (newPitch))
        {
            pitchShiftSlider.setValue (newPitch, juce::sendNotification);
        }
    }

    // Unfocus the text field
    editor.moveKeyboardFocusToSibling (false);
}

void PropertiesPanel::textEditorFocusLost (juce::TextEditor& editor)
{
    if (&editor == &pitchInputField && currentNoteId >= 0 && selectionCount == 1)
    {
        float newPitch = parsePitchInput (editor.getText(), currentMidiNote);
        if (! std::isnan (newPitch))
        {
            pitchShiftSlider.setValue (newPitch, juce::sendNotification);
        }
    }
}

float PropertiesPanel::parsePitchInput (const juce::String& text, int midiNote) const
{
    auto trimmed = text.trim();
    if (trimmed.isEmpty())
        return std::nanf ("");

    // Check for relative shifts: "+2", "-1.5", "+3st", "-0.5st"
    if (trimmed[0] == '+' || (trimmed[0] == '-' && trimmed.length() > 1))
    {
        auto numStr = trimmed.removeCharacters ("st");
        float value = numStr.getFloatValue();
        if (value != 0.0f || numStr == "0" || numStr == "+0" || numStr == "-0")
            return juce::jlimit (-24.0f, 24.0f, value);
    }

    // Parse note name: C4, C#4, Db3, Bb5, etc.
    static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static const char* flatNames[] = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };

    int parsedNote = -1;
    int nameLen = 0;

    for (int i = 0; i < 12; ++i)
    {
        // Try sharp names first (C#, D#, etc.)
        juce::String sharp (noteNames[i]);
        if (trimmed.startsWithIgnoreCase (sharp) && sharp.length() > nameLen)
        {
            parsedNote = i;
            nameLen = sharp.length();
        }

        // Try flat names (Db, Eb, etc.)
        juce::String flat (flatNames[i]);
        if (trimmed.startsWithIgnoreCase (flat) && flat.length() > nameLen)
        {
            parsedNote = i;
            nameLen = flat.length();
        }
    }

    if (parsedNote < 0 || nameLen >= trimmed.length())
        return std::nanf ("");

    // Parse octave number after note name
    auto octaveStr = trimmed.substring (nameLen);
    int octave = octaveStr.getIntValue();
    if (octave < -1 || octave > 9)
        return std::nanf ("");

    // Convert to MIDI: C4 = 60
    int targetMidi = (octave + 1) * 12 + parsedNote;
    targetMidi = juce::jlimit (0, 127, targetMidi);

    // Compute the pitch shift needed to reach the target from the original
    float shift = static_cast<float> (targetMidi - midiNote);
    return juce::jlimit (-24.0f, 24.0f, shift);
}
