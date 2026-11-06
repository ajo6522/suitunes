#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <cmath>
#include "Yin.h"

namespace suitunes {
// Experimental worker boundary, not an enabled plugin detector. Exactly one
// producer and one consumer per queue. Overflow drops work instead of waiting.
// Never reset counters concurrently; transport invalidates results by epoch.
template<class T, unsigned Capacity> class PitchQueue {
public:
    static_assert(Capacity>1);
    static_assert(std::atomic<unsigned>::is_always_lock_free);
    bool push(const T& value) noexcept {
        const auto w=write.load(std::memory_order_relaxed);
        const auto next=(w+1)%Capacity;
        if(next==read.load(std::memory_order_acquire))return false;
        items[w]=value;write.store(next,std::memory_order_release);return true;
    }
    bool pop(T& value) noexcept {
        const auto r=read.load(std::memory_order_relaxed);
        if(r==write.load(std::memory_order_acquire))return false;
        value=items[r];read.store((r+1)%Capacity,std::memory_order_release);return true;
    }
private:
    std::array<T,Capacity> items{};
    alignas(64) std::atomic<unsigned> write{0};
    alignas(64) std::atomic<unsigned> read{0};
};
struct NeuralPitchRequest {
    std::array<float,1024> mono{}; // 64 ms at model-native 16 kHz, trailing only
    uint64_t endSample=0;
    unsigned epoch=0;
};
struct NeuralPitchResult {
    Pitch pitch;
    uint64_t endSample=0;
    unsigned epoch=0;
};
class NeuralPitchBridge {
public:
    PitchQueue<NeuralPitchRequest,4> requests;
    PitchQueue<NeuralPitchResult,4> results;
    // Audio-thread only. No shared neural model, allocation or worker join.
    // If no fresh, credible observation arrives, deterministic DSP wins.
    Pitch choose(Pitch fallback,uint64_t now,uint64_t maxAge,unsigned epoch) noexcept {
        NeuralPitchResult next;
        for(unsigned i=0;i<3 && results.pop(next);++i) {
            if(next.epoch==epoch && next.endSample<=now && now-next.endSample<=maxAge &&
               next.pitch.confidence>=.85f && next.pitch.confidence<=1 &&
               std::isfinite(next.pitch.hz) && next.pitch.hz>=65 && next.pitch.hz<=1000 &&
               (!haveLast || next.endSample>last.endSample || last.epoch!=epoch)) {
                last=next;haveLast=true;
            }
        }
        if(haveLast && last.epoch==epoch && last.endSample<=now && now-last.endSample<=maxAge)
            return last.pitch;
        return fallback;
    }
private:
    NeuralPitchResult last;
    bool haveLast=false;
};
}
