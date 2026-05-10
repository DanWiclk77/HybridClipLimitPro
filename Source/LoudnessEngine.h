#pragma once
#include <juce_dsp/juce_dsp.h>

/**
 * LoudnessEngine: El corazón del procesamiento analógico y loudness.
 */
class LoudnessEngine {
public:
    LoudnessEngine() {}

    void prepare(const juce::dsp::ProcessSpec& spec) {
        oversampler.reset(new juce::dsp::Oversampling<float>(
            spec.numChannels, 2,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true));
        oversampler->initProcessing(spec.maximumBlockSize);

        limiter.prepare(spec);
    }

    void process(juce::AudioBuffer<float>& buffer) {
        // Measure Input Peak (antes de cualquier procesamiento)
        maxIn = buffer.getMagnitude(0, buffer.getNumSamples());

        // --- SMART TARGET LOGIC ---
        float effectiveGain = juce::Decibels::decibelsToGain(clipGainParam);
        float effectiveThreshold = limitThresholdParam;

        if (targetIndex > 0) {
            float targetValue  = -15.0f + (float)targetIndex; // -14 to -2
            float currentRMS   = buffer.getRMSLevel(0, 0, buffer.getNumSamples());
            float currentLUFS  = 20.0f * std::log10(currentRMS + 0.00001f);

            float diff = targetValue - currentLUFS;
            if (diff > 0) {
                effectiveGain      = juce::Decibels::decibelsToGain(diff * 0.5f);
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
        for (size_t ch = 0; ch < upsampledBlock.getNumChannels(); ++ch) {
            auto* samples = upsampledBlock.getChannelPointer(ch);
            for (size_t i = 0; i < upsampledBlock.getNumSamples(); ++i)
                samples[i] = softClip(samples[i], clipKneeParam);
        }
        oversampler->processSamplesDown(block);

        // Capturar nivel PRE-limiter para calcular GR
        float preLimiterPeak = buffer.getMagnitude(0, buffer.getNumSamples());

        // 4. Limitador Final
        juce::dsp::ProcessContextReplacing<float> context(block);
        limiter.process(context);

        // 5. Output Ceiling
        buffer.applyGain(juce::Decibels::decibelsToGain(ceilingParam));

        // Measure Output Peak
        maxOut = buffer.getMagnitude(0, buffer.getNumSamples());

        // Calcular GR manualmente:
        // GR = nivel de salida - nivel de entrada (en dB, negativo cuando hay reducción)
        if (preLimiterPeak > 0.00001f && maxOut > 0.00001f) {
            float preLimDB  = juce::Decibels::gainToDecibels(preLimiterPeak);
            float postLimDB = juce::Decibels::gainToDecibels(maxOut);
            lastGR = postLimDB - preLimDB; // negativo = reducción aplicada
        } else {
            lastGR = 0.0f;
        }
    }

    float getInLevel()  const { return maxIn; }
    float getOutLevel() const { return maxOut; }
    float getGRLevel()  const { return lastGR; } // valor negativo = reducción

    void setClipGain       (float newGain)      { clipGainParam      = newGain;      }
    void setClipKnee       (float newKnee)      { clipKneeParam      = newKnee;      }
    void setLimitThreshold (float newThreshold) { limitThresholdParam = newThreshold; }
    void setRelease        (float newRelease)   { releaseParam        = newRelease;   }
    void setCeiling        (float newCeiling)   { ceilingParam        = newCeiling;   }
    void setWarmth         (float newWarmth)    { warmthParam         = newWarmth;    }
    void setTargetIndex    (int   index)        { targetIndex         = index;        }
    void setLookAhead      (float /*unused*/)   { /* juce::dsp::Limiter no expone lookahead */ }

private:
    int   targetIndex       = 0;
    float maxIn             = 0.0f;
    float maxOut            = 0.0f;
    float lastGR            = 0.0f;
    float clipGainParam     = 0.0f;
    float clipKneeParam     = 0.5f;
    float limitThresholdParam = 0.0f;
    float ceilingParam      = 0.0f;
    float warmthParam       = 0.0f;
    float releaseParam      = 100.0f;

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
