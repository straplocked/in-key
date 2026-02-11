// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 InKey contributors
// This file is part of InKey. See LICENSE for details.

#pragma once

#include "../Data/PitchData.h"
#include "../Data/NoteData.h"

// Segments a continuous pitch curve into discrete notes.
// A note is a region of voiced audio where the pitch stays
// within a semitone of the average, without large gaps.
class NoteSegmenter
{
public:
    struct Settings
    {
        float confidenceThreshold = 0.3f;    // Min confidence to count as voiced
        float maxSemitoneDrift = 0.8f;       // Max drift from note average before splitting
        double minNoteDuration = 0.05;       // Min note duration in seconds (50ms)
        double maxGapDuration = 0.03;        // Max unvoiced gap within a note (30ms)
    };

    NoteSegmenter();

    void setSettings (const Settings& settings);
    const Settings& getSettings() const { return settings; }

    // Segment a pitch curve into notes.
    NoteList segment (const PitchCurve& pitchCurve) const;

private:
    Settings settings;
};
