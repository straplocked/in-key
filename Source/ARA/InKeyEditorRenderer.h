// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 InKey contributors
// This file is part of InKey. See LICENSE for details.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class InKeyEditorRenderer : public juce::ARAEditorRenderer
{
public:
    using juce::ARAEditorRenderer::ARAEditorRenderer;

    void prepareToPlay (double sampleRate, int maxSamplesPerBlock,
                        int numChannels, juce::AudioProcessor::ProcessingPrecision,
                        AlwaysNonRealtime) override;
    void releaseResources() override;

    bool processBlock (juce::AudioBuffer<float>& buffer,
                       juce::AudioProcessor::Realtime realtime,
                       const juce::AudioPlayHead::PositionInfo& positionInfo) noexcept override;

    // For note audition preview
    void startPreview (const juce::AudioBuffer<float>& audio, double sampleRate);
    void stopPreview();

private:
    juce::AudioBuffer<float> previewBuffer;
    std::atomic<int> previewPosition { -1 };
    double currentSampleRate = 44100.0;
};
