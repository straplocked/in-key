# InKey — Roadmap

This document tracks the implementation plan, current progress, and what to work on next.

**Last Updated:** 2026-02-09

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
```

**All 8 phases complete!** The plugin is feature-complete.

---

## Phase 0: Prerequisites

**Goal:** Learn C++ basics and JUCE fundamentals before writing plugin code.

**Estimated Time:** 2-3 weeks

### Tasks

- [ ] Install development tools
  - [ ] Xcode 14+ (macOS) or Visual Studio 2019+ (Windows)
  - [ ] CMake 3.22+ (`brew install cmake`)
  - [ ] Git (should already be installed)
- [ ] Clone JUCE framework: `git clone https://github.com/juce-framework/JUCE.git ThirdParty/JUCE`
- [ ] Complete JUCE official free course (https://juce.com)
- [ ] Complete tutorial: "Create a basic Audio/MIDI plugin"
- [ ] Read about JUCE AudioProcessor lifecycle
- [ ] Read about JUCE Component system (for UI)
- [ ] (Optional) Read chapters 1-4 of "Designing Audio Effect Plugins in C++" by Will Pirkle

### Key Concepts to Understand

- C++ classes, inheritance, smart pointers (`std::unique_ptr`, `std::shared_ptr`)
- STL containers (`std::vector`, `std::map`)
- Lambda functions and callbacks
- RAII (Resource Acquisition Is Initialization)
- Sample rate, buffer size, and latency in audio processing
- Difference between audio thread and message (UI) thread

### Done When

- [ ] Can build and run a JUCE example project
- [ ] Understand how AudioProcessor::processBlock() works
- [ ] Understand how Component::paint() and Component::resized() work

---

## Phase 1: Foundation & Basic Plugin

**Goal:** Get a working plugin that loads in a DAW and passes audio through.

**Estimated Time:** 1-2 weeks

### Tasks

- [ ] Create project using JUCE Projucer or CMake
  - Name: "VocalEditor" (display name: "InKey")
  - Formats: VST3, AU, Standalone
- [ ] Set up CMakeLists.txt with JUCE
- [ ] Implement PluginProcessor
  - Pass audio through unmodified in processBlock()
  - Implement getStateInformation() / setStateInformation() (empty for now)
- [ ] Implement PluginEditor
  - Simple label: "InKey - Vocal Pitch Editor"
  - Set initial window size to 800x600
- [ ] Test in standalone mode (runs without a DAW)
- [ ] Test in REAPER (free DAW, best for plugin testing)
- [ ] Verify audio passes through cleanly (no clicks, no silence)

### Files Created

- `CMakeLists.txt`
- `Source/PluginProcessor.h`
- `Source/PluginProcessor.cpp`
- `Source/PluginEditor.h`
- `Source/PluginEditor.cpp`
- `Source/PluginParameters.h`

### Done When

- [ ] Plugin appears in DAW plugin list
- [ ] Audio plays through the plugin unmodified
- [ ] Plugin UI window opens and displays correctly

---

## Phase 2: Audio Capture & Storage

**Goal:** Plugin captures audio from the DAW and displays a waveform.

**Estimated Time:** 1-2 weeks

### Tasks

- [ ] Implement AudioCapture class
  - Lock-free FIFO for audio thread safety
  - Growing buffer to store captured audio
  - Start/stop capture control
- [ ] Add plugin state machine: IDLE → CAPTURING → ANALYZING → EDITING
  - Use std::atomic for thread-safe state transitions
- [ ] Add "Capture" button to UI
  - Toggles between "Capture" and "Stop"
  - Visual indicator when capturing (red dot / recording indicator)
- [ ] Implement WaveformComponent
  - Draw captured audio as waveform
  - Scale to fit component bounds
  - Handle mono and stereo audio
- [ ] Display capture duration and sample rate in UI

### Files Created

- `Source/Audio/AudioCapture.h/cpp`
- `Source/UI/WaveformComponent.h/cpp`
- `Source/PluginState.h`

### Done When

- [ ] Can click Capture, play audio in DAW, click Stop
- [ ] Waveform of captured audio is displayed
- [ ] Audio is stored correctly (verified by listening to playback)

---

## Phase 3: Pitch Detection Integration

**Goal:** Analyze captured audio and display a pitch curve.

**Estimated Time:** 2-3 weeks

### Tasks

- [ ] Integrate Essentia library via CMake FetchContent
  - Alternative: standalone pYIN implementation if Essentia is too complex to build
- [ ] Create PitchAnalyzer class
  - Wrap Essentia's PitchYin or PitchYinProbabilistic algorithm
  - Frame size: 2048 samples, Hop size: 256 samples
  - Frequency range: 80 Hz to 800 Hz (vocal range)
  - Output: vector<PitchFrame> with time, frequency, confidence
- [ ] Run analysis on background thread
  - Use juce::Thread or std::thread
  - Report progress to UI (0-100%)
  - Show progress bar during analysis
- [ ] Create PitchCurveComponent
  - Draw pitch curve overlaid on waveform
  - Y-axis: frequency (logarithmic scale, mapped to MIDI notes)
  - X-axis: time
  - Color-code by confidence (bright = confident, dim = uncertain)
- [ ] Handle unvoiced regions (silence, consonants)
  - Skip frames with confidence < threshold (~0.3)
  - Don't draw pitch curve during unvoiced sections

### Files Created

- `Source/Analysis/PitchAnalyzer.h/cpp`
- `Source/Data/PitchData.h`
- `Source/UI/PitchCurveComponent.h/cpp`

### Key Decisions

- **Essentia vs standalone pYIN:** Try Essentia first (more features). If the CMake integration proves too difficult, fall back to a standalone pYIN implementation (smaller, simpler to build).
- **Frame/hop size:** 2048/256 gives good frequency resolution (~21 Hz at 44.1kHz) with ~5.8ms time resolution. These can be tuned later.

### Done When

- [ ] After capture + analyze, a pitch curve is displayed
- [ ] Pitch curve is reasonably accurate (test with a known melody)
- [ ] Unvoiced regions don't show spurious pitch detections

---

## Phase 4: Note Segmentation & Piano Roll

**Goal:** Segment pitch data into notes and display on an interactive piano roll.

**Estimated Time:** 2-3 weeks

### Tasks

- [ ] Implement OnsetDetector
  - Use Essentia's onset detection (spectral flux)
  - Detect note boundaries
- [ ] Implement NoteSegmenter
  - Combine pitch data + onset data into discrete Notes
  - Calculate per-note properties: start time, duration, average pitch, MIDI note, confidence
  - Handle edge cases: very short notes (<100ms), overlapping onsets
- [ ] Create Note data structure (see Docs/ARCHITECTURE.md)
- [ ] Create PianoRollComponent
  - Draw piano roll grid (alternating white/grey rows for notes)
  - Draw piano keys on left sidebar
  - Render each note as a colored rectangle
  - Note color = pitch confidence or note name
- [ ] Implement zoom and pan
  - Mouse wheel: zoom horizontally (time)
  - Ctrl/Cmd + mouse wheel: zoom vertically (pitch)
  - Middle-click drag or two-finger scroll: pan
- [ ] Add click-to-select interaction
  - Click a note to select it
  - Highlight selected note
  - Show note info (pitch, duration) in status bar or properties panel

### Files Created

- `Source/Analysis/NoteSegmenter.h/cpp`
- `Source/Analysis/OnsetDetector.h/cpp`
- `Source/Data/Note.h`
- `Source/Data/AnalyzedAudio.h/cpp`
- `Source/UI/PianoRollComponent.h/cpp`

### Reference

Study the Piano Roll Editor project for UI patterns: https://github.com/Sjhunt93/Piano-Roll-Editor

### Done When

- [ ] Notes appear as rectangles on the piano roll
- [ ] Notes are positioned correctly (pitch on Y, time on X)
- [ ] Can zoom and pan the view
- [ ] Can click to select a note

---

## Phase 5: Rubber Band Integration & Pitch Editing

**Goal:** Edit note pitch by dragging and hear the results.

**Estimated Time:** 2-3 weeks

### Tasks

- [ ] Integrate Rubber Band Library via CMake FetchContent
- [ ] Create RubberBandWrapper class
  - Configure for offline processing mode
  - Methods: setPitchScale(), setTimeRatio(), setFormantPreserved(), process()
- [ ] Implement AudioEngine class
  - render(AnalyzedAudio, edits) → output AudioBuffer
  - For each note: extract segment, apply pitch shift, mix to output
  - Cache rendered notes (only re-render when edited)
- [ ] Add pitch editing interaction to PianoRollComponent
  - Drag note vertically to change pitch
  - Show pitch change in semitones during drag
  - Optional: snap to semitone grid
  - Visual feedback: note moves with cursor, ghost of original position shown
- [ ] Implement playback
  - "Play" button starts streaming rendered audio through processBlock()
  - Playhead moves across piano roll during playback
  - "Stop" button halts playback
  - Sync with DAW transport (respond to host play/stop)

### Files Created

- `Source/DSP/RubberBandWrapper.h/cpp`
- `Source/DSP/AudioEngine.h/cpp`
- `Source/UI/NoteEditingComponent.h/cpp`

### Done When

- [ ] Can drag a note to a different pitch
- [ ] Can press Play and hear the pitch-corrected audio
- [ ] Edited notes sound natural (no obvious artifacts)
- [ ] Playhead tracks playback position on piano roll

---

## Phase 6: Timing Editing & Formant Control

**Goal:** Complete the editing toolkit with timing adjustment, formant shifting, quantization, and undo/redo.

**Estimated Time:** 2 weeks

### Tasks

- [ ] Add timing editing
  - Drag note horizontally to adjust start time
  - Drag left/right edge to adjust duration (time-stretches via Rubber Band)
  - Visual feedback during drag
- [ ] Implement formant shifting
  - Use Rubber Band's formant preservation mode
  - Add per-note formant shift parameter (-12 to +12 semitones)
  - Add FormantShiftSlider in PropertiesPanel
- [ ] Implement quantization
  - "Quantize Pitch" button: snap all notes to nearest semitone
  - "Quantize Timing" button: snap note start times to grid
  - Adjustable strength: 0% (no change) to 100% (full snap)
  - Key/Scale selector for pitch quantization target
- [ ] Implement PropertiesPanel
  - Shows selected note's properties
  - Sliders for pitch shift, formant shift, time offset, duration
  - Real-time updates when dragging sliders
- [ ] Implement undo/redo
  - Use juce::UndoManager
  - Track all note edits as undoable actions
  - Cmd+Z / Cmd+Shift+Z keyboard shortcuts

### Files Created

- `Source/DSP/FormantShifter.h/cpp`
- `Source/Editing/QuantizationEngine.h/cpp`
- `Source/Editing/EditHistory.h/cpp`
- `Source/UI/PropertiesPanel.h/cpp`

### Done When

- [ ] Can drag notes to adjust timing
- [ ] Formant slider changes vocal character without changing pitch
- [ ] Quantize buttons work correctly
- [ ] Undo/redo works for all edit operations

---

## Phase 7: Vocal Alignment

**Goal:** Align multiple vocal takes to a reference take for tighter vocal stacks.

**Estimated Time:** 2-3 weeks

### Tasks

- [ ] Implement DTWAligner class
  - DTW (Dynamic Time Warping) algorithm
  - Distance metric: Euclidean distance on MFCC features
  - Output: alignment path + time-warp curve
- [ ] Implement MFCC feature extraction
  - 13 MFCC coefficients per frame
  - Use Essentia's MFCC algorithm
- [ ] Create MultiTakeSession data model
  - Store multiple AnalyzedAudio takes
  - Reference take designation
  - Alignment results per take
- [ ] Build multi-take UI
  - MultiTrackComponent: stacked lanes showing each take
  - "Align to Reference" button
  - Visual alignment quality indicator (color-coded)
  - Before/after comparison playback
- [ ] Apply alignment
  - Generate time-warp curve from DTW path
  - Apply time-stretching to target takes (Rubber Band)
  - Preserve pitch during alignment
- [ ] Allow manual alignment adjustment
  - Drag warp points to fine-tune alignment

### Files Created

- `Source/Alignment/DTWAligner.h/cpp`
- `Source/Alignment/FeatureExtractor.h/cpp`
- `Source/Alignment/AlignmentEngine.h/cpp`
- `Source/Data/MultiTakeSession.h/cpp`
- `Source/UI/MultiTrackComponent.h/cpp`

### Done When

- [ ] Can load/capture multiple vocal takes
- [ ] Can designate one take as reference
- [ ] "Align" produces audibly tighter vocal stacks
- [ ] Alignment quality visualization shows results

---

## Phase 8: Polish & Optimization

**Goal:** Production-ready plugin tested across multiple DAWs.

**Estimated Time:** 2-3 weeks

### Tasks

- [ ] Performance optimization
  - [ ] Profile with JUCE PerformanceCounter
  - [ ] Optimize rendering (consider OpenGL for large files)
  - [ ] Optimize audio caching (smart invalidation)
  - [ ] Multi-threaded analysis if needed
- [ ] UI/UX polish
  - [ ] Keyboard shortcuts for all common actions
  - [ ] Zoom to selection
  - [ ] Auto-scroll during playback
  - [ ] Tooltips on all controls
  - [ ] Resizable plugin window
- [ ] Error handling
  - [ ] Gracefully handle corrupt/empty audio
  - [ ] Input validation on all parameters
  - [ ] User-friendly error messages
- [ ] Cross-platform testing
  - [ ] macOS: REAPER, Logic Pro, Ableton Live
  - [ ] Windows: REAPER, FL Studio, Cubase
  - [ ] Test at 44.1kHz, 48kHz, 96kHz sample rates
  - [ ] Test at 64, 128, 256, 512 buffer sizes
- [ ] Plugin state save/restore
  - [ ] Verify DAW project saves and restores all data correctly
  - [ ] Test with different DAWs' project formats
- [ ] Documentation
  - [ ] User manual
  - [ ] Contributing guide
  - [ ] API docs for key classes

### Done When

- [ ] Plugin works reliably in 3+ DAWs on macOS and Windows
- [ ] No crashes or audio glitches under normal use
- [ ] State saves and restores correctly
- [ ] Documentation is complete

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

---

## Timeline Summary

| Phase | Duration (beginner, part-time) | Cumulative |
|-------|-------------------------------|------------|
| Phase 0: Prerequisites | 2-3 weeks | 3 weeks |
| Phase 1: Foundation | 1-2 weeks | 5 weeks |
| Phase 2: Audio Capture | 1-2 weeks | 7 weeks |
| Phase 3: Pitch Detection | 2-3 weeks | 10 weeks |
| Phase 4: Piano Roll | 2-3 weeks | 13 weeks |
| Phase 5: Pitch Editing | 2-3 weeks | 16 weeks |
| Phase 6: Timing & Formants | 2 weeks | 18 weeks |
| Phase 7: Vocal Alignment | 2-3 weeks | 21 weeks |
| Phase 8: Polish | 2-3 weeks | 24 weeks |

**Total: ~6 months** at 10-15 hours/week
