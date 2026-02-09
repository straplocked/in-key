#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>

// MFCC feature matrix: [numFrames][numCoefficients]
using MFCCMatrix = std::vector<std::vector<float>>;

class MFCCExtractor
{
public:
    struct Settings
    {
        int frameSize = 2048;
        int hopSize = 1024;
        int numMelFilters = 26;
        int numCoefficients = 13;
        float lowFreq = 80.0f;
        float highFreq = 8000.0f;
    };

    MFCCExtractor();

    const Settings& getSettings() const { return settings; }

    // Extract MFCC features from mono audio
    MFCCMatrix extract (const float* audio, int numSamples, double sampleRate);

private:
    Settings settings;

    static float hzToMel (float hz);
    static float melToHz (float mel);
};
