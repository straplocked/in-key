# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Summary

InKey is a Melodyne-style VST3 plugin for graphical vocal pitch correction and multi-take alignment. It uses **offline processing** (capture → analyze → edit), not real-time. The project is GPL-3.0 open source, targeting macOS and Windows.

**Status:** Feature-complete. All 8 development phases plus Melodyne parity batches (A-G) are implemented. See `Docs/ROADMAP.md` for details.

## Build Commands

```bash
# Clone dependencies (if not already present)
git clone --depth 1 --branch 7.0.12 \
  https://github.com/juce-framework/JUCE.git ThirdParty/JUCE
git clone --depth 1 \
  https://github.com/breakfastquay/rubberband.git ThirdParty/rubberband

# Build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# Run tests
cmake --build build --target InKeyTests
./build/Tests/InKeyTests
```

## Architecture

**State machine:** IDLE → CAPTURING → ANALYZING → EDITING ↔ PLAYING

**Three threads with strict rules:**
- **Audio thread** (`processBlock`): Lock-free only. No allocation, no mutexes, no I/O. Use `juce::AbstractFifo` for data transfer.
- **Message thread**: GUI updates, user interaction. Use `juce::MessageManager::callAsync()` for callbacks from background threads.
- **Background thread**: Heavy computation (pitch analysis, audio rendering). Use `std::atomic` for state flags.

**Four processing pipelines:**
1. **Capture:** DAW → lock-free FIFO → AudioBuffer storage
2. **Analysis:** AudioBuffer → standalone YIN pitch detection → note segmentation → NoteData
3. **Playback:** Notes + user edits → per-note Rubber Band pitch/time shift → cached output → DAW
4. **Alignment:** MFCC features (standalone, JUCE FFT) → DTW cost matrix → time-warp curve → Rubber Band time-stretch

**Key data flow:** Edits are per-note. Original analysis data is immutable. Processed audio is cached per-note and only re-rendered when that note's parameters change.

## Tech Stack

- **JUCE 7.0.12** (GPL): Plugin wrapper, GUI, DSP utilities, threading primitives
- **Rubber Band v3.3.0** (GPL): Pitch shifting, time stretching, formant preservation (offline mode with `study()` then `process()`)
- **Standalone YIN**: Pitch detection (implemented in `Source/Analysis/PitchDetector.h/cpp`)
- **Standalone MFCC**: Feature extraction using JUCE FFT (implemented in `Source/Alignment/MFCCExtractor.h/cpp`)
- **C++17**, CMake 3.22+

## Code Organization

Source code is in `Source/` with these subdirectories:
- `Audio/` — Capture and buffer management
- `Analysis/` — YIN pitch detection, note segmentation, sibilant detection, vibrato analysis, key detection
- `DSP/` — AudioEngine with Rubber Band integration
- `Data/` — NoteData, TakeData, PitchData, MidiExporter
- `Alignment/` — DTWAligner, MFCCExtractor
- `Editing/` — EditActions (undo/redo), QuantizationEngine
- `UI/` — PianoRollComponent, WaveformComponent, PitchCurveComponent, PropertiesPanel, DetectionSettingsDialog, InKeyLookAndFeel

## Important Constraints

- The user is new to C++ and audio DSP. Prefer using library APIs (JUCE, Rubber Band) over writing DSP from scratch.
- Audio thread safety is critical: never allocate, lock, or do I/O in `processBlock()`.
- Plugin state must serialize/deserialize via `juce::ValueTree` for DAW project saves.
- Plugin identifiers: manufacturer code `Inky`, plugin code `Vedt`, bundle ID `com.inkey.vocaleditor`.

## Documentation

- `Docs/ARCHITECTURE.md` — System design, data structures, threading model, UI component hierarchy
- `Docs/ROADMAP.md` — 8-phase implementation plan with task checklists and completion criteria
- `Docs/TECH_STACK.md` — Library APIs, algorithm parameters, integration details
