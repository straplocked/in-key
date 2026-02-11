// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 InKey contributors
// This file is part of InKey. See LICENSE for details.

#pragma once

#include "../Data/PitchData.h"
#include <vector>

// Standalone YIN pitch detection algorithm.
// Reference: "YIN, a fundamental frequency estimator for speech and music"
//            — Alain de Cheveigné, Hideki Kawahara (2002)
class PitchDetector
{
public:
    struct Settings
    {
        int frameSize = 2048;
        int hopSize = 256;
        float minFrequency = 80.0f;   // Hz — lowest vocal pitch
        float maxFrequency = 800.0f;  // Hz — highest vocal pitch
        float threshold = 0.15f;      // YIN threshold (lower = stricter)
    };

    PitchDetector();

    Settings& getSettings() { return settings; }
    const Settings& getSettings() const { return settings; }
    void setSettings (const Settings& settings);

    // Analyze an entire audio buffer and return pitch frames.
    // audio: mono audio samples
    // numSamples: number of samples
    // sampleRate: audio sample rate
    PitchCurve analyze (const float* audio, int numSamples, double sampleRate) const;

    // Analyze a single frame. Returns frequency in Hz (0 if unvoiced) and confidence.
    float analyzeFrame (const float* frame, int frameSize, double sampleRate,
                        float& outConfidence) const;

private:
    Settings settings;

    // YIN step 2: Difference function
    void difference (const float* frame, int frameSize, std::vector<float>& output) const;

    // YIN step 3: Cumulative mean normalized difference function
    void cumulativeMeanNormalize (std::vector<float>& buffer) const;

    // YIN step 4: Absolute threshold
    int absoluteThreshold (const std::vector<float>& buffer, float& outConfidence) const;

    // YIN step 5: Parabolic interpolation for sub-sample accuracy
    float parabolicInterpolation (const std::vector<float>& buffer, int tauEstimate) const;
};
