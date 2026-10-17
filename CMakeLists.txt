cmake_minimum_required(VERSION 3.22)

# Deployment target / architectures must be set before project() so they
# propagate to every sub-build (JUCE, Rubber Band, Catch2).
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build universal binary")
endif()

project(InKey VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================
# Options
# ============================================================
# ARA2 support needs the Celemony ARA SDK (Apache-2.0). It is opt-in so the
# default VST3/AU/Standalone build has no extra dependency. When ON, the SDK
# is fetched automatically unless ARA_SDK_PATH points at a local checkout.
option(ENABLE_ARA "Build with ARA2 (Audio Random Access) support" OFF)
set(ARA_SDK_PATH "" CACHE PATH
    "Local ARA SDK checkout (needs ARA_API and ARA_Library). Fetched automatically if empty.")
set(ARA_SDK_TAG "releases/2.2.0" CACHE STRING "ARA SDK git tag fetched when ARA_SDK_PATH is empty")

option(BUILD_TESTS "Build unit tests" ON)

# ============================================================
# Dependencies (all fetched at configure time, nothing to clone by hand)
# ============================================================
include(FetchContent)

# JUCE 7.0.12. A checkout at ThirdParty/JUCE (the documented manual setup) wins;
# otherwise JUCE is fetched.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/JUCE/CMakeLists.txt")
    add_subdirectory(ThirdParty/JUCE)
else()
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 7.0.12
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

# ARA SDK 2.2.0 (optional)
if(ENABLE_ARA)
    if(NOT ARA_SDK_PATH AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/ARA_SDK/ARA_API")
        set(ARA_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/ARA_SDK")
    endif()

    if(NOT ARA_SDK_PATH)
        # Only ARA_API and ARA_Library are needed; skip the large ARA_Examples submodule.
        # SOURCE_SUBDIR points at a directory with no CMakeLists.txt so the SDK is
        # downloaded but not add_subdirectory()'d (JUCE consumes it by include path).
        FetchContent_Declare(
            ara_sdk
            GIT_REPOSITORY https://github.com/Celemony/ARA_SDK.git
            GIT_TAG ${ARA_SDK_TAG}
            GIT_SHALLOW TRUE
            GIT_SUBMODULES ARA_API ARA_Library
            SOURCE_SUBDIR do_not_add_subdirectory
        )
        FetchContent_MakeAvailable(ara_sdk)
        set(ARA_SDK_PATH "${ara_sdk_SOURCE_DIR}")
    endif()

    message(STATUS "ARA2 support enabled, SDK: ${ARA_SDK_PATH}")
    juce_set_ara_sdk_path("${ARA_SDK_PATH}")
endif()

# Rubber Band v3.3.0 (GPL-2.0-or-later) for pitch/time shifting, built from its
# single-file compilation unit. Fetched only; its own build system is not used.
FetchContent_Declare(
    rubberband
    GIT_REPOSITORY https://github.com/breakfastquay/rubberband.git
    GIT_TAG v3.3.0
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR do_not_add_subdirectory
)
FetchContent_MakeAvailable(rubberband)

# Build Rubber Band as a static library from the single-file compilation unit
add_library(rubberband STATIC
    ${rubberband_SOURCE_DIR}/single/RubberBandSingle.cpp
)
target_include_directories(rubberband PUBLIC
    ${rubberband_SOURCE_DIR}
)
# The VST3 is a shared module, so every static library linked into it must be
# position-independent. Without this, the Linux link fails with
# "relocation R_X86_64_PC32 ... can not be used when making a shared object".
# macOS and Windows link fine either way, so only Linux surfaces it.
set_target_properties(rubberband PROPERTIES POSITION_INDEPENDENT_CODE ON)
# MSVC: keep <windows.h> from defining min/max macros, which break
# std::numeric_limits<T>::max() inside Rubber Band.
if(MSVC)
    target_compile_definitions(rubberband PRIVATE NOMINMAX _USE_MATH_DEFINES)
endif()
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

if(ENABLE_ARA)
    set(INKEY_IS_ARA_EFFECT TRUE)
else()
    set(INKEY_IS_ARA_EFFECT FALSE)
endif()

juce_add_plugin(InKey
    COMPANY_NAME "InKey"
    PLUGIN_MANUFACTURER_CODE Inky
    PLUGIN_CODE Vedt
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "InKey"
    BUNDLE_ID "com.inkey.vocaleditor"
    IS_ARA_EFFECT ${INKEY_IS_ARA_EFFECT}
    # ARA ids are spliced into C string literals, so they carry their own quotes
    ARA_FACTORY_ID "\"com.inkey.vocaleditor.arafactory\""
    ARA_DOCUMENT_ARCHIVE_ID "\"com.inkey.vocaleditor.aradocumentarchive\""
    ARA_ANALYSIS_TYPES kARAContentTypeNotes
    ARA_TRANSFORMATION_FLAGS
        kARAPlaybackTransformationTimestretch
        kARAPlaybackTransformationTimestretchReflectingTempo
        kARAPlaybackTransformationContentBasedFadeAtTail
        kARAPlaybackTransformationContentBasedFadeAtHead
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
    Source/Presets/PresetData.cpp
    Source/Presets/PresetManager.cpp
    Source/UI/PresetPanel.cpp
)

if(ENABLE_ARA)
    target_sources(InKey PRIVATE
        Source/ARA/InKeyDocumentController.cpp
        Source/ARA/InKeyPlaybackRenderer.cpp
        Source/ARA/InKeyEditorRenderer.cpp
    )
endif()

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
