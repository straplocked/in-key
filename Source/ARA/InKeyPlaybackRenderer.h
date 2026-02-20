// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 InKey contributors
// This file is part of InKey. See LICENSE for details.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../DSP/AudioEngine.h"
#include "InKeyDocumentController.h"

class InKeyPlaybackRenderer : public juce::ARAPlaybackRenderer
{
public:
    using juce::ARAPlaybackRenderer::ARAPlaybackRenderer;

    void prepareToPlay (double sampleRate, int maxSamplesPerBlock,
                        int numChannels, juce::AudioProcessor::ProcessingPrecision,
                        AlwaysNonRealtime alwaysNonRealtime) override;
    void releaseResources() override;

    bool processBlock (juce::AudioBuffer<float>& buffer,
                       juce::AudioProcessor::Realtime realtime,
                       const juce::AudioPlayHead::PositionInfo& positionInfo) noexcept override;

    // Signal that notes have been edited — triggers re-render
    void notifyEditsChanged();

private:
    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;

    // Pre-rendered buffer with double-buffer swap
    AudioEngine audioEngine;
    juce::AudioBuffer<float> renderedBuffers[2];
    std::atomic<int> activeBufferIndex { 0 };
    std::atomic<bool> hasRenderedAudio { false };
    std::atomic<bool> renderNeeded { false };

    void performRender();

    // Background render thread
    class RenderThread : public juce::Thread
    {
    public:
        RenderThread (InKeyPlaybackRenderer& r) : Thread ("ARA Render"), renderer (r) {}
        void run() override;
    private:
        InKeyPlaybackRenderer& renderer;
    };
    std::unique_ptr<RenderThread> renderThread;
};
