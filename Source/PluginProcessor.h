#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "LoudnessEngine.h"

class LoudnessCatalystAudioProcessor : public juce::AudioProcessor {
public:
    LoudnessCatalystAudioProcessor() 
        : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true).withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
          apvts (*this, nullptr, "Parameters", createParameterLayout())
    {}
    ~LoudnessCatalystAudioProcessor() override {}

    void prepareToPlay (double sampleRate, int samplesPerBlock) override {
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) getTotalNumOutputChannels() };
        engine.prepare(spec);
    }
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override {
        juce::ScopedNoDenormals noDenormals;
        
        int osFactor = (int)apvts.getRawParameterValue("oversampling")->load();
        if (osFactor != lastOsFactor) {
            engine.updateOversampling(osFactor);
            lastOsFactor = osFactor;
        }

        engine.setWarmthBypass(apvts.getRawParameterValue("warmthBypass")->load() > 0.5f);
        engine.setClipBypass(apvts.getRawParameterValue("clipBypass")->load() > 0.5f);
        engine.setLimitBypass(apvts.getRawParameterValue("limitBypass")->load() > 0.5f);
        engine.setTargetIndex((int)apvts.getRawParameterValue("targetLufs")->load());
        engine.setClipMode((int)apvts.getRawParameterValue("clipMode")->load());

        float cg = apvts.getRawParameterValue("clipGain")->load();
        float cc = apvts.getRawParameterValue("clipCeiling")->load();
        bool link = apvts.getRawParameterValue("linkAutoGain")->load() > 0.5f;
        if (link) cc = -cg; 

        engine.setClipGain(cg);
        engine.setClipCeiling(cc);
        engine.setClipKnee(apvts.getRawParameterValue("clipKnee")->load());

        engine.setLimitThreshold(apvts.getRawParameterValue("limitThreshold")->load());
        engine.setLimitCeiling(apvts.getRawParameterValue("limitCeiling")->load());
        engine.setRelease(apvts.getRawParameterValue("release")->load());
        engine.setAttack(apvts.getRawParameterValue("attack")->load());
        engine.setLookahead(apvts.getRawParameterValue("lookahead")->load());
        engine.setLimitStyle((int)apvts.getRawParameterValue("limitStyle")->load());
        engine.setTruePeak(apvts.getRawParameterValue("truePeak")->load() > 0.5f);
        
        engine.setWarmth(apvts.getRawParameterValue("warmth")->load());
        engine.setTargetIndex((int)apvts.getRawParameterValue("targetLufs")->load());
        
        engine.process(buffer);
    }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Loudness Catalyst"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override {
        auto state = apvts.copyState();
        std::unique_ptr<juce::XmlElement> xml (state.createXml());
        copyXmlToBinary (*xml, destData);
    }

    void setStateInformation (const void* data, int sizeInBytes) override {
        std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
        if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
    }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        
        // --- WARMTH / SATURATION ---
        layout.add(std::make_unique<juce::AudioParameterFloat>("warmth", "Analog Warmth", 0.0f, 100.0f, 0.0f));
        layout.add(std::make_unique<juce::AudioParameterBool>("warmthBypass", "Warmth Bypass", false));
        layout.add(std::make_unique<juce::AudioParameterChoice>("targetLufs", "Target LUFS", 
            juce::StringArray {"Off", "-14", "-13", "-12", "-11", "-10", "-9", "-8", "-7", "-6", "-5", "-4", "-3", "-2"}, 0));
        layout.add(std::make_unique<juce::AudioParameterChoice>("oversampling", "Oversampling", 
            juce::StringArray {"Off", "2x", "4x", "8x"}, 1));
        layout.add(std::make_unique<juce::AudioParameterChoice>("clipMode", "Clipping Mode", 
            juce::StringArray {"Hard", "Soft", "Analog"}, 1));
        layout.add(std::make_unique<juce::AudioParameterBool>("linkAutoGain", "Link Auto-Gain", false));

        // --- CLIPPER ---
        layout.add(std::make_unique<juce::AudioParameterFloat>("clipGain", "Clipper Gain", 0.0f, 24.0f, 0.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>("clipCeiling", "Clipper Ceiling", -12.0f, 0.0f, 0.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>("clipKnee", "Clipper Knee", 0.0f, 1.0f, 0.5f));
        layout.add(std::make_unique<juce::AudioParameterBool>("clipBypass", "Clipper Bypass", false));

        // --- LIMITER ---
        layout.add(std::make_unique<juce::AudioParameterFloat>("limitThreshold", "Limiter Threshold", -24.0f, 0.0f, 0.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>("limitCeiling", "Limiter Ceiling", -12.0f, 0.0f, 0.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>("release", "Limiter Release", 10.0f, 1000.0f, 100.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>("attack", "Limiter Attack", 0.1f, 500.0f, 1.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>("lookahead", "Limiter Lookahead", 0.0f, 10.0f, 2.0f));
        layout.add(std::make_unique<juce::AudioParameterChoice>("limitStyle", "Limiter Style", 
            juce::StringArray {"Transparent", "Punchy", "Dynamic", "Safe"}, 0));
        layout.add(std::make_unique<juce::AudioParameterBool>("truePeak", "True Peak Limiting", false));
        layout.add(std::make_unique<juce::AudioParameterBool>("limitBypass", "Limiter Bypass", false));

        return layout;
    }

    float getInLevel() const { return engine.getInLevel(); }
    float getOutLevel() const { return engine.getOutLevel(); }
    float getClipGR() const { return engine.getClipGR(); }
    float getLimiterGR() const { return engine.getLimiterGR(); }
    
    float getClipVisIn() const { return engine.getClipVisIn(); }
    float getClipVisOut() const { return engine.getClipVisOut(); }
    float getLimitVisIn() const { return engine.getLimitVisIn(); }
    float getLimitVisOut() const { return engine.getLimitVisOut(); }

    float getMLUFS() const { return engine.getMLUFS(); }
    float getSLUFS() const { return engine.getSLUFS(); }
    float getILUFS() const { return engine.getILUFS(); }
    void resetIntegrated() { engine.resetLUFS(); }

    juce::AudioProcessorValueTreeState apvts;
private:
    LoudnessEngine engine;
    int lastOsFactor = 1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoudnessCatalystAudioProcessor)
};
