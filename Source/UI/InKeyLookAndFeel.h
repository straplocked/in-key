// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 InKey contributors
// This file is part of InKey. See LICENSE for details.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <BinaryData.h>

namespace InKeyColours
{
    // === Background gradient ===
    const juce::Colour backgroundTop    { 0xff0d0d1a };
    const juce::Colour backgroundBottom { 0xff1a0d2e };

    // === Glass panel ===
    const juce::Colour glassBackground  { 0x18ffffff };
    const juce::Colour glassBorder      { 0x30ffffff };
    const juce::Colour glassHighlight   { 0x0affffff };

    // === Text ===
    const juce::Colour textPrimary      { 0xffe8e8f0 };
    const juce::Colour textSecondary    { 0xff8888a0 };
    const juce::Colour textDim          { 0xff555570 };

    // === Button accents ===
    const juce::Colour accentCapture    { 0xffff4466 };
    const juce::Colour accentPlay       { 0xff44cc66 };
    const juce::Colour accentUndo       { 0xff4488ff };
    const juce::Colour accentRedo       { 0xff44aaff };
    const juce::Colour accentQuantize   { 0xffaa66ff };
    const juce::Colour accentReset      { 0xffff6644 };
    const juce::Colour accentAlign      { 0xff44cccc };

    // === Chromatic note colours (by pitch class 0-11) ===
    const juce::Colour noteChromatic[12] = {
        juce::Colour (0xffff4444),   // C  - red
        juce::Colour (0xffff6633),   // C# - red-orange
        juce::Colour (0xffff9922),   // D  - orange
        juce::Colour (0xffddaa00),   // D# - gold
        juce::Colour (0xffcccc00),   // E  - yellow
        juce::Colour (0xff44bb44),   // F  - green
        juce::Colour (0xff22aa88),   // F# - teal
        juce::Colour (0xff22bbdd),   // G  - cyan
        juce::Colour (0xff3366ff),   // G# - blue
        juce::Colour (0xff4444ff),   // A  - indigo
        juce::Colour (0xff9944ff),   // A# - purple
        juce::Colour (0xffbb44dd),   // B  - violet
    };

    // === Selection / interaction ===
    const juce::Colour selectedGlow     { 0xccffffff };
    const juce::Colour hoverGlow        { 0x40ffffff };

    // === Piano roll ===
    const juce::Colour gridLine         { 0xff1e1e35 };
    const juce::Colour gridLineBeat     { 0xff2a2a4d };
    const juce::Colour gridLineOctave   { 0xff3a3a6d };
    const juce::Colour blackKeyBg       { 0xff121225 };
    const juce::Colour pianoKeySidebar  { 0xff1a1a30 };
    const juce::Colour playheadColour   { 0xffff4466 };

    inline juce::Colour getNoteColour (int midiNote)
    {
        return noteChromatic[midiNote % 12];
    }

    inline juce::Colour getNoteColour (int midiNote, float confidence)
    {
        auto base = noteChromatic[midiNote % 12];
        float brightness = 0.6f + confidence * 0.4f;
        return base.withMultipliedBrightness (brightness);
    }
}

class InKeyLookAndFeel : public juce::LookAndFeel_V4
{
public:
    InKeyLookAndFeel();
    ~InKeyLookAndFeel() override = default;

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font& font) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool shouldDrawButtonAsHighlighted,
                         bool shouldDrawButtonAsDown) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           const juce::Slider::SliderStyle, juce::Slider&) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override;

    void drawLabel (juce::Graphics&, juce::Label&) override;

    void drawTooltip (juce::Graphics&, const juce::String& text,
                      int width, int height) override;

    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;

    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area,
                            bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu,
                            const juce::String& text, const juce::String& shortcutKeyText,
                            const juce::Drawable* icon, const juce::Colour* textColour) override;

    static void drawGlassPanel (juce::Graphics& g, juce::Rectangle<float> area,
                                float cornerRadius = 8.0f,
                                bool hasTopHighlight = true);

private:
    juce::Typeface::Ptr interRegular;
    juce::Typeface::Ptr interBold;
};
