# InKey — Open Source Vocal Pitch Editor

A Melodyne-style VST3 plugin for graphical vocal pitch correction and multi-take alignment, built with JUCE and Rubber Band.

**Status: Feature-complete.** All 8 development phases plus Melodyne parity features (Batches A-G) are implemented.

## Screenshot

> Screenshot coming soon.

## Features

- **YIN pitch detection** with configurable sensitivity, confidence thresholds, and note segmentation
- **Interactive piano roll** with drag-to-edit for pitch, timing, and duration
- **Formant preservation** via Rubber Band — shift pitch without chipmunk artifacts
- **Scale-aware quantization** with 8 built-in scales, custom scale builder, and adjustable strength
- **Multi-take alignment** using MFCC feature extraction and Dynamic Time Warping (DTW)
- **Sibilant detection** and per-note sibilant balance control
- **Vibrato editing** — analyze, flatten, or exaggerate vibrato per note
- **Pitch drift control** — scale intra-note pitch variation from flat to exaggerated
- **Smart tool** — context-sensitive editing based on where you click within a note
- **Split and merge** notes with full undo support
- **Per-note volume**, fade in/out, and auto-leveling
- **Time quantization** to BPM grid with selectable subdivisions
- **Multi-select** with lasso, Shift/Cmd+click, and batch editing
- **Fine pitch editing** — hold Shift for cent-level precision
- **Numeric pitch input** — type note names (C4, Bb3) or relative shifts (+2st)
- **Master tuning** reference (A4 = 415-465 Hz)
- **Full undo/redo** for all edit operations
- **Keyboard shortcuts** — Space (play/stop), R (capture), Q (quantize), T (time quantize), F (zoom-to-fit)
- **DAW transport sync** — playhead follows DAW position, captures align to DAW timeline
- **State persistence** — all takes, edits, and audio survive DAW project save/reload
- **Resizable window** (700x500 to 1800x1200)

## Getting Started

### Prerequisites

- **CMake 3.22+**
- **C++17 compiler**
- **Git**
- macOS: Xcode Command Line Tools (`xcode-select --install`) or full Xcode
- Windows: Visual Studio 2019+ (Community edition is free)

### Build

```bash
# Clone the repository
git clone https://github.com/straplocked/in-key.git
cd in-key

# Clone dependencies into ThirdParty/
git clone --depth 1 --branch 7.0.12 \
  https://github.com/juce-framework/JUCE.git ThirdParty/JUCE
git clone --depth 1 \
  https://github.com/breakfastquay/rubberband.git ThirdParty/rubberband

# Build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The VST3 plugin is automatically installed to your system's plugin directory after building.

### Run Tests

```bash
cmake --build build --target InKeyTests
./build/Tests/InKeyTests
```

114 tests across 10 test suites.

## Tech Stack

| Component | Technology | License |
|-----------|-----------|---------|
| Framework | [JUCE 7.0.12](https://github.com/juce-framework/JUCE) | GPL-3.0 |
| Pitch/Time Shifting | [Rubber Band Library](https://breakfastquay.com/rubberband/) | GPL-2.0 |
| Pitch Detection | Standalone YIN implementation | — |
| MFCC Extraction | Standalone (JUCE FFT) | — |
| DTW Alignment | Standalone (Sakoe-Chiba band) | — |
| Testing | [Catch2](https://github.com/catchorg/Catch2) | BSL-1.0 |

## Project Structure

```
in-key/
├── CMakeLists.txt
├── README.md
├── LICENSE                        # GPL-3.0
├── CONTRIBUTING.md
├── CLAUDE.md                      # AI assistant instructions
│
├── Docs/
│   ├── ARCHITECTURE.md            # System design and data flow
│   ├── ROADMAP.md                 # Development phases and progress
│   └── TECH_STACK.md              # Libraries and algorithms
│
├── Source/
│   ├── PluginProcessor.h/cpp      # Audio processor (state machine, DAW sync)
│   ├── PluginEditor.h/cpp         # Editor window (toolbar, layout)
│   ├── PluginState.h              # State enum
│   ├── Audio/                     # Audio capture (lock-free FIFO)
│   ├── Analysis/                  # YIN pitch detection, note segmentation,
│   │                              #   sibilant detection, vibrato analysis
│   ├── DSP/                       # Audio engine (Rubber Band rendering)
│   ├── Data/                      # Note, Take, PitchData, MIDI export
│   ├── Alignment/                 # MFCC extraction, DTW alignment
│   ├── Editing/                   # Undo/redo actions, quantization engine
│   └── UI/                        # Piano roll, properties panel,
│                                  #   waveform, pitch curve, look-and-feel
│
├── Tests/                         # Catch2 unit tests (114 tests)
│
└── ThirdParty/                    # External libraries (not committed)
    ├── JUCE/
    └── rubberband/
```

## Architecture

InKey operates as an **offline processor**: capture audio from the DAW, analyze it, then edit graphically.

**State machine:** `IDLE → CAPTURING → ANALYZING → EDITING ↔ PLAYING`

**Threading model:**
- **Audio thread** — Lock-free only (`processBlock`). Uses `juce::AbstractFifo` for capture.
- **Message thread** — GUI updates and user interaction.
- **Background thread** — Pitch analysis, audio rendering (double-buffered).

See [Docs/ARCHITECTURE.md](Docs/ARCHITECTURE.md) for the full system design.

## Contributing

Contributions are welcome! See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

## License

GPL-3.0 — See [LICENSE](LICENSE) for the full text.

**Dependencies:** JUCE (GPL-3.0), Rubber Band (GPL-2.0), Inter font (SIL OFL 1.1)

## Acknowledgments

- [JUCE](https://juce.com) — Cross-platform audio application framework
- [Rubber Band Library](https://breakfastquay.com/rubberband/) — High-quality pitch and time shifting
- de Cheveigné & Kawahara (2002) — *"YIN, a fundamental frequency estimator for speech and music"*
- [Catch2](https://github.com/catchorg/Catch2) — C++ testing framework
