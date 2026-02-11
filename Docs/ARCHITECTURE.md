# InKey — Architecture

This document describes the system design, data flow, and technical decisions for the InKey vocal pitch editor plugin.

## High-Level Architecture

The plugin has three distinct modes of operation, controlled by a state machine:

```
IDLE → CAPTURING → ANALYZING → EDITING
                                  ↕
                               PLAYING
```

### System Diagram

```
┌──────────────────────────────────────────────────────────────┐
│                         DAW HOST                             │
│  (Logic Pro, REAPER, Ableton, FL Studio, Cubase, etc.)       │
└──────────────────┬───────────────────────────────────────────┘
                   │ VST3 / AU API
                   ▼
┌──────────────────────────────────────────────────────────────┐
│              PluginProcessor (JUCE AudioProcessor)           │
│                                                              │
│  Responsibilities:                                           │
│  • Receive audio buffers from the DAW                        │
│  • Route audio to capture system during CAPTURING mode       │
│  • Stream processed audio to DAW during PLAYING mode         │
│  • Pass audio through unmodified during IDLE mode            │
│  • Report latency to host for compensation                   │
│  • Manage plugin state and parameters                        │
│  • Serialize/deserialize state for DAW project saves         │
└─────────┬────────────────────────────────┬───────────────────┘
          │                                │
          ▼                                ▼
┌───────────────────┐           ┌──────────────────────────────┐
│   AUDIO ENGINE    │           │        UI LAYER              │
│                   │◄─────────►│   (PluginEditor)             │
│ • AudioCapture    │  shared   │                              │
│ • PitchAnalyzer   │  data     │ • PianoRollComponent         │
│ • AudioEngine     │  model    │ • WaveformComponent          │
│ • RubberBand      │           │ • PitchCurveComponent        │
│ • DTWAligner      │           │ • PropertiesPanel            │
└───────────────────┘           │ • TransportBar               │
                                │ • MultiTrackComponent        │
                                └──────────────────────────────┘
```

## Processing Pipelines

### 1. Audio Capture Pipeline

When the user clicks "Capture", incoming audio from the DAW is stored in a buffer.

```
DAW processBlock()
    → PluginProcessor receives audio buffer (typically 64-1024 samples)
    → AudioCapture appends to growing AudioBuffer<float>
    → WaveformComponent updates display periodically
    → User clicks "Stop" or max duration reached
    → State transitions to ANALYZING
```

**Thread safety:** AudioCapture uses a lock-free FIFO (`juce::AbstractFifo`) to pass audio from the real-time audio thread to the storage thread. The audio thread never allocates memory.

### 2. Analysis Pipeline (Offline)

After capture, the audio is analyzed completely before editing begins. This runs on a background thread to keep the UI responsive.

```
Captured AudioBuffer
    │
    ├──► PitchAnalyzer (standalone YIN)
    │    • Frame size: 2048 samples
    │    • Hop size: 256 samples
    │    • Output: vector<PitchFrame> (time, frequency, confidence)
    │    • Vocal range: 80 Hz – 800 Hz
    │
    ├──► NoteSegmenter
    │    • Segments continuous pitch curve into discrete Notes
    │    • Uses pitch continuity, confidence thresholds, gap detection
    │    • Calculates: start time, duration, average pitch, MIDI note
    │    • Handles unvoiced regions (silence, consonants)
    │
    └──► MFCCExtractor (standalone, JUCE FFT)
         • Extracts MFCC features (for vocal alignment / DTW)
         • 13 coefficients, 26 Mel filterbank channels
         • Stores in AnalyzedAudio data model

    → State transitions to EDITING
    → UI displays piano roll with detected notes
```

**Progress reporting:** The analysis thread sends progress updates to the UI via `juce::MessageManager::callAsync()`.

### 3. Playback Pipeline

When the user clicks "Play", the edited audio is rendered and streamed through the plugin output.

```
AnalyzedAudio + EditState (user modifications)
    │
    ├──► For each Note:
    │    ├── Extract original audio segment from captured buffer
    │    ├── Calculate pitch shift (editedPitch - originalPitch)
    │    ├── Calculate time stretch (editedDuration / originalDuration)
    │    ├── RubberBandWrapper.setPitchScale(shift)
    │    ├── RubberBandWrapper.setTimeRatio(ratio)
    │    ├── RubberBandWrapper.setFormantPreserved(true)
    │    ├── RubberBandWrapper.process(segment) → processed audio
    │    └── Mix processed audio into output buffer at edited position
    │
    └──► Output buffer → PluginProcessor.processBlock() → DAW
```

**Caching strategy:** Processed audio is cached per-note. When a note's parameters change, only that note is re-rendered. Unmodified notes use the original audio directly (no processing overhead).

### 4. Alignment Pipeline

For multi-take vocal alignment, a reference take is compared against target takes.

```
Reference Take (AnalyzedAudio)
    │
    ├──► MFCCExtractor: Extract MFCC features (13 coefficients per frame)
    │
    ▼
DTWAligner
    │
    ├──► For each Target Take:
    │    ├── Extract MFCC features (standalone, JUCE FFT)
    │    ├── Compute DTW cost matrix (reference vs target)
    │    ├── Backtrack for optimal alignment path
    │    ├── Generate time-warp curve
    │    │   (maps each target timestamp to reference timestamp)
    │    ├── Apply time-stretching via Rubber Band
    │    │   (preserves pitch, adjusts timing to match reference)
    │    └── Store aligned audio
    │
    └──► UI shows alignment quality visualization
         (color-coded: green = well aligned, red = large adjustment)
```

## Core Data Structures

### PitchFrame
The smallest unit of pitch data. One frame per analysis hop (every ~5.8ms at 44.1kHz with 256-sample hop).

```cpp
struct PitchFrame {
    double time;        // Timestamp in seconds
    float frequency;    // Detected pitch in Hz (0.0 = unvoiced)
    float confidence;   // Detection confidence (0.0 to 1.0)
};
```

### Note
A discrete musical note segmented from the continuous pitch curve.

```cpp
struct Note {
    // Identity
    int id;                           // Unique identifier

    // Original analysis results (immutable after analysis)
    double startTime;                 // Start time in seconds
    double duration;                  // Duration in seconds
    float originalPitch;              // Average pitch in Hz
    int midiNote;                     // Quantized MIDI note (0-127)
    float confidence;                 // Average detection confidence
    std::vector<float> pitchCurve;    // Detailed pitch over note duration
    std::vector<float> formants;      // Formant frequencies (F1, F2, F3...)

    // User edits (mutable)
    float pitchShiftSemitones = 0;    // Pitch adjustment (-24 to +24)
    double timeOffset = 0;            // Timing adjustment in seconds
    double durationScale = 1.0;       // Duration multiplier
    float formantShift = 0;           // Formant adjustment (-12 to +12 semitones)

    // Computed
    float editedPitch() const;        // originalPitch * 2^(shift/12)
    double editedStartTime() const;   // startTime + timeOffset
    double editedDuration() const;    // duration * durationScale
};
```

### AnalyzedAudio
Container holding all analysis results and the original audio for one take.

```cpp
class AnalyzedAudio {
public:
    // Original audio (immutable after capture)
    juce::AudioBuffer<float> originalAudio;
    double sampleRate;
    int numChannels;

    // Analysis results
    std::vector<PitchFrame> pitchCurve;   // Full pitch contour
    std::vector<Note> notes;               // Segmented notes

    // Metadata
    juce::String name;                     // Take name
    double duration;                       // Total duration in seconds

    // Query methods
    Note* getNoteAtTime(double time);
    std::vector<Note*> getNotesInRange(double start, double end);
    const PitchFrame& getPitchFrameAtTime(double time);

    // Serialization
    juce::ValueTree toValueTree() const;   // For saving plugin state
    void fromValueTree(const juce::ValueTree& tree);
};
```

### MultiTakeSession
Manages multiple vocal takes and their alignment relationships.

```cpp
class MultiTakeSession {
public:
    std::vector<AnalyzedAudio> takes;
    int referenceTakeIndex = 0;            // Which take is the reference

    // Alignment data
    struct AlignmentResult {
        std::vector<std::pair<int, int>> dtwPath;  // Alignment path
        std::vector<double> timeWarpCurve;          // Time mapping
        double alignmentCost;                       // Quality metric
    };
    std::vector<AlignmentResult> alignments;        // One per non-reference take

    // Methods
    void setReferenceTake(int index);
    void alignAllTakes();                           // Run DTW alignment
    void alignSingleTake(int index);
    AnalyzedAudio& getReferenceTake();
};
```

## Threading Model

Audio plugins have strict threading rules. Violating them causes audio glitches, crashes, or undefined behavior.

### Thread Types

| Thread | Purpose | Rules |
|--------|---------|-------|
| **Audio Thread** | Runs `processBlock()` at buffer rate | NO allocation, NO blocking, NO locks, NO I/O |
| **Message Thread** | GUI updates, user interaction | Can allocate, can block briefly |
| **Background Thread** | Analysis, rendering | Can allocate, can do heavy computation |

### Thread Communication

```
Audio Thread ──(lock-free FIFO)──► Background Thread (capture storage)
Audio Thread ──(atomic flags)────► Message Thread (state changes)
Background Thread ──(callAsync)──► Message Thread (progress updates)
Message Thread ──(atomic params)─► Audio Thread (parameter changes)
```

**Key Rules:**
1. Audio thread NEVER allocates memory (`new`, `malloc`, `push_back` that resizes)
2. Audio thread NEVER acquires a mutex or lock
3. Audio thread NEVER does file I/O or logging
4. Use `juce::AbstractFifo` for lock-free audio → background communication
5. Use `std::atomic<>` for simple flags and state
6. Use `juce::MessageManager::callAsync()` for background → UI updates
7. Pre-allocate all buffers in the constructor or `prepareToPlay()`

## UI Component Architecture

### Component Hierarchy

```
PluginEditor (root, ~800x600 default)
│
├── Toolbar (top bar, ~40px height)
│   ├── CaptureButton      — Start/stop audio capture
│   ├── AnalyzeButton       — Run pitch analysis
│   ├── PlayButton          — Play edited audio
│   ├── StopButton          — Stop playback
│   ├── QuantizePitchButton — Snap notes to semitones
│   ├── QuantizeTimeButton  — Snap notes to grid
│   ├── UndoButton          — Undo last edit
│   └── RedoButton          — Redo last undone edit
│
├── MainEditorComponent (center, fills remaining space)
│   │
│   ├── TimeRuler (top, ~24px height)
│   │   └── Displays seconds/beats with playhead
│   │
│   ├── PianoKeys (left sidebar, ~60px width)
│   │   └── Piano key labels (C1..C7) for pitch reference
│   │
│   └── EditorCanvas (scrollable viewport)
│       │
│       ├── WaveformComponent (background layer)
│       │   └── Renders audio waveform
│       │
│       ├── GridComponent (background layer)
│       │   └── Piano roll grid lines (semitone rows, time columns)
│       │
│       ├── PitchCurveComponent (overlay)
│       │   └── Continuous pitch curve, color-coded by confidence
│       │
│       └── NoteComponents (interactive, one per detected note)
│           └── Draggable rectangles for pitch/timing editing
│
└── PropertiesPanel (right sidebar, ~200px width, collapsible)
    ├── NoteProperties (when a note is selected)
    │   ├── Pitch display (Hz, note name, cents offset)
    │   ├── PitchShiftSlider (-24 to +24 semitones)
    │   ├── FormantShiftSlider (-12 to +12 semitones)
    │   ├── TimeOffsetSlider
    │   └── DurationSlider
    │
    └── GlobalSettings
        ├── Key/Scale selector (for pitch quantization)
        ├── Snap settings (grid resolution)
        └── Display options (show/hide pitch curve, waveform)
```

### Interaction Model

| Action | Result |
|--------|--------|
| Click note | Select note, show properties in sidebar |
| Drag note vertically | Change pitch (snaps to semitones if snap enabled) |
| Drag note horizontally | Adjust timing |
| Drag note left/right edge | Adjust duration (time-stretches audio) |
| Scroll wheel | Zoom in/out (horizontal) |
| Ctrl/Cmd + scroll wheel | Zoom in/out (vertical) |
| Middle-click drag | Pan view |
| Cmd+Z / Ctrl+Z | Undo |
| Cmd+Shift+Z / Ctrl+Y | Redo |
| Spacebar | Play/Stop |
| Delete/Backspace | Reset selected note to original pitch |

### Rendering Strategy

**Phase 1-6 (Development):** Use JUCE software renderer (`juce::Graphics`). Simpler, easier to debug.

**Phase 8 (Optimization):** Switch to `juce::OpenGLContext` for the editor canvas if performance is insufficient with large audio files. OpenGL provides:
- Hardware-accelerated waveform rendering
- Smooth 60fps scrolling/zooming
- Efficient rendering of thousands of pitch curve points

**Level of Detail (LOD):**
- Zoomed out: Show note rectangles only, hide pitch curve detail
- Zoomed in: Show full pitch curve, note edges, vibrato detail
- Only render notes/curves visible in current viewport (virtualization)

## Key Technical Decisions

### Why Offline Instead of Real-Time?

| Factor | Offline | Real-Time |
|--------|---------|-----------|
| Algorithm quality | Can use highest quality (pYIN, slow Rubber Band) | Must use faster/lower quality algorithms |
| Latency | Not a concern | Must be <30ms, limits algorithm choices |
| Buffer management | Simple — full audio available | Complex — must handle streaming |
| Learning curve | Lower | Higher |
| User experience | Melodyne-like — full visual editor | Auto-Tune-like — immediate correction |

We chose offline because it produces better results with simpler code, and the graphical editor requires complete audio analysis anyway.

### Why Rubber Band Over Custom PSOLA?

- Professional-grade pitch/time shifting out of the box
- Formant preservation built-in
- Optimized offline mode
- Well-tested with vocals
- GPL license compatible with our project
- Alternative: `signalsmith-stretch` (MIT license) if licensing becomes an issue

### Plugin State Serialization

JUCE plugins need to save/restore state when DAW projects are saved. We use `juce::ValueTree` for serialization:

```
Plugin State (ValueTree)
├── version: "1.0"
├── currentMode: "EDITING"
├── audioData: (binary blob — captured audio)
├── analysisData:
│   ├── pitchCurve: [PitchFrame array]
│   └── notes: [Note array with user edits]
└── alignmentData:
    ├── referenceTakeIndex: 0
    └── takes: [MultiTakeSession data]
```

This allows users to close and reopen their DAW project and pick up where they left off, with all analysis and edits preserved.

## Performance Targets

| Metric | Target | Notes |
|--------|--------|-------|
| Analysis speed | <10s for 5 min vocal | Background thread, ~44k frames to analyze |
| UI frame rate | 30+ fps | During editing and scrolling |
| Playback latency | <50ms from edit to audio | Cache per-note, re-render only changed notes |
| Memory usage | <500MB for 3 min vocal | Original audio + analysis data + cache |
| Plugin load time | <2s | Deserialize state, rebuild UI |
