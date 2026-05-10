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
        
        // Sincronizar parámetros con el motor
        engine.setClipGain(apvts.getRawParameterValue("clipGain")->load());
        engine.setClipKnee(apvts.getRawParameterValue("clipKnee")->load());
        engine.setLimitThreshold(apvts.getRawParameterValue("limitThreshold")->load());
        engine.setLookAhead(apvts.getRawParameterValue("lookAhead")->load());
        engine.setRelease(apvts.getRawParameterValue("release")->load());
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
        
        // Clipper Params
        layout.add(std::make_unique<juce::AudioParameterFloat>("clipGain", "Clipper Gain", 0.0f, 24.0f, 0.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>("clipKnee", "Clipper Knee", 0.0f, 1.0f, 0.5f));
        
        // Limiter Params
        layout.add(std::make_unique<juce::AudioParameterFloat>("limitThreshold", "Limiter Threshold", -24.0f, 0.0f, 0.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>("lookAhead", "Look-ahead", 0.1f, 5.0f, 1.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>("release", "Release", 10.0f, 1000.0f, 100.0f));
        
        // Smart Target Param
        layout.add(std::make_unique<juce::AudioParameterChoice>("targetLufs", "Target LUFS", 
            juce::StringArray {"Off", "-14", "-13", "-12", "-11", "-10", "-9", "-8", "-7", "-6", "-5", "-4", "-3", "-2"}, 0));

        return layout;
    }

    float getInLevel() const { return engine.getInLevel(); }
    float getOutLevel() const { return engine.getOutLevel(); }
    float getGRLevel() const { return engine.getGRLevel(); }

    juce::AudioProcessorValueTreeState apvts;
private:
    LoudnessEngine engine;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoudnessCatalystAudioProcessor)
};
