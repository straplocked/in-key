// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 InKey contributors
// This file is part of InKey. See LICENSE for details.

#include "SibilantDetector.h"
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <algorithm>

std::vector<std::pair<double, double>> SibilantDetector::detect (const float* audio,
                                                                  int numSamples,
                                                                  double sampleRate,
                                                                  double noteStartTime,
                                                                  double noteEndTime)
{
    std::vector<std::pair<double, double>> sibilantFrames;

    if (audio == nullptr || numSamples == 0 || sampleRate <= 0.0)
        return sibilantFrames;

    // Clamp note boundaries to valid sample range
    int startSample = std::max (0, static_cast<int> (noteStartTime * sampleRate));
    int endSample = std::min (numSamples, static_cast<int> (noteEndTime * sampleRate));

    if (endSample - startSample < frameSize)
        return sibilantFrames;

    juce::dsp::FFT fft (fftOrder);
    int fftSize = fft.getSize();                  // 2048
    int numBins = fftSize / 2 + 1;               // 1025
    double binFreqWidth = sampleRate / fftSize;

    // Working buffers: FFT requires 2x size for real+imag interleaved
    std::vector<float> fftData (static_cast<size_t> (fftSize * 2), 0.0f);
    std::vector<float> magnitudes (static_cast<size_t> (numBins), 0.0f);
    std::vector<float> window (static_cast<size_t> (fftSize), 0.0f);

    // Hann window
    for (int i = 0; i < fftSize; ++i)
        window[static_cast<size_t> (i)] = 0.5f * (1.0f - std::cos (2.0f * juce::MathConstants<float>::pi * static_cast<float> (i) / static_cast<float> (fftSize - 1)));

    // Process frames
    for (int frameStart = startSample; frameStart + frameSize <= endSample; frameStart += hopSize)
    {
        // Apply window and copy to FFT buffer
        std::fill (fftData.begin(), fftData.end(), 0.0f);
        for (int i = 0; i < fftSize; ++i)
            fftData[static_cast<size_t> (i)] = audio[frameStart + i] * window[static_cast<size_t> (i)];

        // Perform FFT (in-place, real input -> complex output interleaved)
        fft.performRealOnlyForwardTransform (fftData.data());

        // Compute magnitudes from interleaved real/imag pairs
        for (int i = 0; i < numBins; ++i)
        {
            float real = fftData[static_cast<size_t> (i * 2)];
            float imag = fftData[static_cast<size_t> (i * 2 + 1)];
            magnitudes[static_cast<size_t> (i)] = std::sqrt (real * real + imag * imag);
        }

        // Compute features
        float centroid = computeSpectralCentroid (magnitudes.data(), numBins, binFreqWidth);
        float hfRatio = computeHighFrequencyRatio (magnitudes.data(), numBins, binFreqWidth, 4000.0);
        // Zero-crossing rate is computed but not currently part of the classification threshold.
        // It is available for future refinement of the detection algorithm.
        // float zcr = computeZeroCrossingRate (audio + frameStart, fftSize);

        // Classification: sibilant if HF ratio > 0.4 AND centroid > 3500 Hz
        if (hfRatio > hfRatioThreshold && centroid > centroidThreshold)
        {
            double frameTime = static_cast<double> (frameStart) / sampleRate;
            double frameEndTime = static_cast<double> (frameStart + frameSize) / sampleRate;
            sibilantFrames.push_back ({ frameTime, frameEndTime });
        }
    }

    // Merge adjacent/overlapping sibilant frames with gap tolerance
    return mergeRegions (sibilantFrames);
}

void SibilantDetector::detectForNotes (NoteList& notes,
                                       const float* audio,
                                       int numSamples,
                                       double sampleRate)
{
    for (auto& note : notes)
    {
        double noteStart = note.startTime;
        double noteEnd = note.startTime + note.duration;

        note.sibilantRegions = detect (audio, numSamples, sampleRate, noteStart, noteEnd);
    }
}

float SibilantDetector::computeSpectralCentroid (const float* magnitudes, int numBins, double binFreqWidth)
{
    double weightedSum = 0.0;
    double totalMagnitude = 0.0;

    for (int i = 0; i < numBins; ++i)
    {
        double freq = static_cast<double> (i) * binFreqWidth;
        double mag = static_cast<double> (magnitudes[i]);
        weightedSum += freq * mag;
        totalMagnitude += mag;
    }

    if (totalMagnitude < 1e-10)
        return 0.0f;

    return static_cast<float> (weightedSum / totalMagnitude);
}

float SibilantDetector::computeHighFrequencyRatio (const float* magnitudes, int numBins, double binFreqWidth, double cutoffHz)
{
    double hfEnergy = 0.0;
    double totalEnergy = 0.0;

    int cutoffBin = static_cast<int> (cutoffHz / binFreqWidth);

    for (int i = 0; i < numBins; ++i)
    {
        double mag = static_cast<double> (magnitudes[i]);
        double energy = mag * mag;
        totalEnergy += energy;

        if (i >= cutoffBin)
            hfEnergy += energy;
    }

    if (totalEnergy < 1e-10)
        return 0.0f;

    return static_cast<float> (hfEnergy / totalEnergy);
}

float SibilantDetector::computeZeroCrossingRate (const float* samples, int count)
{
    if (count <= 1)
        return 0.0f;

    int crossings = 0;
    for (int i = 1; i < count; ++i)
    {
        if ((samples[i] >= 0.0f && samples[i - 1] < 0.0f) ||
            (samples[i] < 0.0f && samples[i - 1] >= 0.0f))
        {
            ++crossings;
        }
    }

    return static_cast<float> (crossings) / static_cast<float> (count - 1);
}

std::vector<std::pair<double, double>> SibilantDetector::mergeRegions (std::vector<std::pair<double, double>>& regions)
{
    if (regions.empty())
        return {};

    // Sort by start time (should already be sorted but be safe)
    std::sort (regions.begin(), regions.end(),
               [] (const auto& a, const auto& b) { return a.first < b.first; });

    std::vector<std::pair<double, double>> merged;
    merged.push_back (regions[0]);

    for (size_t i = 1; i < regions.size(); ++i)
    {
        auto& current = merged.back();
        const auto& next = regions[i];

        // Merge if the gap between current end and next start is within tolerance
        if (next.first - current.second <= gapTolerance)
        {
            current.second = std::max (current.second, next.second);
        }
        else
        {
            merged.push_back (next);
        }
    }

    return merged;
}
