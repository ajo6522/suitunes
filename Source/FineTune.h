#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include "Engine.h"
#include <functional>

namespace suitunes {
struct PitchFrame { double time=0; float midi=0,confidence=0; };
struct NoteRegion { double start=0,end=0; float source=0,target=0,rms=0; bool edited=false; };
struct EditSnapshot { std::vector<NoteRegion> notes; double start=0,end=0; };
class FineTuneSession {
public:
    std::shared_ptr<const juce::AudioBuffer<float>> original, rendered;
    double rate=48000, selectionStart=0,selectionEnd=0;
    juce::File sourceFile;
    std::vector<PitchFrame> frames;
    std::vector<NoteRegion> notes;
    std::vector<float> peaks;
    std::vector<EditSnapshot> undoHistory,redoHistory;
    double duration() const { return original ? original->getNumSamples()/rate : 0; }
    void remember() { undoHistory.push_back({notes,selectionStart,selectionEnd}); if(undoHistory.size()>100) undoHistory.erase(undoHistory.begin()); redoHistory.clear(); }
    void undo() {
        if(undoHistory.empty()) return;
        redoHistory.push_back({notes,selectionStart,selectionEnd}); auto s=std::move(undoHistory.back()); undoHistory.pop_back();
        notes=std::move(s.notes); selectionStart=s.start; selectionEnd=s.end;
    }
    void redo() {
        if(redoHistory.empty()) return;
        undoHistory.push_back({notes,selectionStart,selectionEnd}); auto s=std::move(redoHistory.back()); redoHistory.pop_back();
        notes=std::move(s.notes); selectionStart=s.start; selectionEnd=s.end;
    }
    void splitAt(double time,bool history=true) {
        for(size_t i=0;i<notes.size();++i) if(time>notes[i].start+.001 && time<notes[i].end-.001) {
            if(history) remember(); auto right=notes[i]; notes[i].end=time; right.start=time; notes.insert(notes.begin()+int(i)+1,right); return;
        }
    }
    void autoCorrect(int key,int scale,int mask) {
        if(selectionEnd<=selectionStart) return;
        remember(); splitAt(selectionStart,false); splitAt(selectionEnd,false);
        for(auto& n:notes) if(n.start>=selectionStart-1e-9 && n.end<=selectionEnd+1e-9) { n.target=targetNote(n.source,key,scale,mask); n.edited=true; }
    }
    static juce::Result importWav(const juce::File& file,FineTuneSession& session,const std::function<bool()>& cancelled={}) {
        juce::WavAudioFormat wav; auto stream=file.createInputStream();
        if(!stream) return juce::Result::fail("Cannot open WAV.");
        std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(stream.release(),true));
        if(!reader || reader->sampleRate<8000 || reader->sampleRate>192000 || reader->numChannels<1 || reader->numChannels>2 || reader->lengthInSamples<=0
            || reader->lengthInSamples>200000000 || reader->lengthInSamples*reader->numChannels*sizeof(float)>512*1024*1024)
            return juce::Result::fail("Use a mono/stereo WAV, 8–192 kHz, within the 512 MiB decoded-audio limit.");
        auto audio=std::make_shared<juce::AudioBuffer<float>>(int(reader->numChannels),int(reader->lengthInSamples));
        if(!reader->read(audio.get(),0,audio->getNumSamples(),0,true,true)) return juce::Result::fail("WAV read failed.");
        session=FineTuneSession{}; session.original=audio; session.rendered=audio; session.rate=reader->sampleRate; session.sourceFile=file;
        session.selectionEnd=session.duration();
        Yin detector; detector.prepare(session.rate);
        const int samples=audio->getNumSamples(); session.peaks.resize(size_t((samples+255)/256));
        int analysisChannel=0;if(audio->getNumChannels()==2&&audio->getRMSLevel(1,0,samples)>audio->getRMSLevel(0,0,samples)*1.5f)analysisChannel=1;
        for(int i=0;i<samples;++i) {
            if((i&4095)==0 && cancelled && cancelled()) return juce::Result::fail("Cancelled");
            float mono=0; for(int c=0;c<audio->getNumChannels();++c) { float v=audio->getSample(c,i); if(!std::isfinite(v)) { v=0; audio->setSample(c,i,0); } if(c==analysisChannel)mono=v; }
            session.peaks[size_t(i/256)]=std::max(session.peaks[size_t(i/256)],std::abs(mono));
            if(detector.push(mono)) {
                auto p=detector.pitch(); const double t=std::max(0.0,i/session.rate-.0215);
                session.frames.push_back({t,p.hz>0 ? midiFromHz(p.hz) : 0,p.confidence});
            }
        }
        NoteRegion current; int count=0; double sum=0;
        auto finish=[&] {
            if(count>0 && current.end-current.start>=.045) {
                current.source=float(sum/count); current.target=std::round(current.source);
                int first=std::clamp(int(current.start*session.rate),0,samples-1), last=std::clamp(int(current.end*session.rate),first+1,samples);
                double energy=0;for(int c=0;c<audio->getNumChannels();++c){float rms=audio->getRMSLevel(c,first,last-first);energy+=rms*rms;}current.rms=float(std::sqrt(energy/audio->getNumChannels()));session.notes.push_back(current);
            }
            count=0; sum=0;
        };
        for(auto f:session.frames) {
            if(f.midi<=0 || f.confidence<.85f) { finish(); continue; }
            if(count>0 && (std::abs(std::round(f.midi)-current.target)>.1f || f.time-current.end>.035)) finish();
            if(count==0) { current={}; current.start=f.time; current.target=std::round(f.midi); }
            current.end=std::min(session.duration(),f.time+.005); sum+=f.midi; ++count;
        }
        finish(); return juce::Result::ok();
    }
    juce::Result render(Settings settings,const std::function<bool()>& cancelled={}) {
        if(!original) return juce::Result::fail("Import a WAV first.");
        auto audio=std::make_shared<juce::AudioBuffer<float>>(*original);
        if(std::none_of(notes.begin(),notes.end(),[](auto n){return n.edited;})) { rendered=audio; return juce::Result::ok(); }
        signalsmith::stretch::SignalsmithStretch<float> shifter;
        shifter.presetDefault(original->getNumChannels(),float(rate));
        const int latency=shifter.inputLatency()+shifter.outputLatency(), block=256, total=original->getNumSamples(), channels=original->getNumChannels();
        juce::AudioBuffer<float> in(channels,block),out(channels,block);
        size_t noteIndex=0,frameIndex=0; float shift=0,centre=0;bool tracking=false;
        for(int start=0;start<total+latency+block;start+=block) {
            if(cancelled && cancelled()) return juce::Result::fail("Cancelled");
            double time=(start-shifter.inputLatency()+block*.5)/rate;
            while(noteIndex<notes.size() && time>notes[noteIndex].end) ++noteIndex;
            while(frameIndex+1<frames.size() && frames[frameIndex+1].time<=time) ++frameIndex;
            float desired=0;
            if(noteIndex<notes.size() && time>=notes[noteIndex].start && notes[noteIndex].edited && !frames.empty() && frames[frameIndex].confidence>=.85f && frames[frameIndex].midi>0) {
                float sourcePitch=frames[frameIndex].midi;if(!tracking){centre=sourcePitch;tracking=true;}
                float smooth=float(std::exp(-block/(rate*.05)));centre=smooth*centre+(1-smooth)*sourcePitch;
                float vibratoGain=settings.vibratoMode==1?1-settings.vibratoDepth:1;
                float enhancement=settings.vibratoMode==2?float(std::sin(juce::MathConstants<double>::twoPi*settings.vibratoRate*time))*.2f*settings.vibratoDepth:0;
                desired=std::clamp(notes[noteIndex].target-notes[noteIndex].source-settings.drift*(centre-notes[noteIndex].source)+(vibratoGain-1)*(sourcePitch-centre)+enhancement,-24.0f,24.0f)*settings.amount;
            } else tracking=false;
            float alpha=float(std::exp(-block/(rate*.001*std::max(1.0f,settings.retuneMs)))); shift=alpha*shift+(1-alpha)*desired;
            shifter.setTransposeSemitones(shift); shifter.setFormantSemitones(settings.formant,true);
            if(!frames.empty() && frames[frameIndex].midi>0) shifter.setFormantBase(float(440*std::pow(2.0,(frames[frameIndex].midi-69)/12)/rate));
            in.clear();
            if(start<total) for(int c=0;c<channels;++c) in.copyFrom(c,0,*original,c,start,std::min(block,total-start));
            shifter.process(in.getArrayOfReadPointers(),block,out.getArrayOfWritePointers(),block);
            // Copy only edited material. Outside edit regions the source remains bit-exact.
            for(int i=0;i<block;++i) {
                int sample=start+i-latency; if(sample<0||sample>=total) continue;
                double t=sample/rate;
                auto it=std::lower_bound(notes.begin(),notes.end(),t,[](const NoteRegion& n,double v){return n.end<v;});
                if(it==notes.end() || !it->edited || t<it->start || t>=it->end) continue;
                float fade=float(std::clamp(std::min(t-it->start,it->end-t)/.005,0.0,1.0));
                for(int c=0;c<channels;++c) { float dry=original->getSample(c,sample),wet=out.getSample(c,i); audio->setSample(c,sample,dry+fade*(std::isfinite(wet)?wet-dry:0)); }
            }
        }
        rendered=audio; return juce::Result::ok();
    }
    juce::Result exportWav(const juce::File& file,int bits=24,bool selectedOnly=false) const {
        if(!rendered) return juce::Result::fail("No rendered vocal.");
        juce::TemporaryFile temporary(file); auto stream=temporary.getFile().createOutputStream(); if(!stream) return juce::Result::fail("Cannot write WAV.");
        juce::WavAudioFormat wav; std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(),rate,unsigned(rendered->getNumChannels()),bits,{},0));
        if(!writer) return juce::Result::fail("Cannot create WAV writer."); stream.release();
        int first=selectedOnly ? int(selectionStart*rate) : 0, last=selectedOnly ? int(selectionEnd*rate) : rendered->getNumSamples();
        first=std::clamp(first,0,rendered->getNumSamples()); last=std::clamp(last,first,rendered->getNumSamples());
        bool ok=writer->writeFromAudioSampleBuffer(*rendered,first,last-first); writer.reset();
        return ok&&temporary.overwriteTargetFileWithTemporary() ? juce::Result::ok() : juce::Result::fail("WAV export failed.");
    }
    juce::Result exportMidi(const juce::File& file) const {
        if(notes.empty()) return juce::Result::fail("No vocal notes to export.");
        juce::MidiMessageSequence track; auto tempo=juce::MidiMessage::tempoMetaEvent(500000); track.addEvent(tempo,0);
        float loudest=.001f; for(auto n:notes) loudest=std::max(loudest,n.rms);
        for(size_t i=0;i<notes.size();++i) {
            auto n=notes[i]; double end=i+1<notes.size()?std::min(n.end,notes[i+1].start):n.end;
            if(end<=n.start) continue;
            int pitch=std::clamp(int(std::round(n.edited?n.target:n.source)),0,127);
            auto on=juce::MidiMessage::noteOn(1,pitch,juce::uint8(std::clamp(int(127*std::sqrt(n.rms/loudest)),1,127)));
            auto off=juce::MidiMessage::noteOff(1,pitch); track.addEvent(on,n.start*1920); track.addEvent(off,end*1920);
        }
        track.updateMatchedPairs(); juce::MidiFile midi; midi.setTicksPerQuarterNote(960); midi.addTrack(track);
        juce::TemporaryFile temporary(file); auto out=temporary.getFile().createOutputStream(); if(!out || !midi.writeTo(*out,0)) return juce::Result::fail("MIDI export failed.");
        out.reset(); return temporary.overwriteTargetFileWithTemporary()?juce::Result::ok():juce::Result::fail("Cannot replace MIDI file.");
    }
    juce::Result importMidi(const juce::File& file) {
        auto stream=file.createInputStream(); juce::MidiFile midi;
        if(!stream||file.getSize()>16*1024*1024||!midi.readFrom(*stream)) return juce::Result::fail("Cannot read Standard MIDI File.");
        midi.convertTimestampTicksToSeconds();
        for(int t=0;t<midi.getNumTracks();++t) {
            const auto* track=midi.getTrack(t); bool nonempty=false;
            for(int i=0;i<track->getNumEvents();++i) nonempty|=track->getEventPointer(i)->message.isNoteOn();
            if(!nonempty) continue;
            remember();
            for(auto& note:notes) {
                double middle=(note.start+note.end)*.5;
                for(int i=0;i<track->getNumEvents();++i) {
                    auto* event=track->getEventPointer(i);
                    if(event->message.isNoteOn() && event->noteOffObject && middle>=event->message.getTimeStamp() && middle<event->noteOffObject->message.getTimeStamp()) {
                        note.target=float(event->message.getNoteNumber()); note.edited=true; break;
                    }
                }
            }
            return juce::Result::ok();
        }
        return juce::Result::fail("MIDI file contains no note track.");
    }
    juce::String editsJson() const {
        juce::Array<juce::var> rows;
        for(auto n:notes) { auto o=new juce::DynamicObject; o->setProperty("start",n.start); o->setProperty("end",n.end); o->setProperty("source",n.source); o->setProperty("target",n.target); o->setProperty("edited",n.edited); o->setProperty("rms",n.rms); rows.add(juce::var(o)); }
        return juce::JSON::toString(rows,true);
    }
    bool restoreEdits(const juce::String& json) {
        auto parsed=juce::JSON::parse(json); auto* array=parsed.getArray(); if(!array||array->size()>100000) return false;
        std::vector<NoteRegion> restored; double previous=0;
        for(auto row:*array) {
            if(!row.isObject()) return false;
            NoteRegion n{double(row["start"]),double(row["end"]),float(row["source"]),float(row["target"]),float(row["rms"]),bool(row["edited"])};
            if(!std::isfinite(n.start)||!std::isfinite(n.end)||!std::isfinite(n.source)||!std::isfinite(n.target)||!std::isfinite(n.rms)||n.start<previous||n.end<=n.start||n.end>duration()+.001||n.source<0||n.source>127||n.target<0||n.target>127||n.rms<0) return false;
            previous=n.end; restored.push_back(n);
        }
        notes=std::move(restored); return true;
    }
};
}
