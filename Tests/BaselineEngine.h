#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <signalsmith-stretch/signalsmith-stretch.h>
#include <array>
#include <cmath>
#include <vector>

namespace suitunes_baseline {
struct Pitch { float hz = 0, confidence = 0; };
// ~42.7 ms causal YIN window, evaluated every 256 samples.
// One voice only. No heap work during push()/analyse().
class Yin {
public:
    static constexpr int capacity = 8192, hop = 256;
    void prepare(double rate) { sr = rate; window = std::clamp(int(std::round(rate * 2048.0 / 48000.0)), 2048, capacity); ring.fill(0); difference.fill(0); pos = filled = count = 0; result = {}; }
    bool push(float value) {
        ring[pos] = std::isfinite(value) ? value : 0; pos = (pos + 1) % window;
        filled = std::min(filled + 1, window);
        if (++count < hop) return false;
        count = 0; result = filled == window ? analyse() : Pitch{}; return true;
    }
    Pitch pitch() const { return result; }
private:
    Pitch analyse() {
        const int half = window / 2;
        const int minLag = std::max(2, int(sr / 1000.0));
        const int maxLag = std::min(half - 2, int(sr / 65.0));
        double energy = 0;
        for (auto v : ring) energy += v * v;
        if (energy / window < 1.0e-6 || minLag >= maxLag) return {};
        double sum = 0;
        difference[0] = 1;
        for (int lag = 1; lag <= maxLag + 1; ++lag) {
            double d = 0;
            for (int j = 0; j < half; ++j) {
                const double delta = ring[(pos + j) % window] - ring[(pos + j + lag) % window];
                d += delta * delta;
            }
            sum += d; difference[lag] = sum > 0 ? float(d * lag / sum) : 1;
        }
        for (int lag = minLag; lag <= maxLag; ++lag) {
            if (difference[lag] >= 0.15f) continue;
            while (lag < maxLag && difference[lag + 1] < difference[lag]) ++lag;
            const float a = difference[lag - 1], b = difference[lag], c = difference[lag + 1];
            const float denom = a - 2 * b + c;
            const float delta = std::abs(denom) > 1e-9f ? 0.5f * (a - c) / denom : 0;
            return {float(sr / (lag + std::clamp(delta, -0.5f, 0.5f))), 1 - b};
        }
        return {};
    }
    double sr = 48000;
    std::array<float, capacity> ring{};
    std::array<float, capacity / 2 + 1> difference{};
    int window = 2048;
    int pos = 0, filled = 0, count = 0;
    Pitch result;
};

inline float midiFromHz(float hz) { return 69.0f + 12.0f * std::log2(hz / 440.0f); }
// scale: 0 chromatic, 1 major, 2 natural minor. root: C=0 ... B=11.
inline float targetNote(float midi, int root, int scale) {
    constexpr int major[] = {0,2,4,5,7,9,11}, minor[] = {0,2,3,5,7,8,10};
    float best = std::round(midi), distance = 100;
    for (int n = int(std::floor(midi)) - 12; n <= int(std::ceil(midi)) + 12; ++n) {
        int pc = ((n - root) % 12 + 12) % 12;
        bool allowed = scale == 0;
        for (int i = 0; i < 7; ++i) allowed |= pc == (scale == 1 ? major[i] : minor[i]);
        if (allowed && std::abs(midi - n) < distance) { best = float(n); distance = std::abs(midi - n); }
    }
    return best;
}
struct Settings { bool enabled = false; int root = 0, scale = 0; float amount = 1, retuneMs = 40, mix = 1; };

class Engine {
public:
    void prepare(double rate, int channels) {
        sr = rate; numChannels = channels; yin.prepare(rate);
        // Start from the library's quality preset; benchmark latency honestly.
        stretch.presetDefault(channels, float(rate));
        latency = stretch.inputLatency() + stretch.outputLatency();
        delay.assign(size_t(channels), std::vector<float>(size_t(latency + 1), 0));
        wet.setSize(channels, quantum); dry.setSize(channels, quantum);
        delayPos = 0; correction = 0; mix.reset(rate, 0.02); mix.setCurrentAndTargetValue(0);
    }
    int latencySamples() const { return latency; }
    Pitch pitch() const { return yin.pitch(); }
    float correctionSemitones() const { return correction; }
    void process(juce::AudioBuffer<float>& buffer, Settings s) {
        mix.setTargetValue(s.enabled ? s.mix : 0);
        for (int offset = 0; offset < buffer.getNumSamples(); offset += quantum) {
            int count = std::min(quantum, buffer.getNumSamples() - offset);
            for (int i = 0; i < count; ++i) {
                float mono = 0;
                for (int ch = 0; ch < numChannels; ++ch) {
                    float v = buffer.getSample(ch, offset + i);
                    v = std::isfinite(v) ? v : 0;
                    wet.setSample(ch, i, v); mono += v / numChannels;
                    dry.setSample(ch, i, delay[size_t(ch)][size_t(delayPos)]);
                    delay[size_t(ch)][size_t(delayPos)] = v;
                }
                // Delay length is latency (ring storage includes an unused sentinel).
                delayPos = (delayPos + 1) % std::max(1, latency);
                yin.push(mono);
            }
            auto p = yin.pitch(); float desired = 0;
            if (s.enabled && p.hz > 0 && p.confidence >= 0.85f) {
                float note = midiFromHz(p.hz);
                desired = std::clamp(targetNote(note, s.root, s.scale) - note, -12.0f, 12.0f) * s.amount;
            }
            float alpha = float(std::exp(-count / (sr * std::max(1.0f, s.retuneMs) * 0.001)));
            correction = alpha * correction + (1 - alpha) * desired;
            stretch.setTransposeSemitones(correction);
            const float* inputs[2] = {wet.getReadPointer(0), numChannels == 2 ? wet.getReadPointer(1) : nullptr};
            float* outputs[2] = {buffer.getWritePointer(0, offset), numChannels == 2 ? buffer.getWritePointer(1, offset) : nullptr};
            stretch.process(inputs, count, outputs, count);
            for (int i = 0; i < count; ++i) {
                float m = mix.getNextValue();
                for (int ch = 0; ch < numChannels; ++ch) {
                    float w = buffer.getSample(ch, offset + i), d = dry.getSample(ch, i);
                    buffer.setSample(ch, offset + i, d + m * ((std::isfinite(w) ? w : 0) - d));
                }
            }
        }
    }
private:
    static constexpr int quantum = 64;
    double sr = 48000; int numChannels = 2, latency = 0, delayPos = 0;
    float correction = 0;
    Yin yin;
    signalsmith::stretch::SignalsmithStretch<float> stretch;
    std::vector<std::vector<float>> delay;
    juce::AudioBuffer<float> wet, dry;
    juce::SmoothedValue<float> mix;
};
}

