#pragma once
#include <juce_dsp/juce_dsp.h>

/**
 * LoudnessEngine: El corazón del procesamiento analógico y loudness.
 */
class LoudnessEngine {
public:
    LoudnessEngine() {}

    void prepare(const juce::dsp::ProcessSpec& spec) {
        sampleRate = spec.sampleRate;
        oversampler.reset(new juce::dsp::Oversampling<float>(
            spec.numChannels, 2,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true));
        oversampler->initProcessing(spec.maximumBlockSize);

        limiter.prepare(spec);

        // LUFS K-Weighting filters
        juce::dsp::IIR::Coefficients<float>::Ptr stage1Coeffs = 
            juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate, 1500.0f, 1.0f, 4.0f);
        juce::dsp::IIR::Coefficients<float>::Ptr stage2Coeffs = 
            juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 100.0f, 1.0f);

        kFilter1.prepare(spec);
        kFilter2.prepare(spec);
        *kFilter1.state = *stage1Coeffs;
        *kFilter2.state = *stage2Coeffs;

        resetLUFS();
    }

    void resetLUFS() {
        integratedSum = 0.0f;
        integratedCount = 0;
        momentaryMax = -100.0f;
        shortTermMax = -100.0f;
    }

    void process(juce::AudioBuffer<float>& buffer) {
        // Measure Input Peak (Master Input)
        maxIn = buffer.getMagnitude(0, buffer.getNumSamples());

        // --- 1. ANALOG WARMTH / SMART TARGET ---
        if (!warmthBypass) {
            float smartGain = 0.0f;
            if (targetIndex > 0) {
                float targetValue = -15.0f + (float)targetIndex;
                float currentRMS = buffer.getRMSLevel(0, 0, buffer.getNumSamples());
                float currentLUFS = 20.0f * std::log10(currentRMS + 0.00001f);
                float diff = targetValue - currentLUFS;
                if (diff > 0) smartGain = diff * 0.4f;
            }
            float warmthDrive = juce::Decibels::decibelsToGain(smartGain);
            buffer.applyGain(warmthDrive);
            applyAnalogWarmth(buffer);
        }

        // --- 2. CLIPPER SECTION ---
        float preClipPeak = buffer.getMagnitude(0, buffer.getNumSamples());
        if (!clipBypass) {
            buffer.applyGain(juce::Decibels::decibelsToGain(clipGainParam));
            
            juce::dsp::AudioBlock<float> block(buffer);
            auto upsampledBlock = oversampler->processSamplesUp(block);
            for (size_t ch = 0; ch < upsampledBlock.getNumChannels(); ++ch) {
                auto* samples = upsampledBlock.getChannelPointer(ch);
                for (size_t i = 0; i < upsampledBlock.getNumSamples(); ++i)
                    samples[i] = softClip(samples[i], clipKneeParam);
            }
            oversampler->processSamplesDown(block);
            
            buffer.applyGain(juce::Decibels::decibelsToGain(clipCeilingParam));
            
            float postClipPeak = buffer.getMagnitude(0, buffer.getNumSamples());
            clipGR = juce::Decibels::gainToDecibels(postClipPeak) - juce::Decibels::gainToDecibels(preClipPeak + 0.00001f);
        } else {
            clipGR = 0.0f;
        }

        // --- 3. LIMITER SECTION ---
        float preLimiterPeak = buffer.getMagnitude(0, buffer.getNumSamples());
        if (!limitBypass) {
            limiter.setThreshold(limitThresholdParam);
            limiter.setRelease(releaseParam);
            
            juce::dsp::AudioBlock<float> block(buffer);
            juce::dsp::ProcessContextReplacing<float> context(block);
            limiter.process(context);
            
            buffer.applyGain(juce::Decibels::decibelsToGain(limitCeilingParam));
            
            float postLimiterPeak = buffer.getMagnitude(0, buffer.getNumSamples());
            limitGR = juce::Decibels::gainToDecibels(postLimiterPeak) - juce::Decibels::gainToDecibels(preLimiterPeak + 0.00001f);
        } else {
            limitGR = 0.0f;
        }

        // Measure Final Output Peak
        maxOut = buffer.getMagnitude(0, buffer.getNumSamples());

        // --- 4. LUFS MEASUREMENT (Approximate) ---
        calculateLUFS(buffer);
    }

    void calculateLUFS(const juce::AudioBuffer<float>& buffer) {
        juce::AudioBuffer<float> lufsBuffer;
        lufsBuffer.makeCopyOf(buffer);
        
        juce::dsp::AudioBlock<float> block(lufsBuffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        kFilter1.process(context);
        kFilter2.process(context);

        float sumSquares = 0.0f;
        for (int ch = 0; ch < lufsBuffer.getNumChannels(); ++ch)
            sumSquares += lufsBuffer.getRMSLevel(ch, 0, lufsBuffer.getNumSamples()) * lufsBuffer.getRMSLevel(ch, 0, lufsBuffer.getNumSamples());
        
        float meanSquare = sumSquares / (float)lufsBuffer.getNumChannels();
        float currentLoudness = -0.691f + 10.0f * std::log10(meanSquare + 1e-10f);

        // Smoothing for Momentary (approx 400ms) and Short (approx 3s)
        momentaryLUFS = momentaryLUFS * 0.95f + currentLoudness * 0.05f;
        shortTermLUFS = shortTermLUFS * 0.99f + currentLoudness * 0.01f;

        // Integrated
        integratedSum += meanSquare;
        integratedCount++;
        integratedLUFS = -0.691f + 10.0f * std::log10((integratedSum / (float)integratedCount) + 1e-10f);
    }

    float getInLevel()     const { return maxIn; }
    float getOutLevel()    const { return maxOut; }
    float getClipGR()      const { return juce::jmin(0.0f, clipGR); }
    float getLimiterGR()   const { return juce::jmin(0.0f, limitGR); }
    
    float getMLUFS() const { return momentaryLUFS; }
    float getSLUFS() const { return shortTermLUFS; }
    float getILUFS() const { return integratedLUFS; }

    void setWarmthBypass   (bool b) { warmthBypass = b; }
    void setClipBypass     (bool b) { clipBypass = b; }
    void setLimitBypass    (bool b) { limitBypass = b; }
    
    void setClipGain       (float g) { clipGainParam = g; }
    void setClipCeiling    (float c) { clipCeilingParam = c; }
    void setClipKnee       (float k) { clipKneeParam = k; }
    
    void setLimitThreshold (float t) { limitThresholdParam = t; }
    void setLimitCeiling   (float c) { limitCeilingParam = c; }
    void setRelease        (float r) { releaseParam = r; }
    
    void setWarmth         (float w) { warmthParam = w; }
    void setTargetIndex    (int   i) { targetIndex = i; }

private:
    double sampleRate = 44100.0;
    
    // LUFS Measurement
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> kFilter1;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> kFilter2;
    
    float momentaryLUFS = -100.0f;
    float shortTermLUFS = -100.0f;
    float integratedLUFS = -100.0f;
    float integratedSum  = 0.0f;
    long long integratedCount = 0;
    float momentaryMax = -100.0f;
    float shortTermMax = -100.0f;

    bool  warmthBypass      = false;
    bool  clipBypass        = false;
    bool  limitBypass       = false;
    
    int   targetIndex       = 0;
    float maxIn             = 0.0f;
    float maxOut            = 0.0f;
    float clipGR            = 0.0f;
    float limitGR           = 0.0f;
    
    float clipGainParam      = 0.0f;
    float clipCeilingParam   = 0.0f;
    float clipKneeParam      = 0.5f;
    
    float limitThresholdParam = 0.0f;
    float limitCeilingParam   = 0.0f;
    float warmthParam        = 0.0f;
    float releaseParam       = 100.0f;

    float softClip(float x, float knee) {
        float threshold = 1.0f - (knee * 0.5f);
        if (std::abs(x) < threshold) return x;

        if (x > 0.0f)
            return  threshold + (1.0f - threshold) * std::tanh(( x - threshold) / (1.0f - threshold));
        return -threshold + (1.0f - threshold) * std::tanh(( x + threshold) / (1.0f - threshold));
    }

    void applyAnalogWarmth(juce::AudioBuffer<float>& buffer) {
        if (warmthParam <= 0.0f) return;
        
        float drive = 1.0f + (warmthParam * 0.1f); // Scale 0-100 to a subtle drive
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
            auto* data = buffer.getWritePointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                float x = data[i];
                // Soft asymmetrical saturation for "analog" character
                data[i] = (x > 0.0f) ? std::tanh(x * drive) : std::tanh(x * (drive * 0.98f));
            }
        }
    }

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::dsp::Limiter<float> limiter;
};
