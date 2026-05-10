#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

// Custom Meter Component
class LevelMeter : public juce::Component {
public:
    void setLevel(float v) { level = v; }
    void setIsGR(bool is) { isGR = is; }
    void paint(juce::Graphics& g) override {
        auto area = getLocalBounds();
        g.setColour(juce::Colour(0xFF151518));
        g.fillRect(area);
        
        float h;
        if (isGR) {
            float db = juce::jlimit(-24.0f, 0.0f, level);
            h = area.getHeight() * (db / -24.0f);
            g.setColour(color);
            g.fillRect(area.removeFromTop((int)h));
        } else {
            h = area.getHeight() * juce::jlimit(0.0f, 1.2f, level) / 1.2f;
            g.setColour(color);
            g.fillRect(area.withTop(area.getHeight() - (int)h));
        }

        g.setColour(juce::Colours::white.withAlpha(0.05f));
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
        // --- COL 1: SATURATION / WARMTH ---
        setupBypass(warmthBypassBtn, "WARMTH BYPASS", "warmthBypass");
        setupSlider(warmthSlider, warmthLabel, "WARMTH", "%");
        warmthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "warmth", warmthSlider);
        
        addAndMakeVisible(targetCombo);
        targetCombo.addItemList({"Smart Target: Off", "-14 LUFS", "-13 LUFS", "-12 LUFS", "-11 LUFS", "-10 LUFS", "-9 LUFS", "-8 LUFS", "-7 LUFS", "-6 LUFS", "-5 LUFS", "-4 LUFS", "-3 LUFS", "-2 LUFS"}, 1);
        targetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, "targetLufs", targetCombo);

        // --- COL 2: CLIPPER ---
        setupBypass(clipBypassBtn, "CLIP BYPASS", "clipBypass");
        setupSlider(clipGainSlider, clipGainLabel, "CLIP GAIN", " dB");
        clipGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "clipGain", clipGainSlider);
        
        setupSlider(clipKneeSlider, clipKneeLabel, "CLIP KNEE", "");
        clipKneeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "clipKnee", clipKneeSlider);

        setupSlider(clipCeilingSlider, clipCeilingLabel, "CLIP CEIL", " dB");
        clipCeilingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "clipCeiling", clipCeilingSlider);

        // --- COL 3: LIMITER ---
        setupBypass(limitBypassBtn, "LIMIT BYPASS", "limitBypass");
        setupSlider(limitThresholdSlider, limitThresholdLabel, "THRESHOLD", " dB");
        limitThresholdAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "limitThreshold", limitThresholdSlider);
        
        setupSlider(releaseSlider, releaseLabel, "RELEASE", " ms");
        releaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "release", releaseSlider);

        setupSlider(limitCeilingSlider, limitCeilingLabel, "CEILING", " dB");
        limitCeilingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "limitCeiling", limitCeilingSlider);

        // --- METERS ---
        inMeter.color = juce::Colours::cyan;
        outMeter.color = juce::Colours::lime;
        clipGRMeter.color = juce::Colours::orange;
        clipGRMeter.setIsGR(true);
        limitGRMeter.color = juce::Colours::red;
        limitGRMeter.setIsGR(true);

        addAndMakeVisible(inMeter);
        addAndMakeVisible(outMeter);
        addAndMakeVisible(clipGRMeter);
        addAndMakeVisible(limitGRMeter);

        addAndMakeVisible(resetBtn);
        resetBtn.setButtonText("RESET INT");
        resetBtn.onClick = [this] { processor.resetIntegrated(); };

        setSize (920, 520);
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
        clipGRLevel = updateGR(clipGRLevel, processor.getClipGR());
        limitGRLevel = updateGR(limitGRLevel, processor.getLimiterGR());

        inMeter.setLevel(inLevel);
        outMeter.setLevel(outLevel);
        clipGRMeter.setLevel(clipGRLevel);
        limitGRMeter.setLevel(limitGRLevel);
        
        m_lufs = processor.getMLUFS();
        s_lufs = processor.getSLUFS();
        i_lufs = processor.getILUFS();

        repaint();
    }

    void paint (juce::Graphics& g) override {
        g.fillAll (juce::Colour (0xFF08080A));
        auto area = getLocalBounds();
        auto header = area.removeFromTop(70).reduced(20, 0);
        
        g.setColour (juce::Colours::white.withAlpha(0.9f));
        g.setFont (juce::Font ("Inter", 26.0f, juce::Font::bold));
        g.drawText ("LOUDNESS CATALYST", header.getX(), header.getY() + 5, 400, 30, juce::Justification::left);
        g.setColour (juce::Colours::blueviolet);
        g.setFont (juce::Font ("Inter", 11.0f, juce::Font::bold));
        g.drawText ("ANALOG MASTERING TOOLKIT v1.0", header.getX() + 2, header.getY() + 32, 300, 15, juce::Justification::left);

        auto contentArea = area.reduced(15);
        auto meterArea = contentArea.removeFromRight(220); // More space for LUFS
        
        float colWidth = contentArea.getWidth() / 3.0f;
        g.setColour(juce::Colours::white.withAlpha(0.03f));
        g.fillRect(contentArea.removeFromLeft((int)colWidth).reduced(5)); // Warmth
        g.fillRect(contentArea.removeFromLeft((int)colWidth).reduced(5)); // Clipper
        g.fillRect(contentArea.reduced(5)); // Limiter

        // Meter labels & numerical db/gr
        auto readoutY = meterArea.getBottom() - 35;
        g.setColour(juce::Colours::grey);
        g.setFont(juce::Font("Inter", 10.0f, juce::Font::plain));
        
        auto formatDB = [](float v) { return juce::String(juce::Decibels::gainToDecibels(v), 1); };
        auto formatGR = [](float v) { return juce::String(v, 1); };

        g.drawText("IN", meterArea.getX(), readoutY - 15, 35, 15, juce::Justification::centred);
        g.drawText("C-GR", meterArea.getX() + 40, readoutY - 15, 35, 15, juce::Justification::centred);
        g.drawText("L-GR", meterArea.getX() + 80, readoutY - 15, 35, 15, juce::Justification::centred);
        g.drawText("OUT", meterArea.getX() + 120, readoutY - 15, 35, 15, juce::Justification::centred);

        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.drawText(formatDB(inLevel), meterArea.getX(), readoutY, 35, 15, juce::Justification::centred);
        g.drawText(formatGR(clipGRLevel), meterArea.getX() + 40, readoutY, 35, 15, juce::Justification::centred);
        g.drawText(formatGR(limitGRLevel), meterArea.getX() + 80, readoutY, 35, 15, juce::Justification::centred);
        g.drawText(formatDB(outLevel), meterArea.getX() + 120, readoutY, 35, 15, juce::Justification::centred);

        // LUFS Indicators
        auto lufsArea = meterArea.removeFromRight(50);
        g.setColour(juce::Colours::white.withAlpha(0.1f));
        g.drawVerticalLine(lufsArea.getX() - 5, (float)lufsArea.getY() + 20, (float)lufsArea.getBottom() - 50);

        g.setColour(juce::Colours::lightgrey);
        g.setFont(juce::Font("Inter", 11.0f, juce::Font::bold));
        
        auto drawLUFS = [&](juce::String label, float val, int yOffset) {
            g.drawText(label, lufsArea.getX(), lufsArea.getY() + yOffset, 45, 15, juce::Justification::centred);
            juce::String s = (val < -90) ? "---" : juce::String(val, 1);
            g.setColour(juce::Colours::white);
            g.drawText(s, lufsArea.getX(), lufsArea.getY() + yOffset + 18, 45, 20, juce::Justification::centred);
        };

        drawLUFS("MOM", m_lufs, 30);
        drawLUFS("ST", s_lufs, 90);
        drawLUFS("INT", i_lufs, 150);
    }

    void resized() override {
        auto area = getLocalBounds();
        area.removeFromTop(70);
        auto contentArea = area.reduced(15);
        
        auto meterArea = contentArea.removeFromRight(220).reduced(5, 40);
        auto lufsDisplayArea = meterArea.removeFromRight(50);
        resetBtn.setBounds(lufsDisplayArea.getX() - 5, lufsDisplayArea.getY() + 210, 60, 25);

        inMeter.setBounds(meterArea.removeFromLeft(40).reduced(12, 0));
        clipGRMeter.setBounds(meterArea.removeFromLeft(40).reduced(12, 0));
        limitGRMeter.setBounds(meterArea.removeFromLeft(40).reduced(12, 0));
        outMeter.setBounds(meterArea.removeFromLeft(40).reduced(12, 0));

        float colWidth = contentArea.getWidth() / 3.0f;
        auto warmthArea = contentArea.removeFromLeft((int)colWidth).reduced(10);
        auto clipperArea = contentArea.removeFromLeft((int)colWidth).reduced(10);
        auto limiterArea = contentArea.reduced(10);

        // SATURATION LAYOUT
        warmthBypassBtn.setBounds(warmthArea.removeFromTop(30).reduced(10, 0));
        warmthArea.removeFromTop(10);
        warmthSlider.setBounds(warmthArea.removeFromTop(150));
        warmthArea.removeFromTop(30);
        targetCombo.setBounds(warmthArea.removeFromTop(30).reduced(15, 0));

        // CLIPPER LAYOUT
        clipBypassBtn.setBounds(clipperArea.removeFromTop(30).reduced(10, 0));
        clipAreaLabel.setBounds(clipperArea.removeFromTop(10)); // dummy for space
        clipGainSlider.setBounds(clipperArea.removeFromTop(110));
        clipKneeSlider.setBounds(clipperArea.removeFromTop(110));
        clipCeilingSlider.setBounds(clipperArea.removeFromTop(110));

        // LIMITER LAYOUT
        limitBypassBtn.setBounds(limiterArea.removeFromTop(30).reduced(10, 0));
        limitAreaLabel.setBounds(limiterArea.removeFromTop(10)); // dummy for space
        limitThresholdSlider.setBounds(limiterArea.removeFromTop(110));
        releaseSlider.setBounds(limiterArea.removeFromTop(110));
        limitCeilingSlider.setBounds(limiterArea.removeFromTop(110));
    }

private:
    void setupSlider(juce::Slider& s, juce::Label& l, juce::String name, juce::String suffix) {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
        s.setTextValueSuffix (suffix);
        s.setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::blueviolet);
        s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (s);
        
        l.setText (name, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        l.setFont(juce::Font("Inter", 12.0f, juce::Font::bold));
        l.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        addAndMakeVisible (l);
        l.attachToComponent (&s, false);
    }

    void setupBypass(juce::ToggleButton& b, juce::String name, juce::String paramId) {
        b.setButtonText(name);
        addAndMakeVisible(b);
        b.setColour(juce::ToggleButton::textColourId, juce::Colours::grey);
        b.setColour(juce::ToggleButton::tickColourId, juce::Colours::blueviolet);
        attachments.add(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, paramId, b));
    }

    LoudnessCatalystAudioProcessor& processor;
    float inLevel = 0.0f, outLevel = 0.0f, clipGRLevel = 0.0f, limitGRLevel = 0.0f;
    float m_lufs = -100.0f, s_lufs = -100.0f, i_lufs = -100.0f;

    juce::Slider warmthSlider;
    juce::Slider clipGainSlider, clipKneeSlider, clipCeilingSlider;
    juce::Slider limitThresholdSlider, releaseSlider, limitCeilingSlider;
    
    juce::Label warmthLabel;
    juce::Label clipGainLabel, clipKneeLabel, clipCeilingLabel;
    juce::Label limitThresholdLabel, releaseLabel, limitCeilingLabel;
    juce::Label clipAreaLabel, limitAreaLabel;

    juce::ToggleButton warmthBypassBtn, clipBypassBtn, limitBypassBtn;
    juce::TextButton resetBtn;
    
    juce::ComboBox targetCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> targetAttachment;

    LevelMeter inMeter, outMeter, clipGRMeter, limitGRMeter;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> warmthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> clipGainAttachment, clipKneeAttachment, clipCeilingAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> limitThresholdAttachment, releaseAttachment, limitCeilingAttachment;
    
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> attachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoudnessCatalystAudioProcessorEditor)
};
