// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 InKey contributors
// This file is part of InKey. See LICENSE for details.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Analysis/PitchDetector.h"
#include "../Analysis/NoteSegmenter.h"

class DetectionSettingsDialog : public juce::Component
{
public:
    DetectionSettingsDialog (PitchDetector::Settings pitchSettings,
                              NoteSegmenter::Settings segSettings)
    {
        setSize (340, 280);

        titleLabel.setText ("Detection Settings", juce::dontSendNotification);
        titleLabel.setFont (juce::Font (16.0f).boldened());
        titleLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (titleLabel);

        // YIN threshold
        addSlider (yinThresholdSlider, yinThresholdLabel, "YIN Threshold",
                   0.05, 0.5, 0.01, pitchSettings.threshold);

        // Min frequency
        addSlider (minFreqSlider, minFreqLabel, "Min Freq (Hz)",
                   40.0, 200.0, 1.0, pitchSettings.minFrequency);

        // Max frequency
        addSlider (maxFreqSlider, maxFreqLabel, "Max Freq (Hz)",
                   400.0, 1200.0, 10.0, pitchSettings.maxFrequency);

        // Confidence threshold
        addSlider (confidenceSlider, confidenceLabel, "Confidence",
                   0.1, 0.8, 0.05, segSettings.confidenceThreshold);

        // Min note duration
        addSlider (minDurationSlider, minDurationLabel, "Min Duration (ms)",
                   10.0, 200.0, 5.0, segSettings.minNoteDuration * 1000.0);

        // Re-analyze button
        reanalyzeButton.setButtonText ("Re-analyze");
        reanalyzeButton.onClick = [this]()
        {
            if (onReanalyze)
                onReanalyze (getPitchSettings(), getSegmenterSettings());
        };
        addAndMakeVisible (reanalyzeButton);
    }

    PitchDetector::Settings getPitchSettings() const
    {
        PitchDetector::Settings s;
        s.threshold = static_cast<float> (yinThresholdSlider.getValue());
        s.minFrequency = static_cast<float> (minFreqSlider.getValue());
        s.maxFrequency = static_cast<float> (maxFreqSlider.getValue());
        return s;
    }

    NoteSegmenter::Settings getSegmenterSettings() const
    {
        NoteSegmenter::Settings s;
        s.confidenceThreshold = static_cast<float> (confidenceSlider.getValue());
        s.minNoteDuration = minDurationSlider.getValue() / 1000.0;
        return s;
    }

    std::function<void (PitchDetector::Settings, NoteSegmenter::Settings)> onReanalyze;

    void resized() override
    {
        auto area = getLocalBounds().reduced (12);
        titleLabel.setBounds (area.removeFromTop (28));
        area.removeFromTop (4);

        auto addRow = [&] (juce::Label& label, juce::Slider& slider)
        {
            auto row = area.removeFromTop (34);
            label.setBounds (row.removeFromLeft (120));
            slider.setBounds (row);
            area.removeFromTop (2);
        };

        addRow (yinThresholdLabel, yinThresholdSlider);
        addRow (minFreqLabel, minFreqSlider);
        addRow (maxFreqLabel, maxFreqSlider);
        addRow (confidenceLabel, confidenceSlider);
        addRow (minDurationLabel, minDurationSlider);

        area.removeFromTop (6);
        reanalyzeButton.setBounds (area.removeFromTop (30).reduced (40, 0));
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff2a2a3a));
        g.setColour (juce::Colour (0xff555577));
        g.drawRect (getLocalBounds(), 1);
    }

private:
    void addSlider (juce::Slider& slider, juce::Label& label,
                    const juce::String& name, double min, double max,
                    double step, double value)
    {
        label.setText (name, juce::dontSendNotification);
        label.setFont (juce::Font (12.0f));
        label.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (label);

        slider.setRange (min, max, step);
        slider.setValue (value, juce::dontSendNotification);
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 20);
        addAndMakeVisible (slider);
    }

    juce::Label titleLabel;
    juce::Slider yinThresholdSlider;
    juce::Slider minFreqSlider;
    juce::Slider maxFreqSlider;
    juce::Slider confidenceSlider;
    juce::Slider minDurationSlider;
    juce::Label yinThresholdLabel;
    juce::Label minFreqLabel;
    juce::Label maxFreqLabel;
    juce::Label confidenceLabel;
    juce::Label minDurationLabel;
    juce::TextButton reanalyzeButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DetectionSettingsDialog)
};
