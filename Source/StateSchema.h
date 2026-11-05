#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <cmath>
#include <cstdlib>
namespace suitunes {
struct ParameterSpec { const char* id; double minimum,maximum,initial; bool integer; };
inline constexpr std::array<ParameterSpec,15> parameters{{
 {"enabled",0,1,1,true},{"root",0,11,0,true},{"scale",0,3,0,true},
 {"amount",0,1,1,false},{"retune",1,500,40,false},{"mix",0,1,1,false},
 {"bypass",0,1,0,true},{"mode",0,2,2,true},{"mask",0,4095,4095,true},
 {"formant",-12,12,0,false},{"vibratoMode",0,2,0,true},
 {"vibratoDepth",0,1,1,false},{"vibratoRate",.1,12,5,false},
 {"drift",0,1,1,false},{"midi",0,1,0,true}
}};
inline bool numeric(const juce::var& value,double& v) {
    if(value.isString()) {
        auto text=value.toString().trim(); const char* input=text.toRawUTF8(); char* end=nullptr;
        v=std::strtod(input,&end); if(end==input || *end!='\0') return false;
    } else {
        if(!(value.isInt()||value.isInt64()||value.isDouble()||value.isBool())) return false;
        v=double(value);
    }
    return std::isfinite(v);
}
inline bool validState(const juce::ValueTree& tree) {
    int version=int(tree.getProperty("schema",0));
    if(!tree.hasType("SuitunesState") || (version!=1 && version!=2) || tree.getNumChildren()>64) return false;
    for(size_t i=0;i<parameters.size();++i) {
        int matches=0; auto& p=parameters[i];
        for(auto child:tree) if(child.hasType("PARAM") && child["id"].toString()==p.id) {
            ++matches; double v=0;
            if(!numeric(child["value"],v) || v<p.minimum || v>p.maximum || (p.integer&&std::floor(v)!=v)) return false;
        }
        if(matches>1 || (i<size_t(version==1?7:8) && matches!=1)) return false;
    }
    return true;
}
inline juce::ValueTree upgradeState(juce::ValueTree tree) {
    const bool legacy=int(tree.getProperty("schema",0))==1;
    for(auto& p:parameters) if(!tree.getChildWithProperty("id",p.id).isValid()) {
        juce::ValueTree child("PARAM"); child.setProperty("id",p.id,nullptr);
        child.setProperty("value",legacy&&juce::String(p.id)=="mode" ? 2.0 : p.initial,nullptr);
        tree.appendChild(child,nullptr);
    }
    tree.setProperty("schema",2,nullptr); return tree;
}
inline juce::ValueTree defaultState() {
    juce::ValueTree tree("SuitunesState"); tree.setProperty("schema",2,nullptr);
    return upgradeState(tree);
}
}
