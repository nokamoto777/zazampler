#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>

struct LfoSettings {
    int shape=0, target=0, sync=0, division=5, retrigger=0;
    float hz=1.f, skew=0.5f, fade=0.f, depth=0.f;
};
using LfoSettingsBank=std::array<LfoSettings,3>;
struct LfoModulation {
    float cutoffOctaves=0, amplitude=1, pan=0, resonance=0, driveDb=0, pitchCents=0;
    std::array<float,3> waves{};
};
// Three global, sample-clocked oscillators. No allocation or host parameter writes.
class LfoBank {
public:
    void prepare(double sampleRate) {
        rate=std::isfinite(sampleRate) && sampleRate>0?sampleRate:44100.;
        slew=static_cast<float>(1.-std::exp(-1./(rate*0.002)));
        reset();
    }
    void reset() {
        states={};
        for(size_t i=0;i<states.size();++i) {
            states[i].seed=0x51f15eU+static_cast<uint32_t>(i)*0x9e3779b9U;
            states[i].held=random(states[i].seed);
        }
    }
    static double frequency(const LfoSettings& s,float bpm) {
        static constexpr double beats[]={0.125,0.25,0.5,1./3.,0.75,1.,1.5,2.,4.,8.,16.};
        const double tempo=std::isfinite(bpm)?std::clamp(static_cast<double>(bpm),20.,400.):120.;
        return s.sync?tempo/(60.*beats[static_cast<size_t>(std::clamp(s.division,0,10))]):std::clamp(static_cast<double>(s.hz),0.01,30.);
    }
    LfoModulation tick(const LfoSettingsBank& settings,float bpm,bool noteOn=false) {
        LfoModulation out;
        for(size_t i=0;i<states.size();++i) {
            auto& state=states[i];const auto& p=settings[i];
            if(noteOn) {
                state.age=0;
                if(p.retrigger) {state.phase=0;state.held=random(state.seed);}
            }
            const double skew=std::clamp(static_cast<double>(p.skew),0.05,0.95);
            const double warped=state.phase<skew?0.5*state.phase/skew:0.5+0.5*(state.phase-skew)/(1.-skew);
            float wave=0;
            switch(p.shape) {
                case 1:wave=static_cast<float>(1.-4.*std::abs(warped-0.5));break;
                case 2:wave=static_cast<float>(2.*warped-1.);break;
                case 3:wave=static_cast<float>(1.-2.*warped);break;
                case 4:wave=warped<0.5?1.f:-1.f;break;
                case 5:wave=state.held;break;
                default:wave=static_cast<float>(std::sin(6.283185307179586*warped));break;
            }
            const float fade=p.fade<=0?1.f:static_cast<float>(std::min(1.,state.age/(rate*static_cast<double>(p.fade))));
            out.waves[i]=wave*fade;
            const float depth=std::clamp(p.depth,-1.f,1.f)*fade;
            // Slew each destination independently: routing changes crossfade instead of jumping.
            for(int target=1;target<=6;++target) {
                float value=0;
                if(p.target==target) {
                    value=target==2?std::abs(depth)*0.5f*(1.f+(depth<0?-wave:wave)):depth*wave;
                }
                auto& smoothed=state.routes[static_cast<size_t>(target-1)];smoothed+=slew*(value-smoothed);
            }
            out.cutoffOctaves+=5.f*state.routes[0];
            out.amplitude*=1.f-state.routes[1];
            out.pan+=state.routes[2];out.resonance+=4.f*state.routes[3];out.driveDb+=24.f*state.routes[4];out.pitchCents+=1200.f*state.routes[5];
            state.phase+=frequency(p,bpm)/rate;
            if(state.phase>=1.) {state.phase-=std::floor(state.phase);state.held=random(state.seed);}
            // Saturate age after all supported fades are complete; no unbounded counters.
            state.age=std::min(state.age+1.,rate*20.);
        }
        out.cutoffOctaves=std::clamp(out.cutoffOctaves,-10.f,10.f);
        out.amplitude=std::clamp(out.amplitude,0.f,1.f);out.pan=std::clamp(out.pan,-1.f,1.f);
        return out;
    }
private:
    struct State {double phase=0,age=0;uint32_t seed=1;float held=0;std::array<float,6> routes{};};
    std::array<State,3> states;
    double rate=44100.;float slew=0.01f;
    static float random(uint32_t& seed) {
        seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;
        return 2.f*static_cast<float>(seed>>8)/16777215.f-1.f;
    }
};
