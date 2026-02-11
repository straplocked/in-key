// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 InKey contributors
// This file is part of InKey. See LICENSE for details.

#pragma once

#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include "../Data/NoteData.h"

struct ScaleDefinition
{
    std::string name;
    int rootNote = 0;              // 0=C, 1=C#, ... 11=B
    std::vector<bool> degrees;     // 12 booleans, true = in scale
};

class QuantizationEngine
{
public:
    // Calculate pitch correction for a note (returns semitones shift to apply)
    // strength: 0.0 = no correction, 1.0 = full snap to target
    static float calculateCorrection (float originalPitchHz, float strength,
                                       const ScaleDefinition& scale);

    // Intelligent pitch correction: notes further from target get more correction.
    // Close notes (within ~10 cents) barely move at low strength; far notes correct fully.
    static float calculateIntelligentCorrection (float originalPitchHz, float strength,
                                                   const ScaleDefinition& scale);

    // Auto-level notes' volume to a uniform RMS.
    // Computes per-note RMS, finds median, sets gainDB to equalize.
    // Ignores very quiet notes (breaths). Clamps to ±12 dB.
    // strength: 0.0 = no change, 1.0 = full leveling
    static void autoLevel (NoteList& notes, const float* audioData, int numSamples,
                           double sampleRate, float strength = 1.0f);

    // Get the nearest scale degree for a given MIDI value
    static int nearestScaleDegree (float midiFloat, const ScaleDefinition& scale);

    // Built-in scales
    static ScaleDefinition chromaticScale (int root = 0);
    static ScaleDefinition majorScale (int root);
    static ScaleDefinition naturalMinorScale (int root);
    static ScaleDefinition harmonicMinorScale (int root);
    static ScaleDefinition pentatonicScale (int root);
    static ScaleDefinition bluesScale (int root);
    static ScaleDefinition dorianScale (int root);
    static ScaleDefinition mixolydianScale (int root);

    // Quantize note start times to beat grid
    // bpm: beats per minute
    // subdivision: 1=whole, 2=half, 4=quarter, 8=eighth, 16=sixteenth
    // strength: 0.0=no change, 1.0=full snap
    // captureStartTime: DAW time offset (seconds) of capture start relative to bar start
    static void quantizeTime (NoteList& notes, double bpm, int subdivision,
                              float strength, double captureStartTime = 0.0);

    // Note name for root selector
    static const char* rootNoteName (int root);
};
