#pragma once
#include "StateSchema.h"
namespace suitunes {
inline const juce::StringArray keyNames{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
inline const juce::StringArray scaleNames{"chromatic","major","minor","custom"};
inline const juce::StringArray vibratoNames{"preserve","reduce","enhance"};
inline double stateValue(const juce::ValueTree& tree,const char* id) { return double(tree.getChildWithProperty("id",id)["value"]); }
inline void setStateValue(juce::ValueTree& tree,const char* id,double value) { tree.getChildWithProperty("id",id).setProperty("value",value,nullptr); }
inline juce::var presetJson(const juce::ValueTree& state,const juce::String& name) {
    auto tree=upgradeState(state.createCopy());
    auto root=new juce::DynamicObject; auto values=new juce::DynamicObject;
    root->setProperty("magic","SUITUNES_STS"); root->setProperty("format","SuitunesPreset");
    root->setProperty("formatVersion",1); root->setProperty("productVersion","0.2.0"); root->setProperty("name",name);
    values->setProperty("key",keyNames[int(stateValue(tree,"root"))]);
    values->setProperty("scale",scaleNames[int(stateValue(tree,"scale"))]);
    juce::Array<juce::var> notes;
    for(int n=0;n<12;++n) if((int(stateValue(tree,"mask"))&(1<<n))!=0) notes.add(n);
    values->setProperty("enabledNotes",notes);
    values->setProperty("speedMs",stateValue(tree,"retune")); values->setProperty("correctionStrength",stateValue(tree,"amount"));
    values->setProperty("formantSemitones",stateValue(tree,"formant")); values->setProperty("vibratoMode",vibratoNames[int(stateValue(tree,"vibratoMode"))]);
    values->setProperty("vibratoDepth",stateValue(tree,"vibratoDepth")); values->setProperty("vibratoRateHz",stateValue(tree,"vibratoRate"));
    values->setProperty("pitchDrift",stateValue(tree,"drift")); values->setProperty("midiControlMode",stateValue(tree,"midi")>.5 ? "last-held-note" : "off");
    values->setProperty("engineMode",stateValue(tree,"mode")); values->setProperty("wetMix",stateValue(tree,"mix"));
    values->setProperty("enabled",stateValue(tree,"enabled")); values->setProperty("bypass",stateValue(tree,"bypass"));
    root->setProperty("parameters",juce::var(values)); return juce::var(root);
}
inline juce::Result decodePreset(const juce::String& text,juce::ValueTree& decoded) {
    if(text.trimStart().startsWithChar('<')) {
        auto xml=juce::XmlDocument::parse(text); auto tree=xml?juce::ValueTree::fromXml(*xml):juce::ValueTree{};
        if(!validState(tree)) return juce::Result::fail("Malformed legacy Suitunes preset.");
        decoded=upgradeState(tree); return juce::Result::ok();
    }
    juce::var json; auto parsed=juce::JSON::parse(text,json); if(parsed.failed()) return parsed;
    if(!json.isObject() || json["magic"].toString()!="SUITUNES_STS" || json["format"].toString()!="SuitunesPreset"
        || !(json["formatVersion"].isInt() && int(json["formatVersion"])==1) || !json["parameters"].isObject())
        return juce::Result::fail("Unsupported Suitunes preset signature or formatVersion.");
    auto values=json["parameters"]; auto tree=defaultState();
    if(!values.hasProperty("key")||!values.hasProperty("scale")||!values.hasProperty("speedMs")||!values.hasProperty("correctionStrength"))
        return juce::Result::fail("Preset is missing required tuning settings.");
    auto readChoice=[&](const char* field,const char* id,const juce::StringArray& choices) {
        if(!values.hasProperty(field)) return true;
        int index=choices.indexOf(values[field].toString()); if(index<0) return false;
        setStateValue(tree,id,index); return true;
    };
    if(!readChoice("key","root",keyNames)||!readChoice("scale","scale",scaleNames)||!readChoice("vibratoMode","vibratoMode",vibratoNames))
        return juce::Result::fail("Unknown key, scale or vibrato mode.");
    const char* fields[]={"speedMs","correctionStrength","formantSemitones","vibratoDepth","vibratoRateHz","pitchDrift","engineMode","wetMix","enabled","bypass"};
    const char* ids[]={"retune","amount","formant","vibratoDepth","vibratoRate","drift","mode","mix","enabled","bypass"};
    for(int i=0;i<10;++i) if(values.hasProperty(fields[i])) {
        double v=0; if(values[fields[i]].isString() || !numeric(values[fields[i]],v)) return juce::Result::fail("Preset values must be finite numbers.");
        setStateValue(tree,ids[i],v);
    }
    if(values.hasProperty("enabledNotes")) {
        auto* array=values["enabledNotes"].getArray(); if(!array || array->size()>12) return juce::Result::fail("Invalid enabledNotes.");
        int mask=0; for(auto n:*array) {
            if(!n.isInt() || int(n)<0 || int(n)>11) return juce::Result::fail("Note classes must be integers 0–11.");
            mask|=1<<int(n);
        }
        setStateValue(tree,"mask",mask);
    }
    if(values.hasProperty("midiControlMode")) {
        auto m=values["midiControlMode"].toString(); if(m!="off"&&m!="last-held-note") return juce::Result::fail("Unsupported MIDI targeting mode.");
        setStateValue(tree,"midi",m=="off"?0:1);
    }
    if(!validState(tree)) return juce::Result::fail("Preset parameter outside its supported range.");
    tree.setProperty("presetName",json["name"].toString(),nullptr); decoded=tree; return juce::Result::ok();
}
inline juce::Result savePreset(juce::AudioProcessorValueTreeState& state,const juce::File& file) {
    auto tree=state.copyState(); if(!validState(tree)) return juce::Result::fail("Invalid Suitunes state; preset not saved.");
    auto text=juce::JSON::toString(presetJson(tree,file.getFileNameWithoutExtension()),false);
    juce::TemporaryFile temporary(file);
    if(!temporary.getFile().replaceWithText(text)||!temporary.overwriteTargetFileWithTemporary()) return juce::Result::fail("Could not save preset. Check folder permissions.");
    return juce::Result::ok();
}
inline juce::Result loadPreset(juce::AudioProcessorValueTreeState& state,const juce::File& file) {
    if(file.getSize()<=0||file.getSize()>65536) return juce::Result::fail("Invalid preset size (maximum 64 KiB).");
    juce::ValueTree tree; auto result=decodePreset(file.loadFileAsString(),tree);
    if(result.wasOk()) {
        // Portable presets change tuning settings, never the current imported vocal.
        auto current=state.copyState();
        for(int i=0;i<current.getNumProperties();++i){auto key=current.getPropertyName(i);if(key.toString().startsWith("fine")||key.toString()=="userPresetFiles"||key.toString()=="viewMode")tree.setProperty(key,current[key],nullptr);}
        state.replaceState(tree);
    }
    return result;
}
}
