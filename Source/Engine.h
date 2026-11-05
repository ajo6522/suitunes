#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <signalsmith-stretch/signalsmith-stretch.h>
#include <array>
#include <cmath>
#include <vector>
#include "Yin.h"
#include "VoiceShifter.h"

namespace suitunes {
inline float midiFromHz(float hz) { return 69.0f + 12.0f * std::log2(hz / 440.0f); }
// scale: 0 chromatic, 1 major, 2 natural minor. root: C=0 ... B=11.
inline float targetNote(float midi, int root, int scale, int mask=4095) {
    constexpr int major[] = {0,2,4,5,7,9,11}, minor[] = {0,2,3,5,7,8,10};
    float best = std::round(midi), distance = 100;
    for (int n = int(std::floor(midi)) - 12; n <= int(std::ceil(midi)) + 12; ++n) {
        int pc = ((n - root) % 12 + 12) % 12;
        bool allowed = scale == 0;
        if(scale==3) allowed=(mask & (1<<((n%12+12)%12)))!=0;
        else for (int i = 0; i < 7; ++i) allowed |= pc == (scale == 1 ? major[i] : minor[i]);
        if (allowed && std::abs(midi - n) < distance) { best = float(n); distance = std::abs(midi - n); }
    }
    return distance<100 ? best : midi;
}
struct Settings {
    bool enabled=false, midiEnabled=false;
    int root=0,scale=0,mask=4095,midiTarget=-1,vibratoMode=0;
    float amount=1,retuneMs=40,mix=1,formant=0,vibratoDepth=1,vibratoRate=5,drift=1;
};

enum class Mode { FullVoice = 0, Live = 1, Spectral = 2 };
class Engine {
public:
    void prepare(double rate, int channels, Mode selectedMode = Mode::FullVoice) {
        sr = rate; numChannels = channels; yin.prepare(rate);
        mode=selectedMode;
        if(mode==Mode::Spectral) {
            stretch.presetDefault(channels, float(rate));
            latency = stretch.inputLatency() + stretch.outputLatency();
        } else {
            latency=int(std::ceil(rate*(mode==Mode::Live ? 0.010 : 0.016)));
            voice.prepare(rate,channels,latency);
        }
        delay.assign(size_t(channels), std::vector<float>(size_t(latency + 1), 0));
        wet.setSize(channels, quantum); dry.setSize(channels, quantum);
        delayPos = 0; correction = 0; mix.reset(rate, 0.02); mix.setCurrentAndTargetValue(0);
        observed={};
        analysisChannel=0;
        centreFast=centreSlow=heldTarget=0; tracking=false; lfo=0;
        formantSmooth.reset(rate,0.03); formantSmooth.setCurrentAndTargetValue(0);
    }
    int latencySamples() const { return latency; }
    Pitch pitch() const { return observed; }
    Mode currentMode() const { return mode; }
    float minimumHz() const { return mode==Mode::Live ? 110.0f : 65.0f; }
    float correctionSemitones() const { return correction; }
    void process(juce::AudioBuffer<float>& buffer, Settings s, int start=0, int length=-1) {
        const int end=length<0 ? buffer.getNumSamples() : std::min(buffer.getNumSamples(),start+length);
        mix.setTargetValue(s.enabled ? s.mix : 0);
        formantSmooth.setTargetValue(s.formant);
        const float smoothingBase=float(std::exp(-1.0/(sr*std::max(1.0f,s.retuneMs)*0.001)));
        for (int offset = start; offset < end; offset += quantum) {
            int count = std::min(quantum, end - offset);
            if(numChannels==2){float left=buffer.getRMSLevel(0,offset,count),right=buffer.getRMSLevel(1,offset,count);if(right>left*1.5f&&right>.0001f)analysisChannel=1;else if(left>right*1.5f&&left>.0001f)analysisChannel=0;}
            for (int i = 0; i < count; ++i) {
                float mono = 0;
                for (int ch = 0; ch < numChannels; ++ch) {
                    float v = buffer.getSample(ch, offset + i);
                    v = std::isfinite(v) ? v : 0;
                    wet.setSample(ch, i, v); if(ch==analysisChannel)mono=v;
                    dry.setSample(ch, i, delay[size_t(ch)][size_t(delayPos)]);
                    delay[size_t(ch)][size_t(delayPos)] = v;
                }
                // Delay length is latency (ring storage includes an unused sentinel).
                if(++delayPos==latency) delayPos=0;
                if(yin.push(mono)) observed=yin.pitch();
            }
            auto p = observed; float desired = 0;
            if (s.enabled && p.hz >= minimumHz() && p.confidence >= 0.85f && (s.midiEnabled||s.scale!=3||s.mask!=0)) {
                float note = midiFromHz(p.hz);
                if(!tracking) { centreFast=centreSlow=note; heldTarget=targetNote(note,s.root,s.scale,s.mask); tracking=true; }
                float fast=float(std::exp(-count/(sr*.05))), slow=float(std::exp(-count/(sr*.4)));
                centreFast=fast*centreFast+(1-fast)*note; centreSlow=slow*centreSlow+(1-slow)*note;
                float target=targetNote(note,s.root,s.scale,s.mask);
                // A small note-boundary dead band avoids repeated flips on jitter.
                if(s.midiEnabled) target=float(s.midiTarget);
                else if(std::abs(targetNote(heldTarget,s.root,s.scale,s.mask)-heldTarget)>.01f || std::abs(note-target)+.15f<std::abs(note-heldTarget) || std::abs(target-heldTarget)>1.1f) heldTarget=target;
                else target=heldTarget;
                const float residual=note-centreFast, driftComponent=centreFast-centreSlow;
                const float vibratoGain=s.vibratoMode==1 ? 1-s.vibratoDepth : 1;
                lfo+=juce::MathConstants<double>::twoPi*s.vibratoRate*count/sr;
                if(lfo>juce::MathConstants<double>::twoPi) lfo-=juce::MathConstants<double>::twoPi;
                const float enhancement=s.vibratoMode==2 ? float(std::sin(lfo))*.2f*s.vibratoDepth : 0;
                if(!s.midiEnabled || s.midiTarget>=0)
                    desired=std::clamp(target-centreSlow-s.drift*driftComponent+(vibratoGain-1)*residual+enhancement,-12.0f,12.0f)*s.amount;
            } else tracking=false;
            float alpha = std::pow(smoothingBase,float(count));
            correction = alpha * correction + (1 - alpha) * desired;
            const float* inputs[2] = {wet.getReadPointer(0), numChannels == 2 ? wet.getReadPointer(1) : nullptr};
            float* outputs[2] = {buffer.getWritePointer(0, offset), numChannels == 2 ? buffer.getWritePointer(1, offset) : nullptr};
            if(mode==Mode::Spectral) {
                stretch.setTransposeSemitones(correction);
                stretch.setFormantBase(p.hz>0 ? p.hz/float(sr) : 0);
                stretch.setFormantSemitones(formantSmooth.skip(count),true);
                stretch.process(inputs,count,outputs,count);
            } else voice.process(inputs,outputs,count,p.hz,correction,mix.isSmoothing() || mix.getCurrentValue()>0);
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
    int analysisChannel=0;
    float centreFast=0,centreSlow=0,heldTarget=0;
    double lfo=0;
    bool tracking=false;
    Yin yin;
    Pitch observed;
    Mode mode=Mode::FullVoice;
    VoiceShifter voice;
    signalsmith::stretch::SignalsmithStretch<float> stretch;
    std::vector<std::vector<float>> delay;
    juce::AudioBuffer<float> wet, dry;
    juce::SmoothedValue<float> mix;
    juce::SmoothedValue<float> formantSmooth;
};
}

