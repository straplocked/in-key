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
