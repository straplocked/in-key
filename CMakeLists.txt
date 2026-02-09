cmake_minimum_required(VERSION 3.22)
project(InKey VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

add_subdirectory(ThirdParty/JUCE)

# Fetch Rubber Band Library for pitch/time shifting (single-file build)
FetchContent_Declare(
    rubberband
    GIT_REPOSITORY https://github.com/breakfastquay/rubberband.git
    GIT_TAG v3.3.0
    GIT_SHALLOW TRUE
)
FetchContent_Populate(rubberband)

# Build Rubber Band as a static library from the single-file compilation unit
add_library(rubberband STATIC
    ${rubberband_SOURCE_DIR}/single/RubberBandSingle.cpp
)
target_include_directories(rubberband PUBLIC
    ${rubberband_SOURCE_DIR}
)
# macOS needs Accelerate framework for vDSP
if(APPLE)
    target_link_libraries(rubberband PRIVATE "-framework Accelerate")
endif()

# Embed font resources as binary data
juce_add_binary_data(InKeyBinaryData
    SOURCES
        Resources/Fonts/Inter-Regular.ttf
        Resources/Fonts/Inter-Bold.ttf
)

juce_add_plugin(InKey
    COMPANY_NAME "InKey"
    PLUGIN_MANUFACTURER_CODE Inky
    PLUGIN_CODE Vedt
    FORMATS VST3 Standalone
    PRODUCT_NAME "InKey"
    BUNDLE_ID "com.inkey.vocaleditor"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
)

target_sources(InKey PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Audio/AudioCapture.cpp
    Source/Analysis/PitchDetector.cpp
    Source/Analysis/PitchAnalyzer.cpp
    Source/Analysis/NoteSegmenter.cpp
    Source/DSP/AudioEngine.cpp
    Source/UI/WaveformComponent.cpp
    Source/UI/PitchCurveComponent.cpp
    Source/UI/PianoRollComponent.cpp
    Source/UI/PropertiesPanel.cpp
    Source/Alignment/MFCCExtractor.cpp
    Source/Alignment/DTWAligner.cpp
    Source/UI/InKeyLookAndFeel.cpp
)

target_compile_definitions(InKey PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(InKey
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        rubberband
        InKeyBinaryData
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
