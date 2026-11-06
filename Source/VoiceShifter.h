#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>
#include <cmath>

namespace suitunes {
// Pitch-period overlap-add with continuous cubic-interpolated grain reads.
// Re-anchor only near the zero of each Hann window, using bounded waveform
// matching. Reported centre latency is fixed; shifted transients can move
// inside the grain excursion. This is not full epoch-tracked TD-PSOLA and does
// NOT provide low-delay formant preservation. Quality/offline use Signalsmith.
class VoiceShifter {
public:
    void prepare(double rate, int channels, int latencySamples) {
        sr=rate; count=channels; latency=latencySamples;
        int size=1; while(size<latency*2+16) size*=2;
        mask=size-1; ring.assign(size_t(channels),std::vector<float>(size_t(size),0));
        write=0; phase=0.5; period=rate/220; ratio=1; activeMix=0;
        headDelay={double(latency),double(latency)-period};
        mixAlpha=std::exp(-1.0/(rate*.005));
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
                const double old=phase;
                phase+=(1-ratio)/(2*period); phase-=std::floor(phase);
                double second=phase+0.5; if(second>=1) second-=1;
                // Each grain reads at a continuous speed. Updating the F0 must
                // not reposition an audible head. Re-anchor only at zero weight.
                // F0 modulation and repeated ratio reversals can accumulate
                // phase/delay drift. Saturation keeps every read in recorded
                // history until its next silent re-anchor (never ring aliasing).
                for(auto& d:headDelay) d=std::clamp(d+1-ratio,4.0,double(2*latency-4));
                const bool wrapA=std::abs(phase-old)>.5;
                const bool wrapB=(old<.5)!=(phase<.5) && !wrapA;
                if(wrapA) headDelay[0]=matchDelay(latency+(phase-.5)*2*period,headDelay[1]);
                if(wrapB) headDelay[1]=matchDelay(latency+(second-.5)*2*period,headDelay[0]);
                double wi=phase*(weights.size()-1); int index=int(wi);
                float weight=weights[size_t(index)]+float(wi-index)*(weights[size_t(index+1)]-weights[size_t(index)]);
                float da=float(headDelay[0]), db=float(headDelay[1]);
                // Exact dry unity prevents combing when no correction is needed.
                activeMix=mixAlpha*activeMix+(1-mixAlpha)*std::min(1.0,std::abs(ratio-1)*1000);
                float active=float(activeMix);
                for(int c=0;c<count;++c) {
                    float dry=ring[size_t(c)][size_t((write-latency)&mask)];
                    float shifted=read(c,da)*weight+read(c,db)*(1-weight);
                    output[c][i]=dry+active*(shifted-dry);
                }
            } else { for(int c=0;c<count;++c) output[c][i]=0;
                headDelay={double(latency)+(phase-.5)*2*period,double(latency)+(phase<.5?phase:phase-1)*2*period};
            }
            write=(write+1)&mask;
        }
        ratio=targetRatio;
    }
    void reset() {
        for(auto& channel:ring) std::fill(channel.begin(),channel.end(),0.f);
        write=0; phase=.5; period=sr/220; ratio=1; activeMix=0;
        headDelay={double(latency),double(latency)-period};
    }
private:
    double matchDelay(double nominal,double reference) const {
        // Bounded waveform/slope matching around the predicted pitch mark.
        // One reference channel sets a common splice for every stereo channel.
        // No locks, allocation, FFT planning or unbounded searches here.
        double best=std::clamp(nominal,4.0,double(2*latency-4)),costBest=1e30;
        const int radius=std::min(24,int(period*.08));
        for(int shift=-radius;shift<=radius;++shift) {
            double candidate=nominal+shift;
            if(candidate<4 || candidate>2*latency-4) continue;
            double cost=1e-8*shift*shift;
            for(int k=-4;k<=4;++k) {
                double delta=read(0,float(candidate+k))-read(0,float(reference+k));
                cost+=delta*delta;
            }
            if(cost<costBest){costBest=cost;best=candidate;}
        }
        return best;
    }
    float read(int channel,float delay) const {
        double position=write-double(delay); int base=int(std::floor(position)); float f=float(position-base);
        auto& r=ring[size_t(channel)];
        float a=r[size_t((base-1)&mask)], b=r[size_t(base&mask)], c=r[size_t((base+1)&mask)], d=r[size_t((base+2)&mask)];
        return b+0.5f*f*(c-a+f*(2*a-5*b+4*c-d+f*(3*(b-c)+d-a)));
    }
    double sr=48000,phase=0.5,period=218,ratio=1,periodAlpha=0,mixAlpha=0,activeMix=0;
    std::array<double,2> headDelay{};
    int count=2,latency=768,mask=2047,write=0;
    std::vector<std::vector<float>> ring;
    std::array<float,2049> weights{};
};
}
