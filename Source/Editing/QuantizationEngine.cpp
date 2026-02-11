// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 InKey contributors
// This file is part of InKey. See LICENSE for details.

#include "QuantizationEngine.h"

float QuantizationEngine::calculateCorrection (float originalPitchHz, float strength,
                                                 const ScaleDefinition& scale)
{
    if (originalPitchHz <= 0.0f || strength <= 0.0f)
        return 0.0f;

    float exactMidi = 69.0f + 12.0f * std::log2 (originalPitchHz / 440.0f);
    int target = nearestScaleDegree (exactMidi, scale);
    float fullCorrection = static_cast<float> (target) - exactMidi;

    return fullCorrection * strength;
}

float QuantizationEngine::calculateIntelligentCorrection (float originalPitchHz, float strength,
                                                            const ScaleDefinition& scale)
{
    if (originalPitchHz <= 0.0f || strength <= 0.0f)
        return 0.0f;

    float exactMidi = 69.0f + 12.0f * std::log2 (originalPitchHz / 440.0f);
    int target = nearestScaleDegree (exactMidi, scale);
    float fullCorrection = static_cast<float> (target) - exactMidi;
    float absCorrection = std::abs (fullCorrection);

    // Non-linear weighting: notes far from target get proportionally more correction.
    // maxDeviation defines the "close enough" zone (~0.5 semitones = 50 cents).
    // Notes within ~10 cents get almost no correction at low strength.
    // smoothstep gives a smooth S-curve transition.
    constexpr float maxDeviation = 0.5f;
    float t = std::min (absCorrection / maxDeviation, 1.0f);

    // Smoothstep: 3t^2 - 2t^3
    float smoothFactor = t * t * (3.0f - 2.0f * t);
    float effectiveStrength = strength * smoothFactor;

    return fullCorrection * effectiveStrength;
}

void QuantizationEngine::autoLevel (NoteList& notes, const float* audioData, int numSamples,
                                     double sampleRate, float strength)
{
    if (notes.empty() || audioData == nullptr || numSamples == 0 || strength <= 0.0f)
        return;

    // Calculate RMS for each note
    struct NoteRMS { int noteIdx; float rmsDB; };
    std::vector<NoteRMS> noteRmsValues;

    for (size_t i = 0; i < notes.size(); ++i)
    {
        const auto& note = notes[i];
        if (note.isMuted)
            continue;

        int srcStart = static_cast<int> (note.startTime * sampleRate);
        int srcEnd = static_cast<int> (note.endTime() * sampleRate);
        srcStart = std::max (0, std::min (srcStart, numSamples));
        srcEnd = std::max (0, std::min (srcEnd, numSamples));

        if (srcEnd - srcStart < 64)
            continue;

        double sumSq = 0.0;
        for (int s = srcStart; s < srcEnd; ++s)
            sumSq += static_cast<double> (audioData[s]) * static_cast<double> (audioData[s]);

        float rms = static_cast<float> (std::sqrt (sumSq / static_cast<double> (srcEnd - srcStart)));
        float rmsDB = rms > 1e-10f ? 20.0f * std::log10 (rms) : -100.0f;

        noteRmsValues.push_back ({ static_cast<int> (i), rmsDB });
    }

    if (noteRmsValues.size() < 2)
        return;

    // Filter out very quiet notes (likely breaths/noise) — below -40 dB
    std::vector<float> nonBreathRMS;
    for (const auto& nr : noteRmsValues)
    {
        if (nr.rmsDB > -40.0f)
            nonBreathRMS.push_back (nr.rmsDB);
    }

    if (nonBreathRMS.empty())
        return;

    // Target = median RMS of non-breath notes
    std::sort (nonBreathRMS.begin(), nonBreathRMS.end());
    float targetRMS = nonBreathRMS[nonBreathRMS.size() / 2];

    // Apply gain adjustments
    for (const auto& nr : noteRmsValues)
    {
        if (nr.rmsDB <= -40.0f)
            continue;  // Skip breaths

        float correction = targetRMS - nr.rmsDB;
        correction = std::max (-12.0f, std::min (12.0f, correction));  // Clamp ±12 dB
        correction *= strength;

        notes[static_cast<size_t> (nr.noteIdx)].gainDB = correction;
    }
}

void QuantizationEngine::quantizeTime (NoteList& notes, double bpm, int subdivision,
                                        float strength, double captureStartTime)
{
    if (notes.empty() || bpm <= 0.0 || subdivision <= 0 || strength <= 0.0f)
        return;

    // Compute the duration of one grid unit in seconds
    // subdivision: 1=whole (4 beats), 2=half (2 beats), 4=quarter (1 beat),
    //              8=eighth (0.5 beat), 16=sixteenth (0.25 beat)
    double beatDuration = 60.0 / bpm / (static_cast<double> (subdivision) / 4.0);

    for (auto& note : notes)
    {
        // The note's position in the absolute timeline
        double noteTime = note.startTime + note.timeOffset;

        // Offset by captureStartTime so the grid aligns with the DAW's bar start
        double relativeTime = noteTime - captureStartTime;

        // Find nearest grid point
        double gridIndex = std::round (relativeTime / beatDuration);
        double nearestGridTime = gridIndex * beatDuration + captureStartTime;

        // Compute the delta needed to snap to grid
        double delta = nearestGridTime - noteTime;

        // Apply strength-weighted delta to timeOffset
        note.timeOffset += delta * static_cast<double> (strength);
    }
}

int QuantizationEngine::nearestScaleDegree (float midiFloat, const ScaleDefinition& scale)
{
    int nearest = static_cast<int> (std::round (midiFloat));

    // Search outward from the nearest semitone for a scale degree
    for (int offset = 0; offset <= 6; ++offset)
    {
        int up = nearest + offset;
        int down = nearest - offset;

        if (up <= 127 && up >= 0)
        {
            int degreeUp = ((up % 12) - scale.rootNote + 12) % 12;
            if (degreeUp < 12 && scale.degrees[static_cast<size_t> (degreeUp)])
                return up;
        }

        if (offset > 0 && down >= 0 && down <= 127)
        {
            int degreeDown = ((down % 12) - scale.rootNote + 12) % 12;
            if (degreeDown < 12 && scale.degrees[static_cast<size_t> (degreeDown)])
                return down;
        }
    }

    return nearest;
}

// --- Built-in scale definitions ---

ScaleDefinition QuantizationEngine::chromaticScale (int root)
{
    return { "Chromatic", root, { true, true, true, true, true, true,
                                   true, true, true, true, true, true } };
}

ScaleDefinition QuantizationEngine::majorScale (int root)
{
    // W W H W W W H = {0, 2, 4, 5, 7, 9, 11}
    return { "Major", root, { true, false, true, false, true, true,
                               false, true, false, true, false, true } };
}

ScaleDefinition QuantizationEngine::naturalMinorScale (int root)
{
    // W H W W H W W = {0, 2, 3, 5, 7, 8, 10}
    return { "Natural Minor", root, { true, false, true, true, false, true,
                                       false, true, true, false, true, false } };
}

ScaleDefinition QuantizationEngine::harmonicMinorScale (int root)
{
    // W H W W H WH H = {0, 2, 3, 5, 7, 8, 11}
    return { "Harmonic Minor", root, { true, false, true, true, false, true,
                                        false, true, true, false, false, true } };
}

ScaleDefinition QuantizationEngine::pentatonicScale (int root)
{
    // Major pentatonic: {0, 2, 4, 7, 9}
    return { "Pentatonic", root, { true, false, true, false, true, false,
                                    false, true, false, true, false, false } };
}

ScaleDefinition QuantizationEngine::bluesScale (int root)
{
    // Blues: {0, 3, 5, 6, 7, 10}
    return { "Blues", root, { true, false, false, true, false, true,
                               true, true, false, false, true, false } };
}

ScaleDefinition QuantizationEngine::dorianScale (int root)
{
    // Dorian: {0, 2, 3, 5, 7, 9, 10}
    return { "Dorian", root, { true, false, true, true, false, true,
                                false, true, false, true, true, false } };
}

ScaleDefinition QuantizationEngine::mixolydianScale (int root)
{
    // Mixolydian: {0, 2, 4, 5, 7, 9, 10}
    return { "Mixolydian", root, { true, false, true, false, true, true,
                                    false, true, false, true, true, false } };
}

const char* QuantizationEngine::rootNoteName (int root)
{
    static const char* names[] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    return names[root % 12];
}
