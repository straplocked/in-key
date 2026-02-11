// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 InKey contributors
// This file is part of InKey. See LICENSE for details.

#include "PitchDetector.h"
#include <cmath>
#include <algorithm>

PitchDetector::PitchDetector()
{
}

void PitchDetector::setSettings (const Settings& s)
{
    settings = s;
}

PitchCurve PitchDetector::analyze (const float* audio, int numSamples, double sampleRate) const
{
    PitchCurve result;

    int frameSize = settings.frameSize;
    int hopSize = settings.hopSize;

    // We need at least one full frame
    if (numSamples < frameSize)
        return result;

    int numFrames = (numSamples - frameSize) / hopSize + 1;
    result.reserve (static_cast<size_t> (numFrames));

    for (int i = 0; i < numFrames; ++i)
    {
        int offset = i * hopSize;
        float confidence = 0.0f;
        float freq = analyzeFrame (audio + offset, frameSize, sampleRate, confidence);

        PitchFrame frame;
        frame.time = static_cast<double> (offset) / sampleRate;
        frame.frequency = freq;
        frame.confidence = confidence;
        result.push_back (frame);
    }

    return result;
}

float PitchDetector::analyzeFrame (const float* frame, int frameSize, double sampleRate,
                                    float& outConfidence) const
{
    int halfSize = frameSize / 2;

    // Step 2: Difference function
    std::vector<float> diff (static_cast<size_t> (halfSize));
    difference (frame, frameSize, diff);

    // Step 3: Cumulative mean normalized difference
    cumulativeMeanNormalize (diff);

    // Step 4: Absolute threshold — find the first dip below threshold
    int tauEstimate = absoluteThreshold (diff, outConfidence);

    if (tauEstimate == -1)
    {
        outConfidence = 0.0f;
        return 0.0f; // Unvoiced
    }

    // Step 5: Parabolic interpolation
    float betterTau = parabolicInterpolation (diff, tauEstimate);

    float frequency = static_cast<float> (sampleRate) / betterTau;

    // Check if frequency is within vocal range
    if (frequency < settings.minFrequency || frequency > settings.maxFrequency)
    {
        outConfidence = 0.0f;
        return 0.0f;
    }

    return frequency;
}

void PitchDetector::difference (const float* frame, int frameSize, std::vector<float>& output) const
{
    int halfSize = frameSize / 2;
    output.resize (static_cast<size_t> (halfSize));

    for (int tau = 0; tau < halfSize; ++tau)
    {
        float sum = 0.0f;
        for (int j = 0; j < halfSize; ++j)
        {
            float delta = frame[j] - frame[j + tau];
            sum += delta * delta;
        }
        output[static_cast<size_t> (tau)] = sum;
    }
}

void PitchDetector::cumulativeMeanNormalize (std::vector<float>& buffer) const
{
    buffer[0] = 1.0f;
    float runningSum = 0.0f;

    for (size_t tau = 1; tau < buffer.size(); ++tau)
    {
        runningSum += buffer[tau];
        if (runningSum > 0.0f)
            buffer[tau] = buffer[tau] * static_cast<float> (tau) / runningSum;
        else
            buffer[tau] = 1.0f;
    }
}

int PitchDetector::absoluteThreshold (const std::vector<float>& buffer, float& outConfidence) const
{
    // Find the first tau where the CMND drops below threshold,
    // then pick the tau with the minimum value in that dip.
    int size = static_cast<int> (buffer.size());

    for (int tau = 2; tau < size; ++tau)
    {
        if (buffer[static_cast<size_t> (tau)] < settings.threshold)
        {
            // Find the local minimum
            while (tau + 1 < size
                   && buffer[static_cast<size_t> (tau + 1)] < buffer[static_cast<size_t> (tau)])
            {
                ++tau;
            }

            outConfidence = 1.0f - buffer[static_cast<size_t> (tau)];
            return tau;
        }
    }

    // No dip found below threshold — unvoiced
    outConfidence = 0.0f;
    return -1;
}

float PitchDetector::parabolicInterpolation (const std::vector<float>& buffer, int tauEstimate) const
{
    int size = static_cast<int> (buffer.size());

    if (tauEstimate <= 0 || tauEstimate >= size - 1)
        return static_cast<float> (tauEstimate);

    float s0 = buffer[static_cast<size_t> (tauEstimate - 1)];
    float s1 = buffer[static_cast<size_t> (tauEstimate)];
    float s2 = buffer[static_cast<size_t> (tauEstimate + 1)];

    float denominator = 2.0f * (2.0f * s1 - s2 - s0);

    if (std::abs (denominator) < 1e-10f)
        return static_cast<float> (tauEstimate);

    float adjustment = (s2 - s0) / denominator;
    return static_cast<float> (tauEstimate) + adjustment;
}
