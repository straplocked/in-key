// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 InKey contributors
// This file is part of InKey. See LICENSE for details.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Presets/PresetManager.h"
#include "../Presets/PresetData.h"

class PresetPanel : public juce::Component
{
public:
    explicit PresetPanel (PresetManager& manager);

    void resized() override;

    void refreshPresetList();

    std::function<PresetData()>            onGatherSettings;
    std::function<void(const PresetData&)> onApplyPreset;

private:
    void savePreset();
    void loadPreset();
    void deletePreset();

    PresetManager& presetManager;
    juce::ComboBox presetSelector;
    juce::TextButton saveButton;
    juce::TextButton deleteButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetPanel)
};
