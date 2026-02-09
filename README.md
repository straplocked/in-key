# InKey - Open Source Vocal Pitch Editor

A Melodyne-style VST3/AU plugin for graphical pitch correction and multi-take vocal alignment.

## Status: Pre-Development

This project is in the planning phase. No code has been written yet. See [Docs/ROADMAP.md](Docs/ROADMAP.md) for the implementation plan and current progress.

## What Is This?

InKey is an audio plugin that lets you:

- **Visualize vocal pitch** on a piano roll with detailed pitch curves
- **Edit pitch graphically** by dragging notes to correct intonation
- **Adjust timing** of individual notes to tighten performances
- **Shift formants** to maintain natural vocal character when changing pitch
- **Align multiple vocal takes** to a reference take for tighter stacks/harmonies

The plugin operates in **offline mode** — it captures audio from your DAW, analyzes it completely, then provides a graphical editor for making corrections. This is similar to how Melodyne works.

## Tech Stack

| Component | Technology | Purpose |
|-----------|-----------|---------|
| Framework | [JUCE 7.x](https://github.com/juce-framework/JUCE) | Plugin wrapper, GUI, DSP utilities |
| Pitch Detection | [Essentia](https://essentia.upf.edu/) (pYIN) | Analyze vocal pitch from audio |
| Pitch/Time Shifting | [Rubber Band Library](https://breakfastquay.com/rubberband/) | High-quality pitch and time manipulation |
| Build System | [CMake](https://cmake.org/) | Cross-platform builds |

## Plugin Formats

- **VST3** — Windows, macOS (most DAWs)
- **AU (Audio Units)** — macOS (Logic Pro, GarageBand)
- **Standalone** — For development and testing

## Supported Platforms

- macOS 11+ (Intel and Apple Silicon)
- Windows 10+

## Prerequisites

Before you can build this project, you need:

### macOS
- **Xcode 14+** (free from App Store)
- **CMake 3.22+** (`brew install cmake`)
- **Git** (`xcode-select --install` if not already installed)

### Windows
- **Visual Studio 2019+** (Community edition is free)
- **CMake 3.22+** (download from cmake.org)
- **Git** (download from git-scm.com)

## Getting Started

```bash
# Clone the repository
git clone <repo-url>
cd in-key

# Clone JUCE into ThirdParty
git clone https://github.com/juce-framework/JUCE.git ThirdParty/JUCE

# Clone Rubber Band
git clone https://github.com/breakfastquay/rubberband.git ThirdParty/rubberband

# Clone Essentia
git clone https://github.com/MTG/essentia.git ThirdParty/essentia

# Build (macOS)
cmake -B build -G Xcode
cmake --build build --config Release

# Build (Windows)
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

> **Note:** Build instructions will be refined once Phase 1 is complete and the CMake configuration is finalized.

## Project Structure

```
in-key/
├── CMakeLists.txt              # Build configuration
├── README.md                   # This file
├── LICENSE                     # GPL-3.0
├── .gitignore
│
├── Docs/
│   ├── ARCHITECTURE.md         # System design, data flow, technical decisions
│   ├── ROADMAP.md              # Implementation plan, phases, and progress tracking
│   └── TECH_STACK.md           # Libraries, algorithms, and integration details
│
├── Source/
│   ├── PluginProcessor.h/cpp   # Main audio processor
│   ├── PluginEditor.h/cpp      # Main editor window
│   ├── PluginParameters.h      # Parameter definitions
│   ├── Audio/                  # Audio capture and buffering
│   ├── Analysis/               # Pitch detection and segmentation
│   ├── DSP/                    # Audio engine and pitch/time shifting
│   ├── Data/                   # Data structures (Note, AnalyzedAudio)
│   ├── Alignment/              # Multi-take vocal alignment (DTW)
│   ├── Editing/                # Edit operations, undo/redo
│   ├── UI/                     # All visual components
│   └── Utilities/              # Math, conversion, logging helpers
│
├── ThirdParty/                 # External libraries
│   ├── JUCE/
│   ├── rubberband/
│   └── essentia/
│
└── Tests/                      # Unit tests
```

## License

GPL-3.0 — See [LICENSE](LICENSE) for details.

This license is compatible with JUCE (GPL), Rubber Band (GPL), and Essentia (AGPL).

## Learning Resources

If you're new to audio plugin development (like we are), these resources are helpful:

- **JUCE:** [Official free course](https://juce.com) (launched Jan 2026)
- **JUCE Tutorials:** [Create a basic Audio/MIDI plugin](https://juce.com/tutorials/tutorial_create_projucer_basic_plugin/)
- **YouTube:** [The Audio Programmer](https://www.youtube.com/@TheAudioProgrammer)
- **Book:** "Designing Audio Effect Plugins in C++" by Will Pirkle
- **DSP Fundamentals:** [The Scientist and Engineer's Guide to DSP](http://www.dspguide.com/) (free online)
- **Community:** [JUCE Forum](https://forum.juce.com), [r/AudioProgramming](https://reddit.com/r/AudioProgramming)

## Contributing

This is an open source project. Contributions are welcome once the foundation is in place (Phase 1-2 complete). See [Docs/ROADMAP.md](Docs/ROADMAP.md) for what's being worked on.
