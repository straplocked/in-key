# InKey — Tech Stack & Algorithm Reference

Detailed reference for all libraries, algorithms, and technical decisions. Consult this when integrating third-party code or implementing DSP features.

**Last Updated:** 2026-02-09

---

## Libraries

### JUCE 7.x

**What:** Cross-platform C++ framework for audio plugins and applications.

**Website:** https://juce.com
**Repository:** https://github.com/juce-framework/JUCE
**License:** GPL-3.0 (free for open source) or commercial ($900+)
**We use:** GPL-3.0 (compatible with our project license)

**We use it for:**
- VST3 and AU plugin wrappers (handles all DAW communication)
- GUI framework (`juce::Component` hierarchy)
- Audio buffer management (`juce::AudioBuffer<float>`)
- FFT (`juce::dsp::FFT`)
- Threading (`juce::Thread`, `juce::AbstractFifo`)
- State serialization (`juce::ValueTree`)
- Undo/redo (`juce::UndoManager`)
- File I/O (`juce::File`, `juce::FileChooser`)

**Key JUCE modules we depend on:**
- `juce_audio_processors` — Plugin wrapper (AudioProcessor)
- `juce_audio_utils` — Audio utilities
- `juce_dsp` — DSP classes (FFT, filters)
- `juce_gui_basics` — UI components
- `juce_gui_extra` — Additional UI features

**Integration:**
```cmake
add_subdirectory(ThirdParty/JUCE)
juce_add_plugin(VocalEditor
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "InKey"
)
target_link_libraries(VocalEditor PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_basics
)
```

**Documentation:**
- Tutorials: https://juce.com/tutorials
- API Reference: https://docs.juce.com/master/
- Forum: https://forum.juce.com
- Free course (2026): https://juce.com (beginner-friendly)

---

### Rubber Band Library

**What:** High-quality audio time-stretching and pitch-shifting library.

**Website:** https://breakfastquay.com/rubberband/
**Repository:** https://github.com/breakfastquay/rubberband
**License:** GPL-2.0 (or commercial)
**Current version:** v3.3.0

**We use it for:**
- Pitch shifting (when user drags a note to a different pitch)
- Time stretching (when user adjusts note duration or alignment)
- Formant preservation (maintaining vocal character during pitch shifts)

**Key API:**
```cpp
#include <rubberband/RubberBandStretcher.h>

// Create stretcher for offline processing
RubberBand::RubberBandStretcher stretcher(
    sampleRate, channels,
    RubberBandStretcher::OptionProcessOffline |
    RubberBandStretcher::OptionStretchPrecise |
    RubberBandStretcher::OptionFormantPreserved
);

// Set pitch (in ratio: 2^(semitones/12))
stretcher.setPitchScale(ratio);

// Set time stretch ratio (2.0 = twice as slow)
stretcher.setTimeRatio(ratio);

// Process audio
stretcher.study(input, sampleCount, true);  // Offline: study first
stretcher.process(input, sampleCount, true);
int available = stretcher.available();
stretcher.retrieve(output, available);
```

**Important notes:**
- Offline mode requires calling `study()` on the entire audio first, then `process()`
- Real-time mode skips `study()` but produces lower quality
- Formant preservation adds ~10% CPU overhead
- Thread-safe for a single instance (don't share across threads)

**Integration:**
```cmake
FetchContent_Declare(
    rubberband
    GIT_REPOSITORY https://github.com/breakfastquay/rubberband.git
    GIT_TAG v3.3.0
)
FetchContent_MakeAvailable(rubberband)
target_link_libraries(VocalEditor PRIVATE rubberband)
```

**Alternative (MIT license):** signalsmith-stretch
- Repository: https://github.com/Signalsmith-Audio/signalsmith-stretch
- Header-only, MIT license, good quality
- Consider if GPL becomes a problem for any reason

---

### Standalone YIN Pitch Detection

**What:** Fundamental frequency estimator for speech and music, implemented directly in InKey.

**Files:** `Source/Analysis/PitchDetector.h/cpp`

**Implementation:**
- Full YIN algorithm: difference function, cumulative mean normalized difference, absolute threshold, parabolic interpolation
- Frame size: 2048 samples, Hop size: 256 samples
- Vocal range: 80 Hz - 800 Hz
- Confidence threshold: configurable (default 0.3)
- No external dependencies — uses only standard C++ and JUCE audio buffers

**Reference:** de Cheveigné & Kawahara (2002), *"YIN, a fundamental frequency estimator for speech and music"*

---

### Standalone MFCC Extraction

**What:** Mel-Frequency Cepstral Coefficient extraction for vocal alignment features, implemented using JUCE FFT.

**Files:** `Source/Alignment/MFCCExtractor.h/cpp`

**Implementation:**
- Hamming window, pre-emphasis filter
- 26 Mel filterbank channels
- DCT-II for 13 MFCC coefficients per frame
- Frame size: 2048, Hop size: 1024
- Uses `juce::dsp::FFT` — no external dependencies

---

### DTW (Dynamic Time Warping)

**What:** Algorithm for measuring similarity between two temporal sequences that may vary in speed.

**We implement this ourselves** (it's a straightforward algorithm, ~100 lines of C++).

**Reference implementation:** https://github.com/cjekel/DTW_cpp

**Algorithm:**
1. Extract MFCC features from reference and target takes
2. Build cost matrix: `cost[i][j] = distance(ref[i], target[j])`
3. Fill matrix using dynamic programming:
   ```
   cost[i][j] = dist(ref[i], target[j]) + min(
       cost[i-1][j],       // insertion
       cost[i][j-1],       // deletion
       cost[i-1][j-1]      // match
   )
   ```
4. Backtrack from `cost[N][M]` to find optimal alignment path
5. Convert path to time-warp curve (maps target time → reference time)
6. Apply time-stretching to target using Rubber Band

**Optimization for long audio:**
- Use Sakoe-Chiba band constraint (limit alignment to ±N frames from diagonal)
- Reduces O(N*M) to O(N*bandwidth)
- Bandwidth of 2-5 seconds is usually sufficient for vocal alignment

---

## Algorithms

### YIN Pitch Detection

**Paper:** "YIN, a fundamental frequency estimator for speech and music" (de Cheveigné & Kawahara, 2002)

**How it works:**
1. Compute difference function: `d(τ) = Σ(x[j] - x[j+τ])²`
2. Apply cumulative mean normalization (reduces octave errors)
3. Find first dip below threshold (0.1-0.15 for vocals)
4. Parabolic interpolation for sub-sample accuracy

**Parameters for vocals:**
- Frame size: 2048 samples (gives resolution down to ~21 Hz at 44.1kHz)
- Hop size: 256 samples (~5.8ms time resolution)
- Minimum frequency: 80 Hz (covers lowest male vocals)
- Maximum frequency: 800 Hz (covers highest female vocals, excludes harmonics)
- Confidence threshold: 0.3 (below this, mark as unvoiced)

**pYIN enhancement:**
- Uses Hidden Markov Model (HMM) for temporal smoothing
- Outputs probability distribution over pitch candidates
- Better handles vibrato, pitch transitions, and uncertain regions
- Slightly higher CPU cost (~2x YIN)

### Formant Preservation

**Why it matters:** When you shift pitch up, formants naturally shift up too, making the voice sound like a chipmunk. Formant preservation keeps the vocal character (resonance structure) while shifting the pitch.

**How Rubber Band does it:**
1. Extract spectral envelope (formant structure) via cepstral analysis
2. Separate fine structure (pitch harmonics) from envelope
3. Shift fine structure (change pitch)
4. Reapply original envelope (restore formant positions)

**Rubber Band flag:** `RubberBandStretcher::OptionFormantPreserved`

**Independent formant shifting:**
- To shift formants independently of pitch, you need to:
  1. Process with Rubber Band (pitch shift with formant preservation)
  2. Apply a second pass that shifts only the spectral envelope
  3. This is what the "formant shift" slider controls

### Note Segmentation

**Strategy:** Combine pitch detection + onset detection to identify discrete notes.

**Algorithm:**
1. Run pitch detection → continuous pitch curve with confidence
2. Run onset detection → list of onset times
3. Segment rules:
   - Start new note at each onset (if pitch is detected)
   - End note when confidence drops below threshold for >50ms
   - End note when pitch jumps >2 semitones between consecutive frames
   - Minimum note duration: 50ms (ignore shorter segments)
4. For each note:
   - Average pitch = median of pitch values within note (robust to outliers)
   - MIDI note = round(12 * log2(avgPitch / 440) + 69)
   - Confidence = mean confidence across note frames

### MFCC (Mel-Frequency Cepstral Coefficients)

**Used for:** DTW vocal alignment (comparing timbral similarity between takes).

**How it works:**
1. Apply FFT to get magnitude spectrum
2. Apply mel filterbank (triangular filters spaced on mel scale)
3. Take log of filter outputs
4. Apply DCT (Discrete Cosine Transform)
5. Keep first 13 coefficients

**Parameters:**
- FFT size: 2048
- Hop size: 512 (for alignment, we don't need as fine resolution as pitch)
- Number of mel filters: 40
- Number of MFCC coefficients: 13
- Sample rate: match audio sample rate

---

## Build Configuration

### Minimum Requirements

| Tool | Version | Notes |
|------|---------|-------|
| C++ Standard | C++17 | Required by JUCE 7 |
| CMake | 3.22+ | For FetchContent and JUCE CMake API |
| Xcode | 14+ | macOS builds |
| Visual Studio | 2019+ | Windows builds |
| macOS SDK | 11.0+ | Minimum deployment target |
| Windows SDK | 10.0+ | Minimum deployment target |

### Compiler Flags

```cmake
# Recommended JUCE flags (applied automatically)
juce::juce_recommended_config_flags
juce::juce_recommended_lto_flags
juce::juce_recommended_warning_flags

# Additional flags for our project
target_compile_definitions(VocalEditor PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

# Windows-specific
if(WIN32)
    target_compile_definitions(VocalEditor PRIVATE
        _USE_MATH_DEFINES
        NOMINMAX
    )
endif()
```

### Plugin Identifiers

```cmake
juce_add_plugin(VocalEditor
    COMPANY_NAME "InKey"
    PLUGIN_MANUFACTURER_CODE Inky
    PLUGIN_CODE Vedt
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "InKey"
    BUNDLE_ID "com.inkey.vocaleditor"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
)
```

---

## Testing Tools

| Tool | Purpose |
|------|---------|
| **REAPER** | Primary DAW for testing (free trial, great plugin debugging) |
| **Pluginval** | JUCE's plugin validation tool (https://github.com/Tracktion/pluginval) |
| **auval** | macOS AU validation (`auval -v aufx Vedt Inky`) |
| **Catch2** | Unit testing framework for C++ |
| **JUCE UnitTest** | Built-in JUCE testing support |

---

## Reference Projects

Projects to study for implementation patterns:

| Project | What to learn | URL |
|---------|--------------|-----|
| Piano Roll Editor | Piano roll UI, note rendering, mouse interaction | https://github.com/Sjhunt93/Piano-Roll-Editor |
| JUCE Audio Plugins | Plugin architecture, parameter management | https://github.com/jcurtis4207/Juce-Plugins |
| pitch-detection | Multiple pitch detection algorithms in C++ | https://github.com/sevagh/pitch-detection |
| DTW_cpp | C++ DTW implementation | https://github.com/cjekel/DTW_cpp |
| LibPyin | Standalone pYIN implementation | https://github.com/xstreck1/LibPyin |
