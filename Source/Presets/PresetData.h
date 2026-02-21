// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 InKey contributors
// This file is part of InKey. See LICENSE for details.

#pragma once

#include <juce_core/juce_core.h>

struct PresetData
{
    juce::String name = "Default";

    // Pitch detection
    float yinThreshold    = 0.15f;
    float minFrequency    = 80.0f;
    float maxFrequency    = 800.0f;

    // Note segmentation
    float confidenceThreshold = 0.3f;
    double minNoteDuration    = 0.05;

    // Scale
    int  scaleRootNote = 0;           // 0-11
    int  scaleTypeId   = 1;           // 1-9 (matches ComboBox IDs)
    bool customScaleDegrees[12] = { true, true, true, true, true, true,
                                    true, true, true, true, true, true };

    // Quantization
    float quantizeStrength = 100.0f;  // 0-100%

    // Time quantization
    double bpm         = 120.0;
    int    subdivisionId = 3;         // 1-5

    // Tuning
    float a4Reference = 440.0f;

    juce::ValueTree toValueTree() const;
    static PresetData fromValueTree (const juce::ValueTree& tree);
};
