#pragma once

#include <vector>

struct PitchFrame
{
    double time;        // Timestamp in seconds
    float frequency;    // Detected pitch in Hz (0.0 = unvoiced)
    float confidence;   // Detection confidence (0.0 to 1.0)
};

using PitchCurve = std::vector<PitchFrame>;
