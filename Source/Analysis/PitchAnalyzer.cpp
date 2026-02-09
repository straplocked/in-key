#include "PitchAnalyzer.h"

PitchAnalyzer::PitchAnalyzer()
    : juce::Thread ("PitchAnalyzer")
{
}

PitchAnalyzer::~PitchAnalyzer()
{
    stopThread (5000);
}

void PitchAnalyzer::startAnalysis (const juce::AudioBuffer<float>& audio, double sampleRate)
{
    if (isThreadRunning())
        return;

    // Copy audio for thread-safe access
    audioToAnalyze.makeCopyOf (audio);
    analysisSampleRate = sampleRate;
    progress.store (0.0f);
    result.clear();

    startThread();
}

void PitchAnalyzer::cancelAnalysis()
{
    signalThreadShouldExit();
    stopThread (5000);
}

void PitchAnalyzer::addListener (Listener* listener)
{
    listeners.add (listener);
}

void PitchAnalyzer::removeListener (Listener* listener)
{
    listeners.remove (listener);
}

void PitchAnalyzer::run()
{
    const float* audio = audioToAnalyze.getReadPointer (0);
    int numSamples = audioToAnalyze.getNumSamples();
    const auto& settings = detector.getSettings();

    int frameSize = settings.frameSize;
    int hopSize = settings.hopSize;

    if (numSamples < frameSize)
    {
        progress.store (1.0f);
        juce::MessageManager::callAsync ([this]() {
            listeners.call ([] (Listener& l) { l.analysisComplete(); });
        });
        return;
    }

    int numFrames = (numSamples - frameSize) / hopSize + 1;
    result.clear();
    result.reserve (static_cast<size_t> (numFrames));

    for (int i = 0; i < numFrames; ++i)
    {
        if (threadShouldExit())
            return;

        int offset = i * hopSize;
        float confidence = 0.0f;
        float freq = detector.analyzeFrame (audio + offset, frameSize,
                                             analysisSampleRate, confidence);

        PitchFrame frame;
        frame.time = static_cast<double> (offset) / analysisSampleRate;
        frame.frequency = freq;
        frame.confidence = confidence;
        result.push_back (frame);

        // Update progress
        float p = static_cast<float> (i + 1) / static_cast<float> (numFrames);
        progress.store (p);

        // Notify UI periodically (every ~5%)
        if (i % (numFrames / 20 + 1) == 0)
        {
            juce::MessageManager::callAsync ([this, p]() {
                listeners.call ([p] (Listener& l) { l.analysisProgress (p); });
            });
        }
    }

    progress.store (1.0f);

    juce::MessageManager::callAsync ([this]() {
        listeners.call ([] (Listener& l) { l.analysisComplete(); });
    });
}
