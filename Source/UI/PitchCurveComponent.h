// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 InKey contributors
// This file is part of InKey. See LICENSE for details.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Data/PitchData.h"

class PitchCurveComponent : public juce::Component
{
public:
    PitchCurveComponent();

    void setPitchData (const PitchCurve& data, double audioDuration);
    void clear();

    void paint (juce::Graphics& g) override;

private:
    PitchCurve pitchData;
    double totalDuration = 0.0;

    // Map frequency (Hz) to Y position using log scale
    float frequencyToY (float freqHz, float height) const;

    // Pitch display range
    static constexpr float minDisplayFreq = 60.0f;   // ~B1
    static constexpr float maxDisplayFreq = 1000.0f;  // ~B5

    // Confidence threshold — don't draw below this
    static constexpr float confidenceThreshold = 0.3f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchCurveComponent)
};
