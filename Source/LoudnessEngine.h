#pragma once
#include <juce_dsp/juce_dsp.h>

/**
 * LoudnessEngine: El corazón del procesamiento analógico y loudness.
 */
class LoudnessEngine {
public:
    LoudnessEngine() {
        // Inicializar filtros y saturadores
    }

    void prepare(const juce::dsp::ProcessSpec& spec) {
        oversampler.reset(new juce::dsp::Oversampling<float>(spec.numChannels, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true));
        oversampler->initProcessing(spec.maximumBlockSize);
        
        saturation.prepare(spec);
        limiter.prepare(spec);
    }

    void process(juce::AudioBuffer<float>& buffer) {
        juce::dsp::AudioBlock<float> block(buffer);
        
        // 1. Etapa de Calidez Analógica (Saturación antes del clip)
        applyAnalogWarmth(buffer);

        // 2. Clipper con Oversampling
        auto upsampledBlock = oversampler->processSamplesUp(block);
        for (int ch = 0; ch < upsampledBlock.getNumChannels(); ++ch) {
            auto* samples = upsampledBlock.getChannelPointer(ch);
            for (int i = 0; i < upsampledBlock.getNumSamples(); ++i) {
                samples[i] = softClip(samples[i]);
            }
        }
        oversampler->processSamplesDown(block);

        // 3. Limitador Adaptativo
        juce::dsp::ProcessContextReplacing<float> context(block);
        limiter.process(context);
    }

private:
    float softClip(float x) {
        // Curva de saturación analógica estilo Standard Clip
        if (std::abs(x) < 0.5f) return x;
        if (x > 0.0f) return 0.5f + (0.5f * std::tanh((x - 0.5f) / 0.5f));
        return -0.5f + (0.5f * std::tanh((x + 0.5f) / 0.5f));
    }

    void applyAnalogWarmth(juce::AudioBuffer<float>& buffer) {
        // Emulación de distorsión armónica sutil
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
            auto* data = buffer.getWritePointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i) {
                data[i] = std::atan(data[i] * 1.05f) / 1.05f; // Saturador tipo Tape
            }
        }
    }

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::dsp::Gain<float> saturation;
    juce::dsp::Limiter<float> limiter;
};
