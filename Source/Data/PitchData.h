// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 InKey contributors
// This file is part of InKey. See LICENSE for details.

#pragma once

#include <vector>

struct PitchFrame
{
    double time;        // Timestamp in seconds
    float frequency;    // Detected pitch in Hz (0.0 = unvoiced)
    float confidence;   // Detection confidence (0.0 to 1.0)
};

using PitchCurve = std::vector<PitchFrame>;
