// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 InKey contributors
// This file is part of InKey. See LICENSE for details.

#include "InKeyPlaybackRenderer.h"
#include "InKeyDocumentController.h"

void InKeyPlaybackRenderer::prepareToPlay (double sampleRate, int maxSamplesPerBlock,
                                            int /*numChannels*/,
                                            juce::AudioProcessor::ProcessingPrecision,
                                            AlwaysNonRealtime)
{
    currentSampleRate = sampleRate;
    currentBlockSize = maxSamplesPerBlock;
}

void InKeyPlaybackRenderer::releaseResources()
{
    if (renderThread != nullptr)
    {
        renderThread->stopThread (2000);
        renderThread.reset();
    }

    hasRenderedAudio.store (false);
    renderedBuffers[0] = juce::AudioBuffer<float>();
    renderedBuffers[1] = juce::AudioBuffer<float>();
}

bool InKeyPlaybackRenderer::processBlock (
    juce::AudioBuffer<float>& buffer,
    juce::AudioProcessor::Realtime /*realtime*/,
    const juce::AudioPlayHead::PositionInfo& positionInfo) noexcept
{
    buffer.clear();

    const auto timeInSamples = positionInfo.getTimeInSamples().orFallback (0);

    if (! hasRenderedAudio.load())
        return true;  // Analysis not done yet — output silence

    // Read from the pre-rendered double buffer at the DAW's current position
    const auto& activeBuffer = renderedBuffers[activeBufferIndex.load()];
    const int numSamples = buffer.getNumSamples();

    for (const auto* playbackRegion : getPlaybackRegions())
    {
        // Map DAW timeline position to the rendered audio buffer
        const auto regionStart = playbackRegion->getStartInPlaybackSamples (currentSampleRate);
        const auto regionEnd = playbackRegion->getEndInPlaybackSamples (currentSampleRate);

        const auto blockStart = timeInSamples;
        const auto blockEnd = timeInSamples + numSamples;

        // Intersection of this block with the playback region
        const auto renderStart = std::max (blockStart, static_cast<int64_t> (regionStart));
        const auto renderEnd = std::min (blockEnd, static_cast<int64_t> (regionEnd));

        if (renderStart >= renderEnd)
            continue;

        // Offset within the audio modification (region may start at a non-zero point in the source)
        const auto modStart = playbackRegion->getStartInAudioModificationSamples();
        const auto srcOffset = static_cast<int> (renderStart - regionStart + modStart);
        const auto dstOffset = static_cast<int> (renderStart - blockStart);
        const auto length = static_cast<int> (renderEnd - renderStart);

        if (srcOffset >= 0 && srcOffset + length <= activeBuffer.getNumSamples())
        {
            buffer.addFrom (0, dstOffset, activeBuffer, 0, srcOffset, length);

            // Copy mono to all channels
            for (int ch = 1; ch < buffer.getNumChannels(); ++ch)
                buffer.copyFrom (ch, dstOffset, buffer, 0, dstOffset, length);
        }
    }

    return true;
}

void InKeyPlaybackRenderer::notifyEditsChanged()
{
    renderNeeded.store (true);

    // Launch render on a background thread
    if (renderThread != nullptr)
    {
        renderThread->stopThread (2000);
        renderThread.reset();
    }

    renderThread = std::make_unique<RenderThread> (*this);
    renderThread->startThread();
}

void InKeyPlaybackRenderer::performRender()
{
    auto* docController = juce::ARADocumentControllerSpecialisation
        ::getSpecialisedDocumentController<InKeyDocumentController> (getDocumentController());
    if (docController == nullptr)
        return;

    for (const auto* region : getPlaybackRegions())
    {
        auto* source = region->getAudioModification<juce::ARAAudioModification>()
                              ->getAudioSource<juce::ARAAudioSource>();
        auto* analysis = docController->getAnalysis (source);
        if (analysis == nullptr || ! analysis->analysisComplete.load())
            continue;

        // Read source audio from the host
        const auto numSamples = source->getSampleCount();
        const auto sampleRate = source->getSampleRate();

        if (! source->isSampleAccessEnabled() || numSamples == 0)
            continue;

        ARA::PlugIn::HostAudioReader hostReader (source);
        juce::AudioBuffer<float> sourceAudio (1, static_cast<int> (numSamples));
        float* buffers[1] = { sourceAudio.getWritePointer (0) };
        hostReader.readAudioSamples (0, numSamples,
                                      reinterpret_cast<void* const*> (buffers));

        // Render with edits using existing AudioEngine
        auto rendered = audioEngine.render (sourceAudio, sampleRate,
                                            analysis->notes, analysis->pitchCurve);

        // Swap into double buffer
        int inactiveIndex = 1 - activeBufferIndex.load();
        renderedBuffers[inactiveIndex] = std::move (rendered);
        activeBufferIndex.store (inactiveIndex);
        hasRenderedAudio.store (true);
    }

    renderNeeded.store (false);
}

void InKeyPlaybackRenderer::RenderThread::run()
{
    do
    {
        renderer.performRender();
    }
    while (renderer.renderNeeded.exchange (false) && ! threadShouldExit());
}
