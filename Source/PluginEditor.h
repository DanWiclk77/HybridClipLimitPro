#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class LevelMeter : public juce::Component {
public:
    void setLevel(float v) { level = v; }
    void setIsGR(bool is) { isGR = is; }
    void paint(juce::Graphics& g) override {
        auto area = getLocalBounds();
        g.setColour(juce::Colour(0xFF151518));
        g.fillRect(area);
        
        if (isGR) {
            // Gain Reduction: 0 dB to -24 dB range for display
            float db = juce::jlimit(-24.0f, 0.0f, level);
            float h = area.getHeight() * (db / -24.0f);
            g.setColour(color);
            g.fillRect(area.removeFromTop((int)h));
        } else {
            // Signal Level: Linear gain (0.0 to 1.5 for headroom)
            float h = area.getHeight() * juce::jlimit(0.0f, 1.2f, level) / 1.2f;
            g.setColour(color);
            g.fillRect(area.withTop(area.getHeight() - (int)h));
        }

        // Draw scale lines
        g.setColour(juce::Colours::white.withAlpha(0.1f));
        for (int i = 1; i < 4; ++i) {
            float y = area.getHeight() * (i / 4.0f);
            g.drawHorizontalLine((int)y, 0.0f, (float)area.getWidth());
        }
    }
    juce::Colour color = juce::Colours::blue;
private:
    float level = 0.0f;
    bool isGR = false;
};

class LoudnessCatalystAudioProcessorEditor : public juce::AudioProcessorEditor, public juce::Timer {
public:
    LoudnessCatalystAudioProcessorEditor (LoudnessCatalystAudioProcessor& p) : AudioProcessorEditor (&p), processor (p) {
        // --- CLIPPER ---
        setupSlider(clipGainSlider, clipGainLabel, "INPUT GAIN", " dB");
        clipGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "clipGain", clipGainSlider);
        
        setupSlider(clipKneeSlider, clipKneeLabel, "KNEE", "");
        clipKneeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "clipKnee", clipKneeSlider);

        // --- ENGINE ---
        setupSlider(warmthSlider, warmthLabel, "WARMTH", "%");
        warmthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "warmth", warmthSlider);

        addAndMakeVisible(targetCombo);
        targetCombo.addItemList({"Smart Target: Off", "-14 LUFS", "-13 LUFS", "-12 LUFS", "-11 LUFS", "-10 LUFS", "-9 LUFS", "-8 LUFS", "-7 LUFS", "-6 LUFS", "-5 LUFS", "-4 LUFS", "-3 LUFS", "-2 LUFS"}, 1);
        targetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, "targetLufs", targetCombo);

        // --- LIMITER ---
        setupSlider(limitThresholdSlider, limitThresholdLabel, "THRESHOLD", " dB");
        limitThresholdAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "limitThreshold", limitThresholdSlider);
        
        setupSlider(releaseSlider, releaseLabel, "RELEASE", " ms");
        releaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "release", releaseSlider);

        setupSlider(ceilingSlider, ceilingLabel, "CEILING", " dB");
        ceilingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "ceiling", ceilingSlider);

        // --- METERS ---
        inMeter.color = juce::Colours::cyan;
        outMeter.color = juce::Colours::lime;
        grMeter.color = juce::Colours::red;
        grMeter.setIsGR(true);

        addAndMakeVisible(inMeter);
        addAndMakeVisible(outMeter);
        addAndMakeVisible(grMeter);

        setSize (750, 480);
        startTimerHz(30);
    }

    void timerCallback() override {
        auto updateLevel = [](float current, float next) {
            if (next > current) return next;
            return current * 0.85f;
        };
        auto updateGR = [](float current, float next) {
            if (next < current) return next;
            return current * 0.94f;
        };

        inLevel = updateLevel(inLevel, processor.getInLevel());
        outLevel = updateLevel(outLevel, processor.getOutLevel());
        grLevel = updateGR(grLevel, processor.getGRLevel());

        inMeter.setLevel(inLevel);
        outMeter.setLevel(outLevel);
        grMeter.setLevel(grLevel);
        repaint();
    }

    void paint (juce::Graphics& g) override {
        g.fillAll (juce::Colour (0xFF0A0A0C));
        
        auto area = getLocalBounds();
        auto header = area.removeFromTop(80).reduced(20, 0);
        
        g.setColour (juce::Colours::white.withAlpha(0.9f));
        g.setFont (juce::Font ("Inter", 24.0f, juce::Font::bold));
        g.drawText ("LOUDNESS CATALYST", header.getX(), header.getY() + 10, 300, 30, juce::Justification::left);
        
        g.setColour (juce::Colours::blueviolet);
        g.setFont (juce::Font ("Inter", 10.0f, juce::Font::bold));
        g.drawText ("ANALOG MASTERING CHAIN", header.getX() + 2, header.getY() + 38, 300, 15, juce::Justification::left);

        // Section Backgrounds
        g.setColour(juce::Colours::white.withAlpha(0.03f));
        auto contentArea = area.reduced(20);
        auto meterArea = contentArea.removeFromRight(120);
        
        float colWidth = contentArea.getWidth() / 3.0f;
        g.fillRect(contentArea.removeFromLeft((int)colWidth).reduced(5)); // Clipper
        g.fillRect(contentArea.removeFromLeft((int)colWidth).reduced(5)); // Engine
        g.fillRect(contentArea.reduced(5)); // Limiter

        // Meter Labels
        g.setColour (juce::Colours::grey);
        g.setFont (juce::Font("Inter", 10.0f, juce::Font::plain));
        auto labelY = meterArea.getBottom() - 20;
        g.drawText("IN", meterArea.getX() + 10, labelY, 25, 20, juce::Justification::centred);
        g.drawText("GR", meterArea.getX() + 45, labelY, 25, 20, juce::Justification::centred);
        g.drawText("OUT", meterArea.getX() + 80, labelY, 25, 20, juce::Justification::centred);
    }

    void resized() override {
        auto area = getLocalBounds();
        area.removeFromTop(80); // Header
        auto contentArea = area.reduced(20);
        
        auto meterArea = contentArea.removeFromRight(120).reduced(10, 30);
        inMeter.setBounds(meterArea.removeFromLeft(30).reduced(2, 0));
        grMeter.setBounds(meterArea.removeFromLeft(30).reduced(2, 0));
        outMeter.setBounds(meterArea.removeFromLeft(30).reduced(2, 0));

        float colWidth = contentArea.getWidth() / 3.0f;
        auto clipperArea = contentArea.removeFromLeft((int)colWidth).reduced(10);
        auto engineArea = contentArea.removeFromLeft((int)colWidth).reduced(10);
        auto limiterArea = contentArea.reduced(10);

        // Clipper Layout
        clipGainSlider.setBounds(clipperArea.removeFromTop(160));
        clipKneeSlider.setBounds(clipperArea.removeFromTop(160));

        // Engine Layout
        warmthSlider.setBounds(engineArea.removeFromTop(160));
        engineArea.removeFromTop(20); // Spacer
        targetCombo.setBounds(engineArea.removeFromTop(30).reduced(10, 0));

        // Limiter Layout
        limitThresholdSlider.setBounds(limiterArea.removeFromTop(120));
        releaseSlider.setBounds(limiterArea.removeFromTop(120));
        ceilingSlider.setBounds(limiterArea.removeFromTop(120));
    }

private:
    void setupSlider(juce::Slider& s, juce::Label& l, juce::String name, juce::String suffix) {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
        s.setTextValueSuffix (suffix);
        s.setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::blueviolet);
        s.setColour(juce::Slider::thumbColourId, juce::Colours::white);
        addAndMakeVisible (s);

        l.setText (name, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        l.setFont(juce::Font("Inter", 12.0f, juce::Font::bold));
        l.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        addAndMakeVisible (l);
        l.attachToComponent (&s, false);
    }

    LoudnessCatalystAudioProcessor& processor;
    float inLevel = 0.0f, outLevel = 0.0f, grLevel = 0.0f;

    juce::Slider clipGainSlider, clipKneeSlider;
    juce::Slider warmthSlider;
    juce::Slider limitThresholdSlider, releaseSlider, ceilingSlider;
    
    juce::Label clipGainLabel, clipKneeLabel;
    juce::Label warmthLabel;
    juce::Label limitThresholdLabel, releaseLabel, ceilingLabel;
    
    juce::ComboBox targetCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> targetAttachment;

    LevelMeter inMeter, outMeter, grMeter;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> clipGainAttachment, clipKneeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> warmthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> limitThresholdAttachment, releaseAttachment, ceilingAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoudnessCatalystAudioProcessorEditor)
};
