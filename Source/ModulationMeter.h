#pragma once
#include "Lfo.h"
#include <atomic>
// Approximate UI snapshot; each field is atomic. No audio-thread locks/allocation.
struct ModulationMeter {
    std::array<std::atomic<float>,9> values{};
    std::atomic<bool> active{false};
    void publish(const LfoModulation& m,bool enabled) {
        const float v[]={m.cutoffOctaves,m.amplitude,m.pan,m.resonance,m.driveDb,m.pitchCents,m.waves[0],m.waves[1],m.waves[2]};
        for(size_t i=0;i<9;++i)values[i].store(v[i],std::memory_order_relaxed);
        active.store(enabled,std::memory_order_relaxed);
    }
    LfoModulation read() const {
        LfoModulation m;
        m.cutoffOctaves=values[0].load();m.amplitude=values[1].load();m.pan=values[2].load();
        m.resonance=values[3].load();m.driveDb=values[4].load();m.pitchCents=values[5].load();
        for(size_t i=0;i<3;++i)m.waves[i]=values[6+i].load();return m;
    }
};
