#include <juce_audio_utils/juce_audio_utils.h>
#include "Engine.h"
#include "Preset.h"
#include "MidiTargets.h"
#include "FineTune.h"

class SuitunesProcessor final : public juce::AudioProcessor, private juce::AudioProcessorValueTreeState::Listener, private juce::AsyncUpdater, private juce::Timer {
public:
    struct LockedMetadata {
        explicit LockedMetadata(SuitunesProcessor& p):owner(p),lock(p.metadataLock){}
        juce::var getProperty(const juce::Identifier& key,const juce::var& fallback={})const{return owner.state.state.getProperty(key,fallback);}
        juce::var operator[](const juce::Identifier& key)const{return getProperty(key);}
        void setProperty(const juce::Identifier& key,const juce::var& value,juce::UndoManager* undo){owner.state.state.setProperty(key,value,undo);}
        SuitunesProcessor& owner;juce::ScopedLock lock;
    };
    LockedMetadata metadata(){return LockedMetadata(*this);}
    static juce::AudioProcessorValueTreeState::ParameterLayout layout() {
        juce::AudioProcessorValueTreeState::ParameterLayout p;
        // Keep the legacy ID/index so existing FL automation/state still loads.
        p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"enabled",1}, "Legacy correction flag (ignored)", true,
            juce::AudioParameterBoolAttributes().withAutomatable(false)));
        p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"root",1}, "Key", juce::StringArray{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"}, 0));
        p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"scale",1}, "Scale", juce::StringArray{"Chromatic","Major","Minor","Custom"}, 0));
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"amount",1}, "Correction Strength", 0.0f, 1.0f, 1.0f));
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"retune",1}, "Speed", juce::NormalisableRange<float>{1,500,0.1f,0.4f}, 40.0f));
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"mix",1}, "Wet mix", 0.0f, 1.0f, 1.0f));
        p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"bypass",1}, "Bypass", false));
        p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"mode",1}, "Engine (change while stopped)",
            juce::StringArray{"Experimental full voice: 16 ms / 65+ Hz", "Experimental live: 10 ms / 110+ Hz", "Quality: 150 ms / formants"}, 2,
            juce::AudioParameterChoiceAttributes().withAutomatable(false)));
        p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"mask",1},"Custom notes",0,4095,4095));
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"formant",1},"Formant",-12.0f,12.0f,0.0f));
        p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"vibratoMode",1},"Vibrato",juce::StringArray{"Preserve","Reduce","Enhance"},0));
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"vibratoDepth",1},"Vibrato depth",0.0f,1.0f,1.0f));
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"vibratoRate",1},"Vibrato rate",.1f,12.0f,5.0f));
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"drift",1},"Pitch drift correction",0.0f,1.0f,1.0f));
        p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"midi",1},"MIDI target control",false));
        return p;
    }
    SuitunesProcessor() : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true)), state(*this, nullptr, "SuitunesState", layout()) {
        for(size_t i=0;i<raw.size();++i) raw[i]=state.getRawParameterValue(suitunes::parameters[i].id);
        state.state.setProperty("schema", 2, nullptr); state.addParameterListener("mode",this); startTimerHz(5);
    }
    ~SuitunesProcessor() override { stopTimer(); state.removeParameterListener("mode",this); cancelPendingUpdate(); }
    const juce::String getName() const override { return "Pitch Pop"; }
    void prepareToPlay(double sr, int) override { midiTargets.reset(); midiNote=-1; engine.prepare(sr,getTotalNumInputChannels(),selectedMode()); activeMode=int(selectedMode()); setLatencySamples(engine.latencySamples()); }
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& b) const override {
        return b.getMainInputChannelSet() == b.getMainOutputChannelSet() && (b.getMainInputChannelSet() == juce::AudioChannelSet::mono() || b.getMainInputChannelSet() == juce::AudioChannelSet::stereo());
    }
    suitunes::Settings settingsSnapshot() const {
        auto get=[this](size_t i){return raw[i]->load(std::memory_order_relaxed);};
        suitunes::Settings s;
        s.enabled=get(6)<.5f; s.root=int(get(1)); s.scale=int(get(2)); s.amount=get(3); s.retuneMs=get(4); s.mix=get(5);
        s.mask=int(get(8)); s.formant=get(9); s.vibratoMode=int(get(10)); s.vibratoDepth=get(11); s.vibratoRate=get(12); s.drift=get(13); s.midiEnabled=get(14)>.5f; s.midiTarget=midiNote.load(); return s;
    }
    void setPreview(std::shared_ptr<const juce::AudioBuffer<float>> buffer,double rate,bool retainPosition=false) {
        const juce::ScopedLock lock(getCallbackLock());
        preview=std::move(buffer); previewRate=rate; if(!retainPosition) previewPosition=0;
    }
    void seekPreview(double seconds) { const juce::ScopedLock lock(getCallbackLock()); previewPosition=std::max(0.0,seconds*previewRate); }
    void run(juce::AudioBuffer<float>& b, juce::MidiBuffer& midi, bool bypassed) {
        juce::ScopedNoDenormals noDenormals;
        bool playing=false,hasPosition=false; int64_t position=0;
        if(auto* ph=getPlayHead()) if(auto pos=ph->getPosition()) {
            playing=pos->getIsPlaying();
            if(auto time=pos->getTimeInSamples()){hasPosition=true;position=*time;}
        }
        if(playing!=wasPlaying || (playing && hasPosition && positionValid && position!=nextHostSample)) {
            engine.reset(); midiTargets.reset(); midiNote=-1;
        }
        wasPlaying=playing; positionValid=hasPosition; nextHostSample=position+b.getNumSamples(); transportPlaying=playing;
        auto s=settingsSnapshot(); s.enabled=s.enabled&&!bypassed&&!fineViewActive.load();
        if(previewPlaying.load()&&preview&&getSampleRate()>0) {
            const double step=previewRate/getSampleRate();
            for(int i=0;i<b.getNumSamples();++i) {
                int index=int(previewPosition); float fraction=float(previewPosition-index);
                for(int c=0;c<b.getNumChannels();++c) {
                    int channel=std::min(c,preview->getNumChannels()-1); float value=0;
                    if(index<preview->getNumSamples()) { int next=std::min(index+1,preview->getNumSamples()-1); value=preview->getSample(channel,index)*(1-fraction)+preview->getSample(channel,next)*fraction; }
                    b.setSample(c,i,value);
                }
                previewPosition+=step;
            }
            previewSeconds=previewPosition/previewRate; s.enabled=false;
            if(previewPosition>=preview->getNumSamples()+engine.latencySamples()*step) previewPlaying=false;
        }
        float inputRms=0;for(int c=0;c<b.getNumChannels();++c)if(b.getNumSamples()>0)inputRms=std::max(inputRms,b.getRMSLevel(c,0,b.getNumSamples()));inputLevel=inputRms;
        int cursor=0;
        for(const auto event:midi) {
            int offset=std::clamp(event.samplePosition,0,b.getNumSamples());
            if(offset>cursor) engine.process(b,s,cursor,offset-cursor);
            if(event.numBytes>0&&event.numBytes<=3) midiTargets.handle(event.getMessage());
            midiNote=midiTargets.target(); s.midiTarget=midiNote.load(); cursor=offset;
        }
        if(cursor<b.getNumSamples()) engine.process(b,s,cursor,b.getNumSamples()-cursor);
        hz.store(engine.pitch().hz); confidence.store(engine.pitch().confidence); shift.store(engine.correctionSemitones());target.store(engine.targetMidi());
        float outputRms=0;for(int c=0;c<b.getNumChannels();++c)if(b.getNumSamples()>0)outputRms=std::max(outputRms,b.getRMSLevel(c,0,b.getNumSamples()));outputLevel=outputRms;
        telemetrySamples+=b.getNumSamples();
        if(telemetrySamples>=int(getSampleRate()*.025)) {
            telemetrySamples=0;auto& frame=telemetry[size_t(telemetrySerial%telemetry.size())];float f=hz.load();float note=f>0?suitunes::midiFromHz(f):0;
            frame.note.store(note,std::memory_order_relaxed);frame.target.store(target.load(),std::memory_order_relaxed);frame.level.store(inputLevel.load(),std::memory_order_relaxed);
            telemetryCount.store(++telemetrySerial,std::memory_order_release);
        }
    }
    void processBlock(juce::AudioBuffer<float>& b, juce::MidiBuffer& m) override { run(b,m,false); }
    void processBlockBypassed(juce::AudioBuffer<float>& b, juce::MidiBuffer& m) override { run(b,m,true); }
    juce::AudioProcessorParameter* getBypassParameter() const override { return state.getParameter("bypass"); }
    double getTailLengthSeconds() const override { return getSampleRate() > 0 ? getLatencySamples() / getSampleRate() : 0; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& block) override {
        const juce::ScopedLock lock(metadataLock);
        auto tree = state.copyState(); tree.setProperty("schema",2,nullptr); auto xml = tree.createXml(); copyXmlToBinary(*xml, block);
    }
    void setStateInformation(const void* data, int size) override {
        const juce::ScopedLock lock(metadataLock);
        if (size <= 0 || size > 16*1024*1024) return;
        if (auto xml = getXmlFromBinary(data, size)) { auto tree = juce::ValueTree::fromXml(*xml); if (suitunes::validState(tree)) {previewPlaying=false;fineViewActive=int(tree.getProperty("viewMode",0))==1;state.replaceState(suitunes::upgradeState(tree));} }
    }
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;
    juce::AudioProcessorValueTreeState state;
    std::atomic<float> hz{0}, confidence{0}, shift{0},target{0};
    std::atomic<float> inputLevel{0},outputLevel{0};
    std::atomic<int> midiNote{-1},activeMode{0};
    std::atomic<bool> previewPlaying{false},transportPlaying{false};
    std::atomic<bool> fineViewActive{false};
    std::atomic<double> previewSeconds{0};
    std::shared_ptr<suitunes::FineTuneSession> fineSession;
    juce::String fineRevision;
    struct Telemetry {std::atomic<float>note{0},target{0},level{0};};
    std::array<Telemetry,256>telemetry;
    std::atomic<uint64_t>telemetryCount{0};
private:
    juce::CriticalSection metadataLock;
    bool wasPlaying=false,positionValid=false;
    int64_t nextHostSample=0;
    int telemetrySamples=0;uint64_t telemetrySerial=0;
    std::array<std::atomic<float>*,suitunes::parameters.size()> raw{};
    suitunes::MidiTargets midiTargets;
    std::shared_ptr<const juce::AudioBuffer<float>> preview;
    double previewRate=48000,previewPosition=0;
    suitunes::Mode selectedMode() const { return static_cast<suitunes::Mode>(int(raw[7]->load())); }
    void parameterChanged(const juce::String&,float) override { triggerAsyncUpdate(); }
    void timerCallback() override { if(!transportPlaying.load()&&!previewPlaying.load() && int(selectedMode())!=activeMode.load()) triggerAsyncUpdate(); }
    void handleAsyncUpdate() override {
        if(getSampleRate()<=0||transportPlaying.load()||previewPlaying.load()) return;
        // JUCE's VST3/standalone wrappers use this lock around callbacks. Engine
        // reconfiguration/allocations never run inside our audio callback.
        const juce::ScopedLock lock(getCallbackLock());
        if(engine.currentMode()!=selectedMode()) prepareToPlay(getSampleRate(),getBlockSize());
    }
    suitunes::Engine engine;
};

#include "PluginEditor.h"
juce::AudioProcessorEditor* SuitunesProcessor::createEditor() { return new SuitunesEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SuitunesProcessor; }

bool verifyPitchPopKeyboardPolicy() {
    SuitunesProcessor processor; SuitunesSurface surface(processor);
    auto focusDisabled=[](auto&& self,const juce::Component& c)->bool {
        if(c.getWantsKeyboardFocus())return false;
        if(auto* slider=dynamic_cast<const juce::Slider*>(&c);slider && slider->isTextBoxEditable())return false;
        for(auto* child:c.getChildren())if(!self(self,*child))return false;
        return true;
    };
    surface.setFine(false);
    if(!focusDisabled(focusDisabled,surface) || surface.keyPressed(juce::KeyPress(juce::KeyPress::spaceKey)))return false;
    surface.setFine(true);
    if(!surface.keyPressed(juce::KeyPress(juce::KeyPress::spaceKey)))return false;
    surface.setFine(false);
    return focusDisabled(focusDisabled,surface);
}

bool renderSuitunesEditor(const juce::File& target,bool fine,const juce::File& source) {
    SuitunesProcessor processor;processor.setRateAndBufferSizeDetails(48000,256);processor.prepareToPlay(48000,256);
    if(source.existsAsFile()) {
        auto session=std::make_shared<suitunes::FineTuneSession>();auto result=suitunes::FineTuneSession::importWav(source,*session);if(result.failed())return false;
        if(fine){session->selectionStart=session->duration()*.24;session->selectionEnd=session->duration()*.76;session->autoCorrect(0,1,4095);session->render(processor.settingsSnapshot());}
        processor.fineSession=session;
        juce::AudioBuffer<float> signal(2,256);auto* enabled=processor.state.getParameter("enabled");enabled->setValueNotifyingHost(1);
        auto* scale=processor.state.getParameter("scale");scale->setValueNotifyingHost(scale->convertTo0to1(1));
        juce::MidiBuffer midi;
        for(int i=0;i<session->original->getNumSamples();i+=256){signal.clear();for(int c=0;c<2;++c)signal.copyFrom(c,0,*session->original,std::min(c,session->original->getNumChannels()-1),i,std::min(256,session->original->getNumSamples()-i));processor.run(signal,midi,false);}
    }
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());if(fine)static_cast<SuitunesEditor*>(editor.get())->showFineTune();
    editor->setVisible(true);auto image=editor->createComponentSnapshot(editor->getLocalBounds(),true,1.0f);
    juce::MemoryOutputStream output;juce::PNGImageFormat png;
    return png.writeImageToStream(image,output)&&target.replaceWithData(output.getData(),output.getDataSize());
}

