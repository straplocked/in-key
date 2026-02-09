#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Audio/AudioCapture.h"

class WaveformComponent : public juce::Component,
                          public AudioCapture::Listener
{
public:
    WaveformComponent (AudioCapture& capture);
    ~WaveformComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    // AudioCapture::Listener
    void captureUpdated() override;

private:
    AudioCapture& audioCapture;

    // Cached waveform path for efficient repainting
    juce::Path waveformPath;
    bool pathDirty = true;

    void rebuildPath();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformComponent)
};
