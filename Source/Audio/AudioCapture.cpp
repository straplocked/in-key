#include "AudioCapture.h"

AudioCapture::AudioCapture()
{
}

AudioCapture::~AudioCapture()
{
    stopTimer();
}

void AudioCapture::prepare (double sampleRate, int maxBlockSize)
{
    currentSampleRate = sampleRate;

    // FIFO needs to be large enough to hold audio between timer drains.
    // At 50ms timer interval, we need at least sampleRate * 0.05 samples.
    // Use 4x that for safety margin, rounded up to next power of 2.
    fifoSize = juce::nextPowerOfTwo (static_cast<int> (sampleRate * 0.2) + maxBlockSize);
    fifo = std::make_unique<juce::AbstractFifo> (fifoSize);
    fifoBuffer.setSize (1, fifoSize); // mono capture
    fifoBuffer.clear();
}

void AudioCapture::startCapture()
{
    // Reset state
    fifo->reset();
    totalCapturedSamples = 0;

    // Pre-allocate captured audio buffer for expected max duration
    int maxSamples = static_cast<int> (currentSampleRate * maxCaptureDurationSeconds);
    capturedAudio.setSize (1, maxSamples, false, true, false);

    capturing.store (true);
    startTimer (timerIntervalMs);
}

void AudioCapture::stopCapture()
{
    capturing.store (false);
    stopTimer();

    // Drain any remaining audio from the FIFO
    drainFifo();

    // Trim the buffer to actual captured size
    if (totalCapturedSamples > 0)
        capturedAudio.setSize (1, totalCapturedSamples, true, false, false);
    else
        capturedAudio.setSize (1, 0);

    listeners.call ([] (Listener& l) { l.captureUpdated(); });
}

void AudioCapture::writeAudio (const juce::AudioBuffer<float>& buffer, int numSamples)
{
    if (! capturing.load())
        return;

    // Mix down to mono if stereo
    const int numChannels = buffer.getNumChannels();

    int start1, size1, start2, size2;
    fifo->prepareToWrite (numSamples, start1, size1, start2, size2);

    if (size1 + size2 < numSamples)
        return; // FIFO full, drop samples (shouldn't happen with proper sizing)

    if (numChannels == 1)
    {
        if (size1 > 0)
            fifoBuffer.copyFrom (0, start1, buffer.getReadPointer (0), size1);
        if (size2 > 0)
            fifoBuffer.copyFrom (0, start2, buffer.getReadPointer (0, size1), size2);
    }
    else
    {
        // Mix to mono: average all channels
        if (size1 > 0)
        {
            fifoBuffer.copyFrom (0, start1, buffer.getReadPointer (0), size1);
            for (int ch = 1; ch < numChannels; ++ch)
                fifoBuffer.addFrom (0, start1, buffer.getReadPointer (ch), size1);

            juce::FloatVectorOperations::multiply (fifoBuffer.getWritePointer (0, start1),
                                                    1.0f / numChannels, size1);
        }
        if (size2 > 0)
        {
            fifoBuffer.copyFrom (0, start2, buffer.getReadPointer (0, size1), size2);
            for (int ch = 1; ch < numChannels; ++ch)
                fifoBuffer.addFrom (0, start2, buffer.getReadPointer (ch, size1), size2);

            juce::FloatVectorOperations::multiply (fifoBuffer.getWritePointer (0, start2),
                                                    1.0f / numChannels, size2);
        }
    }

    fifo->finishedWrite (size1 + size2);
}

double AudioCapture::getCapturedDurationSeconds() const
{
    if (currentSampleRate <= 0.0)
        return 0.0;
    return static_cast<double> (totalCapturedSamples) / currentSampleRate;
}

void AudioCapture::addListener (Listener* listener)
{
    listeners.add (listener);
}

void AudioCapture::removeListener (Listener* listener)
{
    listeners.remove (listener);
}

void AudioCapture::timerCallback()
{
    drainFifo();
    listeners.call ([] (Listener& l) { l.captureUpdated(); });
}

void AudioCapture::drainFifo()
{
    int start1, size1, start2, size2;
    int available = fifo->getNumReady();

    if (available == 0)
        return;

    // Check if we'd exceed the max capture buffer
    int maxSamples = capturedAudio.getNumSamples();
    int remaining = maxSamples - totalCapturedSamples;
    if (remaining <= 0)
    {
        capturing.store (false);
        return;
    }

    int toRead = juce::jmin (available, remaining);

    fifo->prepareToRead (toRead, start1, size1, start2, size2);

    if (size1 > 0)
        capturedAudio.copyFrom (0, totalCapturedSamples, fifoBuffer, 0, start1, size1);

    if (size2 > 0)
        capturedAudio.copyFrom (0, totalCapturedSamples + size1, fifoBuffer, 0, start2, size2);

    fifo->finishedRead (size1 + size2);
    totalCapturedSamples += size1 + size2;
}
