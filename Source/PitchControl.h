#pragma once
#include <algorithm>
#include <cmath>

namespace suitunes {
inline float midiFromHz(float hz) { return hz > 0 && std::isfinite(hz) ? 69.0f + 12.0f * std::log2(hz / 440.0f) : 0; }
inline float ratioFromSemitones(float semitones) { return std::exp2(semitones / 12.0f); }
inline int pitchClass(int note) { return (note % 12 + 12) % 12; }
inline bool noteAllowed(int note, int root, int scale, int mask=4095) {
    constexpr int major = (1<<0)|(1<<2)|(1<<4)|(1<<5)|(1<<7)|(1<<9)|(1<<11);
    constexpr int minor = (1<<0)|(1<<2)|(1<<3)|(1<<5)|(1<<7)|(1<<8)|(1<<10);
    if (scale == 0) return true;
    if (scale == 3) return (mask & (1 << pitchClass(note))) != 0;
    return ((scale == 1 ? major : minor) & (1 << pitchClass(note-root))) != 0;
}
// Ascending search gives deterministic lower-note ties, including negative MIDI.
inline float targetNote(float midi, int root, int scale, int mask=4095) {
    if (!std::isfinite(midi) || std::abs(midi)>10000) return 0;
    float best=midi, distance=100;
    for (int n=int(std::floor(midi))-12; n<=int(std::ceil(midi))+12; ++n)
        if (noteAllowed(n,root,scale,mask) && std::abs(midi-n)<distance) {
            best=float(n); distance=std::abs(midi-n);
        }
    return best;
}

// Decisions advance only on detector hops, never on host callback boundaries.
class TargetTracker {
public:
    void reset() { valid=false; pendingMs=0; }
    float update(float note, int root, int scale, int mask, float elapsedMs, bool onset) {
        const float nearest=targetNote(note,root,scale,mask);
        if (!valid || onset || !noteAllowed(int(std::round(held)),root,scale,mask)) {
            held=nearest; valid=true; pendingMs=0; return held;
        }
        if (nearest==held || std::abs(note-nearest)+0.15f>=std::abs(note-held)) {
            pendingMs=0; return held;
        }
        if (nearest!=pending) { pending=nearest; pendingMs=0; }
        pendingMs+=elapsedMs;
        // Large genuine intervals acquire quickly; adjacent notes need dwell.
        if (pendingMs >= (std::abs(nearest-held)>2 ? 10.0f : 20.0f)) { held=nearest; pendingMs=0; }
        return held;
    }
private:
    bool valid=false;
    float held=0,pending=0,pendingMs=0;
};
}
