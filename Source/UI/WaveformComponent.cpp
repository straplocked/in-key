// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 InKey contributors
// This file is part of InKey. See LICENSE for details.

#include "WaveformComponent.h"
#include "InKeyLookAndFeel.h"

WaveformComponent::WaveformComponent (AudioCapture& capture)
    : audioCapture (capture)
{
    audioCapture.addListener (this);
}

WaveformComponent::~WaveformComponent()
{
    audioCapture.removeListener (this);
}

void WaveformComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Glass panel background
    InKeyLookAndFeel::drawGlassPanel (g, bounds, 8.0f, true);

    int numSamples = audioCapture.getNumCapturedSamples();

    if (numSamples == 0)
    {
        g.setColour (InKeyColours::textDim);
        g.setFont (juce::Font (14.0f));
        g.drawText ("No audio captured", bounds, juce::Justification::centred);
        return;
    }

    if (pathDirty)
        rebuildPath();

    // Draw the waveform with gradient
    float padding = 4.0f;
    auto drawArea = bounds.reduced (padding);

    juce::ColourGradient waveGradient (
        InKeyColours::accentAlign, drawArea.getX(), drawArea.getY(),
        InKeyColours::accentPlay,  drawArea.getX(), drawArea.getBottom(),
        false);
    g.setGradientFill (waveGradient);
    g.strokePath (waveformPath, juce::PathStrokeType (1.5f),
                  juce::AffineTransform::scale (drawArea.getWidth(), drawArea.getHeight() / 2.0f)
                      .translated (drawArea.getX(), drawArea.getCentreY()));

    // Duration label
    g.setColour (InKeyColours::textSecondary);
    g.setFont (juce::Font (12.0f));
    auto durationStr = juce::String (audioCapture.getCapturedDurationSeconds(), 1) + "s";
    g.drawText (durationStr, bounds.reduced (8.0f), juce::Justification::bottomRight);
}

void WaveformComponent::resized()
{
    pathDirty = true;
}

void WaveformComponent::captureUpdated()
{
    pathDirty = true;
    repaint();
}

void WaveformComponent::rebuildPath()
{
    waveformPath.clear();

    int numSamples = audioCapture.getNumCapturedSamples();
    if (numSamples == 0)
        return;

    const float* data = audioCapture.getCapturedAudio().getReadPointer (0);
    int width = getWidth();

    if (width <= 0)
        return;

    // Downsample: for each pixel, find the min and max sample values
    int numPoints = juce::jmin (width, numSamples);

    waveformPath.startNewSubPath (0.0f, 0.0f);

    for (int i = 0; i < numPoints; ++i)
    {
        int startSample = (i * numSamples) / numPoints;
        int endSample = juce::jmin (((i + 1) * numSamples) / numPoints, numSamples);

        float minVal = 0.0f;
        float maxVal = 0.0f;

        for (int s = startSample; s < endSample; ++s)
        {
            float sample = data[s];
            if (sample < minVal) minVal = sample;
            if (sample > maxVal) maxVal = sample;
        }

        float x = static_cast<float> (i) / static_cast<float> (numPoints);

        // Draw from max to min to create a filled waveform shape
        waveformPath.lineTo (x, -maxVal);
    }

    // Come back along the bottom (min values) for a closed shape
    for (int i = numPoints - 1; i >= 0; --i)
    {
        int startSample = (i * numSamples) / numPoints;
        int endSample = juce::jmin (((i + 1) * numSamples) / numPoints, numSamples);

        float minVal = 0.0f;

        for (int s = startSample; s < endSample; ++s)
        {
            float sample = data[s];
            if (sample < minVal) minVal = sample;
        }

        float x = static_cast<float> (i) / static_cast<float> (numPoints);
        waveformPath.lineTo (x, -minVal);
    }

    waveformPath.closeSubPath();
    pathDirty = false;
}
