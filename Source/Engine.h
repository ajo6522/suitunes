#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <signalsmith-stretch/signalsmith-stretch.h>
#include <array>
#include <cmath>
#include <vector>
#include "Yin.h"
#include "VoiceShifter.h"
#include "PitchControl.h"
#include "PitchContinuity.h"

namespace suitunes {
struct Settings {
    bool enabled=false, midiEnabled=false, preserveFormants=true;
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
            // Split spectral work across callbacks; the extra interval is included
            // in outputLatency(), rather than hiding high-rate deadline bursts.
            stretch.presetDefault(channels, float(rate), true);
            latency = stretch.inputLatency() + stretch.outputLatency();
        } else {
            latency=int(std::ceil(rate*(mode==Mode::Live ? 0.010 : 0.016)));
            voice.prepare(rate,channels,latency);
        }
        delay.assign(size_t(channels), std::vector<float>(size_t(latency + 1), 0));
        wet.setSize(channels, quantum); dry.setSize(channels, quantum);
        delayPos = 0; correction = 0; mix.reset(rate, 0.02); mix.setCurrentAndTargetValue(0);
        observed={}; continuity.reset();
        samplePosition=lastAnalysisSample=0;
        analysisChannel=0;
        centreFast=centreSlow=heldTarget=desired=heldCorrection=0; tracking=false; lfo=0; targetTracker.reset();
        formantSmooth.reset(rate,0.03); formantSmooth.setCurrentAndTargetValue(0);
    }
    int latencySamples() const { return latency; }
    Pitch pitch() const { return observed; }
    Mode currentMode() const { return mode; }
    float minimumHz() const { return mode==Mode::Live ? 110.0f : 65.0f; }
    float correctionSemitones() const { return correction; }
    float targetMidi() const { return tracking ? heldTarget : 0; }
    const Yin& detector() const { return yin; }
    uint64_t analysisSample() const { return lastAnalysisSample; }
    float requestedSemitones() const { return desired; }
    void reset() {
        yin.reset(); continuity.reset(); targetTracker.reset(); observed={}; tracking=false;
        desired=heldCorrection=correction=centreFast=centreSlow=heldTarget=0; lfo=0;
        delayPos=0; samplePosition=lastAnalysisSample=0;
        for(auto& channel:delay) std::fill(channel.begin(),channel.end(),0.f);
        if(mode==Mode::Spectral) stretch.reset(); else voice.reset();
        mix.setCurrentAndTargetValue(0);
    }
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
                ++samplePosition;
                if(yin.push(mono)) { observed=yin.pitch(); updateTarget(s); lastAnalysisSample=samplePosition; }
                correction=smoothingBase*correction+(1-smoothingBase)*desired;
            }
            auto p = observed;
            const float* inputs[2] = {wet.getReadPointer(0), numChannels == 2 ? wet.getReadPointer(1) : nullptr};
            float* outputs[2] = {buffer.getWritePointer(0, offset), numChannels == 2 ? buffer.getWritePointer(1, offset) : nullptr};
            if(mode==Mode::Spectral) {
                stretch.setTransposeSemitones(correction);
                stretch.setFormantBase(p.hz>0 ? p.hz/float(sr) : 0);
                stretch.setFormantSemitones(formantSmooth.skip(count),s.preserveFormants);
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
    void updateTarget(const Settings& s) {
        const auto p=continuity.update(observed,yin.hopMilliseconds(),yin.isOnset(),minimumHz());
        if(!s.enabled || p.hz<minimumHz() ||
           (s.midiEnabled ? s.midiTarget<0 : s.scale==3 && s.mask==0)) {
            desired=heldCorrection=0; tracking=false; targetTracker.reset(); continuity.reset(); return;
        }
        // A carried frame preserves the last trajectory, never votes for a note.
        // Parameter restrictions still take effect immediately during a dropout.
        if(continuity.isCarried() && tracking && !s.midiEnabled &&
           noteAllowed(int(std::round(heldTarget)),s.root,s.scale,s.mask)) { desired=heldCorrection*s.amount; return; }
        const float note=midiFromHz(p.hz), dt=yin.hopMilliseconds()*.001f;
        if(!tracking || yin.isOnset()) centreFast=centreSlow=note;
        tracking=true;
        const float fast=std::exp(-dt/.05f),slow=std::exp(-dt/.4f);
        centreFast=fast*centreFast+(1-fast)*note;
        centreSlow=slow*centreSlow+(1-slow)*note;
        heldTarget=s.midiEnabled ? float(s.midiTarget) : targetTracker.update(note,s.root,s.scale,s.mask,dt*1000,yin.isOnset(),p.confidence);
        const float residual=note-centreFast, driftComponent=centreFast-centreSlow;
        const float vibratoGain=s.vibratoMode==1 ? 1-s.vibratoDepth : 1;
        lfo=std::fmod(lfo+juce::MathConstants<double>::twoPi*s.vibratoRate*dt,juce::MathConstants<double>::twoPi);
        const float enhancement=s.vibratoMode==2 ? float(std::sin(lfo))*.2f*s.vibratoDepth : 0;
        heldCorrection=std::clamp(heldTarget-centreSlow-s.drift*driftComponent+(vibratoGain-1)*residual+enhancement,-12.0f,12.0f);
        desired=heldCorrection*s.amount;
    }
    static constexpr int quantum = 64;
    double sr = 48000; int numChannels = 2, latency = 0, delayPos = 0;
    float correction = 0;
    int analysisChannel=0;
    float centreFast=0,centreSlow=0,heldTarget=0,desired=0,heldCorrection=0;
    TargetTracker targetTracker;
    uint64_t samplePosition=0,lastAnalysisSample=0;
    double lfo=0;
    bool tracking=false;
    Yin yin;
    PitchContinuity continuity;
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

