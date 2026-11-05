#include "Engine.h"
#include "AllocationTrace.h"
#include <iostream>
#include <stdexcept>
#include <random>

static void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
static float sine(int i,double sr,float hz) { return .2f*float(std::sin(6.283185307179586*hz*i/sr)); }
// Independent output oracle: positive zero-crossing interpolation on pure tones.
struct Crossings {
    double previous=0,first=0,last=0; int count=0,index=0;
    void push(float v) {
        if(previous<0 && v>=0) { double x=index-1-previous/(v-previous); if(count==0)first=x;last=x;++count; }
        previous=v;++index;
    }
    double hz(double sr) const { return count>1 ? (count-1)*sr/(last-first) : 0; }
};
int main() {
  try {
    constexpr int allowed=(1<<7)|(1<<9)|(1<<10)|(1<<0)|(1<<2)|(1<<3)|(1<<5);
    // Independent exhaustive nearest-neighbour oracle, quarter-cent resolution.
    for(int quarter=-48000;quarter<=50800;quarter+=7) {
        float input=quarter/400.f,expected=input,best=100;
        for(int note=int(std::floor(input))-12;note<=int(std::ceil(input))+12;++note)
            if((allowed & (1<<((note%12+12)%12))) && std::abs(input-note)<best) { expected=float(note);best=std::abs(input-note); }
        float actual=suitunes::targetNote(input,7,2);
        require(actual==expected,"G-minor mapper differs from oracle");
    }
    require(suitunes::targetNote(68,7,2)==67,"Midpoint tie must choose lower G");
    suitunes::TargetTracker tracker;
    require(tracker.update(60.4f,0,0,4095,5,false)==60,"Initial target");
    for(int i=0;i<400;++i)require(tracker.update(i%2?60.52f:60.48f,0,0,4095,5,false)==60,"Boundary chatter");
    for(int i=0;i<4;++i)tracker.update(61.1f,0,0,4095,5,false);
    require(tracker.update(61.1f,0,0,4095,5,false)==61,"Adjacent transition stuck");
    require(tracker.update(72.f,0,0,4095,5,true)==72,"Onset interval blocked");
    for(float semitones:{-12.f,-6.f,-3.f,-1.f,0.f,1.f,3.f,6.f,12.f}) {
        require(std::abs(suitunes::ratioFromSemitones(semitones)-std::pow(2.f,semitones/12))<1e-6,"Ratio math");
        for(bool spectral:{false,true}) {
            constexpr double sr=48000; constexpr int block=64;
            suitunes::VoiceShifter voice; signalsmith::stretch::SignalsmithStretch<float> stretch;
            if(spectral){stretch.presetDefault(1,float(sr));stretch.setTransposeSemitones(semitones);stretch.setFormantSemitones(0,true);stretch.setFormantBase(220.f/float(sr));}
            else voice.prepare(sr,1,768);
            float input[block],output[block]; const float* in[]={input};float* out[]={output};Crossings crossing;
            for(int start=0;start<int(sr*1.5);start+=block) {
                for(int i=0;i<block;++i)input[i]=sine(start+i,sr,220);
                if(spectral)stretch.process(in,block,out,block);else voice.process(in,out,block,220,semitones,true);
                if(start>sr)for(float sample:output)crossing.push(sample);
            }
            double expected=220*std::exp2(semitones/12),error=std::abs(1200*std::log2(crossing.hz(sr)/expected));
            std::cout<<(spectral?"spectral":"voice")<<" shift="<<semitones<<" measured="<<crossing.hz(sr)<<" error_cents="<<error<<'\n';
            require(error<15,"Independent shifter direction/accuracy failed");
        }
    }
    // Same source/decisions with fragmented host buffers: controller must match.
    for(double sr:{44100.,48000.,88200.,96000.}) {
        suitunes::Engine a,b;a.prepare(sr,1);b.prepare(sr,1);
        suitunes::Settings settings;settings.enabled=true;settings.root=7;settings.scale=2;
        juce::AudioBuffer<float> large(1,1024),small(1,1);
        for(int start=0;start<int(sr*.5);start+=1024) {
            for(int i=0;i<1024;++i)large.setSample(0,i,sine(start+i,sr,226));
            for(int i=0;i<1024;++i){small.setSample(0,0,large.getSample(0,i));b.process(small,settings);}
            a.process(large,settings);
            require(std::abs(a.correctionSemitones()-b.correctionSemitones())<1e-5,"Host blocks alter controller trajectory");
            require(a.targetMidi()==b.targetMidi(),"Host blocks alter target");
        }
    }
    for(auto mode:{suitunes::Mode::FullVoice,suitunes::Mode::Live,suitunes::Mode::Spectral}) {
        suitunes::Engine engine;engine.prepare(48000,2,mode);
        allocationTrace::count=0;allocationTrace::enabled=true;engine.reset();allocationTrace::enabled=false;
        require(allocationTrace::count==0,"Transport reset allocated");
    }
    // Deterministic noise rejection and harmonic-rich low/high detection.
    std::mt19937 random(1234);std::uniform_real_distribution<float> noise(-.1f,.1f);
    suitunes::Yin detector;detector.prepare(48000);int voiced=0,hops=0;
    for(int i=0;i<48000;++i)if(detector.push(noise(random))){++hops;if(detector.pitch().hz>0)++voiced;}
    require(voiced==0,"White noise falsely voiced");
    double worst=0;
    for(double sr:{44100.,48000.,88200.,96000.})for(float hz:{65.41f,82.41f,110.f,220.f,440.f,880.f,987.77f}) {
        detector.prepare(sr);
        for(int i=0;i<int(sr*.35);++i) {
            float phase=float(6.283185307179586*hz*i/sr);
            detector.push(.12f*(std::sin(phase)+.8f*std::sin(2*phase)+.6f*std::sin(3*phase)));
        }
        double error=std::abs(1200*std::log2(detector.pitch().hz/hz));worst=std::max(worst,error);
        require(detector.pitch().hz>0 && error<10,"Harmonic detector accuracy");
    }
    std::cout<<"PASS G minor / target dwell / independent ratio direction / buffer-invariant controller / reset allocations / noise rejection; worst detector cents="<<worst<<'\n';
    return 0;
  } catch(const std::exception& error){allocationTrace::enabled=false;std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
