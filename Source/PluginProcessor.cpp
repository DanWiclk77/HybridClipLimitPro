#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorEditor* LoudnessCatalystAudioProcessor::createEditor() {
    return new LoudnessCatalystAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new LoudnessCatalystAudioProcessor();
}

