// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 InKey contributors
// This file is part of InKey. See LICENSE for details.

#include "PresetPanel.h"
#include "InKeyLookAndFeel.h"

PresetPanel::PresetPanel (PresetManager& manager)
    : presetManager (manager)
{
    presetSelector.setTooltip ("Select a preset");
    addAndMakeVisible (presetSelector);

    saveButton.setButtonText ("Save");
    saveButton.setColour (juce::TextButton::buttonColourId, InKeyColours::accentQuantize);
    saveButton.setTooltip ("Save current settings as a preset");
    saveButton.onClick = [this]() { savePreset(); };
    addAndMakeVisible (saveButton);

    deleteButton.setButtonText ("Del");
    deleteButton.setColour (juce::TextButton::buttonColourId, InKeyColours::accentReset);
    deleteButton.setTooltip ("Delete the selected preset");
    deleteButton.onClick = [this]() { deletePreset(); };
    addAndMakeVisible (deleteButton);

    refreshPresetList();

    // Set onChange after refreshPresetList to avoid spurious callbacks during init
    presetSelector.onChange = [this]() { loadPreset(); };
}

void PresetPanel::resized()
{
    auto area = getLocalBounds().reduced (2, 2);
    deleteButton.setBounds (area.removeFromRight (36));
    area.removeFromRight (2);
    saveButton.setBounds (area.removeFromRight (44));
    area.removeFromRight (2);
    presetSelector.setBounds (area);
}

void PresetPanel::refreshPresetList()
{
    int currentId = presetSelector.getSelectedId();
    juce::String currentName;
    if (currentId > 1)
        currentName = presetSelector.getText();

    presetSelector.clear (juce::dontSendNotification);
    presetSelector.addItem ("(No Preset)", 1);

    auto names = presetManager.getPresetNames();
    int restoreId = 0;

    for (int i = 0; i < names.size(); ++i)
    {
        int itemId = i + 2;
        presetSelector.addItem (names[i], itemId);

        if (names[i] == currentName)
            restoreId = itemId;
    }

    if (restoreId > 0)
        presetSelector.setSelectedId (restoreId, juce::dontSendNotification);
    else
        presetSelector.setSelectedId (1, juce::dontSendNotification);

    deleteButton.setEnabled (presetSelector.getSelectedId() > 1);
}

void PresetPanel::savePreset()
{
    if (! onGatherSettings)
        return;

    int selectedId = presetSelector.getSelectedId();

    if (selectedId > 1)
    {
        // Overwrite existing preset
        juce::String existingName = presetSelector.getText();

        auto options = juce::MessageBoxOptions()
            .withTitle ("Overwrite Preset")
            .withMessage ("Overwrite preset \"" + existingName + "\"?")
            .withButton ("Overwrite")
            .withButton ("Cancel");

        juce::AlertWindow::showAsync (options,
            [safe = juce::Component::SafePointer<PresetPanel> (this), existingName] (int result)
            {
                if (safe == nullptr) return;
                if (result == 1)
                {
                    auto data = safe->onGatherSettings();
                    data.name = existingName;
                    safe->presetManager.savePreset (data);
                    safe->refreshPresetList();
                }
            });
    }
    else
    {
        // New preset: ask for a name
        auto* aw = new juce::AlertWindow ("Save Preset", "Enter a name for the preset:",
                                           juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("presetName", "", "Name:");
        aw->addButton ("Save", 1);
        aw->addButton ("Cancel", 0);

        aw->enterModalState (true, juce::ModalCallbackFunction::create (
            [safe = juce::Component::SafePointer<PresetPanel> (this), aw] (int result)
            {
                if (result == 1 && safe != nullptr)
                {
                    auto name = aw->getTextEditorContents ("presetName").trim();

                    if (name.isNotEmpty())
                    {
                        if (safe->presetManager.presetExists (name))
                        {
                            auto overwriteOptions = juce::MessageBoxOptions()
                                .withTitle ("Overwrite Preset")
                                .withMessage ("A preset named \"" + name + "\" already exists. Overwrite?")
                                .withButton ("Overwrite")
                                .withButton ("Cancel");

                            juce::AlertWindow::showAsync (overwriteOptions,
                                [safe, name] (int r)
                                {
                                    if (safe == nullptr) return;
                                    if (r == 1)
                                    {
                                        auto data = safe->onGatherSettings();
                                        data.name = name;
                                        safe->presetManager.savePreset (data);
                                        safe->refreshPresetList();
                                    }
                                });
                        }
                        else
                        {
                            auto data = safe->onGatherSettings();
                            data.name = name;
                            safe->presetManager.savePreset (data);
                            safe->refreshPresetList();
                        }
                    }
                }

                delete aw;
            }), false);  // false: we delete aw manually in the callback
    }
}

void PresetPanel::loadPreset()
{
    int selectedId = presetSelector.getSelectedId();
    deleteButton.setEnabled (selectedId > 1);

    if (selectedId <= 1 || ! onApplyPreset)
        return;

    juce::String name = presetSelector.getText();
    auto data = presetManager.loadPreset (name);
    onApplyPreset (data);
}

void PresetPanel::deletePreset()
{
    int selectedId = presetSelector.getSelectedId();

    if (selectedId <= 1)
        return;

    juce::String name = presetSelector.getText();

    auto options = juce::MessageBoxOptions()
        .withTitle ("Delete Preset")
        .withMessage ("Delete preset \"" + name + "\"?")
        .withButton ("Delete")
        .withButton ("Cancel");

    juce::AlertWindow::showAsync (options,
        [safe = juce::Component::SafePointer<PresetPanel> (this), name] (int result)
        {
            if (safe == nullptr) return;
            if (result == 1)
            {
                safe->presetManager.deletePreset (name);
                safe->refreshPresetList();
            }
        });
}
