#include "Engine.h"
#include "Preset.h"
#include "MidiTargets.h"
#include "FineTune.h"
#include <iostream>
#include <stdexcept>
#include "AllocationTrace.h"
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
bool renderSuitunesEditor(const juce::File&,bool,const juce::File&);
bool verifyPitchPopKeyboardPolicy();
static void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
static float tone(double t, double rate, float hz) { return 0.25f * float(std::sin(juce::MathConstants<double>::twoPi * hz * t / rate)); }
int main(int argc,char** argv) {
  try {
    juce::ScopedJuceInitialiser_GUI gui;
    check(verifyPitchPopKeyboardPolicy(),"Auto Tune keyboard focus/space policy");
    if(argc>=4&&juce::String(argv[1])=="--snapshot") {
      check(renderSuitunesEditor(juce::File(argv[2]),juce::String(argv[3])=="fine",argc>=5?juce::File(argv[4]):juce::File{}),"Editor snapshot failed");return 0;
    }
    // Independently recorded sustained vowels (MIT, provenance beside WAVs).
    // Nominal note identity is an oracle; these are not samplewise F0 labels.
    // A3/B3 filenames are misleading: spectral harmonic spacing is ~109/~122
    // Hz, i.e. A2/B2. See the fixture README for independently checked labels.
    for(auto entry : {std::pair<const char*,int>{"C3",48},{"D3",50},{"E3",52},{"G3",55},{"A3",45},{"B3",47}}) {
        juce::File file=juce::File(SUITUNES_FIXTURE_DIR).getChildFile("singing").getChildFile(juce::String(entry.first)+".wav");
        juce::WavAudioFormat format;
        std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(file.createInputStream().release(),true));
        check(bool(reader),"Recorded singing fixture missing");
        juce::AudioBuffer<float> audio(int(reader->numChannels),int(reader->lengthInSamples));
        check(reader->read(&audio,0,audio.getNumSamples(),0,true,true),"Singing fixture read");
        suitunes::Yin detector;detector.prepare(reader->sampleRate);
        int total=0,correct=0,octaves=0;
        for(int i=0;i<audio.getNumSamples();++i)if(detector.push(audio.getSample(0,i)) &&
            i>audio.getNumSamples()/4 && i<audio.getNumSamples()*3/4) {
            ++total;auto p=detector.pitch();
            float error=p.hz>0?std::abs(suitunes::midiFromHz(p.hz)-entry.second):100;
            if(error<.5f)++correct;if(error>11 && error<13)++octaves;
        }
        std::cout<<"sung "<<entry.first<<" correct="<<correct<<" frames="<<total<<" octave_errors="<<octaves<<'\n';
        check(total>10 && correct>total*.8 && octaves==0,"Sustained recorded-note identity");
        // Process the entire take with varied buffers, formants and key changes.
        for(auto mode:{suitunes::Mode::FullVoice,suitunes::Mode::Live,suitunes::Mode::Spectral}) {
            suitunes::Engine engine;engine.prepare(reader->sampleRate,1,mode);
            suitunes::Settings settings;settings.enabled=true;settings.root=7;settings.scale=2;
            juce::AudioBuffer<float> block(1,1024);int position=0,iteration=0;
            while(position<audio.getNumSamples()) {
                int n=std::min(audio.getNumSamples()-position,iteration++%2?127:1024);
                block.copyFrom(0,0,audio,0,position,n);
                engine.process(block,settings,0,n);
                for(int i=0;i<n;++i)check(std::isfinite(block.getSample(0,i)) && std::abs(block.getSample(0,i))<4,"Recorded voice unbounded");
                position+=n;
            }
        }
    }
    {juce::File directory(SUITUNES_FIXTURE_DIR);juce::ValueTree legacyFixture,portableFixture;
      check(suitunes::decodePreset(directory.getChildFile("legacy-prototype.sts").loadFileAsString(),legacyFixture).wasOk(),"Frozen legacy STS fixture");
      check(suitunes::stateValue(legacyFixture,"mode")==2,"Legacy fixture did not retain spectral mode");
      check(suitunes::decodePreset(directory.getChildFile("portable-v1.sts").loadFileAsString(),portableFixture).wasOk(),"Frozen portable STS fixture");
      check(suitunes::stateValue(portableFixture,"amount")==.75,"Portable fixture strength");
    }
    for (double sr : {44100.0,48000.0,88200.0,96000.0}) {
      for (float hz : {82.41f,220.0f,440.0f,880.0f}) {
        suitunes::Yin y; y.prepare(sr);
        for (int i=0;i<int(sr/4);++i) y.push(tone(i,sr,hz));
        if (!(std::abs(1200 * std::log2(y.pitch().hz/hz)) < 10)) std::cerr << "Detector sr=" << sr << " input=" << hz << " output=" << y.pitch().hz << '\n';
        check(std::abs(1200 * std::log2(y.pitch().hz/hz)) < 10, "YIN sine accuracy >10 cents");
      }
      suitunes::Yin silent; silent.prepare(sr); for (int i=0;i<4096;++i) silent.push(0);
      check(silent.pitch().hz == 0, "Silence voiced");
      for (int channels : {1,2}) for (int block : {1,16,32,64,127,128,256,512,1024,4096}) {
        suitunes::Engine e; e.prepare(sr, channels); juce::AudioBuffer<float> b(channels,block);
        int position=0;
        for (int pass=0;pass< int(sr*0.12/block)+1;++pass) {
          for (int ch=0;ch<channels;++ch) for (int i=0;i<block;++i) b.setSample(ch,i,tone(position+i,sr,220+ch*30.0f));
          e.process(b, {});
          for (int ch=0;ch<channels;++ch) for (int i=0;i<block;++i) {
            float expected = position+i < e.latencySamples() ? 0 : tone(position+i-e.latencySamples(),sr,220+ch*30.0f);
            check(std::abs(b.getSample(ch,i)-expected)<1e-6f, "Delayed pass-through mismatch");
          }
          position+=block;
        }
      }
    }
    check(suitunes::targetNote(61,0,1)==60, "C major snapping");
    check(suitunes::targetNote(63.1f,0,2)==63, "Minor snapping");
    double worstCents=0;
    for(auto mode : {suitunes::Mode::FullVoice,suitunes::Mode::Live,suitunes::Mode::Spectral}) {
      for(double sr : {44100.0,48000.0,88200.0,96000.0}) {
        for(float target : {82.4069f,110.0f,220.0f,440.0f,880.0f}) {
          if(mode==suitunes::Mode::Live && target<110) continue;
          for(float cents : {-30.0f,30.0f}) {
            if(mode==suitunes::Mode::Live && target*std::pow(2.0f,cents/1200)<110) continue;
            suitunes::Engine candidate; candidate.prepare(sr,2,mode); suitunes::Settings settings;
            settings.enabled=true; settings.retuneMs=1;
            juce::AudioBuffer<float> signal(2,127); suitunes::Yin output; output.prepare(sr);
            double energy=0; int n=0;
            for(int start=0;start<int(sr*.7);start+=127) {
              for(int i=0;i<127;++i) {
                float phase=float(juce::MathConstants<double>::twoPi*(start+i)*target*std::pow(2.0f,cents/1200)/sr);
                float v=.14f*(std::sin(phase)+.6f*std::sin(2*phase)+.4f*std::sin(3*phase));
                signal.setSample(0,i,v); signal.setSample(1,i,v*.5f);
              }
              candidate.process(signal,settings);
              for(int i=0;i<127;++i) {
                check(std::isfinite(signal.getSample(0,i)),"Non-finite wet output");
                if(mode!=suitunes::Mode::Spectral) check(std::abs(signal.getSample(1,i)-.5f*signal.getSample(0,i))<1e-5,"Stereo coherence lost");
                if(start>sr*.5) { output.push(signal.getSample(0,i)); energy+=signal.getSample(0,i)*signal.getSample(0,i);++n; }
              }
            }
            double error=output.pitch().hz>0 ? std::abs(1200*std::log2(output.pitch().hz/target)) : 9999;
            // Legacy spectral is retained for comparison, with its known error.
            if(mode!=suitunes::Mode::Spectral) {
              worstCents=std::max(worstCents,error);
              if(error>=12) std::cerr<<"mode="<<int(mode)<<" sr="<<sr<<" target="<<target<<" cents="<<cents<<" measured="<<output.pitch().hz<<'\n';
              check(error<12,"Harmonic voice correction >12 cents");
            }
            check(std::sqrt(energy/std::max(1,n))>.03,"Shifter cancelled the signal");
          }
        }
      }
      suitunes::Engine unity; unity.prepare(48000,1,mode);
      juce::AudioBuffer<float> impulse(1,4096); impulse.clear(); impulse.setSample(0,0,1);
      suitunes::Settings off; off.enabled=true; off.amount=0;
      unity.process(impulse,off);
      if(mode!=suitunes::Mode::Spectral) {
        check(std::abs(impulse.getSample(0,unity.latencySamples())-1)<1e-6,"Wet unity latency mismatch");
        check(unity.latencySamples()==(mode==suitunes::Mode::Live?480:768),"Wrong latency profile");
      }
    }
    std::cout<<"Worst harmonic correction error across rates/ranges: "<<worstCents<<" cents\n";
    // Below the Live range, do not pretend that safe 10ms correction is possible.
    suitunes::Engine lowLive; lowLive.prepare(48000,1,suitunes::Mode::Live);
    juce::AudioBuffer<float> low(1,128); suitunes::Settings liveSettings; liveSettings.enabled=true;
    for(int block=0;block<300;++block) { for(int i=0;i<128;++i) low.setSample(0,i,tone(block*128+i,48000,82.4f)); lowLive.process(low,liveSettings); }
    check(std::abs(lowLive.correctionSemitones())<1e-6,"Live mode shifted below supported range");
    for(bool rightOnly:{false,true}){suitunes::Engine stereo;stereo.prepare(48000,2);suitunes::Settings settings;settings.enabled=true;settings.retuneMs=1;juce::AudioBuffer<float> signal(2,128);suitunes::Yin output;output.prepare(48000);
      for(int block=0;block<400;++block){for(int i=0;i<128;++i){float v=tone(block*128+i,48000,447.69f);signal.setSample(0,i,rightOnly?0:v);signal.setSample(1,i,rightOnly?v:-v);}stereo.process(signal,settings);for(int i=0;i<128;++i)output.push(signal.getSample(rightOnly?1:0,i));}
      check(stereo.pitch().hz>0&&std::abs(1200*std::log2(output.pitch().hz/440))<12,"Antiphase/right-only stereo lost pitch detection");
    }
    {suitunes::Engine spectral;spectral.prepare(48000,1,suitunes::Mode::Spectral);juce::AudioBuffer<float> impulse(1,16384);impulse.clear();impulse.setSample(0,2000,1);suitunes::Settings unity;unity.enabled=true;unity.amount=0;spectral.process(impulse,unity);int peak=0;float magnitude=0;for(int i=0;i<impulse.getNumSamples();++i)if(std::abs(impulse.getSample(0,i))>magnitude){magnitude=std::abs(impulse.getSample(0,i));peak=i;}check(magnitude>.1f&&std::abs(peak-2000-spectral.latencySamples())<=128,"Spectral impulse latency differs from report");}
    check(suitunes::targetNote(61.2f,0,3,1<<4)==64,"Custom-note quantization");
    {suitunes::Engine variable;variable.prepare(48000,1);juce::AudioBuffer<float> storage(1,4096);int position=0;
      for(int repeat=0;repeat<12;++repeat)for(int size:{0,1,16,1024,32,127,4096,64,512,256}){
        for(int i=0;i<size;++i)storage.setSample(0,i,tone(position+i,48000,220));variable.process(storage,{},0,size);
        for(int i=0;i<size;++i){float expected=position+i<variable.latencySamples()?0:tone(position+i-variable.latencySamples(),48000,220);check(std::abs(storage.getSample(0,i)-expected)<1e-6,"Variable block sequence broke dry alignment");}position+=size;
      }
    }
    check(std::abs(suitunes::targetNote(61.2f,0,3,0)-61.2f)<.001,"Empty note mask must pass pitch unchanged");
    suitunes::MidiTargets targets;targets.reset();
    targets.handle(juce::MidiMessage::noteOn(1,60,juce::uint8(100)));targets.handle(juce::MidiMessage::noteOn(2,64,juce::uint8(100)));
    check(targets.target()==64,"Last held MIDI note did not win");targets.handle(juce::MidiMessage::noteOff(2,64));check(targets.target()==60,"MIDI note-off lost earlier held note");
    targets.handle(juce::MidiMessage::allNotesOff(1));check(targets.target()==-1,"MIDI all-notes-off stuck");
    for(auto mode:{suitunes::Mode::FullVoice,suitunes::Mode::Live,suitunes::Mode::Spectral}) {
      suitunes::Engine rt;rt.prepare(48000,2,mode);juce::AudioBuffer<float> signal(2,128);suitunes::Settings settings;settings.enabled=true;
      for(int i=0;i<500;++i){for(int j=0;j<128;++j){float v=tone(i*128+j,48000,223.84f);signal.setSample(0,j,v);signal.setSample(1,j,v*.5f);}rt.process(signal,settings);}
      allocationTrace::count=0;allocationTrace::enabled=true;
      for(int i=0;i<500;++i){for(int j=0;j<128;++j){float v=tone(i*128+j,48000,223.84f);signal.setSample(0,j,v);signal.setSample(1,j,v*.5f);}settings.amount=float(i%100)*.01f;settings.retuneMs=1+float(i%99);settings.formant=float(i%25)*.1f;rt.process(signal,settings);}
      allocationTrace::enabled=false;check(allocationTrace::count==0,"Steady-state engine allocated C++ heap storage");
    }
    // End-to-end measured correction: A4 +30 cents -> chromatic A4.
    suitunes::Engine e; e.prepare(48000,1); juce::AudioBuffer<float> b(1,256);
    suitunes::Yin measured; measured.prepare(48000); suitunes::Settings s; s.enabled=true; s.retuneMs=1;
    for(int block=0;block<400;++block) {
      for(int i=0;i<256;++i) b.setSample(0,i,tone(block*256+i,48000,440*std::pow(2.0f,30.0f/1200)));
      e.process(b,s);
      for(int i=0;i<256;++i) { check(std::isfinite(b.getSample(0,i)),"Non-finite output"); measured.push(b.getSample(0,i)); }
    }
    std::cout << "Input detected: " << e.pitch().hz << "; correction: " << e.correctionSemitones() << "; output detected: " << measured.pitch().hz << '\n';
    check(std::abs(1200*std::log2(measured.pitch().hz/440))<12, "Correction did not reach A4");
    std::cout << "Measured corrected pitch: " << measured.pitch().hz << " Hz; delay: " << e.latencySamples() << " samples at 48 kHz\n";
    juce::ValueTree tree("SuitunesState"); tree.setProperty("schema",1,nullptr);
    const char* ids[]={"enabled","root","scale","amount","retune","mix","bypass"};
    for(auto id:ids) { juce::ValueTree child("PARAM"); child.setProperty("id",id,nullptr); child.setProperty("value",juce::String(id)=="retune"?40.0:0.0,nullptr); tree.appendChild(child,nullptr); }
    check(suitunes::validState(tree),"Valid schema rejected");
    auto xml=tree.createXml(); check(suitunes::validState(juce::ValueTree::fromXml(*xml)),"XML round trip");
    auto portable=suitunes::presetJson(suitunes::upgradeState(tree.createCopy()),"My vocal");portable.getDynamicObject()->setProperty("futureMetadata","ignored");
    juce::ValueTree decoded;check(suitunes::decodePreset(juce::JSON::toString(portable),decoded).wasOk(),"Portable JSON/unknown metadata rejected");
    check(suitunes::validState(decoded),"Decoded STS invalid");
    portable.getDynamicObject()->setProperty("formatVersion",2);check(suitunes::decodePreset(juce::JSON::toString(portable),decoded).failed(),"Future STS version accepted");
    tree.getChild(0).setProperty("value","garbage",nullptr); check(!suitunes::validState(tree),"Non-numeric XML accepted");
    tree.getChild(0).setProperty("value",2,nullptr); check(!suitunes::validState(tree),"Out-of-range state accepted");
    tree.setProperty("schema",99,nullptr); check(!suitunes::validState(tree),"Future schema accepted");
    auto legacy=juce::ValueTree::fromXml(*xml); auto upgraded=suitunes::upgradeState(legacy);
    check(suitunes::validState(upgraded) && int(upgraded.getChild(7)["value"])==2,"Legacy mode migration");
    std::unique_ptr<juce::AudioProcessor> plugin(createPluginFilter());
    auto* amount = plugin->getParameters()[3]; amount->setValueNotifyingHost(0.37f);
    juce::MemoryBlock saved; plugin->getStateInformation(saved); amount->setValueNotifyingHost(0.9f);
    plugin->setStateInformation(saved.getData(),int(saved.getSize()));
    check(std::abs(amount->getValue()-0.37f)<1e-5f,"Host state did not restore parameter");
    plugin->setRateAndBufferSizeDetails(48000,128);plugin->prepareToPlay(48000,128);
    check(plugin->getName()=="Pitch Pop","Product rename");
    // Legacy disabled projects/automation must no longer turn correction off.
    plugin->getParameters()[0]->setValueNotifyingHost(0);plugin->getParameters()[4]->setValueNotifyingHost(0);plugin->getParameters()[14]->setValueNotifyingHost(1);
    plugin->getParameters()[3]->setValueNotifyingHost(1);juce::AudioBuffer<float> processorSignal(2,128);juce::MidiBuffer processorMidi;suitunes::Yin processorOutput;processorOutput.prepare(48000);
    for(int i=0;i<400;++i){processorMidi.clear();if(i==0)processorMidi.addEvent(juce::MidiMessage::noteOn(1,71,juce::uint8(100)),17);for(int j=0;j<128;++j)for(int c=0;c<2;++c)processorSignal.setSample(c,j,tone(i*128+j,48000,440));plugin->processBlock(processorSignal,processorMidi);for(int j=0;j<128;++j)processorOutput.push(processorSignal.getSample(0,j));}
    check(std::abs(1200*std::log2(processorOutput.pitch().hz/493.8833f))<12,"Processor MIDI target did not control pitch");
    processorMidi.clear();processorMidi.addEvent(juce::MidiMessage::noteOff(1,71),0);plugin->processBlock(processorSignal,processorMidi);processorMidi.clear();
    allocationTrace::count=0;allocationTrace::enabled=true;for(int i=0;i<50;++i)plugin->processBlock(processorSignal,processorMidi);allocationTrace::enabled=false;
    check(allocationTrace::count==0,"Processor callback allocated C++ heap storage");plugin->releaseResources();
    // Use the same APVTS schema with a standalone dummy processor for file persistence.
    juce::AudioProcessorValueTreeState state(*plugin, nullptr, "SuitunesState", {});
    auto good = juce::ValueTree::fromXml(*xml); state.replaceState(good);
    auto file = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("Suitunes-test", ".sts");
    check(suitunes::savePreset(state,file).wasOk(),"Preset file save failed");
    state.state.setProperty("schema",99,nullptr);
    check(suitunes::loadPreset(state,file).wasOk(),"Preset file load failed");
    check(suitunes::validState(state.copyState()),"Loaded file state invalid");
    file.replaceWithText("<broken>"); check(suitunes::loadPreset(state,file).failed(),"Malformed file loaded");
    check(suitunes::validState(state.copyState()),"Invalid load mutated current state"); file.deleteFile();
    // Full offline import/edit/render/export exercise with known synthetic audio.
    const auto temp=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("Suitunes-offline-test",{},false);
    check(temp.createDirectory().wasOk(),"Cannot create test folder");
    juce::AudioBuffer<float> fixture(1,96000);
    for(int i=0;i<fixture.getNumSamples();++i){double hz=i<48000?447.69:335.39;float phase=float(juce::MathConstants<double>::twoPi*i*hz/48000);float envelope=float(std::min(1.0,std::min(i/480.0,(96000-i)/480.0)));fixture.setSample(0,i,envelope*.15f*(std::sin(phase)+.4f*std::sin(2*phase)));}
    suitunes::FineTuneSession written;written.original=std::make_shared<juce::AudioBuffer<float>>(fixture);written.rendered=written.original;written.rate=48000;
    auto wav=temp.getChildFile("source.wav");check(written.exportWav(wav,32).wasOk(),"Fixture WAV export");
    suitunes::FineTuneSession session;check(suitunes::FineTuneSession::importWav(wav,session).wasOk(),"Fine Tune WAV import");check(session.notes.size()>=2,"Offline note segmentation");
    session.selectionStart=.2;session.selectionEnd=.6;auto before=session.editsJson();session.autoCorrect(0,1,4095);
    check(std::any_of(session.notes.begin(),session.notes.end(),[](auto n){return n.edited;}),"Auto Fine Tune edited no region");
    for(auto n:session.notes)if(n.edited)check(n.start>=.2-1e-8&&n.end<=.6+1e-8,"Auto Fine Tune escaped selection");
    auto after=session.editsJson();session.undo();check(session.editsJson()==before,"Undo was not exact");session.redo();check(session.editsJson()==after,"Redo was not exact");
    suitunes::Settings offlineSettings;offlineSettings.retuneMs=1;check(session.render(offlineSettings).wasOk(),"Fine Tune render");
    check(session.rendered->getNumSamples()==fixture.getNumSamples(),"Offline render changed duration");
    double changedEnergy=0;
    for(int i=0;i<fixture.getNumSamples();++i){float a=session.original->getSample(0,i),b=session.rendered->getSample(0,i);check(std::isfinite(b),"Nonfinite offline sample");if(i<9600||i>=28800)check(a==b,"Offline render changed material outside selection");else changedEnergy+=std::abs(a-b);}
    check(changedEnergy>1,"Offline correction did not change selected audio");
    auto out24=temp.getChildFile("corrected-24.wav"),out32=temp.getChildFile("selected-32.wav"),mid=temp.getChildFile("notes.mid");
    check(session.exportWav(out24,24).wasOk()&&session.exportWav(out32,32,true).wasOk(),"WAV formats export");
    juce::WavAudioFormat waveFormat;std::unique_ptr<juce::AudioFormatReader> r24(waveFormat.createReaderFor(out24.createInputStream().release(),true)),r32(waveFormat.createReaderFor(out32.createInputStream().release(),true));
    check(r24&&r32&&r24->bitsPerSample==24&&r32->bitsPerSample==32&&r32->usesFloatingPointData,"WAV depth/float contract");check(r24->lengthInSamples==96000&&r32->lengthInSamples==19200,"WAV full/selected length");
    check(session.exportMidi(mid).wasOk(),"Fine Tune MIDI export");check(session.importMidi(mid).wasOk(),"Fine Tune MIDI reimport");
    juce::MidiMessageSequence tempoTrack,melodyTrack;tempoTrack.addEvent(juce::MidiMessage::tempoMetaEvent(500000),0);
    melodyTrack.addEvent(juce::MidiMessage::noteOn(1,72,juce::uint8(100)),0);melodyTrack.addEvent(juce::MidiMessage::noteOff(1,72),3840);melodyTrack.updateMatchedPairs();
    juce::MidiFile type1;type1.setTicksPerQuarterNote(960);type1.addTrack(tempoTrack);type1.addTrack(melodyTrack);auto type1File=temp.getChildFile("guidance-type1.mid");{auto stream=type1File.createOutputStream();check(stream&&type1.writeTo(*stream,1),"Type1 fixture export");}
    check(session.importMidi(type1File).wasOk(),"Type1 first nonempty track import");for(auto note:session.notes)check(note.target==72,"MIDI guidance did not map the melody");
    auto savedEdits=session.editsJson();suitunes::FineTuneSession restored=session;check(restored.restoreEdits(savedEdits)&&restored.editsJson()==savedEdits,"Fine Tune edit metadata roundtrip");
    // Keep only the explicitly requested preview fixture when useful to developers.
    if(auto folder=juce::SystemStats::getEnvironmentVariable("SUITUNES_TEST_FIXTURE_DIR",{});folder.isNotEmpty()){juce::File destination(folder);destination.createDirectory();wav.copyFileTo(destination.getChildFile("voice-fixture.wav"));}
    r24.reset();r32.reset();check(temp.getParentDirectory()==juce::File::getSpecialLocation(juce::File::tempDirectory),"Unexpected test cleanup path");temp.deleteRecursively();
    // Isolated host-like lifecycle stress: simultaneous processors, state loads,
    // rate/block changes, releases and destruction. No process-global DSP state.
    // CTest bounds the process; this is not a test inside FL Studio.
    for(int cycle=0;cycle<12;++cycle) {
        std::array<std::unique_ptr<juce::AudioProcessor>,3> instances;
        for(auto& instance:instances) {
            instance.reset(createPluginFilter());
            double rate=cycle%2?44100:96000;
            instance->setRateAndBufferSizeDetails(rate,127);
            instance->prepareToPlay(rate,127);
            juce::AudioBuffer<float> b(2,127);juce::MidiBuffer midi;
            for(int pass=0;pass<16;++pass){b.clear();instance->processBlock(b,midi);}
            juce::MemoryBlock saved;instance->getStateInformation(saved);
            instance->setStateInformation(saved.getData(),int(saved.getSize()));
            instance->releaseResources();
        }
        instances[1].reset();instances[0].reset();instances[2].reset();
    }
    std::cout << "PASS: detector, recorded vowels, correction, custom scales, MIDI targets, C++ allocation guard, presets, Fine Tune edits/undo/selection, WAV 24/32, MIDI Type0/1, metadata, 36 processor lifecycles\n";
    return 0;
  } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
