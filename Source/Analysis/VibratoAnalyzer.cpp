// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 InKey contributors
// This file is part of InKey. See LICENSE for details.

#include "VibratoAnalyzer.h"
#include <cmath>
#include <algorithm>
#include <numeric>

VibratoInfo VibratoAnalyzer::analyzeNote (const PitchCurve& pitchCurve,
                                           double noteStartTime, double noteEndTime)
{
    VibratoInfo info;

    // Extract pitch frames that fall within this note's time range
    std::vector<float> midiValues;
    std::vector<double> frameTimes;

    for (const auto& frame : pitchCurve)
    {
        if (frame.time >= noteStartTime && frame.time <= noteEndTime
            && frame.frequency > 0.0f && frame.confidence > 0.3f)
        {
            // Convert frequency to MIDI note number
            float midi = 69.0f + 12.0f * std::log2 (frame.frequency / 440.0f);
            midiValues.push_back (midi);
            frameTimes.push_back (frame.time);
        }
    }

    // Need enough frames for meaningful analysis
    if ((int) midiValues.size() < kMinFramesForAnalysis)
        return info;

    // Compute mean MIDI value
    float meanMidi = std::accumulate (midiValues.begin(), midiValues.end(), 0.0f)
                     / (float) midiValues.size();

    // Compute deviations from mean
    std::vector<float> deviations (midiValues.size());
    for (size_t i = 0; i < midiValues.size(); ++i)
        deviations[i] = midiValues[i] - meanMidi;

    // Estimate the effective sample rate of the pitch curve frames
    // (time between consecutive frames)
    double totalDuration = frameTimes.back() - frameTimes.front();
    if (totalDuration <= 0.0)
        return info;

    float pitchFrameRate = (float) (midiValues.size() - 1) / (float) totalDuration;

    // Compute lag range corresponding to vibrato frequency search range (3-8 Hz)
    int minLag = std::max (1, (int) std::floor (pitchFrameRate / kMaxVibratoRate));
    int maxLag = std::min ((int) midiValues.size() / 2,
                            (int) std::ceil (pitchFrameRate / kMinVibratoRate));

    if (minLag >= maxLag || maxLag < 2)
        return info;

    // Compute autocorrelation
    auto autocorr = computeAutocorrelation (deviations, maxLag + 1);

    // Find dominant peak in the vibrato frequency range
    int peakLag = findDominantPeak (autocorr, minLag, maxLag);

    if (peakLag <= 0)
        return info;

    // Check if peak is strong enough
    if (autocorr[peakLag] < kMinAutocorrelationPeak)
        return info;

    // Convert lag to frequency
    float detectedRate = pitchFrameRate / (float) peakLag;

    // Estimate vibrato depth
    float depth = estimateDepth (deviations, detectedRate, pitchFrameRate);

    // Only report vibrato if depth is significant
    if (depth < kMinVibratoDepth)
        return info;

    info.vibratoRate = detectedRate;
    info.vibratoDepth = depth;
    info.hasVibrato = true;

    return info;
}

float VibratoAnalyzer::decomposePitchDeviation (float deviation, float avgMidi,
                                                  float frameMidi, double frameTime,
                                                  double noteStartTime, float vibratoRate,
                                                  float vibratoDepthScale, float driftScale)
{
    // If no vibrato detected (rate is zero or near-zero), treat everything as drift
    if (vibratoRate < 0.1f)
        return deviation * driftScale;

    // The total deviation is: deviation = frameMidi - avgMidi
    // We decompose into vibrato (periodic) and drift (aperiodic) components.
    //
    // Vibrato component approximated as: depth * sin(2 * pi * rate * (t - t0))
    // where depth is estimated from the actual deviation at this point.
    //
    // Since we don't store depth per-frame, we use the instantaneous approach:
    // compute what a pure vibrato sine would look like at this time, then
    // the drift is whatever is left over.

    double timeSinceNoteStart = frameTime - noteStartTime;
    const double twoPi = 2.0 * 3.14159265358979323846;

    // Compute the vibrato phase at this time
    float vibratoPhase = (float) std::sin (twoPi * vibratoRate * timeSinceNoteStart);

    // Estimate the vibrato component's contribution to the deviation.
    // We project the deviation onto the sine at the current phase.
    // The vibrato component magnitude is estimated from the deviation and the sine phase.
    float vibratoComponent;
    float driftComponent;

    if (std::abs (vibratoPhase) > 0.01f)
    {
        // Project deviation onto the vibrato sine to estimate amplitude at this instant.
        // This is a simple decomposition: assume vibrato = A * sin(phase),
        // and drift = deviation - vibrato.
        // We use the sign and magnitude of the sine to guide separation.
        //
        // A more robust approach: amplitude A = deviation / sin(phase) when |sin| is large,
        // then vibrato = A * sin(phase) = deviation (trivially). Instead, we use the
        // overall deviation magnitude scaled by |sin(phase)| to estimate the vibrato portion.
        float absSin = std::abs (vibratoPhase);

        // The vibrato component is the portion of deviation that aligns with the sine
        vibratoComponent = deviation * absSin;
        driftComponent = deviation - vibratoComponent;
    }
    else
    {
        // At zero-crossings of vibrato, all deviation is drift
        vibratoComponent = 0.0f;
        driftComponent = deviation;
    }

    // Apply scaling factors and reconstruct
    return vibratoComponent * vibratoDepthScale + driftComponent * driftScale;
}

std::vector<float> VibratoAnalyzer::computeAutocorrelation (const std::vector<float>& signal,
                                                              int maxLag)
{
    int n = (int) signal.size();
    std::vector<float> autocorr (maxLag, 0.0f);

    // Compute energy (autocorrelation at lag 0) for normalization
    float energy = 0.0f;
    for (int i = 0; i < n; ++i)
        energy += signal[i] * signal[i];

    if (energy < 1e-10f)
        return autocorr;

    // Compute autocorrelation for each lag
    for (int lag = 0; lag < maxLag; ++lag)
    {
        float sum = 0.0f;
        for (int i = 0; i < n - lag; ++i)
            sum += signal[i] * signal[i + lag];

        // Normalize by energy at lag 0
        autocorr[lag] = sum / energy;
    }

    return autocorr;
}

int VibratoAnalyzer::findDominantPeak (const std::vector<float>& autocorrelation,
                                        int minLag, int maxLag)
{
    if (autocorrelation.empty())
        return -1;

    maxLag = std::min (maxLag, (int) autocorrelation.size() - 1);

    if (minLag > maxLag)
        return -1;

    // Find the highest peak in the valid lag range
    int bestLag = -1;
    float bestValue = 0.0f;

    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        // Check if this is a local peak (higher than neighbors)
        bool isPeak = true;

        if (lag > 0 && autocorrelation[lag] <= autocorrelation[lag - 1])
            isPeak = false;
        if (lag < (int) autocorrelation.size() - 1
            && autocorrelation[lag] <= autocorrelation[lag + 1])
            isPeak = false;

        if (isPeak && autocorrelation[lag] > bestValue)
        {
            bestValue = autocorrelation[lag];
            bestLag = lag;
        }
    }

    // If no local peak found, just use the maximum value in the range
    if (bestLag < 0)
    {
        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            if (autocorrelation[lag] > bestValue)
            {
                bestValue = autocorrelation[lag];
                bestLag = lag;
            }
        }
    }

    return bestLag;
}

float VibratoAnalyzer::estimateDepth (const std::vector<float>& deviations, float rate,
                                       float sampleRate)
{
    int n = (int) deviations.size();
    if (n < 2)
        return 0.0f;

    const double twoPi = 2.0 * 3.14159265358979323846;

    // Estimate vibrato amplitude using least-squares fit of a sine wave.
    // Model: deviation[i] = A * sin(2*pi*rate*i/sampleRate) + B * cos(2*pi*rate*i/sampleRate) + C
    // Vibrato depth (peak amplitude) = sqrt(A^2 + B^2)

    double sumSin = 0.0, sumCos = 0.0;
    double sumSinSq = 0.0, sumCosSq = 0.0;
    double sumDevSin = 0.0, sumDevCos = 0.0;

    for (int i = 0; i < n; ++i)
    {
        double phase = twoPi * rate * (double) i / (double) sampleRate;
        double sinVal = std::sin (phase);
        double cosVal = std::cos (phase);

        sumSin += sinVal;
        sumCos += cosVal;
        sumSinSq += sinVal * sinVal;
        sumCosSq += cosVal * cosVal;
        sumDevSin += deviations[i] * sinVal;
        sumDevCos += deviations[i] * cosVal;
    }

    // Simple projection (ignoring cross-terms for robustness):
    // A = 2/N * sum(deviation * sin), B = 2/N * sum(deviation * cos)
    // This works well when we have multiple complete cycles.
    double A = 2.0 * sumDevSin / (double) n;
    double B = 2.0 * sumDevCos / (double) n;

    // Peak amplitude of the vibrato
    float depth = (float) std::sqrt (A * A + B * B);

    return depth;
}
