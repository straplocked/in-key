#pragma once

#include "../Data/NoteData.h"
#include "../Editing/QuantizationEngine.h"

// Detects the musical key/scale from a set of notes using pitch class histogram analysis.
class KeyDetector
{
public:
    struct KeyResult
    {
        int rootNote = 0;       // 0-11 (C=0, C#=1, ...)
        int scaleTypeIndex = 0; // Index into QuantizationEngine scale types
        float confidence = 0.0f;
    };

    // Detect the most likely key from the given notes.
    // Returns the root note (0-11) and scale type index (matching QuantizationEngine order).
    static KeyResult detect (const NoteList& notes);
};
