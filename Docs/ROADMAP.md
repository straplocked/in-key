# InKey — Roadmap

This document tracks the implementation plan, current progress, and what to work on next.

**Last Updated:** 2026-02-11

---

## Current Status

```
Phase 0: Prerequisites        [x] Complete
Phase 1: Foundation            [x] Complete
Phase 2: Audio Capture         [x] Complete
Phase 3: Pitch Detection       [x] Complete
Phase 4: Piano Roll            [x] Complete
Phase 5: Pitch Editing         [x] Complete
Phase 6: Timing & Formants     [x] Complete
Phase 7: Vocal Alignment       [x] Complete
Phase 8: Polish                [x] Complete
Melodyne Parity Batches A-G    [x] Complete
```

**All 8 phases and Melodyne parity batches are complete!** The plugin is feature-complete.

---

## Phase 0: Prerequisites

**Goal:** Learn C++ basics and JUCE fundamentals before writing plugin code.

### Tasks

- [x] Install development tools
  - [x] CMake 3.22+
  - [x] Git
- [x] Clone JUCE framework
- [x] Complete JUCE tutorials and learn AudioProcessor lifecycle
- [x] Understand Component system, threading model, and audio fundamentals

### Done When

- [x] Can build and run a JUCE example project
- [x] Understand how AudioProcessor::processBlock() works
- [x] Understand how Component::paint() and Component::resized() work

---

## Phase 1: Foundation & Basic Plugin

**Goal:** Get a working plugin that loads in a DAW and passes audio through.

### Tasks

- [x] Create project with CMake + JUCE
- [x] Set up CMakeLists.txt with JUCE
- [x] Implement PluginProcessor (pass-through audio)
- [x] Implement PluginEditor (basic UI)
- [x] Test in standalone mode
- [x] Verify audio passes through cleanly

### Files Created

- `CMakeLists.txt`
- `Source/PluginProcessor.h/cpp`
- `Source/PluginEditor.h/cpp`

### Done When

- [x] Plugin appears in DAW plugin list
- [x] Audio plays through the plugin unmodified
- [x] Plugin UI window opens and displays correctly

---

## Phase 2: Audio Capture & Storage

**Goal:** Plugin captures audio from the DAW and displays a waveform.

### Tasks

- [x] Implement AudioCapture class (lock-free FIFO, growing buffer, start/stop)
- [x] Add plugin state machine: IDLE → CAPTURING → ANALYZING → EDITING
- [x] Add "Capture" button to UI
- [x] Implement WaveformComponent (waveform rendering, min/max downsampling)
- [x] Display capture duration and sample rate

### Files Created

- `Source/Audio/AudioCapture.h/cpp`
- `Source/UI/WaveformComponent.h/cpp`
- `Source/PluginState.h`

### Done When

- [x] Can click Capture, play audio in DAW, click Stop
- [x] Waveform of captured audio is displayed
- [x] Audio is stored correctly

---

## Phase 3: Pitch Detection

**Goal:** Analyze captured audio and display a pitch curve.

**Implementation note:** Used standalone YIN algorithm instead of Essentia (avoids complex dependency chain). Full YIN with difference function, cumulative mean normalization, absolute threshold, and parabolic interpolation.

### Tasks

- [x] Implement standalone YIN pitch detection (PitchDetector)
- [x] Create PitchAnalyzer class (background thread wrapper, progress reporting)
- [x] Create PitchCurveComponent (log-scale frequency display, confidence coloring)
- [x] Handle unvoiced regions
- [x] Auto-start analysis after capture

### Files Created

- `Source/Analysis/PitchDetector.h/cpp`
- `Source/Analysis/PitchAnalyzer.h/cpp`
- `Source/Data/PitchData.h`
- `Source/UI/PitchCurveComponent.h/cpp`

### Done When

- [x] After capture + analyze, a pitch curve is displayed
- [x] Pitch curve is reasonably accurate
- [x] Unvoiced regions don't show spurious pitch detections

---

## Phase 4: Note Segmentation & Piano Roll

**Goal:** Segment pitch data into notes and display on an interactive piano roll.

### Tasks

- [x] Implement NoteSegmenter (pitch continuity, confidence thresholds, gap detection)
- [x] Create Note data structure with MIDI conversion and edit fields
- [x] Create PianoRollComponent (grid, piano keys, note blocks, pitch curve overlay, time ruler)
- [x] Implement zoom and pan
- [x] Add click-to-select interaction

### Files Created

- `Source/Analysis/NoteSegmenter.h/cpp`
- `Source/Data/NoteData.h`
- `Source/UI/PianoRollComponent.h/cpp`

### Done When

- [x] Notes appear as rectangles on the piano roll
- [x] Notes are positioned correctly (pitch on Y, time on X)
- [x] Can zoom and pan the view
- [x] Can click to select a note

---

## Phase 5: Rubber Band Integration & Pitch Editing

**Goal:** Edit note pitch by dragging and hear the results.

### Tasks

- [x] Integrate Rubber Band Library (single-file build)
- [x] Implement AudioEngine class (per-note rendering, caching)
- [x] Add pitch editing interaction (vertical drag, semitone snapping, ghost notes)
- [x] Implement playback (Play/Stop, playhead, processBlock output)

### Files Created

- `Source/DSP/AudioEngine.h/cpp`

### Done When

- [x] Can drag a note to a different pitch
- [x] Can press Play and hear the pitch-corrected audio
- [x] Edited notes sound natural
- [x] Playhead tracks playback position

---

## Phase 6: Timing Editing & Formant Control

**Goal:** Complete the editing toolkit with timing adjustment, formant shifting, quantization, and undo/redo.

### Tasks

- [x] Add timing editing (horizontal drag, left/right edge resize)
- [x] Implement formant shifting (Rubber Band `setFormantScale()`, per-note control)
- [x] Implement pitch quantization (snap to nearest semitone)
- [x] Implement PropertiesPanel (note label, pitch/formant sliders)
- [x] Implement undo/redo (juce::UndoManager, Cmd+Z / Cmd+Shift+Z)

### Files Created

- `Source/Editing/EditActions.h`
- `Source/Editing/QuantizationEngine.h/cpp`
- `Source/UI/PropertiesPanel.h/cpp`

### Done When

- [x] Can drag notes to adjust timing
- [x] Formant slider changes vocal character without changing pitch
- [x] Quantize button works correctly
- [x] Undo/redo works for all edit operations

---

## Phase 7: Vocal Alignment

**Goal:** Align multiple vocal takes to a reference take for tighter vocal stacks.

### Tasks

- [x] Implement DTWAligner (banded DTW with Sakoe-Chiba constraint)
- [x] Implement MFCC feature extraction (standalone, JUCE FFT)
- [x] Create multi-take data model (Take struct, TakeList)
- [x] Build multi-take UI (ComboBox selector, "Align" button)
- [x] Apply alignment (time-warp curve → Rubber Band time-stretch)

### Files Created

- `Source/Alignment/DTWAligner.h/cpp`
- `Source/Alignment/MFCCExtractor.h/cpp`
- `Source/Data/TakeData.h`

### Done When

- [x] Can capture multiple vocal takes
- [x] Can designate one take as reference
- [x] "Align" produces audibly tighter vocal stacks

---

## Phase 8: Polish & Optimization

**Goal:** Production-ready plugin with keyboard shortcuts, auto-scroll, tooltips, and state persistence.

### Tasks

- [x] Keyboard shortcuts (Space, R, Q, F, Escape)
- [x] Auto-scroll during playback
- [x] Tooltips on all controls
- [x] Resizable plugin window (700x500 to 1800x1200)
- [x] Zoom-to-fit (F key)
- [x] Plugin state save/restore via ValueTree

### Done When

- [x] Plugin works reliably with keyboard shortcuts
- [x] State saves and restores correctly
- [x] UI is responsive and polished

---

## Melodyne Parity Batches

Additional features implemented to bring InKey closer to Melodyne's capabilities.

### Batch A: Multi-select, Fine Pitch, Pitch Curves

- [x] Multi-select with Shift/Cmd+click, lasso selection, Cmd+A
- [x] Batch editing — drag multiple selected notes together
- [x] Shift+drag for cent-level pitch precision (0.01 semitone)
- [x] Intra-note pitch curve visualization
- [x] Sub-semitone note positioning

### Batch B: Scale-Aware Quantization

- [x] 8 built-in scales (Major, Minor, Dorian, Mixolydian, Pentatonic, Blues, Harmonic Minor, Chromatic)
- [x] Scale root selector and strength slider
- [x] Out-of-scale piano roll rows shaded with red tint
- [x] Partial-strength correction (0-100%)

### Batch C: Split/Merge + Volume

- [x] Split note at time position (with undo support)
- [x] Merge adjacent selected notes (with undo support)
- [x] Per-note gain (-24 to +12 dB)
- [x] Volume slider in properties panel
- [x] Volume indicator bars on notes

### Batch D: Context Menu + Crossfading + Pitch Drift

- [x] Right-click context menu (Select All, Quantize, Reset, Split, Merge)
- [x] 5ms raised-cosine crossfade at note boundaries
- [x] Per-note pitch drift scale (0=flat, 1=original, 2=exaggerated)
- [x] Drift slider in properties panel
- [x] DSP: 50ms sub-segment processing with per-segment pitch correction

### Batch E: State Persistence

- [x] Full serialization of all takes (audio, pitch curves, notes, edit fields)
- [x] Audio stored as Base64-encoded float arrays
- [x] Version field for forward compatibility
- [x] Automatic state restore on DAW project load

### Batch F: Advanced Vocal Processing

- [x] Intelligent pitch correction with smoothstep weighting
- [x] Sibilant detection (FFT spectral analysis, HF energy ratio)
- [x] Vibrato analysis and editing (autocorrelation rate detection)
- [x] Note fade in/out tool
- [x] Auto note leveling (median RMS targeting)
- [x] Per-note sibilant balance control
- [x] Detection settings dialog

### Batch G: Workflow Polish

- [x] Time quantization to BPM grid with subdivisions
- [x] Smart tool (context-sensitive editing zones within notes)
- [x] Custom scale builder (12 pitch-class toggles)
- [x] Numeric pitch input (note names, relative shifts)
- [x] Double-click to snap pitch to nearest semitone
- [x] Master tuning reference (A4 = 415-465 Hz)
- [x] Click-to-play from any position (Cmd+click)

### Additional Improvements

- [x] DAW transport sync (capture position, playhead tracking)
- [x] Intuitive mouse/trackpad scroll and zoom controls
- [x] Double-buffered audio rendering (background thread)
- [x] 114 unit tests across 10 test suites

---

## Future Ideas (Post v1.0)

These are features to consider after the core plugin is complete:

- **Real-time mode** — Low-latency auto-correction for live use (Auto-Tune style)
- **ARA2 integration** — Tighter DAW integration (Melodyne-style direct editing)
- **Automatic harmony generation** — Create harmony parts from lead vocal
- **MIDI export** — Export detected notes as MIDI for use in other instruments
- **Batch processing** — Process multiple audio files at once
- **Machine learning pitch correction** — Train on professional vocals for style matching
- **Dark mode** — Alternative color scheme
- **Touch screen support** — For touch-enabled Windows devices
- **AAX format** — Pro Tools support (requires iLok licensing deal with Avid)
- **pYIN upgrade** — HMM-based probabilistic pitch detection for better accuracy
- **Scrub playback** — Drag to scrub audio at cursor position
