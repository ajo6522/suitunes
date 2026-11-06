#pragma once
#include <cmath>
#include "Yin.h"

namespace suitunes {
// A missing observation is not a newly measured pitch. Keep the two separate
// so offline traces and the UI cannot mistake extrapolation for evidence.
class PitchContinuity {
public:
    void reset() { held={}; missingMs=0; carried=false; }
    Pitch update(Pitch observation, float elapsedMs, bool onset, float minimumHz) {
        carried=false;
        const bool valid=std::isfinite(observation.hz) && std::isfinite(observation.confidence)
            && observation.hz>=minimumHz && observation.hz<=1000
            && observation.confidence >= (held.hz>0 && !onset ? .78f : .85f);
        if(valid) { held=observation; missingMs=0; return held; }
        missingMs+=elapsedMs;
        if(!onset && held.hz>=minimumHz && missingMs<=15.f) {
            carried=true; return held;
        }
        held={}; return {};
    }
    bool isCarried() const { return carried; }
private:
    Pitch held;
    float missingMs=0;
    bool carried=false;
};
}
