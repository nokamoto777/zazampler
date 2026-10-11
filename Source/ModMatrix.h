#pragma once
#include "Performance.h"
#include "Lfo.h"
#include "Envelope.h"
struct MatrixRoute {int source=0,target=0;float depth=0;};
using MatrixSettings=std::array<MatrixRoute,32>;

inline float matrixSource(int source,const LfoModulation& m,const EnvelopeValues& e,const PerformanceFrame& p) {
    const std::array<float,21> values {0,m.waves[0],m.waves[1],m.waves[2],e.amp,e.filter,e.mod,
        p.velocity,(p.note-60)/60,p.wheel,p.pressure,p.bend,1,p.foot,p.expression,
        m.waves[0]*p.wheel,m.waves[1]*p.wheel,m.waves[2]*p.wheel,
        m.waves[0]*p.pressure,m.waves[1]*p.pressure,m.waves[2]*p.pressure};
    return values[static_cast<size_t>(std::clamp(source,0,20))];
}
inline void addMatrixTarget(LfoModulation& m,int target,float value) {
    switch(target) {
        case 1:m.cutoffOctaves+=5*value;break;
        case 2:m.amplitude*=std::pow(10.f,12*value/20);break;
        case 3:m.pan+=value;break;
        case 4:m.resonance+=4*value;break;
        case 5:m.driveDb+=24*value;break;
        case 6:m.pitchCents+=1200*value;break;
    }
}
