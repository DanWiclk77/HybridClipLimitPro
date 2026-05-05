#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "LoudnessEngine.h"

class LoudnessCatalystAudioProcessor : public juce::AudioProcessor {
public:
    LoudnessCatalystAudioProcessor() : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true).withOutput ("Output", juce::AudioChannelSet::stereo(), true)) {}
    ~LoudnessCatalystAudioProcessor() override {}

    void prepareToPlay (double sampleRate, int samplesPerBlock) override {
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) getTotalNumOutputChannels() };
        engine.prepare(spec);
    }
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override {
        engine.process(buffer);
    }

    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    const juce::String getName() const override { return "Loudness Catalyst"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

private:
    LoudnessEngine engine;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoudnessCatalystAudioProcessor)
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new LoudnessCatalystAudioProcessor();
}
