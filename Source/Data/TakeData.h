// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 InKey contributors
// This file is part of InKey. See LICENSE for details.

#pragma once

#include "NoteData.h"
#include "PitchData.h"
#include <juce_audio_basics/juce_audio_basics.h>

struct Take
{
    int id = 0;
    juce::AudioBuffer<float> audio;
    double sampleRate = 44100.0;
    PitchCurve pitchCurve;
    NoteList notes;
    double duration = 0.0;
    bool isReference = false;
    bool isAligned = false;
    int64_t captureStartSampleInDAW = 0;  // DAW timeline position when capture began
};

using TakeList = std::vector<Take>;
