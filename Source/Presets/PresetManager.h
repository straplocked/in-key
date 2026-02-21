// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 InKey contributors
// This file is part of InKey. See LICENSE for details.

#pragma once

#include "PresetData.h"

class PresetManager
{
public:
    PresetManager();

    juce::File getPresetsDirectory() const;

    juce::StringArray getPresetNames() const;

    bool savePreset (const PresetData& preset);

    PresetData loadPreset (const juce::String& name) const;

    bool deletePreset (const juce::String& name);

    bool presetExists (const juce::String& name) const;

private:
    juce::File getPresetFile (const juce::String& name) const;

    juce::File presetsDirectory;
};
