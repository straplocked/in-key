cmake_minimum_required(VERSION 3.22)
project(InKey VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

add_subdirectory(ThirdParty/JUCE)

# ARA SDK 2.2.0 for ARA2 plugin integration
juce_set_ara_sdk_path("${CMAKE_SOURCE_DIR}/ThirdParty/ARA_SDK")

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
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "InKey"
    BUNDLE_ID "com.inkey.vocaleditor"
    IS_ARA_EFFECT TRUE
    ARA_FACTORY_ID "com.inkey.vocaleditor.arafactory"
    ARA_DOCUMENT_ARCHIVE_ID "com.inkey.vocaleditor.aradocumentarchive"
    ARA_ANALYSIS_TYPES kARAContentTypeNotes
    ARA_TRANSFORMATION_FLAGS kARAPlaybackTransformationTimestretch|kARAPlaybackTransformationTimestretchReflectingTempo|kARAPlaybackTransformationContentBasedFadeAtTail|kARAPlaybackTransformationContentBasedFadeAtHead
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
    Source/Editing/QuantizationEngine.cpp
    Source/UI/InKeyLookAndFeel.cpp
    Source/Analysis/KeyDetector.cpp
    Source/Analysis/SibilantDetector.cpp
    Source/Analysis/VibratoAnalyzer.cpp
    Source/Data/MidiExporter.cpp
    Source/ARA/InKeyDocumentController.cpp
    Source/ARA/InKeyPlaybackRenderer.cpp
    Source/ARA/InKeyEditorRenderer.cpp
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
        juce::juce_audio_formats
        juce::juce_dsp
        rubberband
        InKeyBinaryData
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ============================================================
# Unit Tests
# ============================================================
option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_TESTS)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.5.2
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(Catch2)

    juce_add_console_app(InKeyTests
        PRODUCT_NAME "InKey Tests"
    )

    target_sources(InKeyTests PRIVATE
        # Test runner
        Tests/TestMain.cpp
        # Test files
        Tests/NoteDataTests.cpp
        Tests/PitchDetectorTests.cpp
        Tests/NoteSegmenterTests.cpp
        Tests/QuantizationEngineTests.cpp
        Tests/MFCCExtractorTests.cpp
        Tests/DTWAlignerTests.cpp
        Tests/AudioEngineTests.cpp
        Tests/AudioCaptureTests.cpp
        Tests/EditActionsTests.cpp
        # Source files under test
        Source/Analysis/PitchDetector.cpp
        Source/Analysis/NoteSegmenter.cpp
        Source/Editing/QuantizationEngine.cpp
        Source/Alignment/MFCCExtractor.cpp
        Source/Alignment/DTWAligner.cpp
        Source/Audio/AudioCapture.cpp
        Source/DSP/AudioEngine.cpp
        Source/Analysis/SibilantDetector.cpp
        Source/Analysis/VibratoAnalyzer.cpp
    )

    target_include_directories(InKeyTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
    )

    target_compile_definitions(InKeyTests PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_STANDALONE_APPLICATION=1
    )

    target_link_libraries(InKeyTests PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        rubberband
        Catch2::Catch2
    )

    enable_testing()
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    include(Catch)
    catch_discover_tests(InKeyTests)
endif()
