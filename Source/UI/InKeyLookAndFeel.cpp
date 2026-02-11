// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 InKey contributors
// This file is part of InKey. See LICENSE for details.

#include "InKeyLookAndFeel.h"

InKeyLookAndFeel::InKeyLookAndFeel()
{
    interRegular = juce::Typeface::createSystemTypefaceFor (
        BinaryData::InterRegular_ttf,
        BinaryData::InterRegular_ttfSize);

    interBold = juce::Typeface::createSystemTypefaceFor (
        BinaryData::InterBold_ttf,
        BinaryData::InterBold_ttfSize);

    setColourScheme (getDarkColourScheme());

    setColour (juce::TextButton::textColourOffId, InKeyColours::textPrimary);
    setColour (juce::TextButton::textColourOnId, InKeyColours::textPrimary);
    setColour (juce::ComboBox::backgroundColourId, InKeyColours::glassBackground);
    setColour (juce::ComboBox::textColourId, InKeyColours::textPrimary);
    setColour (juce::ComboBox::outlineColourId, InKeyColours::glassBorder);
    setColour (juce::Label::textColourId, InKeyColours::textPrimary);
    setColour (juce::Slider::thumbColourId, juce::Colour (0xff5588cc));
    setColour (juce::Slider::trackColourId, juce::Colour (0xff3a3a5d));
    setColour (juce::Slider::backgroundColourId, juce::Colour (0xff1a1a30));
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xee141428));
    setColour (juce::TooltipWindow::textColourId, InKeyColours::textPrimary);
    setColour (juce::TooltipWindow::outlineColourId, InKeyColours::glassBorder);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xf0141428));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0x20ffffff));
    setColour (juce::PopupMenu::textColourId, InKeyColours::textPrimary);
    setColour (juce::PopupMenu::highlightedTextColourId, InKeyColours::textPrimary);
}

juce::Typeface::Ptr InKeyLookAndFeel::getTypefaceForFont (const juce::Font& font)
{
    if (font.isBold())
        return interBold;
    return interRegular;
}

void InKeyLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                              const juce::Colour& backgroundColour,
                                              bool isHighlighted, bool isDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    float cornerRadius = bounds.getHeight() * 0.4f;

    auto baseColour = backgroundColour;
    if (isDown)
        baseColour = baseColour.brighter (0.15f);
    else if (isHighlighted)
        baseColour = baseColour.brighter (0.08f);

    if (! button.isEnabled())
        baseColour = baseColour.withAlpha (0.4f);

    // Gradient fill
    juce::ColourGradient gradient (
        baseColour.brighter (0.15f), bounds.getX(), bounds.getY(),
        baseColour.darker (0.15f),   bounds.getX(), bounds.getBottom(),
        false);
    g.setGradientFill (gradient);
    g.fillRoundedRectangle (bounds, cornerRadius);

    // Glass border
    g.setColour (InKeyColours::glassBorder);
    g.drawRoundedRectangle (bounds, cornerRadius, 1.0f);

    // Hover: inner glow
    if (isHighlighted && button.isEnabled())
    {
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawRoundedRectangle (bounds.reduced (1.0f), cornerRadius - 1.0f, 1.0f);
    }
}

void InKeyLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                        bool /*isHighlighted*/, bool /*isDown*/)
{
    auto bounds = button.getLocalBounds().toFloat();
    auto colour = button.findColour (button.getToggleState()
                                         ? juce::TextButton::textColourOnId
                                         : juce::TextButton::textColourOffId);

    if (! button.isEnabled())
        colour = colour.withAlpha (0.5f);

    g.setColour (colour);
    g.setFont (juce::Font (13.0f));
    g.drawText (button.getButtonText(), bounds, juce::Justification::centred, false);
}

void InKeyLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                                          const juce::Slider::SliderStyle /*style*/, juce::Slider& slider)
{
    float fx = static_cast<float> (x);
    float fy = static_cast<float> (y);
    float fw = static_cast<float> (width);
    float fh = static_cast<float> (height);

    auto trackBounds = juce::Rectangle<float> (fx, fy + fh * 0.5f - 3.0f, fw, 6.0f);

    // Track background
    g.setColour (slider.findColour (juce::Slider::backgroundColourId));
    g.fillRoundedRectangle (trackBounds, 3.0f);

    // Filled portion
    float centerX = fx + fw * 0.5f;
    auto fillRect = trackBounds;

    if (slider.getMinimum() < 0 && slider.getMaximum() > 0)
    {
        float fillStart = juce::jmin (centerX, sliderPos);
        float fillEnd = juce::jmax (centerX, sliderPos);
        fillRect = juce::Rectangle<float> (fillStart, trackBounds.getY(),
                                            fillEnd - fillStart, trackBounds.getHeight());
    }
    else
    {
        fillRect.setWidth (sliderPos - fillRect.getX());
    }

    auto thumbColour = slider.findColour (juce::Slider::thumbColourId);
    g.setColour (thumbColour.withAlpha (0.5f));
    g.fillRoundedRectangle (fillRect, 3.0f);

    // Thumb
    float thumbSize = 14.0f;
    float thumbY = fy + fh * 0.5f;
    auto thumbBounds = juce::Rectangle<float> (sliderPos - thumbSize * 0.5f,
                                                 thumbY - thumbSize * 0.5f,
                                                 thumbSize, thumbSize);

    juce::ColourGradient thumbGradient (
        thumbColour.brighter (0.3f), thumbBounds.getCentreX(), thumbBounds.getY(),
        thumbColour.darker (0.2f),   thumbBounds.getCentreX(), thumbBounds.getBottom(),
        false);
    g.setGradientFill (thumbGradient);
    g.fillEllipse (thumbBounds);

    g.setColour (InKeyColours::glassBorder);
    g.drawEllipse (thumbBounds, 1.0f);
}

void InKeyLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height,
                                      bool isButtonDown, int, int, int, int,
                                      juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float> (0, 0, static_cast<float> (width), static_cast<float> (height));
    float cornerRadius = 6.0f;

    auto bgColour = box.findColour (juce::ComboBox::backgroundColourId);
    if (isButtonDown)
        bgColour = bgColour.brighter (0.1f);

    g.setColour (bgColour);
    g.fillRoundedRectangle (bounds, cornerRadius);

    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (bounds.reduced (0.5f), cornerRadius, 1.0f);

    // Down arrow
    float arrowSize = 8.0f;
    float arrowX = static_cast<float> (width) - 20.0f;
    float arrowY = static_cast<float> (height) * 0.5f - arrowSize * 0.25f;

    juce::Path arrow;
    arrow.addTriangle (arrowX, arrowY,
                       arrowX + arrowSize, arrowY,
                       arrowX + arrowSize * 0.5f, arrowY + arrowSize * 0.5f);
    g.setColour (InKeyColours::textSecondary);
    g.fillPath (arrow);
}

void InKeyLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    g.fillAll (label.findColour (juce::Label::backgroundColourId));

    auto textArea = label.getLocalBounds().toFloat();
    g.setColour (label.findColour (juce::Label::textColourId));
    g.setFont (label.getFont());
    g.drawText (label.getText(), textArea, label.getJustificationType(), true);
}

void InKeyLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text,
                                     int width, int height)
{
    auto bounds = juce::Rectangle<float> (0, 0, static_cast<float> (width), static_cast<float> (height));

    g.setColour (juce::Colour (0xee141428));
    g.fillRoundedRectangle (bounds, 6.0f);

    g.setColour (InKeyColours::glassBorder);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

    g.setColour (InKeyColours::textPrimary);
    g.setFont (juce::Font (13.0f));
    g.drawText (text, bounds.reduced (8, 4), juce::Justification::centredLeft);
}

void InKeyLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    auto bounds = juce::Rectangle<float> (0, 0, static_cast<float> (width), static_cast<float> (height));

    g.setColour (juce::Colour (0xf0141428));
    g.fillRoundedRectangle (bounds, 6.0f);

    g.setColour (InKeyColours::glassBorder);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);
}

void InKeyLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                                           bool isSeparator, bool isActive, bool isHighlighted,
                                           bool isTicked, bool /*hasSubMenu*/,
                                           const juce::String& text, const juce::String& /*shortcutKeyText*/,
                                           const juce::Drawable* /*icon*/, const juce::Colour* /*textColour*/)
{
    if (isSeparator)
    {
        g.setColour (InKeyColours::glassBorder);
        g.fillRect (area.reduced (5, 0).withHeight (1).withY (area.getCentreY()));
        return;
    }

    if (isHighlighted && isActive)
    {
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRoundedRectangle (area.toFloat().reduced (2, 1), 4.0f);
    }

    g.setColour (isActive ? InKeyColours::textPrimary : InKeyColours::textDim);
    g.setFont (juce::Font (14.0f));
    g.drawText (text, area.reduced (10, 0), juce::Justification::centredLeft);

    if (isTicked)
    {
        g.setColour (InKeyColours::accentPlay);
        g.fillEllipse (static_cast<float> (area.getX()) + 4.0f,
                       static_cast<float> (area.getCentreY()) - 2.5f, 5.0f, 5.0f);
    }
}

void InKeyLookAndFeel::drawGlassPanel (juce::Graphics& g, juce::Rectangle<float> area,
                                        float cornerRadius, bool hasTopHighlight)
{
    juce::ColourGradient bgGradient (
        InKeyColours::glassBackground.brighter (0.05f),
        area.getX(), area.getY(),
        InKeyColours::glassBackground.darker (0.05f),
        area.getX(), area.getBottom(),
        false);
    g.setGradientFill (bgGradient);
    g.fillRoundedRectangle (area, cornerRadius);

    if (hasTopHighlight)
    {
        g.setColour (InKeyColours::glassHighlight);
        if (cornerRadius > 0.0f)
        {
            auto highlightRect = area.withHeight (1.0f).reduced (cornerRadius, 0);
            g.fillRect (highlightRect);
        }
        else
        {
            g.fillRect (area.withHeight (1.0f));
        }
    }

    g.setColour (InKeyColours::glassBorder);
    g.drawRoundedRectangle (area, cornerRadius, 1.0f);
}
