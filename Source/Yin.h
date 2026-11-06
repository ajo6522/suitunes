#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>

namespace suitunes {
struct Pitch { float hz = 0, confidence = 0; };
struct PitchCandidate { float hz=0, confidence=0; };
// Analyse a filtered ~12 kHz signal, not every high-rate input sample.
// Power-of-two rings, contiguous analysis and bounded storage avoid modulo
// divisions in the expensive lag loop. All filter design happens in prepare().
class Yin {
public:
    void prepare(double rate) {
        factor = std::clamp(int(rate / 12000), 1, 16); sr = rate / factor;
        taps = factor * 16 + 1; filter.fill(0); history.fill(0); ring.fill(0);
        inputPos = write = filled = decimation = sinceAnalysis = 0; result = {};
        candidateCount=octaveHold=unvoicedHops=0; previousHz=previousRms=rms=0; onset=octaveRejected=false;
        window = std::clamp(int(std::ceil(sr * 0.043)), 256, 1024);
        hop = std::max(1, int(std::round(sr * 0.005)));
        double sum = 0, cutoff = 0.40 / factor;
        for(int i=0;i<taps;++i) {
            double x = i - (taps - 1)*0.5;
            double h = x == 0 ? 2*cutoff : std::sin(2*juce::MathConstants<double>::pi*cutoff*x)/(juce::MathConstants<double>::pi*x);
            h *= 0.42 - 0.5*std::cos(2*juce::MathConstants<double>::pi*i/(taps-1)) + 0.08*std::cos(4*juce::MathConstants<double>::pi*i/(taps-1));
            filter[size_t(i)] = float(h); sum += h;
        }
        for(int i=0;i<taps;++i) filter[size_t(i)] /= float(sum);
    }
    bool push(float v) {
        history[size_t(inputPos)] = std::isfinite(v) ? v : 0; inputPos = (inputPos + 1) & 511;
        if(++decimation < factor) return false; decimation = 0;
        float filtered = 0;
        for(int j=0;j<taps;++j) filtered += filter[size_t(j)] * history[size_t((inputPos - 1 - j) & 511)];
        ring[size_t(write)] = filtered; write = (write + 1) & 1023; filled = std::min(filled+1,window);
        if(++sinceAnalysis < hop) return false; sinceAnalysis = 0;
        if(filled < window) { result = {}; return true; }
        for(int j=0;j<window;++j) contiguous[size_t(j)] = ring[size_t((write-window+j)&1023)];
        result = analyse(); return true;
    }
    Pitch pitch() const { return result; }
    void reset() {
        history.fill(0); ring.fill(0); inputPos=write=filled=decimation=sinceAnalysis=0;
        result={}; candidateCount=octaveHold=unvoicedHops=0; previousHz=previousRms=rms=0; onset=octaveRejected=false;
    }
    const std::array<PitchCandidate,16>& pitchCandidates() const { return candidates; }
    int numCandidates() const { return candidateCount; }
    float rmsLevel() const { return rms; }
    bool isOnset() const { return onset; }
    bool rejectedOctave() const { return octaveRejected; }
    float hopMilliseconds() const { return float(1000*hop/sr); }
private:
    Pitch analyse() {
        candidateCount=0; octaveRejected=false;
        double power=0; for(int i=0;i<window;++i) power+=contiguous[size_t(i)]*contiguous[size_t(i)];
        rms=float(std::sqrt(power/window));
        onset=rms>0.003f && rms>previousRms*1.8f;
        previousRms=rms;
        const int half = window/2;
        int minLag=std::max(2,int(sr/1000)), maxLag=std::min(half-2,int(sr/65));
        float energy = juce::FloatVectorOperations::findMinAndMax(contiguous.data(),window).getLength();
        if(energy < 0.002f || rms<0.0005f) { if(++unvoicedHops>=4)previousHz=0; return {}; }
        double sum=0; difference[0]=1;
        for(int lag=1;lag<=maxLag+1;++lag) {
            juce::FloatVectorOperations::subtract(delta.data(),contiguous.data(),contiguous.data()+lag,half);
            juce::FloatVectorOperations::multiply(delta.data(),delta.data(),half);
            float d=0; for(int j=0;j<half;++j) d+=delta[size_t(j)];
            sum+=d; difference[size_t(lag)] = sum>0 ? float(d*lag/sum) : 1;
        }
        for(int lag=minLag;lag<=maxLag;++lag) {
            if(difference[size_t(lag)]>=(previousHz>0 && !onset ? .22f : .15f)) continue;
            if(lag>minLag && difference[size_t(lag)]>difference[size_t(lag-1)]) continue;
            while(lag<maxLag && difference[size_t(lag+1)]<difference[size_t(lag)]) ++lag;
            float a=difference[size_t(lag-1)],b=difference[size_t(lag)],c=difference[size_t(lag+1)],denom=a-2*b+c;
            float offset=std::abs(denom)>1e-9f ? std::clamp(0.5f*(a-c)/denom,-0.5f,0.5f) : 0;
            if(candidateCount<int(candidates.size())) candidates[size_t(candidateCount++)]={float(sr/(lag+offset)),1-b};
        }
        if(candidateCount==0) { if(++unvoicedHops>=4)previousHz=0; return {}; }
        unvoicedHops=0;
        auto chosen=candidates[0];
        // Continuity resolves brief octave ambiguity, never pins a singer forever.
        if(previousHz>0 && !onset) {
            const float jump=std::abs(12*std::log2(chosen.hz/previousHz));
            if(jump>10.5f && jump<13.5f && octaveHold<8) {
                for(int i=1;i<candidateCount;++i) {
                    auto c=candidates[size_t(i)];
                    if(std::abs(12*std::log2(c.hz/previousHz))<.65f && c.confidence>=chosen.confidence-.08f) {
                        chosen=c; octaveRejected=true; break;
                    }
                }
            }
        }
        if(octaveRejected) ++octaveHold; else octaveHold=0;
        previousHz=chosen.hz;
        return {chosen.hz,chosen.confidence};
    }
    double sr=12000; int factor=4,taps=65,inputPos=0,write=0,filled=0,decimation=0,sinceAnalysis=0,window=516,hop=60;
    std::array<float,257> filter{};
    std::array<float,512> history{};
    std::array<float,1024> ring{},contiguous{},delta{},difference{};
    Pitch result;
    std::array<PitchCandidate,16> candidates{};
    int candidateCount=0,octaveHold=0,unvoicedHops=0;
    float rms=0,previousRms=0,previousHz=0;
    bool onset=false,octaveRejected=false;
};
}
