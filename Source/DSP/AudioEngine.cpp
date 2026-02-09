#include "AudioEngine.h"
#include <rubberband/RubberBandStretcher.h>
#include <cmath>
#include <algorithm>

AudioEngine::AudioEngine()
{
}

AudioEngine::~AudioEngine()
{
    stopPlayback();
}

juce::AudioBuffer<float> AudioEngine::render (const juce::AudioBuffer<float>& originalAudio,
                                                double sampleRate,
                                                const NoteList& notes)
{
    int totalSamples = originalAudio.getNumSamples();
    if (totalSamples == 0)
        return {};

    const float* originalData = originalAudio.getReadPointer (0);

    // Output buffer — same length as original
    juce::AudioBuffer<float> output (1, totalSamples);
    output.clear();

    float* outputData = output.getWritePointer (0);

    for (const auto& note : notes)
    {
        // Source audio comes from original positions
        int srcStart = static_cast<int> (note.startTime * sampleRate);
        int srcEnd = static_cast<int> (note.endTime() * sampleRate);
        srcStart = juce::jlimit (0, totalSamples, srcStart);
        srcEnd = juce::jlimit (0, totalSamples, srcEnd);
        int srcLength = srcEnd - srcStart;

        if (srcLength <= 0)
            continue;

        // Destination position uses edited times
        int dstStart = static_cast<int> (note.editedStartTime() * sampleRate);
        int dstEnd = static_cast<int> ((note.editedStartTime() + note.editedDuration()) * sampleRate);
        dstStart = juce::jlimit (0, totalSamples, dstStart);
        dstEnd = juce::jlimit (0, totalSamples, dstEnd);
        int dstLength = dstEnd - dstStart;

        if (dstLength <= 0)
            continue;

        bool needsProcessing = std::abs (note.pitchShiftSemitones) > 0.01f
                            || std::abs (note.formantShiftSemitones) > 0.01f
                            || std::abs (note.durationScale - 1.0) > 0.01;

        if (! needsProcessing)
        {
            // No edits — copy original audio directly
            int toCopy = juce::jmin (srcLength, dstLength);
            for (int i = 0; i < toCopy; ++i)
                outputData[dstStart + i] += originalData[srcStart + i];
        }
        else
        {
            // Process with Rubber Band
            auto processed = processNote (originalData + srcStart, srcLength,
                                           sampleRate, note.pitchShiftSemitones,
                                           note.formantShiftSemitones, note.durationScale);

            const float* procData = processed.getReadPointer (0);
            int procLength = juce::jmin (processed.getNumSamples(), dstLength);

            for (int i = 0; i < procLength; ++i)
                outputData[dstStart + i] += procData[i];
        }
    }

    // Fill gaps between notes with original audio (unvoiced regions)
    std::vector<bool> covered (static_cast<size_t> (totalSamples), false);
    for (const auto& note : notes)
    {
        int dstStart = juce::jlimit (0, totalSamples, static_cast<int> (note.editedStartTime() * sampleRate));
        int dstEnd = juce::jlimit (0, totalSamples, static_cast<int> ((note.editedStartTime() + note.editedDuration()) * sampleRate));
        for (int i = dstStart; i < dstEnd; ++i)
            covered[static_cast<size_t> (i)] = true;
    }

    for (int i = 0; i < totalSamples; ++i)
    {
        if (! covered[static_cast<size_t> (i)])
            outputData[i] = originalData[i];
    }

    return output;
}

juce::AudioBuffer<float> AudioEngine::processNote (const float* input, int numSamples,
                                                     double sampleRate, float pitchSemitones,
                                                     float formantSemitones, double timeRatio)
{
    using namespace RubberBand;

    RubberBandStretcher stretcher (
        static_cast<size_t> (sampleRate),
        1, // mono
        RubberBandStretcher::OptionProcessOffline
        | RubberBandStretcher::OptionPitchHighConsistency
        | RubberBandStretcher::OptionFormantPreserved);

    // Set pitch scale
    double pitchScale = std::pow (2.0, static_cast<double> (pitchSemitones) / 12.0);
    stretcher.setPitchScale (pitchScale);

    // Set time ratio (> 1.0 = slower/longer, < 1.0 = faster/shorter)
    if (std::abs (timeRatio - 1.0) > 0.01)
        stretcher.setTimeRatio (timeRatio);

    // Set formant scale if there's a formant shift
    if (std::abs (formantSemitones) > 0.01f)
    {
        double formantScale = std::pow (2.0, static_cast<double> (formantSemitones) / 12.0);
        stretcher.setFormantScale (formantScale);
    }

    stretcher.setExpectedInputDuration (static_cast<size_t> (numSamples));

    // Study phase — Rubber Band needs to see all the audio first in offline mode
    stretcher.study (&input, static_cast<size_t> (numSamples), true);

    // Process phase
    stretcher.process (&input, static_cast<size_t> (numSamples), true);

    // Retrieve output
    int available = stretcher.available();
    if (available <= 0)
        return {};

    juce::AudioBuffer<float> output (1, available);
    float* outputPtr = output.getWritePointer (0);
    stretcher.retrieve (&outputPtr, static_cast<size_t> (available));

    return output;
}

void AudioEngine::startPlayback (const juce::AudioBuffer<float>& renderedAudio, double sampleRate)
{
    playbackBuffer.makeCopyOf (renderedAudio);
    playbackSampleRate = sampleRate;
    playbackPosition.store (0);
    playing.store (true);
}

void AudioEngine::stopPlayback()
{
    playing.store (false);
    playbackPosition.store (0);
}

bool AudioEngine::fillBuffer (juce::AudioBuffer<float>& outputBuffer, int numSamples)
{
    if (! playing.load())
        return false;

    int pos = playbackPosition.load();
    int available = playbackBuffer.getNumSamples() - pos;

    if (available <= 0)
    {
        playing.store (false);
        return false;
    }

    int toCopy = juce::jmin (numSamples, available);
    const float* src = playbackBuffer.getReadPointer (0, pos);

    // Mix into all output channels
    for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch)
    {
        float* dst = outputBuffer.getWritePointer (ch);
        for (int i = 0; i < toCopy; ++i)
            dst[i] = src[i];
    }

    playbackPosition.store (pos + toCopy);

    return (pos + toCopy) < playbackBuffer.getNumSamples();
}

double AudioEngine::getPlaybackPosition() const
{
    if (playbackSampleRate <= 0.0)
        return 0.0;
    return static_cast<double> (playbackPosition.load()) / playbackSampleRate;
}
