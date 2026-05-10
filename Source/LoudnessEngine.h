#pragma once
#include <juce_dsp/juce_dsp.h>

/**
 * LoudnessEngine: El corazón del procesamiento analógico y loudness.
 */
class LoudnessEngine {
public:
    LoudnessEngine() {}

    void prepare(const juce::dsp::ProcessSpec& spec) {
        oversampler.reset(new juce::dsp::Oversampling<float>(spec.numChannels, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true));
        oversampler->initProcessing(spec.maximumBlockSize);
        
        limiter.prepare(spec);
    }

    void process(juce::AudioBuffer<float>& buffer) {
        // Measure Input Peak
        maxIn = buffer.getMagnitude(0, buffer.getNumSamples());
        
        // --- SMART TARGET LOGIC ---
        float effectiveGain = juce::Decibels::decibelsToGain(clipGainParam);
        float effectiveThreshold = limitThresholdParam;

        if (targetIndex > 0) {
            float targetValue = -15.0f + (float)targetIndex; // -14 to -2
            float currentRMS = buffer.getRMSLevel(0, 0, buffer.getNumSamples());
            float currentLUFS = 20.0f * std::log10(currentRMS + 0.00001f);
            
            float diff = targetValue - currentLUFS;
            if (diff > 0) {
                // Distributed Auto-Mastering
                effectiveGain = juce::Decibels::decibelsToGain(diff * 0.5f);
                effectiveThreshold = -(diff * 0.5f);
            }
        }
        // ---------------------------

        juce::dsp::AudioBlock<float> block(buffer);
        
        // Sincronizar parámetros del limitador
        limiter.setThreshold(effectiveThreshold);
        limiter.setRelease(releaseParam);
        
        // 1. Ganancia de entrada
        buffer.applyGain(effectiveGain);

        // 2. Etapa de Calidez Analógica
        applyAnalogWarmth(buffer);

        // 3. Clipper con Oversampling
        auto upsampledBlock = oversampler->processSamplesUp(block);
        for (int ch = 0; ch < upsampledBlock.getNumChannels(); ++ch) {
            auto* samples = upsampledBlock.getChannelPointer(ch);
            for (int i = 0; i < upsampledBlock.getNumSamples(); ++i) {
                samples[i] = softClip(samples[i], clipKneeParam);
            }
        }
        oversampler->processSamplesDown(block);

        // 4. Limitador Final
        juce::dsp::ProcessContextReplacing<float> context(block);
        limiter.process(context);

        // Measure Output Peak and GR
        maxOut = buffer.getMagnitude(0, buffer.getNumSamples());
        lastGR = limiter.getGainReduction();
    }

    float getInLevel() const { return maxIn; }
    float getOutLevel() const { return maxOut; }
    float getGRLevel() const { return lastGR; }

    void setClipGain(float newGain) { clipGainParam = newGain; }
    void setClipKnee(float newKnee) { clipKneeParam = newKnee; }
    void setLimitThreshold(float newThreshold) { limitThresholdParam = newThreshold; }
    void setLookAhead(float newLookAhead) { /* JUCE Limiter doesn't have a direct lookahead set, it's internal or via buffer */ }
    void setRelease(float newRelease) { releaseParam = newRelease; }
    void setTargetIndex(int index) { targetIndex = index; }

private:
    int targetIndex = 0;
    float maxIn = 0.0f;
    float maxOut = 0.0f;
    float lastGR = 0.0f;
    float clipGainParam = 0.0f;
    float clipKneeParam = 0.5f;
    float limitThresholdParam = 0.0f;
    float releaseParam = 100.0f;

    float softClip(float x, float knee) {
        float threshold = 1.0f - (knee * 0.5f);
        if (std::abs(x) < threshold) return x;
        
        if (x > 0.0f) return threshold + (1.0f - threshold) * std::tanh((x - threshold) / (1.0f - threshold));
        return -threshold + (1.0f - threshold) * std::tanh((x + threshold) / (1.0f - threshold));
    }

    void applyAnalogWarmth(juce::AudioBuffer<float>& buffer) {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
            auto* data = buffer.getWritePointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i) {
                // Sutil saturadores asimétricos para carácter analógico
                data[i] = (data[i] > 0) ? std::tanh(data[i] * 1.02f) : std::tanh(data[i] * 1.01f);
            }
        }
    }

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::dsp::Limiter<float> limiter;
};
