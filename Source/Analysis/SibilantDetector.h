#pragma once
#include <vector>
#include <utility>
#include "../Data/NoteData.h"

class SibilantDetector
{
public:
    SibilantDetector() = default;

    // Detect sibilant regions within a time range of audio.
    // Returns pairs of (startTime, endTime) in seconds for each sibilant region.
    std::vector<std::pair<double, double>> detect (const float* audio,
                                                   int numSamples,
                                                   double sampleRate,
                                                   double noteStartTime,
                                                   double noteEndTime);

    // Process all notes and fill their sibilantRegions field.
    void detectForNotes (NoteList& notes,
                         const float* audio,
                         int numSamples,
                         double sampleRate);

private:
    static constexpr int fftOrder = 11;            // 2^11 = 2048
    static constexpr int frameSize = 2048;
    static constexpr int hopSize = 512;
    static constexpr float hfRatioThreshold = 0.4f;
    static constexpr float centroidThreshold = 3500.0f;
    static constexpr double gapTolerance = 0.020;  // 20ms merge tolerance

    // Compute spectral centroid in Hz from magnitude spectrum.
    float computeSpectralCentroid (const float* magnitudes, int numBins, double binFreqWidth);

    // Compute ratio of energy above cutoffHz to total energy.
    float computeHighFrequencyRatio (const float* magnitudes, int numBins, double binFreqWidth, double cutoffHz);

    // Compute zero-crossing rate for a frame of audio samples.
    float computeZeroCrossingRate (const float* samples, int count);

    // Merge nearby sibilant regions that are within gapTolerance of each other.
    std::vector<std::pair<double, double>> mergeRegions (std::vector<std::pair<double, double>>& regions);
};
