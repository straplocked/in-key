// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 InKey contributors
// This file is part of InKey. See LICENSE for details.

#include "AudioEngine.h"
#include "../Analysis/VibratoAnalyzer.h"
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
        // Skip muted notes
        if (note.isMuted)
            continue;

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

        bool hasDrift = (std::abs (note.pitchDriftScale - 1.0f) > 0.01f
                         || (note.vibratoRate > 0.1f
                             && (std::abs (note.vibratoDepthScale - 1.0f) > 0.01f
                                 || std::abs (note.driftScale - 1.0f) > 0.01f)))
                        && ! pitchCurve.empty();
        bool hasEnvelope = ! note.pitchEnvelope.empty();

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
        else if (hasEnvelope)
        {
            // Pitch envelope: sub-segment processing based on control points
            int segmentSamples = juce::jmax (256, static_cast<int> (0.02 * sampleRate));

            for (int segStart = 0; segStart < srcLength; segStart += segmentSamples)
            {
                int segLen = juce::jmin (segmentSamples, srcLength - segStart);
                if (segLen < 64) break;

                // Compute time position within note
                double segTimeNorm = static_cast<double> (segStart) / srcLength;

                // Interpolate pitch offset from envelope
                float pitchOffset = 0.0f;
                if (note.pitchEnvelope.size() == 1)
                {
                    pitchOffset = note.pitchEnvelope[0].pitchOffsetSemitones;
                }
                else
                {
                    // Linear interpolation between surrounding control points
                    for (size_t ci = 0; ci + 1 < note.pitchEnvelope.size(); ++ci)
                    {
                        const auto& cpA = note.pitchEnvelope[ci];
                        const auto& cpB = note.pitchEnvelope[ci + 1];
                        if (segTimeNorm >= cpA.timeNormalized && segTimeNorm <= cpB.timeNormalized)
                        {
                            double range = cpB.timeNormalized - cpA.timeNormalized;
                            float t = range > 0.001 ? static_cast<float> ((segTimeNorm - cpA.timeNormalized) / range) : 0.0f;
                            pitchOffset = cpA.pitchOffsetSemitones + t * (cpB.pitchOffsetSemitones - cpA.pitchOffsetSemitones);
                            break;
                        }
                    }
                    // Before first point
                    if (segTimeNorm < note.pitchEnvelope.front().timeNormalized)
                        pitchOffset = note.pitchEnvelope.front().pitchOffsetSemitones;
                    // After last point
                    if (segTimeNorm > note.pitchEnvelope.back().timeNormalized)
                        pitchOffset = note.pitchEnvelope.back().pitchOffsetSemitones;
                }

                float totalPitch = note.pitchShiftSemitones + pitchOffset;
                bool segNeedsProcessing = std::abs (totalPitch) > 0.01f
                    || std::abs (note.formantShiftSemitones) > 0.01f
                    || std::abs (note.durationScale - 1.0) > 0.01;

                int dstOffset = static_cast<int> (static_cast<double> (segStart) / srcLength * dstLength);
                int nextDstOffset = static_cast<int> (static_cast<double> (segStart + segLen) / srcLength * dstLength);
                int dstSegLen = juce::jmin (nextDstOffset - dstOffset, dstLength - dstOffset);

                if (dstSegLen <= 0 || dstOffset >= dstLength) continue;

                if (segNeedsProcessing)
                {
                    auto segResult = processNote (originalData + srcStart + segStart, segLen,
                                                    sampleRate, totalPitch,
                                                    note.formantShiftSemitones, note.durationScale);
                    const float* segData = segResult.getReadPointer (0);
                    int segOutLen = juce::jmin (segResult.getNumSamples(), dstSegLen);
                    for (int i = 0; i < segOutLen; ++i)
                        noteBuf[dstOffset + i] = segData[i] * gainFactor;
                }
                else
                {
                    int toCopy = juce::jmin (segLen, dstSegLen);
                    for (int i = 0; i < toCopy; ++i)
                        noteBuf[dstOffset + i] = originalData[srcStart + segStart + i] * gainFactor;
                }
            }
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

        // Apply pitch transition glide at the beginning of the note
        if (note.transitionInDuration > 0.001f)
        {
            // Find previous note to glide from
            const Note* prevNote = nullptr;
            double closestGap = 0.1;
            for (const auto& other : notes)
            {
                if (other.id == note.id || other.isMuted) continue;
                double otherEnd = other.editedStartTime() + other.editedDuration();
                double gap = note.editedStartTime() - otherEnd;
                if (gap >= -0.01 && gap < closestGap)
                {
                    prevNote = &other;
                    closestGap = gap;
                }
            }

            if (prevNote != nullptr)
            {
                int transitionSamples = static_cast<int> (note.transitionInDuration * sampleRate);
                transitionSamples = juce::jmin (transitionSamples, dstLength / 2);

                float prevMidi = static_cast<float> (prevNote->midiNote) + prevNote->pitchShiftSemitones;
                float curMidi = static_cast<float> (note.midiNote) + note.pitchShiftSemitones;
                float pitchDiff = prevMidi - curMidi;

                // Apply pitch ramp by reprocessing the transition region
                if (std::abs (pitchDiff) > 0.1f && transitionSamples > 64)
                {
                    int subSegLen = juce::jmax (64, transitionSamples / 4);
                    for (int segStart = 0; segStart < transitionSamples; segStart += subSegLen)
                    {
                        int segLen = juce::jmin (subSegLen, transitionSamples - segStart);
                        float t = static_cast<float> (segStart + segLen / 2) / static_cast<float> (transitionSamples);

                        // Shape the interpolation curve
                        if (note.transitionInShape > 0.01f)
                            t = std::pow (t, 1.0f + note.transitionInShape);
                        else if (note.transitionInShape < -0.01f)
                            t = 1.0f - std::pow (1.0f - t, 1.0f - note.transitionInShape);

                        float glidePitch = pitchDiff * (1.0f - t);
                        float totalPitch = note.pitchShiftSemitones + glidePitch;

                        // Only reprocess if there's meaningful pitch difference
                        if (std::abs (glidePitch) > 0.05f)
                        {
                            int srcSeg = static_cast<int> (static_cast<double> (segStart) / dstLength * srcLength);
                            int srcSegLen = juce::jmin (segLen, srcLength - srcSeg);
                            if (srcSegLen > 32 && srcSeg >= 0 && srcSeg < srcLength)
                            {
                                auto segResult = processNote (originalData + srcStart + srcSeg, srcSegLen,
                                                                sampleRate, totalPitch,
                                                                note.formantShiftSemitones, note.durationScale);
                                const float* segData = segResult.getReadPointer (0);
                                int segOutLen = juce::jmin (segResult.getNumSamples(), segLen);
                                for (int i = 0; i < segOutLen; ++i)
                                    noteBuf[segStart + i] = segData[i] * gainFactor;
                            }
                        }
                    }
                }
            }
        }

        // Apply user-specified note fades (separate from crossfade)
        if (note.fadeInMs > 0.01f)
        {
            int fadeInSamples = juce::jmin (static_cast<int> (note.fadeInMs * 0.001 * sampleRate),
                                             dstLength / 2);
            for (int i = 0; i < fadeInSamples; ++i)
            {
                float fade = static_cast<float> (i) / static_cast<float> (fadeInSamples);
                noteBuf[i] *= fade;
            }
        }

        if (note.fadeOutMs > 0.01f)
        {
            int fadeOutSamples = juce::jmin (static_cast<int> (note.fadeOutMs * 0.001 * sampleRate),
                                              dstLength / 2);
            for (int i = 0; i < fadeOutSamples; ++i)
            {
                float fade = static_cast<float> (i) / static_cast<float> (fadeOutSamples);
                noteBuf[dstLength - 1 - i] *= fade;
            }
        }

        // Sibilant preservation: replace pitch-shifted audio with original in sibilant regions
        if (! note.sibilantRegions.empty())
        {
            float sibilantGain = std::pow (10.0f, note.sibilantGainDB / 20.0f) * gainFactor;
            int sibFadeSamples = static_cast<int> (0.003 * sampleRate); // 3ms crossfade

            for (const auto& region : note.sibilantRegions)
            {
                // Map sibilant region (original time) to position within the note buffer
                double relStart = (region.first - note.startTime) / note.duration;
                double relEnd = (region.second - note.startTime) / note.duration;
                relStart = juce::jlimit (0.0, 1.0, relStart);
                relEnd = juce::jlimit (0.0, 1.0, relEnd);

                int sibDstStart = static_cast<int> (relStart * dstLength);
                int sibDstEnd = static_cast<int> (relEnd * dstLength);
                sibDstStart = juce::jlimit (0, dstLength, sibDstStart);
                sibDstEnd = juce::jlimit (0, dstLength, sibDstEnd);

                // Source samples from original audio
                int sibSrcStart = static_cast<int> (region.first * sampleRate);
                sibSrcStart = juce::jlimit (0, totalSamples, sibSrcStart);

                int sibFade = juce::jmin (sibFadeSamples, (sibDstEnd - sibDstStart) / 4);

                for (int i = sibDstStart; i < sibDstEnd; ++i)
                {
                    int srcIdx = sibSrcStart + (i - sibDstStart);
                    if (srcIdx < 0 || srcIdx >= totalSamples) continue;

                    float origSample = originalData[srcIdx] * sibilantGain;
                    float crossfade = 1.0f;

                    // Crossfade in at start of sibilant region
                    int fromStart = i - sibDstStart;
                    if (fromStart < sibFade && sibFade > 0)
                        crossfade = static_cast<float> (fromStart) / static_cast<float> (sibFade);

                    // Crossfade out at end of sibilant region
                    int fromEnd = sibDstEnd - 1 - i;
                    if (fromEnd < sibFade && sibFade > 0)
                        crossfade = juce::jmin (crossfade,
                                                  static_cast<float> (fromEnd) / static_cast<float> (sibFade));

                    // Blend: crossfade from processed to original
                    noteBuf[i] = noteBuf[i] * (1.0f - crossfade) + origSample * crossfade;
                }
            }
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

    // Build note envelope that mirrors the per-note raised-cosine fades.
    // This lets us smoothly crossfade between note audio and gap (original) audio
    // at note boundaries, avoiding discontinuities.
    std::vector<float> noteEnvelope (static_cast<size_t> (totalSamples), 0.0f);
    for (const auto& note : notes)
    {
        if (note.isMuted)
            continue;

        int dstStart = juce::jlimit (0, totalSamples, static_cast<int> (note.editedStartTime() * sampleRate));
        int dstEnd = juce::jlimit (0, totalSamples, static_cast<int> ((note.editedStartTime() + note.editedDuration()) * sampleRate));
        int dstLength = dstEnd - dstStart;
        if (dstLength <= 0) continue;

        int fadeLen = juce::jmin (crossfadeSamples, dstLength / 2);

        for (int i = 0; i < dstLength; ++i)
        {
            float env = 1.0f;
            if (i < fadeLen)
            {
                env = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::pi
                                                  * static_cast<float> (i) / static_cast<float> (fadeLen)));
            }
            else if (i >= dstLength - fadeLen)
            {
                int fromEnd = dstLength - 1 - i;
                env = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::pi
                                                  * static_cast<float> (fromEnd) / static_cast<float> (fadeLen)));
            }

            auto idx = static_cast<size_t> (dstStart + i);
            noteEnvelope[idx] = juce::jmax (noteEnvelope[idx], env);
        }
    }

    // Blend gap audio: where noteEnvelope < 1.0, mix in original audio.
    // Note audio was already faded by the same envelope shape, so
    // note * env + original * (1 - env) = smooth crossfade at boundaries.
    for (int i = 0; i < totalSamples; ++i)
    {
        float env = noteEnvelope[static_cast<size_t> (i)];
        if (env < 0.999f)
            outputData[i] += originalData[i] * (1.0f - env);
    }

    // Apply short fade at rendered buffer edges to prevent pops when
    // playback starts/stops at the buffer boundaries
    int bufferFadeLen = juce::jmin (256, totalSamples / 2);
    for (int i = 0; i < bufferFadeLen; ++i)
    {
        float fade = static_cast<float> (i) / static_cast<float> (bufferFadeLen);
        outputData[i] *= fade;
        outputData[totalSamples - 1 - i] *= fade;
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

    // Process each sub-segment with its own pitch correction.
    // Use overlap-add with raised-cosine crossfades at segment boundaries.
    juce::AudioBuffer<float> output (1, numSamples * 2); // extra space for time stretching
    output.clear();
    int outputPos = 0;

    int crossLen = juce::jmin (static_cast<int> (0.005 * sampleRate), segmentSamples / 4); // 5ms

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

        // Decompose deviation into vibrato + drift if vibrato was detected
        float deviation = segMidi - avgMidi;
        float targetDeviation;

        if (note.vibratoRate > 0.1f
            && (std::abs (note.vibratoDepthScale - 1.0f) > 0.01f
                || std::abs (note.driftScale - 1.0f) > 0.01f))
        {
            // Use vibrato-aware decomposition
            double segTimeMid = segTimeStart + static_cast<double> (segLen) / (2.0 * sampleRate);
            targetDeviation = VibratoAnalyzer::decomposePitchDeviation (
                deviation, avgMidi, segMidi, segTimeMid,
                note.startTime, note.vibratoRate,
                note.vibratoDepthScale, note.driftScale);
        }
        else
        {
            // Legacy: uniform scaling with pitchDriftScale
            targetDeviation = deviation * note.pitchDriftScale;
        }

        float driftCorrection = targetDeviation - deviation;

        // Total pitch shift for this segment
        float totalPitchShift = note.pitchShiftSemitones + driftCorrection;

        bool needsProcessing = std::abs (totalPitchShift) > 0.01f
                            || std::abs (note.formantShiftSemitones) > 0.01f
                            || std::abs (note.durationScale - 1.0) > 0.01;

        // Get segment audio (processed or direct)
        juce::AudioBuffer<float> segResult;
        if (needsProcessing)
        {
            segResult = processNote (input + segStart, segLen, sampleRate,
                                      totalPitchShift, note.formantShiftSemitones,
                                      note.durationScale);
        }
        else
        {
            segResult.setSize (1, segLen);
            segResult.copyFrom (0, 0, input + segStart, segLen);
        }

        int resultLen = juce::jmin (segResult.getNumSamples(),
                                     output.getNumSamples() - outputPos);
        const float* segData = segResult.getReadPointer (0);
        float* outPtr = output.getWritePointer (0);

        bool isFirstSeg = (segStart == 0);
        bool isLastSeg = (segStart + segmentSamples >= numSamples);

        // Apply raised-cosine crossfade at segment boundaries
        for (int i = 0; i < resultLen; ++i)
        {
            float window = 1.0f;

            // Fade-in at segment start (skip for first segment)
            if (i < crossLen && ! isFirstSeg)
            {
                window = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::pi
                                                     * static_cast<float> (i) / static_cast<float> (crossLen)));
            }
            // Fade-out at segment end (skip for last segment)
            else if (i >= resultLen - crossLen && ! isLastSeg)
            {
                int fromEnd = resultLen - 1 - i;
                window = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::pi
                                                     * static_cast<float> (fromEnd) / static_cast<float> (crossLen)));
            }

            outPtr[outputPos + i] += segData[i] * window;
        }

        // Advance by segment length minus overlap so segments overlap at boundaries
        int advance = resultLen;
        if (! isLastSeg)
            advance = juce::jmax (1, resultLen - crossLen);
        outputPos += advance;
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
    rampUpRemaining.store (kRampSamples);
    playing.store (true);
}

void AudioEngine::startPlaybackFromSample (const juce::AudioBuffer<float>& renderedAudio, double sampleRate, int startSample)
{
    playbackBuffer.makeCopyOf (renderedAudio);
    playbackSampleRate = sampleRate;
    int clamped = juce::jlimit (0, playbackBuffer.getNumSamples(), startSample);
    playbackPosition.store (clamped);
    rampUpRemaining.store (kRampSamples);
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
    bool isLastBlock = (pos + toCopy) >= playbackBuffer.getNumSamples();
    const float* src = playbackBuffer.getReadPointer (0, pos);

    // Copy into all output channels
    for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch)
    {
        float* dst = outputBuffer.getWritePointer (ch);
        for (int i = 0; i < toCopy; ++i)
            dst[i] = src[i];
    }

    // Apply fade-in ramp at playback start
    int rampUp = rampUpRemaining.load();
    if (rampUp > 0)
    {
        int rampThisBlock = juce::jmin (rampUp, toCopy);
        int rampOffset = kRampSamples - rampUp;

        for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch)
        {
            float* dst = outputBuffer.getWritePointer (ch);
            for (int i = 0; i < rampThisBlock; ++i)
            {
                float fade = static_cast<float> (rampOffset + i) / static_cast<float> (kRampSamples);
                dst[i] *= fade;
            }
        }
        rampUpRemaining.store (rampUp - rampThisBlock);
    }

    // Apply fade-out ramp on the last block
    if (isLastBlock)
    {
        int fadeLen = juce::jmin (kRampSamples, toCopy);
        int fadeStart = toCopy - fadeLen;

        for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch)
        {
            float* dst = outputBuffer.getWritePointer (ch);
            for (int i = 0; i < fadeLen; ++i)
            {
                float fade = 1.0f - (static_cast<float> (i) / static_cast<float> (fadeLen));
                dst[fadeStart + i] *= fade;
            }
        }
    }

    playbackPosition.store (pos + toCopy);
    return ! isLastBlock;
}

double AudioEngine::getPlaybackPosition() const
{
    if (playbackSampleRate <= 0.0)
        return 0.0;
    return static_cast<double> (playbackPosition.load()) / playbackSampleRate;
}

bool AudioEngine::fillBufferAtPosition (juce::AudioBuffer<float>& outputBuffer, int numSamples,
                                         int64_t dawPositionInSamples, int64_t captureStartSample)
{
    if (! hasRenderedAudio.load())
    {
        wasPlayingLastBlock.store (false);
        return false;
    }

    int bufIdx = activeBufferIndex.load();
    const auto& rendered = renderedBuffers[bufIdx];

    if (rendered.getNumSamples() == 0)
    {
        wasPlayingLastBlock.store (false);
        return false;
    }

    int64_t offsetInCapture = dawPositionInSamples - captureStartSample;

    // If DAW position is entirely outside our capture region, output silence
    if (offsetInCapture >= rendered.getNumSamples() || offsetInCapture + numSamples <= 0)
    {
        wasPlayingLastBlock.store (false);
        return false;
    }

    const float* src = rendered.getReadPointer (0);

    // Detect buffer swap for crossfade (Fix 4)
    int prevBufIdx = lastUsedBufferIndex.exchange (bufIdx);
    bool swapOccurred = (prevBufIdx != bufIdx) && wasPlayingLastBlock.load();

    if (swapOccurred)
        swapCrossfadeRemaining.store (kSwapCrossfadeSamples);

    int crossfadeLeft = swapCrossfadeRemaining.load();

    for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch)
    {
        float* dst = outputBuffer.getWritePointer (ch);

        for (int i = 0; i < numSamples; ++i)
        {
            int64_t readPos = offsetInCapture + i;
            if (readPos >= 0 && readPos < rendered.getNumSamples())
            {
                float newSample = src[readPos];

                // Crossfade from old to new buffer after a swap
                if (crossfadeLeft > 0 && i < crossfadeLeft)
                {
                    const auto& oldRendered = renderedBuffers[prevBufIdx];
                    float oldSample = 0.0f;
                    if (readPos < oldRendered.getNumSamples())
                        oldSample = oldRendered.getReadPointer (0)[readPos];

                    int crossfadePos = kSwapCrossfadeSamples - crossfadeLeft + i;
                    float fade = static_cast<float> (crossfadePos) / static_cast<float> (kSwapCrossfadeSamples);
                    dst[i] = oldSample * (1.0f - fade) + newSample * fade;
                }
                else
                {
                    dst[i] = newSample;
                }
            }
            // else: already cleared to 0
        }
    }

    // Update crossfade remaining
    if (crossfadeLeft > 0)
    {
        int consumed = juce::jmin (crossfadeLeft, numSamples);
        swapCrossfadeRemaining.store (crossfadeLeft - consumed);
    }

    // Fade-in if this is the first block we're providing audio
    bool wasPlaying = wasPlayingLastBlock.exchange (true);
    if (! wasPlaying)
    {
        int fadeLen = juce::jmin (kRampSamples, numSamples);
        for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch)
        {
            float* dst = outputBuffer.getWritePointer (ch);
            for (int i = 0; i < fadeLen; ++i)
            {
                float fade = static_cast<float> (i) / static_cast<float> (fadeLen);
                dst[i] *= fade;
            }
        }
    }

    return true;
}

void AudioEngine::swapRenderedBuffer (juce::AudioBuffer<float>&& newBuffer)
{
    // Write to the inactive buffer slot, then flip the index
    int inactiveIndex = 1 - activeBufferIndex.load();
    renderedBuffers[inactiveIndex] = std::move (newBuffer);
    activeBufferIndex.store (inactiveIndex);
    hasRenderedAudio.store (true);
}
