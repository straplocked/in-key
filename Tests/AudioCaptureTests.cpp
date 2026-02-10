#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "Audio/AudioCapture.h"

using Catch::Matchers::WithinAbs;

// ============================================================
// Initialization
// ============================================================
TEST_CASE ("AudioCapture is not capturing by default", "[AudioCapture]")
{
    AudioCapture capture;
    REQUIRE_FALSE (capture.isCapturing());
}

TEST_CASE ("AudioCapture prepare sets sample rate", "[AudioCapture]")
{
    AudioCapture capture;
    capture.prepare (48000.0, 512);

    REQUIRE_THAT (capture.getSampleRate(), WithinAbs (48000.0, 0.1));
}

TEST_CASE ("AudioCapture duration is zero before capture", "[AudioCapture]")
{
    AudioCapture capture;
    capture.prepare (44100.0, 512);

    REQUIRE_THAT (capture.getCapturedDurationSeconds(), WithinAbs (0.0, 0.001));
}

TEST_CASE ("AudioCapture num samples is zero before capture", "[AudioCapture]")
{
    AudioCapture capture;
    capture.prepare (44100.0, 512);

    REQUIRE (capture.getNumCapturedSamples() == 0);
}

// ============================================================
// Capture state
// ============================================================
TEST_CASE ("AudioCapture startCapture sets capturing state", "[AudioCapture]")
{
    AudioCapture capture;
    capture.prepare (44100.0, 512);

    capture.startCapture();
    REQUIRE (capture.isCapturing());

    capture.stopCapture();
    REQUIRE_FALSE (capture.isCapturing());
}

// ============================================================
// Write and drain
// ============================================================
TEST_CASE ("AudioCapture captures written audio after stop", "[AudioCapture]")
{
    AudioCapture capture;
    capture.prepare (44100.0, 512);

    capture.startCapture();

    // Write some audio blocks
    juce::AudioBuffer<float> block (1, 512);
    block.clear();
    float* data = block.getWritePointer (0);
    for (int i = 0; i < 512; ++i)
        data[i] = static_cast<float> (i) / 512.0f;

    capture.writeAudio (block, 512);
    capture.writeAudio (block, 512);
    capture.writeAudio (block, 512);

    capture.stopCapture();

    // After stop, FIFO should be drained and audio available
    REQUIRE (capture.getNumCapturedSamples() > 0);
    REQUIRE (capture.getCapturedDurationSeconds() > 0.0);
}

TEST_CASE ("AudioCapture writeAudio does nothing when not capturing", "[AudioCapture]")
{
    AudioCapture capture;
    capture.prepare (44100.0, 512);

    // Don't start capture — write should be ignored
    juce::AudioBuffer<float> block (1, 512);
    block.clear();
    capture.writeAudio (block, 512);

    // No capture happened
    REQUIRE (capture.getNumCapturedSamples() == 0);
}

TEST_CASE ("AudioCapture mixes stereo to mono", "[AudioCapture]")
{
    AudioCapture capture;
    capture.prepare (44100.0, 512);

    capture.startCapture();

    // Create stereo buffer: left=1.0, right=0.0 → mono should be 0.5
    juce::AudioBuffer<float> stereoBlock (2, 512);
    for (int i = 0; i < 512; ++i)
    {
        stereoBlock.setSample (0, i, 1.0f);
        stereoBlock.setSample (1, i, 0.0f);
    }

    capture.writeAudio (stereoBlock, 512);
    capture.stopCapture();

    // Check that captured audio is mono and approximately the average
    REQUIRE (capture.getNumCapturedSamples() > 0);
    REQUIRE (capture.getCapturedAudio().getNumChannels() == 1);

    const float* capturedData = capture.getCapturedAudio().getReadPointer (0);
    // First sample should be approximately 0.5 (average of 1.0 and 0.0)
    REQUIRE_THAT (capturedData[0], WithinAbs (0.5f, 0.01f));
}

// ============================================================
// Duration calculation
// ============================================================
TEST_CASE ("AudioCapture duration calculation is correct", "[AudioCapture]")
{
    AudioCapture capture;
    capture.prepare (44100.0, 512);

    capture.startCapture();

    // Write a small amount that fits in the FIFO (sized ~0.2s + maxBlockSize).
    // The FIFO is nextPowerOfTwo(44100*0.2 + 512) = 16384 samples.
    // Write 10 blocks of 512 = 5120 samples, well within FIFO capacity.
    juce::AudioBuffer<float> block (1, 512);
    block.clear();

    for (int i = 0; i < 10; ++i)
        capture.writeAudio (block, 512);

    capture.stopCapture();

    // 5120 samples at 44100 Hz ≈ 0.116 seconds
    double expectedDuration = 5120.0 / 44100.0;
    REQUIRE_THAT (capture.getCapturedDurationSeconds(), WithinAbs (expectedDuration, 0.01));
}

// ============================================================
// Listener
// ============================================================
TEST_CASE ("AudioCapture listener is called on stopCapture", "[AudioCapture]")
{
    AudioCapture capture;
    capture.prepare (44100.0, 512);

    struct TestListener : public AudioCapture::Listener
    {
        bool called = false;
        void captureUpdated() override { called = true; }
    };

    TestListener listener;
    capture.addListener (&listener);

    capture.startCapture();

    juce::AudioBuffer<float> block (1, 512);
    block.clear();
    capture.writeAudio (block, 512);

    capture.stopCapture();

    REQUIRE (listener.called);

    capture.removeListener (&listener);
}
