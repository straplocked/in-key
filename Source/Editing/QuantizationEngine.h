#pragma once

#include <vector>
#include <string>
#include <cmath>

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

    // Note name for root selector
    static const char* rootNoteName (int root);
};
