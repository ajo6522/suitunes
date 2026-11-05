#pragma once
#include "Theme.h"
#include "Timeline.h"

class SuitunesSurface final : public juce::Component,private juce::Timer,public juce::FileDragAndDropTarget {
    using State=juce::AudioProcessorValueTreeState;
    using SliderAttachment=State::SliderAttachment;
    using ComboAttachment=State::ComboBoxAttachment;
    using ButtonAttachment=State::ButtonAttachment;
public:
    explicit SuitunesSurface(SuitunesProcessor& p):proc(p) {
        const bool reopenFine=int(proc.metadata().getProperty("viewMode",0))==1;
        auditionOriginal=bool(proc.metadata().getProperty("fineAudition",false));
        setLookAndFeel(&theme);
        for(auto* button:{&autoTab,&fineTab,&save,&reset,&menu,&enable,&midi,&autoFine,&importWav,&selectAll,&undo,&redo,&split,&selectTool,&splitTool,&play,&stop,&original,&corrected,&exportWav,&exportMidi,&snap,&editPitch,&editTiming,&editVibrato}) addAndMakeVisible(button);
        for(auto* box:{&preset,&key,&scale,&vibrato,&engine}) addAndMakeVisible(box);
        for(auto* knob:{&formant,&speed,&strength,&depth,&rate,&drift}) addAndMakeVisible(knob);
        addAndMakeVisible(timeline);
        key.addItemList(suitunes::keyNames,1); scale.addItemList({"Chromatic","Major","Minor","Custom"},1);
        vibrato.addItemList({"Preserve","Reduce","Enhance"},1);
        engine.addItemList({"Full voice | 16 ms | 65+ Hz","Live | 10 ms | 110+ Hz","Spectral | 120 ms | Formants"},1);
        comboAttachments.push_back(std::make_unique<ComboAttachment>(proc.state,"root",key));
        comboAttachments.push_back(std::make_unique<ComboAttachment>(proc.state,"scale",scale));
        comboAttachments.push_back(std::make_unique<ComboAttachment>(proc.state,"vibratoMode",vibrato));
        comboAttachments.push_back(std::make_unique<ComboAttachment>(proc.state,"mode",engine));
        attach(strength,"amount"); attach(speed,"retune"); attach(formant,"formant");attach(depth,"vibratoDepth");attach(rate,"vibratoRate");attach(drift,"drift");
        strength.percent(); depth.percent();drift.percent();
        speed.slider.textFromValueFunction=[](double v){return juce::String(v,v<10?1:0)+" ms";};
        formant.slider.textFromValueFunction=[](double v){return (v>0?"+":"")+juce::String(v,1)+" st";};
        rate.slider.textFromValueFunction=[](double v){return juce::String(v,1)+" Hz";};
        for(auto* knob:{&formant,&speed,&strength,&depth,&rate,&drift})knob->slider.updateText();
        enable.setClickingTogglesState(true);midi.setClickingTogglesState(true);
        buttonAttachments.push_back(std::make_unique<ButtonAttachment>(proc.state,"enabled",enable));
        buttonAttachments.push_back(std::make_unique<ButtonAttachment>(proc.state,"midi",midi));
        speed.slider.setTooltip("Lower milliseconds = faster, harder correction. Double-click resets to 40 ms."); speed.slider.setDoubleClickReturnValue(true,40);
        strength.slider.setTooltip("How much correction is applied: 0-100%.");strength.slider.setDoubleClickReturnValue(true,1);
        formant.slider.setDoubleClickReturnValue(true,0);
        engine.setTooltip("Stop playback before changing engines. Live mode does not tune input below 110 Hz. Spectral mode supports formant processing.");
        for(size_t i=0;i<noteButtons.size();++i) {
            auto& b=noteButtons[i]; addAndMakeVisible(b); b.setButtonText(suitunes::keyNames[int(i)]);
            b.onClick=[this,i] {
                auto* mask=proc.state.getParameter("mask"); int value=int(proc.state.getRawParameterValue("mask")->load())^(1<<int(i));
                mask->beginChangeGesture();mask->setValueNotifyingHost(mask->convertTo0to1(float(value)));mask->endChangeGesture();
                auto* mode=proc.state.getParameter("scale");mode->beginChangeGesture();mode->setValueNotifyingHost(mode->convertTo0to1(3));mode->endChangeGesture();syncNotes();
            };
        }
        autoTab.onClick=[this]{setFine(false);};fineTab.onClick=[this]{setFine(true);};
        autoTab.setToggleState(true,juce::dontSendNotification);
        save.onClick=[this]{choosePreset(true);};reset.onClick=[this]{resetSettings();};menu.onClick=[this]{showMenu();};
        preset.onChange=[this] {
            if(refreshingPresets) return;
            if(preset.getSelectedId()==2) choosePreset(false);
            else if(preset.getSelectedId()>=100) loadPresetFile(presetFiles[preset.getSelectedId()-100]);
        };
        importWav.onClick=[this]{chooseWav();};selectAll.onClick=[this]{if(auto s=proc.fineSession){s->selectionStart=0;s->selectionEnd=s->duration();timeline.changed();storeSession();}};
        autoFine.onClick=[this]{if(auto s=proc.fineSession){auto settings=proc.settingsSnapshot();s->autoCorrect(settings.root,settings.scale,settings.mask);renderSession();}};
        undo.onClick=[this]{if(auto s=proc.fineSession){s->undo();renderSession();}};redo.onClick=[this]{if(auto s=proc.fineSession){s->redo();renderSession();}};
        split.onClick=[this]{if(auto s=proc.fineSession){s->splitAt(timeline.cursor);renderSession();}};
        selectTool.onClick=[this]{timeline.splitTool=false;selectTool.setToggleState(true,juce::dontSendNotification);splitTool.setToggleState(false,juce::dontSendNotification);};
        splitTool.onClick=[this]{timeline.splitTool=true;selectTool.setToggleState(false,juce::dontSendNotification);splitTool.setToggleState(true,juce::dontSendNotification);};
        play.onClick=[this]{if(proc.fineSession){proc.previewPlaying=!proc.previewPlaying.load();if(proc.previewSeconds.load()>=proc.fineSession->duration())proc.seekPreview(0);}};
        stop.onClick=[this]{proc.previewPlaying=false;proc.seekPreview(0);};
        original.onClick=[this]{auditionOriginal=true;updatePreview(true);};corrected.onClick=[this]{auditionOriginal=false;updatePreview(true);};
        exportWav.onClick=[this]{chooseExport(false);};exportMidi.onClick=[this]{chooseExport(true);};
        snap.setClickingTogglesState(true);snap.setToggleState(true,juce::dontSendNotification);snap.onClick=[this]{timeline.snap=snap.getToggleState();};
        editPitch.setToggleState(true,juce::dontSendNotification);editTiming.setEnabled(false);editVibrato.setEnabled(false);
        autoFine.setToggleState(true,juce::dontSendNotification);
        editTiming.setTooltip("Timing edits are unspecified for V1 and not implemented.");editVibrato.setTooltip("Detailed offline vibrato editing is not implemented. Auto Tune vibrato controls are available.");
        selectTool.setToggleState(true,juce::dontSendNotification);
        timeline.edited=[this]{renderSession();};timeline.selectionChanged=[this]{storeSession();};timeline.seek=[this](double t){proc.seekPreview(t);};
        timeline.viewChanged=[this]{proc.metadata().setProperty("fineViewStart",timeline.start,nullptr);proc.metadata().setProperty("fineViewSpan",timeline.span,nullptr);};
        timeline.snapPitch=[this](float note){auto s=proc.settingsSnapshot();return suitunes::targetNote(note,s.root,s.scale,s.mask);};
        for(auto* knob:{&strength,&speed,&formant,&depth})knob->slider.onValueChange=[this]{if(fine&&proc.fineSession)rerenderAfter=juce::Time::getMillisecondCounterHiRes()+250;};
        vibrato.onChange=[this]{if(fine&&proc.fineSession)rerenderAfter=juce::Time::getMillisecondCounterHiRes()+250;};
        if(auto recent=juce::JSON::parse(proc.metadata().getProperty("userPresetFiles").toString());recent.isArray())for(auto path:*recent.getArray()){if(presetFiles.size()>=128)break;juce::File file(path.toString());if(file.existsAsFile())presetFiles.addIfNotAlreadyThere(file);}
        refreshPresets();setFine(reopenFine);syncNotes();
        if(proc.fineSession&&proc.fineRevision==proc.metadata().getProperty("fineRevision").toString()) commitSession(proc.fineSession,false);else restoreSession();
        startTimerHz(15);setSize(1280,640);timerCallback();
    }
    ~SuitunesSurface() override { stopTimer();jobs.removeAllJobs(true,-1);setLookAndFeel(nullptr); }
    void paint(juce::Graphics& g) override {
        using namespace suitunes::ui;
        g.fillAll(juce::Colour(0xff070b10));
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff23313d),640,0,juce::Colour(0xff090d12),640,640,false));g.fillRect(getLocalBounds());
        // Metallic frame, blue edge lighting and procedural wordmark scale cleanly.
        for(int i=0;i<5;++i) { g.setColour(blue.withAlpha(.025f*float(5-i)));g.drawHorizontalLine(71+i,20.f,1260.f); }
        g.setColour(blue);g.drawHorizontalLine(72,24.f,1256.f);
        juce::Font logo(juce::FontOptions(48,juce::Font::bold|juce::Font::italic));g.setFont(logo);
        g.setGradientFill(juce::ColourGradient(juce::Colours::white,540,5,juce::Colour(0xff9caab7),540,53,false));g.drawText("Suit",juce::Rectangle<int>(485,0,128,55),juce::Justification::centredRight);
        g.setGradientFill(juce::ColourGradient(cyan,630,5,blue.darker(.2f),630,53,false));g.drawText("unes",juce::Rectangle<int>(610,0,148,55),juce::Justification::centredLeft);
        for(int i=0;i<7;++i) {float h=float(10+int(24*std::sin((i+1)*.42)));g.setColour(i%2?blue:cyan);g.fillRoundedRectangle(444.f+i*5.5f,29-h*.5f,3,h,1.5f);}
        label(g,"V O C A L   P I T C H   C O R R E C T I O N",{455,52,360,15},9.5f,text);
        if(!fine) paintAuto(g);else paintFine(g);
        card(g,{10,581,1260,57},false,15);
        paintMeter(g,1000,600,proc.inputLevel.load(),"INPUT");paintMeter(g,1158,600,proc.outputLevel.load(),"OUTPUT");
        label(g,status,{24,621,1232,13},9.5f,muted,juce::Justification::centredLeft);
    }
    void resized() override {
        preset.setBounds(28,17,235,37);save.setBounds(1075,15,58,40);reset.setBounds(1138,15,58,40);menu.setBounds(1201,15,52,40);
        autoTab.setBounds(335,84,303,48);fineTab.setBounds(642,84,303,48);
        if(!fine) {
            key.setBounds(53,174,156,36);scale.setBounds(258,174,185,36);
            for(int i=0;i<12;++i)noteButtons[size_t(i)].setBounds(505+i*59,175,53,34);
            formant.setBounds(49,251,172,169);speed.setBounds(260,240,222,182);strength.setBounds(502,240,222,182);
            vibrato.setBounds(772,260,193,32);depth.setBounds(769,305,98,113);rate.setBounds(874,305,98,113);drift.setBounds(1050,251,172,169);
            midi.setBounds(199,587,125,31);enable.setBounds(30,587,157,31);engine.setBounds(346,587,286,31);
        } else {
            key.setBounds(208,173,104,32);scale.setBounds(333,173,141,32);autoFine.setBounds(505,167,244,44);snap.setBounds(779,171,218,35);
            importWav.setBounds(24,462,131,36);selectAll.setBounds(191,223,102,30);undo.setBounds(300,223,69,30);redo.setBounds(375,223,69,30);split.setBounds(450,223,65,30);
            timeline.setBounds(187,259,844,307);
            strength.setBounds(1049,222,102,143);speed.setBounds(1157,222,102,143);depth.setBounds(1049,398,102,141);formant.setBounds(1157,398,102,141);
            vibrato.setBounds(1060,365,196,30);
            editPitch.setBounds(21,182,137,42);editTiming.setBounds(21,239,137,38);editVibrato.setBounds(21,291,137,38);
            selectTool.setBounds(24,385,63,38);splitTool.setBounds(92,385,63,38);
            play.setBounds(27,587,56,31);stop.setBounds(89,587,52,31);original.setBounds(159,587,93,31);corrected.setBounds(257,587,102,31);
            exportWav.setBounds(380,587,143,31);exportMidi.setBounds(531,587,143,31);
        }
    }
    bool isInterestedInFileDrag(const juce::StringArray& files) override {return files.size()==1&&files[0].endsWithIgnoreCase(".wav");}
    void filesDropped(const juce::StringArray& files,int,int) override {setFine(true);importFile(juce::File(files[0]));}
    void setFine(bool value) {
        fine=value;autoTab.setToggleState(!fine,juce::dontSendNotification);fineTab.setToggleState(fine,juce::dontSendNotification);
        proc.metadata().setProperty("viewMode",fine?1:0,nullptr);
        proc.fineViewActive=fine;
        for(auto* c:std::initializer_list<juce::Component*>{&timeline,&autoFine,&snap,&importWav,&selectAll,&undo,&redo,&split,&selectTool,&splitTool,&play,&stop,&original,&corrected,&exportWav,&exportMidi,&editPitch,&editTiming,&editVibrato})c->setVisible(fine);
        for(auto* c:std::initializer_list<juce::Component*>{&drift,&rate,&midi,&enable,&engine})c->setVisible(!fine);
        for(auto& button:noteButtons)button.setVisible(!fine);
        speed.title(fine?"TRANSITION":"SPEED");strength.title(fine?"STRENGTH":"CORRECTION STRENGTH");
        formant.setEnabled(fine||proc.activeMode.load()==2);
        resized();repaint();
    }
    bool isFine() const {return fine;}
private:
    void attach(suitunes::ui::Knob& knob,const char* id) {sliderAttachments.push_back(std::make_unique<SliderAttachment>(proc.state,id,knob.slider));}
    void paintAuto(juce::Graphics& g) {
        using namespace suitunes::ui;
        card(g,{21,146,1238,78});label(g,"KEY",{52,151,150,20},13,muted,juce::Justification::centredLeft);label(g,"SCALE",{258,151,180,20},13,muted,juce::Justification::centredLeft);label(g,"CUSTOM NOTES",{505,151,500,20},13,muted,juce::Justification::centredLeft);
        card(g,{21,235,1238,196},true);
        for(int x:{251,493,738,1006}){g.setColour(blue.withAlpha(.25f));g.drawVerticalLine(x,256,410);}
        label(g,"VIBRATO",{772,240,193,19},16);
        label(g,proc.activeMode.load()==2?"FORMANT COMPENSATION":"FORMANT: SPECTRAL ENGINE ONLY",{30,413,210,14},9,muted);
        label(g,"DEPTH",{769,295,98,14},10,muted);label(g,"RATE",{874,295,98,14},10,muted);
        card(g,{21,444,1238,130},true);
        label(g,"DETECTED",{33,451,142,19},11,muted);
        float hz=proc.hz.load(),note=hz>0?suitunes::midiFromHz(hz):0;
        label(g,noteName(note),{33,469,142,60},42,cyan);
        label(g,hz>0?juce::String(hz,1)+" Hz":"Waiting for vocal",{30,542,148,20},11,muted);
        float target=hz>0?note+proc.shift.load():0;
        label(g,"TARGET "+noteName(target),{1068,453,176,22},13,text);
        label(g,(proc.shift.load()>0?"+":"")+juce::String(int(std::round(proc.shift.load()*100)))+" cents",{1068,479,176,30},18,cyan);
        label(g,"Confidence "+juce::String(int(std::round(proc.confidence.load()*100)))+"%",{1068,541,176,20},11,muted);
        auto plot=juce::Rectangle<float>(188,455,870,105);
        for(int i=0;i<=12;++i){g.setColour(juce::Colour(0xff1d2b38));g.drawVerticalLine(int(plot.getX()+plot.getWidth()*i/12),plot.getY(),plot.getBottom());}
        for(int i=0;i<5;++i){g.setColour(juce::Colour(0xff1d2b38));g.drawHorizontalLine(int(plot.getY()+i*plot.getHeight()/4),plot.getX(),plot.getRight());}
        float bottom=note>0?note-3:57,top=bottom+6;for(auto h:history)if(h.note>0){bottom=std::min(bottom,std::min(h.note,h.target)-1);top=std::max(top,std::max(h.note,h.target)+1);}float centre=(top+bottom)*.5f,spacing=plot.getHeight()/(top-bottom);juce::Path source,targets;bool drawing=false;
        for(size_t i=0;i<history.size();++i){auto f=history[(historyWrite+i)%history.size()];if(f.note<=0){drawing=false;continue;}float x=plot.getX()+float(i)*plot.getWidth()/float(history.size()-1),y=std::clamp(plot.getCentreY()-(f.note-centre)*spacing,plot.getY()+2,plot.getBottom()-2),ty=std::clamp(plot.getCentreY()-(f.target-centre)*spacing,plot.getY()+2,plot.getBottom()-2);if(!drawing){source.startNewSubPath(x,y);targets.startNewSubPath(x,ty);drawing=true;}else{source.lineTo(x,y);targets.lineTo(x,ty);}g.setColour(blue.withAlpha(.4f));g.drawVerticalLine(int(x),plot.getBottom()-f.level*20,plot.getBottom());}
        g.setColour(blue.withAlpha(.18f));g.strokePath(source,juce::PathStrokeType(6));g.setColour(cyan);g.strokePath(source,juce::PathStrokeType(1.8f));g.setColour(juce::Colours::white.withAlpha(.65f));g.strokePath(targets,juce::PathStrokeType(1));
    }
    void paintFine(juce::Graphics& g) {
        using namespace suitunes::ui;card(g,{10,148,157,426});card(g,{178,148,861,426});card(g,{1047,148,223,426});
        label(g,"TOOLS",{25,350,130,22},13,muted,juce::Justification::centredLeft);label(g,"ADJUSTMENTS",{1061,165,194,30},17,text);
        label(g,"KEY",{209,152,104,19},11,muted,juce::Justification::centredLeft);label(g,"SCALE",{334,152,141,19},11,muted,juce::Justification::centredLeft);
        label(g,"Editing applies to the selected range",{529,225,490,26},11,muted,juce::Justification::centredRight);
        label(g,"Drag notes to change pitch",{1057,552,202,16},10,muted);
        label(g,"Import MIDI: Menu",{22,517,134,40},10,muted);
    }
    void paintMeter(juce::Graphics& g,int x,int y,float rms,const char* name) {
        using namespace suitunes::ui;label(g,name,{x-54,y-1,50,20},10,muted,juce::Justification::centredRight);
        float db=juce::Decibels::gainToDecibels(std::isfinite(rms)?rms:0,-60.f),level=std::clamp((db+60)/60,0.f,1.f);
        for(int i=0;i<12;++i){g.setColour(i<int(level*12)?(i>=11?juce::Colour(0xfff36a7c):cyan):juce::Colour(0xff26323e));g.fillRoundedRectangle(float(x+i*6),float(y+2),4,13,1);}
    }
    void timerCallback() override {
        auto settings=proc.settingsSnapshot();auto count=proc.telemetryCount.load(std::memory_order_acquire);
        if(count>lastTelemetry){lastTelemetry=std::max(lastTelemetry,count>history.size()?count-history.size():uint64_t(0));for(;lastTelemetry<count;++lastTelemetry){auto& f=proc.telemetry[size_t(lastTelemetry%history.size())];history[historyWrite]={f.note.load(),f.target.load(),std::min(1.f,f.level.load()*4)};historyWrite=(historyWrite+1)%history.size();}}
        syncNotes();formant.setEnabled(fine||proc.activeMode.load()==2);
        rate.setEnabled(settings.vibratoMode==2);depth.setEnabled(settings.vibratoMode!=0);
        if(fine&&!busy&&rerenderAfter>0&&juce::Time::getMillisecondCounterHiRes()>rerenderAfter){rerenderAfter=0;renderSession();}
        enable.setButtonText(settings.enabled?"Correction ON":"Correction OFF");
        midi.setButtonText(settings.midiEnabled?"MIDI Control":"Scale Follow");
        if(!busy){bool loaded=bool(proc.fineSession);for(auto* b:{&autoFine,&selectAll,&split,&play,&stop,&original,&corrected,&exportWav,&exportMidi})b->setEnabled(loaded);undo.setEnabled(loaded&&!proc.fineSession->undoHistory.empty());redo.setEnabled(loaded&&!proc.fineSession->redoHistory.empty());}
        play.setToggleState(proc.previewPlaying.load(),juce::dontSendNotification);original.setToggleState(auditionOriginal,juce::dontSendNotification);corrected.setToggleState(!auditionOriginal,juce::dontSendNotification);
        timeline.playhead=proc.previewSeconds.load();if(fine)timeline.repaint();
        auto revision=proc.metadata().getProperty("fineRevision").toString();if(revision!=restoredRevision&&!busy)restoreSession();
        if(!busy&&status.isEmpty())status="Use a single vocal | "+juce::String(proc.getLatencySamples())+" samples delay | Change engines while stopped";
        bool pending=int(proc.state.getRawParameterValue("mode")->load())!=proc.activeMode.load();
        if(pending&&!busy)status="Engine change queued. Stop playback and the vocal preview to apply it.";
        else if(enginePending&&!busy)status="Engine updated | "+juce::String(proc.getLatencySamples())+" samples delay";
        enginePending=pending;
        if(!fine)repaint(21,444,1238,130);
        repaint(10,581,1260,57);
    }
    void syncNotes() {
        auto s=proc.settingsSnapshot();int mask=0;
        for(int n=0;n<12;++n)if(s.scale==3?(s.mask&(1<<n))!=0:std::abs(suitunes::targetNote(float(60+n),s.root,s.scale)-float(60+n))<.01f)mask|=1<<n;
        for(int n=0;n<12;++n)noteButtons[size_t(n)].setToggleState((mask&(1<<n))!=0,juce::dontSendNotification);
    }
    struct LambdaJob final : juce::ThreadPoolJob {
        explicit LambdaJob(std::function<void(juce::ThreadPoolJob&)> fn):ThreadPoolJob("Suitunes offline"),run(std::move(fn)){}
        JobStatus runJob() override {run(*this);return jobHasFinished;}
        std::function<void(juce::ThreadPoolJob&)> run;
    };
    void background(std::function<juce::Result(juce::ThreadPoolJob&)> task,std::function<void()> complete) {
        if(busy)return;busy=true;proc.previewPlaying=false;status="Processing vocal...";
        for(auto* button:{&autoFine,&importWav,&undo,&redo,&split,&selectAll,&exportWav,&exportMidi})button->setEnabled(false);timeline.setEnabled(false);
        auto safe=juce::Component::SafePointer<SuitunesSurface>(this);
        auto generation=proc.metadata().getProperty("fineRevision").toString();
        jobs.addJob(new LambdaJob([safe,generation,task=std::move(task),complete=std::move(complete)](auto& job){
            juce::Result result=juce::Result::ok();try{result=task(job);}catch(const std::exception& e){result=juce::Result::fail(e.what());}
            juce::MessageManager::callAsync([safe,generation,result,complete]{if(!safe)return;safe->busy=false;safe->timeline.setEnabled(true);safe->importWav.setEnabled(true);if(generation!=safe->proc.metadata().getProperty("fineRevision").toString()){safe->status="Project changed; earlier results were not applied to this session.";return;}if(result.wasOk()){complete();safe->status="Ready";}else safe->error(result.getErrorMessage());safe->repaint();});
        }),true);
    }
    void importFile(juce::File file,juce::String edits={},double first=0,double last=-1) {
        if(busy)return;
        auto stored=proc.metadata();auto oldPath=stored.getProperty("fineSource").toString();
        if(edits.isEmpty()&&oldPath.isNotEmpty()&&!juce::File(oldPath).existsAsFile()&&file.getSize()==juce::int64(stored["fineSourceSize"])&&file.getLastModificationTime().toMilliseconds()==juce::int64(stored["fineSourceModified"])) {
            edits=stored["fineEdits"].toString();first=double(stored["fineSelectionStart"]);last=double(stored["fineSelectionEnd"]);
        }
        auto result=std::make_shared<suitunes::FineTuneSession>();auto safe=juce::Component::SafePointer<SuitunesSurface>(this);auto settings=proc.settingsSnapshot();
        background([result,file,edits,first,last,settings](auto& job){
            auto r=suitunes::FineTuneSession::importWav(file,*result,[&job]{return job.shouldExit();});if(r.failed())return r;
            if(edits.isNotEmpty()){if(!result->restoreEdits(edits))return juce::Result::fail("Saved Fine Tune edits are invalid for this WAV.");result->selectionStart=std::clamp(first,0.0,result->duration());result->selectionEnd=std::clamp(last,result->selectionStart,result->duration());return result->render(settings,[&job]{return job.shouldExit();});}return r;
        },[safe,result,edits]{if(safe)safe->commitSession(result,edits.isEmpty());});
    }
    void renderSession() {
        if(busy||!proc.fineSession)return;timeline.changed();auto result=std::make_shared<suitunes::FineTuneSession>(*proc.fineSession);auto safe=juce::Component::SafePointer<SuitunesSurface>(this);auto settings=proc.settingsSnapshot();
        background([result,settings](auto& job){return result->render(settings,[&job]{return job.shouldExit();});},[safe,result]{if(safe)safe->commitSession(result,false);});
    }
    void commitSession(std::shared_ptr<suitunes::FineTuneSession> s,bool reset) {
        bool first=!timeline.session;proc.fineSession=std::move(s);timeline.session=proc.fineSession;timeline.changed(reset||first);
        if(!reset){double span=double(proc.metadata().getProperty("fineViewSpan",0));if(span>0){timeline.span=std::clamp(span,std::min(.25,proc.fineSession->duration()),proc.fineSession->duration());timeline.start=std::clamp(double(proc.metadata().getProperty("fineViewStart",0)),0.0,std::max(0.0,proc.fineSession->duration()-timeline.span));timeline.changed();}}
        updatePreview(!reset);storeSession();
    }
    void updatePreview(bool retain) {if(auto s=proc.fineSession)proc.setPreview(auditionOriginal?s->original:s->rendered,s->rate,retain);proc.metadata().setProperty("fineAudition",auditionOriginal,nullptr);repaint();}
    void storeSession() {
        if(!proc.fineSession)return;auto s=proc.fineSession;auto tree=proc.metadata();
        tree.setProperty("fineSource",s->sourceFile.getFullPathName(),nullptr);tree.setProperty("fineSourceSize",s->sourceFile.getSize(),nullptr);tree.setProperty("fineSourceModified",s->sourceFile.getLastModificationTime().toMilliseconds(),nullptr);
        tree.setProperty("fineEdits",s->editsJson(),nullptr);tree.setProperty("fineSelectionStart",s->selectionStart,nullptr);tree.setProperty("fineSelectionEnd",s->selectionEnd,nullptr);
        tree.setProperty("fineViewStart",timeline.start,nullptr);tree.setProperty("fineViewSpan",timeline.span,nullptr);
        restoredRevision=juce::Uuid().toString();proc.fineRevision=restoredRevision;tree.setProperty("fineRevision",restoredRevision,nullptr);
    }
    void restoreSession() {
        juce::String path,edits;juce::int64 size=0,modified=0;double first=0,last=0;
        {auto tree=proc.metadata();restoredRevision=tree["fineRevision"].toString();path=tree["fineSource"].toString();edits=tree["fineEdits"].toString();size=juce::int64(tree["fineSourceSize"]);modified=juce::int64(tree["fineSourceModified"]);first=double(tree["fineSelectionStart"]);last=double(tree["fineSelectionEnd"]);}
        proc.previewPlaying=false;proc.setPreview(nullptr,48000);proc.fineSession.reset();timeline.session.reset();timeline.changed(true);
        if(path.isEmpty())return;
        juce::File file(path);if(!file.existsAsFile()){status="Source WAV is missing. Import it again to relink: "+file.getFileName();return;}
        if(file.getSize()!=size||file.getLastModificationTime().toMilliseconds()!=modified){status="Source WAV has changed. Import it again before applying saved edits.";return;}
        importFile(file,edits,first,last);
    }
    void choose(const juce::String& title,const juce::String& wildcard,bool saving,std::function<void(juce::File)> completed) {
        chooser=std::make_unique<juce::FileChooser>(title,juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),wildcard);auto safe=juce::Component::SafePointer<SuitunesSurface>(this);
        chooser->launchAsync(juce::FileBrowserComponent::canSelectFiles|(saving?juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::warnAboutOverwriting:juce::FileBrowserComponent::openMode),[safe,completed](const juce::FileChooser& fc){if(!safe)return;auto file=fc.getResult();if(file!=juce::File{})completed(file);});
    }
    void chooseWav() {auto safe=juce::Component::SafePointer<SuitunesSurface>(this);choose("Import monophonic vocal WAV","*.wav",false,[safe](auto file){if(safe)safe->importFile(file);});}
    void chooseExport(bool midiFile) {
        auto safe=juce::Component::SafePointer<SuitunesSurface>(this);choose(midiFile?"Export MIDI":"Export WAV",midiFile?"*.mid":"*.wav",true,[safe,midiFile](auto file){
            if(!safe||!safe->proc.fineSession)return;auto session=safe->proc.fineSession;int bits=safe->exportBits;bool selected=safe->exportSelected;
            safe->background([session,file,midiFile,bits,selected](auto&){return midiFile?session->exportMidi(file.withFileExtension("mid")):session->exportWav(file.withFileExtension("wav"),bits,selected);},[]{});
        });
    }
    void chooseMidi() {
        if(!proc.fineSession){error("Import a vocal WAV before importing MIDI guidance.");return;}
        auto safe=juce::Component::SafePointer<SuitunesSurface>(this);choose("Import MIDI target melody (first note track)","*.mid;*.midi",false,[safe](auto file){if(!safe)return;auto s=safe->proc.fineSession;auto r=s->importMidi(file);if(r.failed())safe->error(r.getErrorMessage());else safe->renderSession();});
    }
    void choosePreset(bool saving) {
        auto safe=juce::Component::SafePointer<SuitunesSurface>(this);choose(saving?"Save custom Suitunes preset":"Load Suitunes preset","*.sts",saving,[safe,saving](auto file){
            if(!safe)return;if(!saving){safe->loadPresetFile(file);return;}file=file.withFileExtension("sts");auto r=suitunes::savePreset(safe->proc.state,file);if(r.failed())safe->error(r.getErrorMessage());else{safe->presetFiles.addIfNotAlreadyThere(file);safe->refreshPresets(file);safe->status="Saved "+file.getFileName();}
        });
    }
    void loadPresetFile(const juce::File& file) {auto r=suitunes::loadPreset(proc.state,file);if(r.failed())error(r.getErrorMessage());else{presetFiles.addIfNotAlreadyThere(file);refreshPresets(file);status="Loaded "+file.getFileName();}}
    void refreshPresets(juce::File selected={}) {
        refreshingPresets=true;preset.clear();preset.addItem("Custom settings",1);preset.addItem("Load .sts preset...",2);for(int i=0;i<presetFiles.size();++i)preset.addItem(presetFiles[i].getFileNameWithoutExtension(),100+i);
        preset.setSelectedId(selected==juce::File{}?1:100+presetFiles.indexOf(selected),juce::dontSendNotification);refreshingPresets=false;
        juce::Array<juce::var> paths;for(auto file:presetFiles)paths.add(file.getFullPathName());proc.metadata().setProperty("userPresetFiles",juce::JSON::toString(paths,true),nullptr);
    }
    void resetSettings() {for(auto& p:suitunes::parameters){auto* parameter=proc.state.getParameter(p.id);parameter->beginChangeGesture();parameter->setValueNotifyingHost(parameter->convertTo0to1(float(p.initial)));parameter->endChangeGesture();}refreshPresets();status="Settings reset. Custom preset files were kept.";}
    void showMenu() {
        juce::PopupMenu m;m.addItem(1,"Load .sts preset...");m.addItem(2,"Import vocal WAV...");m.addItem(3,"Import MIDI melody...");m.addSeparator();m.addItem(4,"WAV export: 24-bit PCM",true,exportBits==24);m.addItem(5,"WAV export: 32-bit float",true,exportBits==32);m.addItem(6,"Export selected range only",true,exportSelected);m.addSeparator();m.addItem(7,"About this prototype / limitations");
        auto safe=juce::Component::SafePointer<SuitunesSurface>(this);m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(menu),[safe](int result){if(!safe)return;switch(result){case 1:safe->choosePreset(false);break;case 2:safe->setFine(true);safe->chooseWav();break;case 3:safe->chooseMidi();break;case 4:safe->exportBits=24;break;case 5:safe->exportBits=32;break;case 6:safe->exportSelected=!safe->exportSelected;break;case 7:juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,"Suitunes 0.2 prototype","Built from Suitunes V1.0.0-dev-spec.\n\nLive: 10ms, input 110+ Hz. Full voice: 16ms, input 65+ Hz. Spectral: 120ms, formant processing. Low-latency formant preservation is not implemented.\n\nFine Tune: import, selection correction, pitch drag, split, undo/redo, WAV/MIDI export and first-track MIDI guidance. Timing edits and detailed vibrato editing are not implemented. Keep the source WAV available for project restoration.\n\nActual-vocal listening, 30-minute host soak and macOS validation remain required.");break;default:break;}});
    }
    void error(const juce::String& message) {status=message;juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Suitunes",message);}
    SuitunesProcessor& proc;
    suitunes::ui::Theme theme;
    juce::TooltipWindow tooltips{this,650};
    suitunes::ui::Knob formant{"FORMANT"},speed{"SPEED"},strength{"CORRECTION STRENGTH"},depth{"DEPTH"},rate{"RATE"},drift{"PITCH DRIFT"};
    juce::ComboBox preset,key,scale,vibrato,engine;
    std::array<juce::TextButton,12> noteButtons;
    juce::TextButton autoTab{"Auto Tune"},fineTab{"Fine Tune"},save{"Save"},reset{"Reset"},menu{"Menu"},enable{"Correction OFF"},midi{"Scale Follow"};
    juce::TextButton autoFine{"Auto Fine Tune"},importWav{"Import WAV"},selectAll{"Select All"},undo{"Undo"},redo{"Redo"},split{"Split"},selectTool{"Select"},splitTool{"Split"};
    juce::TextButton play{"Play"},stop{"Stop"},original{"Original"},corrected{"Corrected"},exportWav{"Export WAV"},exportMidi{"Export MIDI"},snap{"Snap to Scale"};
    juce::TextButton editPitch{"Edit Pitch"},editTiming{"Edit Timing | later"},editVibrato{"Edit Vibrato | later"};
    suitunes::ui::PitchTimeline timeline;
    std::vector<std::unique_ptr<SliderAttachment>>sliderAttachments;
    std::vector<std::unique_ptr<ComboAttachment>>comboAttachments;
    std::vector<std::unique_ptr<ButtonAttachment>>buttonAttachments;
    juce::ThreadPool jobs{1};
    std::unique_ptr<juce::FileChooser>chooser;
    juce::Array<juce::File>presetFiles;
    struct History {float note=0,target=0,level=0;};std::array<History,256>history{};size_t historyWrite=0;
    uint64_t lastTelemetry=0;
    bool fine=false,busy=false,auditionOriginal=false,refreshingPresets=false,exportSelected=false,enginePending=false;
    int exportBits=24;
    double rerenderAfter=0;
    juce::String status,restoredRevision;
};

class SuitunesEditor final : public juce::AudioProcessorEditor {
public:
    explicit SuitunesEditor(SuitunesProcessor& p):AudioProcessorEditor(p),surface(p) {
        addAndMakeVisible(surface);setResizable(true,true);setResizeLimits(960,480,1920,960);getConstrainer()->setFixedAspectRatio(2.0);setSize(1280,640);
    }
    void resized() override {surface.setBounds(0,0,1280,640);surface.setTransform(juce::AffineTransform::scale(float(getWidth())/1280.f,float(getHeight())/640.f));}
    void showFineTune() {surface.setFine(true);}
private:SuitunesSurface surface;
};
