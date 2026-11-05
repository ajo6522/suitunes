#include "Engine.h"
#include "BaselineEngine.h"
#include <chrono>
#include <iostream>
#include <iomanip>
#include <type_traits>

using Clock = std::chrono::steady_clock;
constexpr double pi = 3.14159265358979323846;
static float source(int t, double sr, double hz, bool harmonic) {
    double phase = 2*pi*hz*t/sr;
    return float(harmonic ? 0.14*(std::sin(phase)+0.6*std::sin(2*phase)+0.4*std::sin(3*phase)+0.2*std::sin(5*phase)) : 0.25*std::sin(phase));
}
template<class Engine, class Settings> void timing(const char* name, double sr=48000, int block=128, bool enabled=true,suitunes::Mode mode=suitunes::Mode::FullVoice) {
    Engine e;if constexpr(std::is_same_v<Engine,suitunes::Engine>)e.prepare(sr,2,mode);else e.prepare(sr,2);Settings s;s.enabled=enabled;
    juce::AudioBuffer<float> b(2,block); std::vector<double> times; times.reserve(int(sr*3/block)+1);
    double total=0; int missed=0;
    for(int start=-int(sr*.25);start<int(sr*3);start+=block) {
        for(int i=0;i<block;++i) for(int c=0;c<2;++c) b.setSample(c,i,source(start+i,sr,223.84,true));
        auto t=Clock::now(); e.process(b,s);
        double us=std::chrono::duration<double,std::micro>(Clock::now()-t).count(); if(start>=0) { times.push_back(us); total+=us; if(us>block/sr*1e6)++missed; }
    }
    std::sort(times.begin(),times.end());
    std::cout<<name<<",sr="<<sr<<",block="<<block<<",enabled="<<enabled<<",delay_ms="<<1000.0*e.latencySamples()/sr<<",cpu_percent="<<total/(double(times.size()*block)/sr*10000)<<",mean_us="<<total/times.size()<<",p95_us="<<times[size_t(times.size()*.95)]<<",missed_deadlines="<<missed<<",p99_us="<<times[size_t(times.size()*.99)]<<",max_us="<<times.back()<<std::endl;
}
int main(int argc,char**) {
    std::cout<<std::fixed<<std::setprecision(3);
    timing<suitunes_baseline::Engine,suitunes_baseline::Settings>("baseline");
    timing<suitunes::Engine,suitunes::Settings>("current");
    if(argc==1) {
        for(double sr : {44100.0,48000.0,88200.0,96000.0}) for(int block : {32,64,128,512,1024}) {
            timing<suitunes::Engine,suitunes::Settings>("full-voice",sr,block,true);
            timing<suitunes::Engine,suitunes::Settings>("live",sr,block,true,suitunes::Mode::Live);
            timing<suitunes::Engine,suitunes::Settings>("quality",sr,block,true,suitunes::Mode::Spectral);
        }
    }
    if(argc==1) return 0;
    std::cout<<"window_ms,hop_divisor,reported_ms,worst_cents,minimum_output_rms,cpu_percent\n";
    for(double ms : {5.0,7.5,10.0,15.0,20.0,30.0,40.0,60.0,80.0,120.0}) for(int divisor : {4,8}) {
        double worst=0,minRms=1,totalUs=0,totalSeconds=0; int latency=0;
        for(bool harmonic : {false,true}) for(double target : {82.4069,110.0,220.0,440.0,880.0}) for(double cents : {-30.0,30.0}) {
            constexpr int sr=48000, count=64;
            signalsmith::stretch::SignalsmithStretch<float> stretch;
            int window=int(sr*ms*.001); stretch.configure(1,window,std::max(1,window/divisor));
            latency=stretch.inputLatency()+stretch.outputLatency(); stretch.setTransposeSemitones(float(-cents/100));
            float input[count],output[count]; const float* ins[]={input}; float* outs[]={output};
            suitunes::Yin y; y.prepare(sr); double energy=0; int n=0;
            double hz=target*std::pow(2.0,cents/1200);
            for(int start=0;start<sr;start+=count) {
                for(int i=0;i<count;++i) input[i]=source(start+i,sr,hz,harmonic);
                auto t=Clock::now(); stretch.process(ins,count,outs,count);
                totalUs+=std::chrono::duration<double,std::micro>(Clock::now()-t).count();
                for(int i=0;i<count;++i) if(start>sr*3/4) { y.push(output[i]); energy+=output[i]*output[i];++n; }
            }
            double error=y.pitch().hz>0 ? std::abs(1200*std::log2(y.pitch().hz/target)) : 9999;
            worst=std::max(worst,error); minRms=std::min(minRms,std::sqrt(energy/std::max(1,n))); totalSeconds+=1;
        }
        std::cout<<ms<<','<<divisor<<','<<1000.0*latency/48000<<','<<worst<<','<<minRms<<','<<totalUs/1e4/totalSeconds<<std::endl;
    }
}
