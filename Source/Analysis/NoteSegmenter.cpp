// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 InKey contributors
// This file is part of InKey. See LICENSE for details.

#include "NoteSegmenter.h"
#include <cmath>

NoteSegmenter::NoteSegmenter()
{
}

void NoteSegmenter::setSettings (const Settings& s)
{
    settings = s;
}

NoteList NoteSegmenter::segment (const PitchCurve& pitchCurve) const
{
    NoteList notes;

    if (pitchCurve.empty())
        return notes;

    int nextId = 1;

    // State for the current note being built
    bool inNote = false;
    double noteStartTime = 0.0;
    double lastVoicedTime = 0.0;
    float pitchSum = 0.0f;
    float confidenceSum = 0.0f;
    int frameCount = 0;
    int currentMidi = 0;

    auto finishNote = [&]()
    {
        if (! inNote || frameCount == 0)
            return;

        double dur = lastVoicedTime - noteStartTime;

        // Add the last frame's duration estimate
        if (pitchCurve.size() > 1)
        {
            double frameDuration = pitchCurve[1].time - pitchCurve[0].time;
            dur += frameDuration;
        }

        if (dur < settings.minNoteDuration)
        {
            inNote = false;
            return;
        }

        Note note;
        note.id = nextId++;
        note.startTime = noteStartTime;
        note.duration = dur;
        note.originalPitch = pitchSum / static_cast<float> (frameCount);
        note.midiNote = Note::frequencyToMidi (note.originalPitch);
        note.confidence = confidenceSum / static_cast<float> (frameCount);
        notes.push_back (note);

        inNote = false;
    };

    for (size_t i = 0; i < pitchCurve.size(); ++i)
    {
        const auto& frame = pitchCurve[i];
        bool voiced = frame.frequency > 0.0f && frame.confidence >= settings.confidenceThreshold;

        if (! voiced)
        {
            // Check if the gap is too long to continue the current note
            if (inNote)
            {
                double gap = frame.time - lastVoicedTime;
                if (gap > settings.maxGapDuration)
                    finishNote();
            }
            continue;
        }

        int frameMidi = Note::frequencyToMidi (frame.frequency);

        if (inNote)
        {
            // Check if pitch has drifted too far from the current note
            float avgPitch = pitchSum / static_cast<float> (frameCount);
            float semitonesDiff = std::abs (12.0f * std::log2 (frame.frequency / avgPitch));

            if (semitonesDiff > settings.maxSemitoneDrift || std::abs (frameMidi - currentMidi) >= 2)
            {
                // Pitch jumped — finish the current note and start a new one
                finishNote();
            }
        }

        if (! inNote)
        {
            // Start a new note
            inNote = true;
            noteStartTime = frame.time;
            pitchSum = 0.0f;
            confidenceSum = 0.0f;
            frameCount = 0;
            currentMidi = frameMidi;
        }

        pitchSum += frame.frequency;
        confidenceSum += frame.confidence;
        frameCount++;
        lastVoicedTime = frame.time;
    }

    // Finish any note in progress
    finishNote();

    return notes;
}
