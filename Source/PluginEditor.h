#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class LoudnessCatalystAudioProcessorEditor : public juce::AudioProcessorEditor {
public:
    LoudnessCatalystAudioProcessorEditor (juce::AudioProcessor& p) : AudioProcessorEditor (&p) {
        setSize (800, 600);
    }
    void paint (juce::Graphics& g) override {
        g.fillAll (juce::Colours::black);
        g.setColour (juce::Colours::white);
        g.setFont (15.0f);
        g.drawFittedText ("Loudness Catalyst - Interface loading...", getLocalBounds(), juce::Justification::centred, 1);
    }
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoudnessCatalystAudioProcessorEditor)
};
