#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "NoteData.h"

// Exports notes to a standard MIDI file.
class MidiExporter
{
public:
    // Export the given notes to a MIDI file at the specified path.
    // Returns true on success.
    static bool exportToFile (const juce::File& file, const NoteList& notes,
                              double bpm = 120.0, int ticksPerQuarter = 480);
};
