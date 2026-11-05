#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>
#include <cmath>

namespace suitunes {
// Two overlap-added, cubic-interpolated read heads. Their delay separation is
// one detected fundamental period, so steady periodic waveforms meet in phase.
// Fixed centre latency permits host compensation; the read excursion is ±one
// period. This is monophonic and does NOT promise formant preservation.
class VoiceShifter {
public:
    void prepare(double rate, int channels, int latencySamples) {
        sr=rate; count=channels; latency=latencySamples;
        int size=1; while(size<latency*2+16) size*=2;
        mask=size-1; ring.assign(size_t(channels),std::vector<float>(size_t(size),0));
        write=0; phase=0.5; period=rate/220; ratio=1;
        periodAlpha=std::exp(-1.0/(rate*0.03));
        for(size_t i=0;i<weights.size();++i) weights[i]=float(0.5-0.5*std::cos(2*juce::MathConstants<double>::pi*i/(weights.size()-1)));
    }
    void process(const float* const* input, float* const* output, int samples, float hz, float semitones, bool audible) {
        const double targetPeriod=std::clamp(hz>0 ? sr/hz : period, sr/1000, double(latency-4));
        const double targetRatio=std::pow(2.0,double(semitones)/12);
        const double ratioStep=(targetRatio-ratio)/std::max(1,samples);
        for(int i=0;i<samples;++i) {
            for(int c=0;c<count;++c) ring[size_t(c)][size_t(write)]=input[c][i];
            ratio+=ratioStep; period=periodAlpha*period+(1-periodAlpha)*targetPeriod;
            if(audible) {
                phase+=(1-ratio)/(2*period); phase-=std::floor(phase);
                double second=phase+0.5; if(second>=1) second-=1;
                double wi=phase*(weights.size()-1); int index=int(wi);
                float weight=weights[size_t(index)]+float(wi-index)*(weights[size_t(index+1)]-weights[size_t(index)]);
                float da=float(latency+(phase-0.5)*2*period), db=float(latency+(second-0.5)*2*period);
                // Exact dry unity prevents combing when no correction is needed.
                float active=float(std::min(1.0,std::abs(ratio-1)*1000));
                for(int c=0;c<count;++c) {
                    float dry=ring[size_t(c)][size_t((write-latency)&mask)];
                    float shifted=read(c,da)*weight+read(c,db)*(1-weight);
                    output[c][i]=dry+active*(shifted-dry);
                }
            } else for(int c=0;c<count;++c) output[c][i]=0;
            write=(write+1)&mask;
        }
        ratio=targetRatio;
    }
    void reset() {
        for(auto& channel:ring) std::fill(channel.begin(),channel.end(),0.f);
        write=0; phase=.5; period=sr/220; ratio=1;
    }
private:
    float read(int channel,float delay) const {
        double position=write-double(delay); int base=int(std::floor(position)); float f=float(position-base);
        auto& r=ring[size_t(channel)];
        float a=r[size_t((base-1)&mask)], b=r[size_t(base&mask)], c=r[size_t((base+1)&mask)], d=r[size_t((base+2)&mask)];
        return b+0.5f*f*(c-a+f*(2*a-5*b+4*c-d+f*(3*(b-c)+d-a)));
    }
    double sr=48000,phase=0.5,period=218,ratio=1,periodAlpha=0;
    int count=2,latency=768,mask=2047,write=0;
    std::vector<std::vector<float>> ring;
    std::array<float,2049> weights{};
};
}
