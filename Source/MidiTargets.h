#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
namespace suitunes {
class MidiTargets {
public:
    void reset() { held.fill(0); order.fill(0); clock=0; }
    void handle(const juce::MidiMessage& m) {
        const int ch=std::clamp(m.getChannel()-1,0,15);
        if(m.isNoteOn()) { auto i=size_t(ch*128+m.getNoteNumber()); held[i]=true; order[i]=++clock; }
        else if(m.isNoteOff()) held[size_t(ch*128+m.getNoteNumber())]=false;
        else if(m.isAllNotesOff() || m.isAllSoundOff()) for(int n=0;n<128;++n) held[size_t(ch*128+n)]=false;
    }
    int target() const {
        int note=-1; uint64_t latest=0;
        for(size_t i=0;i<held.size();++i) if(held[i] && order[i]>=latest) { latest=order[i]; note=int(i%128); }
        return note;
    }
private:
    std::array<bool,2048> held{};
    std::array<uint64_t,2048> order{};
    uint64_t clock=0;
};
}
