// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 InKey contributors
// This file is part of InKey. See LICENSE for details.

#include "KeyDetector.h"
#include <cmath>
#include <array>

KeyDetector::KeyResult KeyDetector::detect (const NoteList& notes)
{
    if (notes.empty())
        return {};

    // Build pitch class histogram weighted by note duration
    std::array<double, 12> histogram {};
    for (const auto& note : notes)
    {
        if (note.isMuted || note.originalPitch <= 0.0f)
            continue;

        int pitchClass = note.midiNote % 12;
        histogram[static_cast<size_t> (pitchClass)] += note.editedDuration();
    }

    // Score each root + scale combination
    // Scale types: 0=Chromatic (skip), 1=Major, 2=NatMinor, 3=HarmMinor,
    //              4=Pentatonic, 5=Blues, 6=Dorian, 7=Mixolydian
    struct ScaleCandidate { int root; int scaleType; float score; };
    ScaleCandidate best { 0, 1, -1e9f };

    for (int root = 0; root < 12; ++root)
    {
        // Test each non-chromatic scale type (indices 1-7)
        for (int scaleType = 1; scaleType <= 7; ++scaleType)
        {
            ScaleDefinition scale;
            switch (scaleType)
            {
                case 1: scale = QuantizationEngine::majorScale (root); break;
                case 2: scale = QuantizationEngine::naturalMinorScale (root); break;
                case 3: scale = QuantizationEngine::harmonicMinorScale (root); break;
                case 4: scale = QuantizationEngine::pentatonicScale (root); break;
                case 5: scale = QuantizationEngine::bluesScale (root); break;
                case 6: scale = QuantizationEngine::dorianScale (root); break;
                case 7: scale = QuantizationEngine::mixolydianScale (root); break;
                default: continue;
            }

            float score = 0.0f;
            for (int pc = 0; pc < 12; ++pc)
            {
                int degreeIndex = ((pc - root) + 12) % 12;
                double weight = histogram[static_cast<size_t> (pc)];

                if (scale.degrees.size() == 12 && scale.degrees[static_cast<size_t> (degreeIndex)])
                    score += static_cast<float> (weight);
                else
                    score -= static_cast<float> (weight) * 0.5f;
            }

            if (score > best.score)
                best = { root, scaleType, score };
        }
    }

    // Compute confidence as ratio of best score to total duration
    double totalDuration = 0.0;
    for (const auto& note : notes)
    {
        if (! note.isMuted && note.originalPitch > 0.0f)
            totalDuration += note.editedDuration();
    }

    float confidence = totalDuration > 0.0 ? best.score / static_cast<float> (totalDuration) : 0.0f;

    return { best.root, best.scaleType, confidence };
}
