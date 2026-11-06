#include "Engine.h"
#include "AllocationTrace.h"
#include "NeuralPitchBridge.h"
#include <iostream>
#include <stdexcept>
static void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
int main(){try{
    suitunes::NeuralPitchBridge bridge;
    check(bridge.choose({220,.9f},100,20,1).hz==220,"Missing worker fallback");
    bridge.results.push({{440,.95f},80,1});
    check(bridge.choose({220,.9f},100,20,1).hz==440,"Fresh neural result");
    check(bridge.choose({220,.9f},101,20,1).hz==220,"Stale neural result fallback");
    bridge.results.push({{880,.95f},100,1});
    check(bridge.choose({220,.9f},101,20,2).hz==220,"Old transport epoch leaked");
    for(int i=0;i<3;++i)check(bridge.results.push({{440,.95f},100,2}),"Queue capacity");
    check(!bridge.results.push({{440,.95f},100,2}),"Queue must drop on overflow");
    bridge.choose({220,.9f},101,20,2);
    bridge.results.push({{440,.1f},150,2});
    bridge.results.push({{440,.95f},250,2});
    check(bridge.choose({220,.9f},160,20,2).hz==220,"Low confidence/future results must fall back");
    suitunes::PitchContinuity tracker;
    check(tracker.update({220,.8f},5,false,65).hz==0,"Weak acquisition");
    check(tracker.update({220,.95f},5,false,65).hz==220,"Acquisition");
    for(int i=0;i<3;++i)check(tracker.update({},5,false,65).hz==220 && tracker.isCarried(),"Brief dropout continuity");
    check(tracker.update({},5,false,65).hz==0,"Dropout must expire");
    tracker.update({220,.95f},5,false,65);
    check(tracker.update({},5,true,65).hz==0,"Onset must invalidate extrapolation");
    tracker.reset();check(tracker.update({},5,false,65).hz==0,"Reset retained pitch");
    suitunes::TargetTracker target;
    target.update(51,7,2,4095,5,true);
    for(int i=0;i<100;++i)check(target.update(i%2 ? 52.1f:51.9f,7,2,4095,5,false)==51,"Scale midpoint chatter");
    for(int i=0;i<10;++i)check(target.update(52.2f,7,2,4095,5,false,.8f)==51,"Uncertain target acquired prematurely");
    for(int i=0;i<10;++i)target.update(52.8f,7,2,4095,5,false,.95f);
    check(target.update(52.8f,7,2,4095,5,false,.95f)==53,"Confirmed transition stalled");
    check(target.update(52.8f,7,3,1<<7,5,false)==55,"New custom mask retained excluded target");
    double worstStep=0;
    for(double sr:{44100.,48000.,96000.}){
        suitunes::VoiceShifter shifter;shifter.prepare(sr,2,int(std::ceil(sr*.016)));
        float input[2],output[2];const float* in[]={input,input+1};float* out[]={output,output+1};
        double phase=0;float last=0;
        allocationTrace::count=0;allocationTrace::enabled=true;
        for(int i=0;i<int(sr*2);++i){
            float hz=180+80*std::sin(float(i/sr)*5);
            phase+=6.283185307179586*hz/sr;
            input[0]=input[1]=.2f*float(std::sin(phase));
            float shift=(i/int(sr*.1))%2 ? 3.f:-3.f;
            shifter.process(in,out,1,hz,shift,true);
            check(std::isfinite(output[0]) && std::abs(output[0])<.3f,"Unbounded resynthesis");
            check(output[0]==output[1],"Stereo splice phase diverged");
            if(i>sr*.1)worstStep=std::max(worstStep,double(std::abs(output[0]-last)));
            last=output[0];
        }
        allocationTrace::enabled=false;
        check(allocationTrace::count==0,"Grain processing allocated");
    }
    std::cout<<"maximum_adjacent_sample_step="<<worstStep<<'\n';
    check(worstStep<.06,"Abrupt splice on swept sine / ratio reversal");
    // A real octave must eventually acquire; short harmonic ambiguity must not
    // permanently pin the detector. This does not stand in for sung intervals.
    suitunes::Yin yin;yin.prepare(48000);
    for(int i=0;i<48000;++i)yin.push(.2f*float(std::sin(6.283185307179586*220*i/48000)));
    for(int i=0;i<24000;++i)yin.push(.2f*float(std::sin(6.283185307179586*440*i/48000)));
    check(std::abs(1200*std::log2(yin.pitch().hz/440))<10,"True octave remained pinned");
    std::cout<<"PASS bounded dropout, confidence acquisition, onset/reset, frozen grains, stereo coherence, no allocations, true octave\n";
    return 0;
}catch(const std::exception& e){allocationTrace::enabled=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
