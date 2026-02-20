// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 InKey contributors
// This file is part of InKey. See LICENSE for details.

#include "InKeyEditorRenderer.h"

void InKeyEditorRenderer::prepareToPlay (double sampleRate, int /*maxSamplesPerBlock*/,
                                          int /*numChannels*/,
                                          juce::AudioProcessor::ProcessingPrecision,
                                          AlwaysNonRealtime)
{
    currentSampleRate = sampleRate;
}

void InKeyEditorRenderer::releaseResources()
{
    stopPreview();
    previewBuffer = juce::AudioBuffer<float>();
}

bool InKeyEditorRenderer::processBlock (
    juce::AudioBuffer<float>& buffer,
    juce::AudioProcessor::Realtime /*realtime*/,
    const juce::AudioPlayHead::PositionInfo& /*positionInfo*/) noexcept
{
    // Editor renderer handles note audition preview
    int pos = previewPosition.load();
    if (pos < 0 || previewBuffer.getNumSamples() == 0)
        return true;  // No preview playing — let playback renderer handle it

    const int numSamples = buffer.getNumSamples();
    const int available = juce::jmin (numSamples, previewBuffer.getNumSamples() - pos);

    if (available <= 0)
    {
        previewPosition.store (-1);  // Preview finished
        return true;
    }

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.addFrom (ch, 0, previewBuffer, 0, pos, available);

    previewPosition.store (pos + available);

    if (pos + available >= previewBuffer.getNumSamples())
        previewPosition.store (-1);  // Preview finished

    return true;
}

void InKeyEditorRenderer::startPreview (const juce::AudioBuffer<float>& audio, double /*sampleRate*/)
{
    previewBuffer.makeCopyOf (audio);
    previewPosition.store (0);
}

void InKeyEditorRenderer::stopPreview()
{
    previewPosition.store (-1);
}
