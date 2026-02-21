// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 InKey contributors
// This file is part of InKey. See LICENSE for details.

#include "PresetManager.h"

PresetManager::PresetManager()
{
    presetsDirectory = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                           .getChildFile ("InKey")
                           .getChildFile ("Presets");

    if (! presetsDirectory.exists())
        presetsDirectory.createDirectory();
}

juce::File PresetManager::getPresetsDirectory() const
{
    return presetsDirectory;
}

juce::StringArray PresetManager::getPresetNames() const
{
    juce::StringArray names;
    juce::Array<juce::File> results;
    presetsDirectory.findChildFiles (results, juce::File::findFiles, false, "*.inkeypreset");

    results.sort();

    for (const auto& file : results)
        names.add (file.getFileNameWithoutExtension());

    return names;
}

bool PresetManager::savePreset (const PresetData& preset)
{
    auto file = getPresetFile (preset.name);

    auto tree = preset.toValueTree();

    juce::MemoryBlock block;
    juce::MemoryOutputStream stream (block, false);
    tree.writeToStream (stream);

    return file.replaceWithData (block.getData(), block.getSize());
}

PresetData PresetManager::loadPreset (const juce::String& name) const
{
    auto file = getPresetFile (name);

    if (! file.existsAsFile())
        return PresetData();

    juce::MemoryBlock block;
    if (! file.loadFileAsData (block))
        return PresetData();

    auto tree = juce::ValueTree::readFromData (block.getData(), block.getSize());
    return PresetData::fromValueTree (tree);
}

bool PresetManager::deletePreset (const juce::String& name)
{
    auto file = getPresetFile (name);

    if (file.existsAsFile())
        return file.deleteFile();

    return false;
}

bool PresetManager::presetExists (const juce::String& name) const
{
    return getPresetFile (name).existsAsFile();
}

juce::File PresetManager::getPresetFile (const juce::String& name) const
{
    auto safeName = juce::File::createLegalFileName (name);
    return presetsDirectory.getChildFile (safeName + ".inkeypreset");
}
