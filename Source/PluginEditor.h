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

// Scrolling Waveform Visualizer
class ScrollingVisualizer : public juce::Component {
public:
    void pushValues(float in, float out) {
        historyIn.add(juce::jlimit(0.0f, 2.0f, in));
        historyOut.add(juce::jlimit(0.0f, 2.0f, out));
        if (historyIn.size() > 150) {
            historyIn.remove(0);
            historyOut.remove(0);
        }
    }

    void paint(juce::Graphics& g) override {
        auto area = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xFF0C0C0E));
        g.fillRoundedRectangle(area, 4.0f);
        
        if (historyIn.size() < 2) return;

        float midY = area.getCentreY();
        float halfH = area.getHeight() * 0.45f;
        float stepX = area.getWidth() / 150.0f;

        juce::Path inPath, outPath;
        
        for (int i = 0; i < historyIn.size(); ++i) {
            float x = i * stepX;
            float yIn = midY - (historyIn[i] * halfH);
            float yOut = midY - (historyOut[i] * halfH);
            
            if (i == 0) {
                inPath.startNewSubPath(x, yIn);
                outPath.startNewSubPath(x, yOut);
            } else {
                inPath.lineTo(x, yIn);
                outPath.lineTo(x, yOut);
            }
        }

        // Draw In (Red/Orange for clipped area)
        g.setColour(juce::Colours::red.withAlpha(0.4f));
        g.strokePath(inPath, juce::PathStrokeType(1.5f));
        
        // Draw Out
        g.setColour(color);
        g.strokePath(outPath, juce::PathStrokeType(1.5f));

        // Center line
        g.setColour(juce::Colours::white.withAlpha(0.1f));
        g.drawHorizontalLine((int)midY, 0.0f, area.getWidth());
    }
    
    juce::Colour color = juce::Colours::cyan;
private:
    juce::Array<float> historyIn, historyOut;
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

        addAndMakeVisible(oversamplingCombo);
        oversamplingCombo.addItemList({"Oversampling: Off", "Oversampling: 2x", "Oversampling: 4x", "Oversampling: 8x"}, 1);
        oversamplingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, "oversampling", oversamplingCombo);

        addAndMakeVisible(clipModeCombo);
        clipModeCombo.addItemList({"Mode: Hard", "Mode: Soft", "Mode: Analog"}, 1);
        clipModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, "clipMode", clipModeCombo);

        addAndMakeVisible(linkBtn);
        linkBtn.setButtonText("LINK");
        linkBtn.setTooltip("Link Gain and Ceiling (Auto-Gain calculation)");
        linkAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, "linkAutoGain", linkBtn);

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

        addAndMakeVisible(limitStyleCombo);
        limitStyleCombo.addItemList({"Transparent", "Punchy", "Dynamic", "Safe"}, 1);
        limitStyleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, "limitStyle", limitStyleCombo);

        addAndMakeVisible(truePeakBtn);
        truePeakBtn.setButtonText("TRUE PEAK");
        truePeakBtn.setColour(juce::ToggleButton::tickColourId, juce::Colours::blueviolet);
        truePeakAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, "truePeak", truePeakBtn);

        setupSlider(attackSlider, attackLabel, "ATTACK", " ms");
        attackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "attack", attackSlider);

        setupSlider(lookaheadSlider, lookaheadLabel, "LOOKAHEAD", " ms");
        lookaheadAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "lookahead", lookaheadSlider);

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
        
        clipVis.color = juce::Colours::lime;
        limitVis.color = juce::Colours::orange;
        addAndMakeVisible(clipVis);
        addAndMakeVisible(limitVis);

        addAndMakeVisible(resetBtn);
        resetBtn.setButtonText("RESET");
        resetBtn.setTooltip("Reset Integrated LUFS measurement");
        resetBtn.onClick = [this] { processor.resetIntegrated(); };

        setSize (1000, 680);
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

        clipVis.pushValues(processor.getClipVisIn(), processor.getClipVisOut());
        limitVis.pushValues(processor.getLimitVisIn(), processor.getLimitVisOut());
        
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
        g.setFont (juce::Font ("Inter", 28.0f, juce::Font::bold));
        g.drawText ("LOUDNESS CATALYST", header.getX(), header.getY() + 10, 400, 35, juce::Justification::left);
        g.setColour (juce::Colours::blueviolet);
        g.setFont (juce::Font ("Inter", 12.0f, juce::Font::bold));
        g.drawText ("ANALOG MASTERING TOOLKIT v1.0", header.getX() + 2, header.getY() + 42, 300, 15, juce::Justification::left);

        auto contentArea = area.reduced(20);
        auto meterArea = contentArea.removeFromRight(240);
        
        float colWidth = contentArea.getWidth() / 3.0f;
        g.setColour(juce::Colours::white.withAlpha(0.025f));
        g.fillRect(contentArea.removeFromLeft((int)colWidth).reduced(5)); // Warmth
        g.fillRect(contentArea.removeFromLeft((int)colWidth).reduced(5)); // Clipper
        g.fillRect(contentArea.reduced(5)); // Limiter

        // Meter labels & numerical db/gr
        auto meterReadoutArea = meterArea.removeFromBottom(100).reduced(10, 0);
        g.setColour(juce::Colours::grey);
        g.setFont(juce::Font("Inter", 11.0f, juce::Font::plain));
        
        auto formatDB = [](float v) { return juce::String(juce::Decibels::gainToDecibels(v), 1); };
        auto formatGR = [](float v) { return juce::String(v, 1); };

        int meterX = meterReadoutArea.getX();
        int meterTargetY = meterReadoutArea.getY() + 15;

        g.drawText("IN",   meterX,      meterTargetY, 40, 15, juce::Justification::centred);
        g.drawText("C-GR", meterX + 45, meterTargetY, 40, 15, juce::Justification::centred);
        g.drawText("L-GR", meterX + 90, meterTargetY, 40, 15, juce::Justification::centred);
        g.drawText("OUT",  meterX + 135, meterTargetY, 40, 15, juce::Justification::centred);

        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.setFont(juce::Font("Inter", 12.0f, juce::Font::bold));
        g.drawText(formatDB(inLevel),      meterX,      meterTargetY + 20, 40, 20, juce::Justification::centred);
        g.drawText(formatGR(clipGRLevel),  meterX + 45, meterTargetY + 20, 40, 20, juce::Justification::centred);
        g.drawText(formatGR(limitGRLevel), meterX + 90, meterTargetY + 20, 40, 20, juce::Justification::centred);
        g.drawText(formatDB(outLevel),     meterX + 135, meterTargetY + 20, 40, 20, juce::Justification::centred);

        // LUFS Indicators
        auto lufsArea = meterArea.removeFromRight(60);
        g.setColour(juce::Colours::white.withAlpha(0.1f));
        g.drawVerticalLine(lufsArea.getX() - 5, (float)meterArea.getY() + 20, (float)meterArea.getBottom() - 10);

        g.setColour(juce::Colours::lightgrey);
        g.setFont(juce::Font("Inter", 12.0f, juce::Font::bold));
        
        auto drawLUFS = [&](juce::String label, float val, int yOffset) {
            g.setColour(juce::Colours::lightgrey);
            g.drawText(label, lufsArea.getX(), meterArea.getY() + yOffset, 55, 15, juce::Justification::centred);
            juce::String s = (val < -90) ? "---" : juce::String(val, 1);
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font("Inter", 14.0f, juce::Font::bold));
            g.drawText(s, lufsArea.getX(), meterArea.getY() + yOffset + 22, 55, 20, juce::Justification::centred);
        };

        drawLUFS("MOM", m_lufs, 40);
        drawLUFS("ST",  s_lufs, 110);
        drawLUFS("INT", i_lufs, 180);
    }

    void resized() override {
        auto area = getLocalBounds();
        area.removeFromTop(70);
        auto contentArea = area.reduced(20);
        
        auto meterOverallArea = contentArea.removeFromRight(240);
        auto meterVisuals = meterOverallArea;
        meterVisuals.removeFromBottom(100); // Leave more room for numbers
        
        auto meterBarsArea = meterVisuals.reduced(10, 40);
        auto lufsDisplayArea = meterBarsArea.removeFromRight(60);
        
        inMeter.setBounds(meterBarsArea.removeFromLeft(45).reduced(12, 0));
        clipGRMeter.setBounds(meterBarsArea.removeFromLeft(45).reduced(12, 0));
        limitGRMeter.setBounds(meterBarsArea.removeFromLeft(45).reduced(12, 0));
        outMeter.setBounds(meterBarsArea.removeFromLeft(45).reduced(12, 0));

        // Reset button: placed relative to meterReadoutArea (bottom right)
        resetBtn.setBounds(meterOverallArea.getRight() - 75, meterOverallArea.getBottom() - 45, 65, 28);

        float colWidth = contentArea.getWidth() / 3.0f;
        auto warmthArea = contentArea.removeFromLeft((int)colWidth).reduced(15);
        auto clipperArea = contentArea.removeFromLeft((int)colWidth).reduced(15);
        auto limiterArea = contentArea.reduced(15);

        auto layoutSlider = [](auto& area, auto& slider, auto& label) {
            auto sArea = area.removeFromTop(145);
            label.setBounds(sArea.removeFromTop(20));
            slider.setBounds(sArea);
        };

        // SATURATION LAYOUT
        warmthBypassBtn.setBounds(warmthArea.removeFromTop(40).reduced(10, 0));
        warmthArea.removeFromTop(10);
        layoutSlider(warmthArea, warmthSlider, warmthLabel);
        warmthArea.removeFromTop(20);
        targetCombo.setBounds(warmthArea.removeFromTop(40).reduced(15, 0));
        warmthArea.removeFromTop(10);
        oversamplingCombo.setBounds(warmthArea.removeFromTop(40).reduced(15, 0));

        // CLIPPER LAYOUT
        clipBypassBtn.setBounds(clipperArea.removeFromTop(40).reduced(10, 0));
        clipperArea.removeFromTop(5);
        layoutSlider(clipperArea, clipGainSlider, clipGainLabel);
        
        auto clipExtras = clipperArea.removeFromTop(40).reduced(15, 0);
        clipModeCombo.setBounds(clipExtras.removeFromLeft(clipExtras.getWidth() * 0.6f));
        linkBtn.setBounds(clipExtras.reduced(5, 5));
        
        clipVis.setBounds(clipperArea.removeFromBottom(80).reduced(5, 0));

        layoutSlider(clipperArea, clipKneeSlider, clipKneeLabel);
        layoutSlider(clipperArea, clipCeilingSlider, clipCeilingLabel);

        // LIMITER LAYOUT
        auto limitHeader = limiterArea.removeFromTop(40);
        limitBypassBtn.setBounds(limitHeader.removeFromLeft(limitHeader.getWidth() * 0.5f).reduced(5, 0));
        truePeakBtn.setBounds(limitHeader.reduced(5, 0));
        
        limiterArea.removeFromTop(5);
        limitStyleCombo.setBounds(limiterArea.removeFromTop(30).reduced(15, 0));
        limiterArea.removeFromTop(5);
        
        layoutSlider(limiterArea, limitThresholdSlider, limitThresholdLabel);
        
        auto midSliders = limiterArea.removeFromTop(145);
        auto attArea = midSliders.removeFromLeft(midSliders.getWidth() * 0.5f);
        attackLabel.setBounds(attArea.removeFromTop(20));
        attackSlider.setBounds(attArea);
        releaseLabel.setBounds(midSliders.removeFromTop(20));
        releaseSlider.setBounds(midSliders);

        limitVis.setBounds(limiterArea.removeFromBottom(80).reduced(5, 0));

        layoutSlider(limiterArea, lookaheadSlider, lookaheadLabel);
        layoutSlider(limiterArea, limitCeilingSlider, limitCeilingLabel);
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
    juce::Slider limitThresholdSlider, releaseSlider, limitCeilingSlider, attackSlider, lookaheadSlider;
    
    juce::Label warmthLabel;
    juce::Label clipGainLabel, clipKneeLabel, clipCeilingLabel;
    juce::Label limitThresholdLabel, releaseLabel, limitCeilingLabel, attackLabel, lookaheadLabel;
    juce::Label clipAreaLabel, limitAreaLabel;
    
    juce::ToggleButton warmthBypassBtn, clipBypassBtn, limitBypassBtn, truePeakBtn;
    juce::TextButton resetBtn, linkBtn;
    
    ScrollingVisualizer clipVis, limitVis;
    juce::ComboBox targetCombo, oversamplingCombo, clipModeCombo, limitStyleCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> targetAttachment, oversamplingAttachment, clipModeAttachment, limitStyleAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> linkAttachment, truePeakAttachment;

    LevelMeter inMeter, outMeter, clipGRMeter, limitGRMeter;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> warmthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> clipGainAttachment, clipKneeAttachment, clipCeilingAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> limitThresholdAttachment, releaseAttachment, limitCeilingAttachment, attackAttachment, lookaheadAttachment;
    
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> attachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoudnessCatalystAudioProcessorEditor)
};
