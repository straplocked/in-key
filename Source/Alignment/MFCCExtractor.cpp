#include "MFCCExtractor.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

MFCCExtractor::MFCCExtractor()
{
}

float MFCCExtractor::hzToMel (float hz)
{
    return 2595.0f * std::log10 (1.0f + hz / 700.0f);
}

float MFCCExtractor::melToHz (float mel)
{
    return 700.0f * (std::pow (10.0f, mel / 2595.0f) - 1.0f);
}

MFCCMatrix MFCCExtractor::extract (const float* audio, int numSamples, double sampleRate)
{
    MFCCMatrix result;

    if (numSamples < settings.frameSize)
        return result;

    // Determine FFT order (next power of 2 >= frameSize)
    int fftOrder = 0;
    int fftSize = 1;
    while (fftSize < settings.frameSize)
    {
        fftSize *= 2;
        ++fftOrder;
    }

    juce::dsp::FFT fft (fftOrder);
    int numBins = fftSize / 2 + 1;

    // Create Hamming window
    std::vector<float> window (static_cast<size_t> (settings.frameSize));
    for (int i = 0; i < settings.frameSize; ++i)
        window[static_cast<size_t> (i)] = 0.54f - 0.46f * std::cos (2.0f * static_cast<float> (M_PI) * i / (settings.frameSize - 1));

    // Create Mel filterbank
    float melLow = hzToMel (settings.lowFreq);
    float melHigh = hzToMel (juce::jmin (settings.highFreq, static_cast<float> (sampleRate) / 2.0f));
    int numPoints = settings.numMelFilters + 2;

    std::vector<float> melPoints (static_cast<size_t> (numPoints));
    for (int i = 0; i < numPoints; ++i)
        melPoints[static_cast<size_t> (i)] = melLow + static_cast<float> (i) * (melHigh - melLow) / static_cast<float> (numPoints - 1);

    std::vector<int> binPoints (static_cast<size_t> (numPoints));
    for (int i = 0; i < numPoints; ++i)
    {
        float hz = melToHz (melPoints[static_cast<size_t> (i)]);
        binPoints[static_cast<size_t> (i)] = static_cast<int> (std::floor ((settings.frameSize + 1) * hz / sampleRate));
    }

    // Build triangular filterbank
    std::vector<std::vector<float>> filterbank (
        static_cast<size_t> (settings.numMelFilters),
        std::vector<float> (static_cast<size_t> (numBins), 0.0f));

    for (int m = 0; m < settings.numMelFilters; ++m)
    {
        int left = binPoints[static_cast<size_t> (m)];
        int center = binPoints[static_cast<size_t> (m + 1)];
        int right = binPoints[static_cast<size_t> (m + 2)];

        for (int k = left; k < center && k < numBins; ++k)
        {
            if (center > left)
                filterbank[static_cast<size_t> (m)][static_cast<size_t> (k)] =
                    static_cast<float> (k - left) / static_cast<float> (center - left);
        }

        for (int k = center; k <= right && k < numBins; ++k)
        {
            if (right > center)
                filterbank[static_cast<size_t> (m)][static_cast<size_t> (k)] =
                    static_cast<float> (right - k) / static_cast<float> (right - center);
        }
    }

    // FFT buffer (needs to be 2 * fftSize for JUCE FFT)
    std::vector<float> fftBuffer (static_cast<size_t> (fftSize * 2), 0.0f);
    std::vector<float> magnitudes (static_cast<size_t> (numBins));
    std::vector<float> melEnergies (static_cast<size_t> (settings.numMelFilters));
    std::vector<float> mfccs (static_cast<size_t> (settings.numCoefficients));

    // Process frames
    int numFrames = (numSamples - settings.frameSize) / settings.hopSize + 1;
    result.reserve (static_cast<size_t> (numFrames));

    for (int frame = 0; frame < numFrames; ++frame)
    {
        int offset = frame * settings.hopSize;

        // Apply pre-emphasis and window
        std::fill (fftBuffer.begin(), fftBuffer.end(), 0.0f);
        for (int i = 0; i < settings.frameSize; ++i)
        {
            float sample = audio[offset + i];
            if (i > 0)
                sample -= 0.97f * audio[offset + i - 1]; // Pre-emphasis

            fftBuffer[static_cast<size_t> (i)] = sample * window[static_cast<size_t> (i)];
        }

        // FFT
        fft.performRealOnlyForwardTransform (fftBuffer.data());

        // Compute magnitude spectrum
        for (int k = 0; k < numBins; ++k)
        {
            float real = fftBuffer[static_cast<size_t> (k * 2)];
            float imag = fftBuffer[static_cast<size_t> (k * 2 + 1)];
            magnitudes[static_cast<size_t> (k)] = real * real + imag * imag; // Power spectrum
        }

        // Apply Mel filterbank
        for (int m = 0; m < settings.numMelFilters; ++m)
        {
            float sum = 0.0f;
            for (int k = 0; k < numBins; ++k)
                sum += magnitudes[static_cast<size_t> (k)] * filterbank[static_cast<size_t> (m)][static_cast<size_t> (k)];

            melEnergies[static_cast<size_t> (m)] = std::log (juce::jmax (1e-10f, sum));
        }

        // DCT (Type II) to get MFCCs
        for (int i = 0; i < settings.numCoefficients; ++i)
        {
            float sum = 0.0f;
            for (int j = 0; j < settings.numMelFilters; ++j)
            {
                sum += melEnergies[static_cast<size_t> (j)]
                       * std::cos (static_cast<float> (M_PI) * i * (2.0f * j + 1.0f)
                                    / (2.0f * settings.numMelFilters));
            }
            mfccs[static_cast<size_t> (i)] = sum;
        }

        result.push_back (std::vector<float> (mfccs.begin(), mfccs.end()));
    }

    return result;
}
