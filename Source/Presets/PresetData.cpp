// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 InKey contributors
// This file is part of InKey. See LICENSE for details.

#include "PresetData.h"

juce::ValueTree PresetData::toValueTree() const
{
    juce::ValueTree tree ("InKeyPreset");
    tree.setProperty ("version", 1, nullptr);
    tree.setProperty ("name", name, nullptr);

    // Pitch detection
    tree.setProperty ("yinThreshold", static_cast<double> (yinThreshold), nullptr);
    tree.setProperty ("minFrequency", static_cast<double> (minFrequency), nullptr);
    tree.setProperty ("maxFrequency", static_cast<double> (maxFrequency), nullptr);

    // Note segmentation
    tree.setProperty ("confidenceThreshold", static_cast<double> (confidenceThreshold), nullptr);
    tree.setProperty ("minNoteDuration", minNoteDuration, nullptr);

    // Scale
    tree.setProperty ("scaleRootNote", scaleRootNote, nullptr);
    tree.setProperty ("scaleTypeId", scaleTypeId, nullptr);

    // Custom scale degrees
    juce::ValueTree customScale ("CustomScale");
    for (int i = 0; i < 12; ++i)
        customScale.setProperty ("d" + juce::String (i), customScaleDegrees[i], nullptr);
    tree.appendChild (customScale, nullptr);

    // Quantization
    tree.setProperty ("quantizeStrength", static_cast<double> (quantizeStrength), nullptr);

    // Time quantization
    tree.setProperty ("bpm", bpm, nullptr);
    tree.setProperty ("subdivisionId", subdivisionId, nullptr);

    // Tuning
    tree.setProperty ("a4Reference", static_cast<double> (a4Reference), nullptr);

    return tree;
}

PresetData PresetData::fromValueTree (const juce::ValueTree& tree)
{
    PresetData data;

    if (! tree.isValid() || tree.getType().toString() != "InKeyPreset")
        return data;

    data.name = tree.getProperty ("name", "Default").toString();

    // Pitch detection
    data.yinThreshold = static_cast<float> ((double) tree.getProperty ("yinThreshold", 0.15));
    data.minFrequency = static_cast<float> ((double) tree.getProperty ("minFrequency", 80.0));
    data.maxFrequency = static_cast<float> ((double) tree.getProperty ("maxFrequency", 800.0));

    // Note segmentation
    data.confidenceThreshold = static_cast<float> ((double) tree.getProperty ("confidenceThreshold", 0.3));
    data.minNoteDuration = tree.getProperty ("minNoteDuration", 0.05);

    // Scale
    data.scaleRootNote = tree.getProperty ("scaleRootNote", 0);
    data.scaleTypeId = tree.getProperty ("scaleTypeId", 1);

    // Custom scale degrees
    auto customScale = tree.getChildWithName ("CustomScale");
    if (customScale.isValid())
    {
        for (int i = 0; i < 12; ++i)
            data.customScaleDegrees[i] = customScale.getProperty ("d" + juce::String (i), true);
    }

    // Quantization
    data.quantizeStrength = static_cast<float> ((double) tree.getProperty ("quantizeStrength", 100.0));

    // Time quantization
    data.bpm = tree.getProperty ("bpm", 120.0);
    data.subdivisionId = tree.getProperty ("subdivisionId", 3);

    // Tuning
    data.a4Reference = static_cast<float> ((double) tree.getProperty ("a4Reference", 440.0));

    return data;
}
