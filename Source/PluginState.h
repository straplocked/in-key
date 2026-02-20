// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 InKey contributors
// This file is part of InKey. See LICENSE for details.

#pragma once

enum class PluginState
{
    IDLE,        // Non-ARA only: waiting for user to start capture
    CAPTURING,   // Non-ARA only: recording audio from processBlock
    ANALYZING,   // Both modes: running pitch detection
    EDITING,     // Both modes: user editing notes
    PLAYING      // Non-ARA only: plugin-driven playback (ARA uses renderer)
};
