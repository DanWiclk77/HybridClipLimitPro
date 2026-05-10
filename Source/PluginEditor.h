#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class LevelMeter : public juce::Component {
public:
    void setLevel(float v) { level = v; }
    void paint(juce::Graphics& g) override {
        auto area = getLocalBounds();
        g.setColour(juce::Colours::black);
        g.fillRect(area);
        
        float h = area.getHeight() * level;
        g.setColour(color);
        g.fillRect(area.withTop(area.getHeight() - (int)h));
    }
    juce::Colour color = juce::Colours::blue;
private:
    float level = 0.0f;
};

class LoudnessCatalystAudioProcessorEditor : public juce::AudioProcessorEditor, public juce::Timer {
public:
    LoudnessCatalystAudioProcessorEditor (LoudnessCatalystAudioProcessor& p) : AudioProcessorEditor (&p), processor (p) {
        // Setup Clipper Sliders
        setupSlider(clipGainSlider, clipGainLabel, "Clipper Gain", " dB");
        clipGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "clipGain", clipGainSlider);
        
        setupSlider(clipKneeSlider, clipKneeLabel, "Knee", "");
        clipKneeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "clipKnee", clipKneeSlider);

        // Setup Limiter Sliders
        setupSlider(limitThresholdSlider, limitThresholdLabel, "Limit Thr", " dB");
        limitThresholdAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "limitThreshold", limitThresholdSlider);
        
        setupSlider(lookAheadSlider, lookAheadLabel, "Look-Ahead", " ms");
        lookAheadAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "lookAhead", lookAheadSlider);

        addAndMakeVisible(targetCombo);
        targetCombo.addItemList({"Smart Target: Off", "-14 LUFS", "-13 LUFS", "-12 LUFS", "-11 LUFS", "-10 LUFS", "-9 LUFS", "-8 LUFS", "-7 LUFS", "-6 LUFS", "-5 LUFS", "-4 LUFS", "-3 LUFS", "-2 LUFS"}, 1);
        targetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, "targetLufs", targetCombo);

        inMeter.color = juce::Colours::cyan;
        outMeter.color = juce::Colours::lime;
        grMeter.color = juce::Colours::red;

        addAndMakeVisible(inMeter);
        addAndMakeVisible(outMeter);
        addAndMakeVisible(grMeter);

        setSize (650, 450);
        startTimerHz(30); // 30 FPS UI Update
    }

    void timerCallback() override {
        // Smoothing ballistics for human eyes
        auto updateLevel = [](float current, float next) {
            if (next > current) return next; // Instant rise
            return current * 0.9f;          // Smooth decay
        };

        inLevel = updateLevel(inLevel, processor.getInLevel());
        outLevel = updateLevel(outLevel, processor.getOutLevel());
        grLevel = updateLevel(grLevel, processor.getGRLevel());

        inMeter.setLevel(inLevel);
        outMeter.setLevel(outLevel);
        grMeter.setLevel(grLevel);
        repaint();
    }

    void paint (juce::Graphics& g) override {
        // Background Master
        g.fillAll (juce::Colour (0xFF0A0A0C));
        
        g.setColour (juce::Colours::white.withAlpha(0.7f));
        g.setFont (juce::Font ("Inter", 24.0f, juce::Font::bold));
        g.drawText ("LOUDNESS CATALYST", 20, 20, 300, 30, juce::Justification::left);
        
        g.setColour (juce::Colours::blueviolet);
        g.setFont (juce::Font (10.0f, juce::Font::bold));
        g.drawText ("ANALOG MASTER ENGINE", 22, 45, 300, 15, juce::Justification::left);

        // Draw section headers
        g.setColour (juce::Colours::darkgrey);
        g.drawLine (20, 70, 580, 70, 1.0f);

        // Meter Labels
        g.setColour (juce::Colours::grey);
        g.setFont (8.0f);
        auto meterArea = getLocalBounds().removeFromRight(100).reduced(10);
        g.drawText("IN", meterArea.getX(), meterArea.getBottom() + 5, 20, 10, juce::Justification::centred);
        g.drawText("GR", meterArea.getX() + 25, meterArea.getBottom() + 5, 20, 10, juce::Justification::centred);
        g.drawText("OUT", meterArea.getX() + 50, meterArea.getBottom() + 5, 20, 10, juce::Justification::centred);
    }

    void resized() override {
        auto area = getLocalBounds().reduced (20);
        
        auto topBar = area.removeFromTop(60);
        targetCombo.setBounds(topBar.removeFromRight(150).reduced(10));

        auto clipperArea = area.removeFromLeft (area.getWidth() / 2).reduced(10);
        auto limiterArea = area.reduced(10);

        // Layout Clipper
        clipGainSlider.setBounds (clipperArea.removeFromTop (100));
        clipKneeSlider.setBounds (clipperArea.removeFromTop (100));

        // Layout Limiter
        limitThresholdSlider.setBounds (limiterArea.removeFromTop (100));
        lookAheadSlider.setBounds (limiterArea.removeFromTop (100));

        // Layout Meters
        auto meterArea = getLocalBounds().removeFromRight(100).reduced(10);
        inMeter.setBounds(meterArea.removeFromLeft(20));
        grMeter.setBounds(meterArea.removeFromLeft(20));
        outMeter.setBounds(meterArea.removeFromLeft(20));
    }

private:
    void setupSlider(juce::Slider& s, juce::Label& l, juce::String name, juce::String suffix) {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
        s.setTextValueSuffix (suffix);
        addAndMakeVisible (s);

        l.setText (name, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (l);
        l.attachToComponent (&s, false);
    }

    LoudnessCatalystAudioProcessor& processor;
    float inLevel = 0.0f, outLevel = 0.0f, grLevel = 0.0f;

    juce::Slider clipGainSlider, clipKneeSlider;
    juce::Slider limitThresholdSlider, lookAheadSlider;
    
    juce::Label clipGainLabel, clipKneeLabel;
    juce::Label limitThresholdLabel, lookAheadLabel;
    
    juce::ComboBox targetCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> targetAttachment;

    LevelMeter inMeter, outMeter, grMeter;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> clipGainAttachment, clipKneeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> limitThresholdAttachment, lookAheadAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoudnessCatalystAudioProcessorEditor)
};
