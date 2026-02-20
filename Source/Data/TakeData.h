// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 InKey contributors
// This file is part of InKey. See LICENSE for details.

#pragma once

#include "NoteData.h"
#include "PitchData.h"
#include <juce_audio_basics/juce_audio_basics.h>
#if JucePlugin_Enable_ARA
 #include <juce_audio_processors/juce_audio_processors.h>
#endif

struct Take
{
    int id = 0;
    juce::AudioBuffer<float> audio;       // Owned audio (non-ARA mode)
    double sampleRate = 44100.0;
    PitchCurve pitchCurve;
    NoteList notes;
    double duration = 0.0;
    bool isReference = false;
    bool isAligned = false;
    int64_t captureStartSampleInDAW = 0;  // DAW timeline position when capture began

#if JucePlugin_Enable_ARA
    // ARA mode: reference to host audio source (non-owning)
    juce::ARAAudioSource* araSource = nullptr;
    bool isARATake() const { return araSource != nullptr; }
#else
    bool isARATake() const { return false; }
#endif
};

using TakeList = std::vector<Take>;
