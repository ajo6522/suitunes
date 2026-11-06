#include "Engine.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <fstream>
#include <iostream>

// Offline only: no disk I/O or trace formatting is called from processBlock.
int main(int argc,char** argv) {
    if(argc<3) { std::cerr<<"Usage: PitchPopDiagnostics dry.wav output-directory [mode:0/1/2] [speed-ms] [strength:0..1] [preserve-formants:0/1]\nG natural minor, A4=440, formant compensated by default in mode 2.\n"; return 2; }
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    auto reader=std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(juce::File(argv[1])));
    if(!reader || reader->lengthInSamples<=0 || reader->lengthInSamples>int64_t(reader->sampleRate*600)) { std::cerr<<"Expected WAV <=10 minutes\n"; return 2; }
    auto folder=juce::File(argv[2]); folder.createDirectory();
    const int channels=std::min(2,int(reader->numChannels)),length=int(reader->lengthInSamples);
    const double sr=reader->sampleRate;
    const auto mode=static_cast<suitunes::Mode>(argc>3 ? std::clamp(std::atoi(argv[3]),0,2) : 2);
    suitunes::Engine engine; engine.prepare(sr,channels,mode);
    suitunes::Settings settings; settings.enabled=true; settings.root=7; settings.scale=2;
    if(argc>4)settings.retuneMs=std::clamp(float(std::atof(argv[4])),1.f,500.f);
    if(argc>5)settings.amount=std::clamp(float(std::atof(argv[5])),0.f,1.f);
    if(argc>6)settings.preserveFormants=std::atoi(argv[6])!=0;
    const int latency=engine.latencySamples(),block=64;
    juce::AudioBuffer<float> input(channels,length),work(channels,block),rendered(channels,length);
    if(!reader->read(&input,0,length,0,true,true)) return 3;
    rendered.clear();
    std::ofstream trace(folder.getChildFile("trace.csv").getFullPathName().toStdString());
    trace<<"sample_position,rms,voiced_confidence,onset,candidates_hz,candidates_confidence,chosen_f0_hz,detected_midi,octave_rejected,key,scale_mask,target_midi,requested_cents,applied_cents,shift_ratio,formant_semitones,preserve_formants,latency_samples\n";
    uint64_t previous=0;
    for(int start=0;start<length+latency;start+=block) {
        work.clear();
        if(start<length)for(int c=0;c<channels;++c)work.copyFrom(c,0,input,c,start,std::min(block,length-start));
        engine.process(work,settings);
        const auto& d=engine.detector(); const auto p=engine.pitch();
        if(engine.analysisSample()!=previous) {
            previous=engine.analysisSample();
            trace<<previous<<','<<d.rmsLevel()<<','<<p.confidence<<','<<d.isOnset()<<",\"";
            for(int j=0;j<d.numCandidates();++j){if(j)trace<<';';trace<<d.pitchCandidates()[size_t(j)].hz;}
            trace<<"\",\"";
            for(int j=0;j<d.numCandidates();++j){if(j)trace<<';';trace<<d.pitchCandidates()[size_t(j)].confidence;}
            trace<<"\","<<p.hz<<','<<suitunes::midiFromHz(p.hz)<<','<<d.rejectedOctave()<<",G,1709,"<<engine.targetMidi()<<','<<100*engine.requestedSemitones()<<','<<100*engine.correctionSemitones()<<','<<suitunes::ratioFromSemitones(engine.correctionSemitones())<<",0,"<<settings.preserveFormants<<','<<latency<<'\n';
        }
        for(int i=0;i<block;++i){int dst=start+i-latency;if(dst>=0&&dst<length)for(int c=0;c<channels;++c)rendered.setSample(c,dst,work.getSample(c,i));}
    }
    auto output=folder.getChildFile("corrected.wav").createOutputStream();
    // JUCE opens existing output streams at EOF. A rerun must replace the WAV,
    // otherwise the final header is written over the beginning of appended data.
    if(!output || !output->setPosition(0) || output->truncate().failed())return 3;
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer(output ? wav.createWriterFor(output.get(),sr,unsigned(channels),24,{},0) : nullptr);
    if(!writer)return 3; output.release();
    if(!writer->writeFromAudioSampleBuffer(rendered,0,length) || !trace.good())return 3;
    std::cout<<"Aligned corrected.wav and trace.csv; latency="<<latency<<" samples; mode="<<int(mode)<<"; speed="<<settings.retuneMs<<"; strength="<<settings.amount<<"\n";
    return 0;
}
