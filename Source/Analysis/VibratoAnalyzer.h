// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 InKey contributors
// This file is part of InKey. See LICENSE for details.

#pragma once
#include "../Data/PitchData.h"
#include "../Data/NoteData.h"
#include <vector>

struct VibratoInfo
{
    float vibratoRate = 0.0f;      // Detected vibrato rate in Hz (typically 4-7 Hz)
    float vibratoDepth = 0.0f;     // Detected vibrato depth in semitones
    bool hasVibrato = false;       // Whether significant vibrato was detected
};

class VibratoAnalyzer
{
public:
    // Analyze vibrato for a single note's pitch curve segment.
    // Returns vibrato info including rate and depth.
    static VibratoInfo analyzeNote (const PitchCurve& pitchCurve,
                                     double noteStartTime, double noteEndTime);

    // Decompose a pitch deviation value into vibrato and drift components.
    // Uses the detected vibrato rate to separate periodic from aperiodic variation.
    // vibratoScale: 0=no vibrato, 1=original, 2=double
    // driftScale: 0=no drift, 1=original
    // Returns the target pitch deviation after scaling.
    static float decomposePitchDeviation (float deviation, float avgMidi,
                                           float frameMidi, double frameTime,
                                           double noteStartTime, float vibratoRate,
                                           float vibratoDepthScale, float driftScale);

private:
    // Compute normalized autocorrelation of a signal
    static std::vector<float> computeAutocorrelation (const std::vector<float>& signal,
                                                       int maxLag);

    // Find the dominant peak in autocorrelation within a lag range
    static int findDominantPeak (const std::vector<float>& autocorrelation,
                                  int minLag, int maxLag);

    // Estimate vibrato depth (amplitude) given a signal and detected period
    static float estimateDepth (const std::vector<float>& deviations, float rate,
                                 float sampleRate);

    // Minimum number of pitch frames required for vibrato analysis
    static constexpr int kMinFramesForAnalysis = 8;

    // Minimum autocorrelation peak value to consider vibrato present
    static constexpr float kMinAutocorrelationPeak = 0.3f;

    // Minimum vibrato depth in semitones to consider vibrato significant
    static constexpr float kMinVibratoDepth = 0.05f;

    // Vibrato frequency search range (Hz)
    static constexpr float kMinVibratoRate = 3.0f;
    static constexpr float kMaxVibratoRate = 8.0f;
};
