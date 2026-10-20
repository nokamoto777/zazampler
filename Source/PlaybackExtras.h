#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
struct OutputLimiter {
    float gain=1;double release=0.999;
    void prepare(double sr){gain=1;release=std::exp(-1./(sr*.05));}
    void process(float& l,float& r){
        if(!std::isfinite(l) || !std::isfinite(r)){l=r=0;gain=0;return;}
        const float peak=std::max(std::abs(l),std::abs(r));
        const float target=peak>.98f?.98f/peak:1.f;
        gain=target<gain?target:static_cast<float>(target+(gain-target)*release);
        l*=gain;r*=gain;
    }
};
struct RexSliceEvent {double beat;int slice;};
class RexClock {
public:
    void begin(const std::vector<RexSliceEvent>& events,double length,double ppq){
        loop=length;index=0;cycle=0;
        if(events.empty() || !(length>0))return;
        cycle=std::floor(ppq/length);
        const double position=ppq-cycle*length;
        index=static_cast<size_t>(std::lower_bound(events.begin(),events.end(),position-1.e-9,
            [](const RexSliceEvent& e,double beat){return e.beat<beat;})-events.begin());
        if(index==events.size()){index=0;++cycle;}
    }
    template<class Emit>void tick(const std::vector<RexSliceEvent>& events,double ppq,double step,Emit emit){
        if(events.empty() || !(loop>0))return;
        while(cycle*loop+events[index].beat<ppq+step-1.e-9){
            if(cycle*loop+events[index].beat>=ppq-1.e-9)emit(events[index].slice);
            if(++index==events.size()){index=0;++cycle;}
        }
    }
private: size_t index=0;double loop=0,cycle=0;
};
