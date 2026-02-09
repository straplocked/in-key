# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Summary

InKey is a Melodyne-style VST3/AU plugin for graphical vocal pitch correction and multi-take alignment. It uses **offline processing** (capture → analyze → edit), not real-time. The project is GPL-3.0 open source, targeting macOS and Windows.

**Status:** Pre-development. No source code exists yet. Check `Docs/ROADMAP.md` for current phase and next steps.

## Build Commands

Once Phase 1 is complete, the build system will be CMake + JUCE:

```bash
# macOS
cmake -B build -G Xcode
cmake --build build --config Release

# Windows
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

ThirdParty dependencies must be cloned manually (not committed to repo):
```bash
git clone https://github.com/juce-framework/JUCE.git ThirdParty/JUCE
git clone https://github.com/breakfastquay/rubberband.git ThirdParty/rubberband
git clone https://github.com/MTG/essentia.git ThirdParty/essentia
```

## Architecture

**State machine:** IDLE → CAPTURING → ANALYZING → EDITING ↔ PLAYING

**Three threads with strict rules:**
- **Audio thread** (`processBlock`): Lock-free only. No allocation, no mutexes, no I/O. Use `juce::AbstractFifo` for data transfer.
- **Message thread**: GUI updates, user interaction. Use `juce::MessageManager::callAsync()` for callbacks from background threads.
- **Background thread**: Heavy computation (pitch analysis, audio rendering). Use `std::atomic` for state flags.

**Four processing pipelines:**
1. **Capture:** DAW → lock-free FIFO → AudioBuffer storage
2. **Analysis:** AudioBuffer → Essentia pYIN pitch detection → onset detection → note segmentation → AnalyzedAudio
3. **Playback:** AnalyzedAudio + user edits → per-note Rubber Band pitch/time shift → cached output → DAW
4. **Alignment:** MFCC features → DTW cost matrix → time-warp curve → Rubber Band time-stretch

**Key data flow:** Edits are per-note. Original analysis data is immutable. Processed audio is cached per-note and only re-rendered when that note's parameters change.

## Tech Stack

- **JUCE 7.x** (GPL): Plugin wrapper, GUI, DSP utilities, threading primitives
- **Rubber Band v3.3.0** (GPL): Pitch shifting, time stretching, formant preservation (offline mode with `study()` then `process()`)
- **Essentia v2.1_beta6** (AGPL): pYIN pitch detection, onset detection, MFCC extraction. Fallback: standalone YIN/pYIN if Essentia CMake integration is too complex
- **C++17**, CMake 3.22+

## Code Organization (Planned)

Source code goes in `Source/` with these subdirectories:
- `Audio/` — Capture and buffer management
- `Analysis/` — Pitch detection, onset detection, note segmentation
- `DSP/` — AudioEngine, RubberBandWrapper, FormantShifter
- `Data/` — Note, AnalyzedAudio, EditState, MultiTakeSession
- `Alignment/` — DTWAligner, MFCC FeatureExtractor
- `Editing/` — QuantizationEngine, EditHistory (undo/redo)
- `UI/` — PianoRollComponent, WaveformComponent, PitchCurveComponent, PropertiesPanel
- `Utilities/` — Math helpers, pitch/time conversion

## Important Constraints

- The user is new to C++ and audio DSP. Prefer using library APIs (JUCE, Rubber Band, Essentia) over writing DSP from scratch.
- Audio thread safety is critical: never allocate, lock, or do I/O in `processBlock()`.
- Plugin state must serialize/deserialize via `juce::ValueTree` for DAW project saves.
- Plugin identifiers: manufacturer code `Inky`, plugin code `Vedt`, bundle ID `com.inkey.vocaleditor`.

## Documentation

- `Docs/ARCHITECTURE.md` — System design, data structures, threading model, UI component hierarchy
- `Docs/ROADMAP.md` — 8-phase implementation plan with task checklists and completion criteria
- `Docs/TECH_STACK.md` — Library APIs, algorithm parameters, CMake integration snippets, fallback plans
