// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 InKey contributors
// This file is part of InKey. See LICENSE for details.

#include "MidiExporter.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

bool MidiExporter::exportToFile (const juce::File& file, const NoteList& notes,
                                  double bpm, int ticksPerQuarter)
{
    if (notes.empty())
        return false;

    juce::MidiMessageSequence sequence;

    double ticksPerSecond = (bpm / 60.0) * ticksPerQuarter;

    for (const auto& note : notes)
    {
        if (note.isMuted)
            continue;

        // MIDI note number: original + rounded pitch shift
        int midiNote = note.midiNote + static_cast<int> (std::round (note.pitchShiftSemitones));
        midiNote = juce::jlimit (0, 127, midiNote);

        // Velocity from confidence (1-127)
        int velocity = juce::jlimit (1, 127,
            static_cast<int> (note.confidence * 127.0f));

        // Timing in ticks
        double startTick = note.editedStartTime() * ticksPerSecond;
        double endTick = (note.editedStartTime() + note.editedDuration()) * ticksPerSecond;

        sequence.addEvent (juce::MidiMessage::noteOn (1, midiNote, static_cast<juce::uint8> (velocity)),
                           startTick);
        sequence.addEvent (juce::MidiMessage::noteOff (1, midiNote),
                           endTick);
    }

    sequence.updateMatchedPairs();

    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote (ticksPerQuarter);

    // Add tempo track
    juce::MidiMessageSequence tempoTrack;
    tempoTrack.addEvent (juce::MidiMessage::tempoMetaEvent (
        static_cast<int> (60000000.0 / bpm)), 0.0);
    midiFile.addTrack (tempoTrack);

    // Add note track
    midiFile.addTrack (sequence);

    // Write to file
    juce::FileOutputStream stream (file);
    if (! stream.openedOk())
        return false;

    return midiFile.writeTo (stream);
}
