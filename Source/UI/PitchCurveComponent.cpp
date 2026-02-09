#include "PitchCurveComponent.h"
#include "InKeyLookAndFeel.h"
#include <cmath>

PitchCurveComponent::PitchCurveComponent()
{
    setInterceptsMouseClicks (false, false);
}

void PitchCurveComponent::setPitchData (const PitchCurve& data, double audioDuration)
{
    pitchData = data;
    totalDuration = audioDuration;
    repaint();
}

void PitchCurveComponent::clear()
{
    pitchData.clear();
    totalDuration = 0.0;
    repaint();
}

void PitchCurveComponent::paint (juce::Graphics& g)
{
    if (pitchData.empty() || totalDuration <= 0.0)
        return;

    auto bounds = getLocalBounds().toFloat().reduced (4.0f);
    float width = bounds.getWidth();
    float height = bounds.getHeight();

    // Draw pitch reference lines (C notes)
    g.setColour (InKeyColours::glassBorder.withAlpha (0.12f));
    float cFreqs[] = { 130.81f, 261.63f, 523.25f }; // C3, C4, C5
    const char* cLabels[] = { "C3", "C4", "C5" };

    for (int i = 0; i < 3; ++i)
    {
        float y = frequencyToY (cFreqs[i], height);
        if (y >= 0.0f && y <= height)
        {
            g.drawHorizontalLine (static_cast<int> (bounds.getY() + y), bounds.getX(), bounds.getRight());
            g.setFont (juce::Font (10.0f));
            g.drawText (cLabels[i],
                        juce::Rectangle<float> (bounds.getX() + 2.0f, bounds.getY() + y - 12.0f,
                                                 30.0f, 12.0f),
                        juce::Justification::centredLeft);
        }
    }

    // Draw the pitch curve as connected segments
    // Color: bright where confident, dim where uncertain
    bool inSegment = false;
    float lastX = 0.0f;
    float lastY = 0.0f;

    for (size_t i = 0; i < pitchData.size(); ++i)
    {
        const auto& frame = pitchData[i];

        bool voiced = frame.frequency > 0.0f && frame.confidence >= confidenceThreshold;

        if (! voiced)
        {
            inSegment = false;
            continue;
        }

        float x = bounds.getX() + static_cast<float> (frame.time / totalDuration) * width;
        float y = bounds.getY() + frequencyToY (frame.frequency, height);

        // Color based on confidence: green (high) → yellow (medium) → red (low)
        float conf = frame.confidence;
        auto colour = juce::Colour::fromHSV (conf * 0.3f, 0.8f, 0.9f, 0.9f);
        g.setColour (colour);

        if (inSegment)
        {
            g.drawLine (lastX, lastY, x, y, 2.0f);
        }
        else
        {
            // Start a new segment — draw a dot at the first point
            g.fillEllipse (x - 1.5f, y - 1.5f, 3.0f, 3.0f);
        }

        lastX = x;
        lastY = y;
        inSegment = true;
    }
}

float PitchCurveComponent::frequencyToY (float freqHz, float height) const
{
    // Log scale: map frequency to 0..1 range (bottom = low, top = high)
    // Then invert because screen Y goes downward
    if (freqHz <= 0.0f)
        return height;

    float logMin = std::log2 (minDisplayFreq);
    float logMax = std::log2 (maxDisplayFreq);
    float logFreq = std::log2 (freqHz);

    float normalized = (logFreq - logMin) / (logMax - logMin);
    return height * (1.0f - normalized); // Invert: high freq = top
}
