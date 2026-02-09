#pragma once

#include <vector>
#include <cmath>

struct Note
{
    int id = 0;

    // Original analysis results (immutable after analysis)
    double startTime = 0.0;       // Start time in seconds
    double duration = 0.0;        // Duration in seconds
    float originalPitch = 0.0f;   // Average pitch in Hz
    int midiNote = 0;             // Quantized MIDI note number (0-127)
    float confidence = 0.0f;      // Average detection confidence

    // User edits (mutable)
    float pitchShiftSemitones = 0.0f;
    float formantShiftSemitones = 0.0f;
    double timeOffset = 0.0;
    double durationScale = 1.0;

    // Check if note has any edits
    bool isEdited() const
    {
        return std::abs (pitchShiftSemitones) > 0.01f
            || std::abs (formantShiftSemitones) > 0.01f
            || std::abs (timeOffset) > 0.001
            || std::abs (durationScale - 1.0) > 0.001;
    }

    // Reset all edits to defaults
    void resetEdits()
    {
        pitchShiftSemitones = 0.0f;
        formantShiftSemitones = 0.0f;
        timeOffset = 0.0;
        durationScale = 1.0;
    }

    // Computed helpers
    float editedPitch() const
    {
        return originalPitch * std::pow (2.0f, pitchShiftSemitones / 12.0f);
    }

    double editedStartTime() const { return startTime + timeOffset; }
    double editedDuration() const { return duration * durationScale; }
    double endTime() const { return startTime + duration; }

    // Convert frequency to MIDI note number
    static int frequencyToMidi (float freqHz)
    {
        if (freqHz <= 0.0f) return 0;
        return static_cast<int> (std::round (69.0f + 12.0f * std::log2 (freqHz / 440.0f)));
    }

    // Convert MIDI note number to frequency
    static float midiToFrequency (int midi)
    {
        return 440.0f * std::pow (2.0f, (static_cast<float> (midi) - 69.0f) / 12.0f);
    }

    // Get note name string (e.g. "C4", "F#3")
    static const char* midiToNoteName (int midi)
    {
        static const char* noteNames[] = {
            "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
        };
        int note = midi % 12;
        return noteNames[note];
    }

    static int midiToOctave (int midi)
    {
        return (midi / 12) - 1;
    }
};

using NoteList = std::vector<Note>;
