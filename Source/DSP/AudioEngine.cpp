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
                                                const NoteList& notes,
                                                const PitchCurve& pitchCurve)
{
    int totalSamples = originalAudio.getNumSamples();
    if (totalSamples == 0)
        return {};

    const float* originalData = originalAudio.getReadPointer (0);

    // Output buffer — same length as original
    juce::AudioBuffer<float> output (1, totalSamples);
    output.clear();

    float* outputData = output.getWritePointer (0);

    // Crossfade length: 5ms
    int crossfadeSamples = static_cast<int> (0.005 * sampleRate);

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

        bool needsPitchTimeProcessing = std::abs (note.pitchShiftSemitones) > 0.01f
                                     || std::abs (note.formantShiftSemitones) > 0.01f
                                     || std::abs (note.durationScale - 1.0) > 0.01;

        bool hasDrift = std::abs (note.pitchDriftScale - 1.0f) > 0.01f && ! pitchCurve.empty();

        // Gain factor from dB
        float gainFactor = std::pow (10.0f, note.gainDB / 20.0f);

        // Render note audio into a temporary buffer, then mix with crossfade
        juce::AudioBuffer<float> noteBuffer (1, dstLength);
        noteBuffer.clear();
        float* noteBuf = noteBuffer.getWritePointer (0);

        if (hasDrift)
        {
            // Drift processing: sub-segment pitch correction
            auto driftResult = processNoteWithDrift (originalData + srcStart, srcLength,
                                                      sampleRate, note, pitchCurve);

            const float* driftData = driftResult.getReadPointer (0);
            int driftLen = juce::jmin (driftResult.getNumSamples(), dstLength);

            for (int i = 0; i < driftLen; ++i)
                noteBuf[i] = driftData[i] * gainFactor;
        }
        else if (! needsPitchTimeProcessing)
        {
            int toCopy = juce::jmin (srcLength, dstLength);
            for (int i = 0; i < toCopy; ++i)
                noteBuf[i] = originalData[srcStart + i] * gainFactor;
        }
        else
        {
            auto processed = processNote (originalData + srcStart, srcLength,
                                           sampleRate, note.pitchShiftSemitones,
                                           note.formantShiftSemitones, note.durationScale);

            const float* procData = processed.getReadPointer (0);
            int procLength = juce::jmin (processed.getNumSamples(), dstLength);

            for (int i = 0; i < procLength; ++i)
                noteBuf[i] = procData[i] * gainFactor;
        }

        // Apply raised-cosine fade-in/fade-out at note edges
        int fadeLen = juce::jmin (crossfadeSamples, dstLength / 2);
        for (int i = 0; i < fadeLen; ++i)
        {
            float fade = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::pi
                                                    * static_cast<float> (i) / static_cast<float> (fadeLen)));
            noteBuf[i] *= fade;
            noteBuf[dstLength - 1 - i] *= fade;
        }

        // Mix into output
        for (int i = 0; i < dstLength; ++i)
            outputData[dstStart + i] += noteBuf[i];
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

juce::AudioBuffer<float> AudioEngine::processNoteWithDrift (const float* input, int numSamples,
                                                              double sampleRate, const Note& note,
                                                              const PitchCurve& pitchCurve)
{
    // Sub-segment processing for drift control.
    // Split the note into ~50ms segments, compute per-segment pitch correction
    // that flattens or exaggerates the original pitch variation.

    int segmentSamples = static_cast<int> (0.05 * sampleRate); // 50ms segments
    segmentSamples = juce::jmax (256, segmentSamples);

    // Compute average pitch for this note from the pitch curve
    float avgMidi = 0.0f;
    int avgCount = 0;
    auto frameIt = std::lower_bound (pitchCurve.begin(), pitchCurve.end(), note.startTime,
        [] (const PitchFrame& f, double t) { return f.time < t; });

    for (auto it = frameIt; it != pitchCurve.end() && it->time <= note.endTime(); ++it)
    {
        if (it->frequency > 0.0f && it->confidence >= 0.3f)
        {
            avgMidi += 69.0f + 12.0f * std::log2 (it->frequency / 440.0f);
            avgCount++;
        }
    }

    if (avgCount > 0)
        avgMidi /= static_cast<float> (avgCount);
    else
        avgMidi = static_cast<float> (note.midiNote);

    // Process each sub-segment with its own pitch correction
    juce::AudioBuffer<float> output (1, numSamples * 2); // extra space for time stretching
    output.clear();
    int outputPos = 0;

    int crossLen = juce::jmin (static_cast<int> (0.003 * sampleRate), segmentSamples / 4);

    for (int segStart = 0; segStart < numSamples; segStart += segmentSamples)
    {
        int segLen = juce::jmin (segmentSamples, numSamples - segStart);
        if (segLen < 64)
            break;

        // Find the average pitch of this segment from pitch curve
        double segTimeStart = note.startTime + static_cast<double> (segStart) / sampleRate;
        double segTimeEnd = segTimeStart + static_cast<double> (segLen) / sampleRate;

        float segMidi = avgMidi;
        int segCount = 0;
        float segSum = 0.0f;

        for (auto it = frameIt; it != pitchCurve.end() && it->time <= segTimeEnd; ++it)
        {
            if (it->time >= segTimeStart && it->frequency > 0.0f && it->confidence >= 0.3f)
            {
                segSum += 69.0f + 12.0f * std::log2 (it->frequency / 440.0f);
                segCount++;
            }
        }
        if (segCount > 0)
            segMidi = segSum / static_cast<float> (segCount);

        // Drift correction: at driftScale=0, flatten to average (correct all deviation)
        // at driftScale=1, no change. at driftScale=2, double the deviation.
        float deviation = segMidi - avgMidi;
        float targetDeviation = deviation * note.pitchDriftScale;
        float driftCorrection = targetDeviation - deviation;

        // Total pitch shift for this segment
        float totalPitchShift = note.pitchShiftSemitones + driftCorrection;

        bool needsProcessing = std::abs (totalPitchShift) > 0.01f
                            || std::abs (note.formantShiftSemitones) > 0.01f
                            || std::abs (note.durationScale - 1.0) > 0.01;

        if (! needsProcessing)
        {
            // Direct copy
            int copyLen = juce::jmin (segLen, output.getNumSamples() - outputPos);
            float* outPtr = output.getWritePointer (0);
            for (int i = 0; i < copyLen; ++i)
                outPtr[outputPos + i] = input[segStart + i];
            outputPos += copyLen;
        }
        else
        {
            auto segResult = processNote (input + segStart, segLen, sampleRate,
                                           totalPitchShift, note.formantShiftSemitones,
                                           note.durationScale);

            int resultLen = juce::jmin (segResult.getNumSamples(),
                                         output.getNumSamples() - outputPos);
            const float* segData = segResult.getReadPointer (0);
            float* outPtr = output.getWritePointer (0);

            // Apply short crossfade at segment boundary to avoid clicks
            for (int i = 0; i < resultLen; ++i)
            {
                if (i < crossLen && outputPos > 0)
                {
                    float fade = static_cast<float> (i) / static_cast<float> (crossLen);
                    outPtr[outputPos + i] = outPtr[outputPos + i] * (1.0f - fade) + segData[i] * fade;
                }
                else
                {
                    outPtr[outputPos + i] = segData[i];
                }
            }
            outputPos += resultLen;
        }
    }

    // Trim output to actual length
    if (outputPos > 0 && outputPos < output.getNumSamples())
    {
        juce::AudioBuffer<float> trimmed (1, outputPos);
        trimmed.copyFrom (0, 0, output, 0, 0, outputPos);
        return trimmed;
    }

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
