#pragma once

#include "NoteData.h"
#include "PitchData.h"
#include <juce_audio_basics/juce_audio_basics.h>

struct Take
{
    int id = 0;
    juce::AudioBuffer<float> audio;
    double sampleRate = 44100.0;
    PitchCurve pitchCurve;
    NoteList notes;
    double duration = 0.0;
    bool isReference = false;
    bool isAligned = false;
};

using TakeList = std::vector<Take>;
