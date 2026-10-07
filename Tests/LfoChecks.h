#pragma once
#include "Effects.h"
#include <vector>
#include <stdexcept>
namespace lfochecks {
inline void require(bool v,const char* msg){if(!v)throw std::runtime_error(msg);}
inline void run() {
    LfoSettingsBank p;LfoBank lfo;lfo.prepare(48000);
    p[0].target=3;p[0].depth=1;p[0].hz=2;
    int crossings=0;float last=0;
    for(int i=0;i<96000;++i) {const float x=lfo.tick(p,120).pan;if(i>2400 && last<0 && x>=0)++crossings;last=x;}
    require(crossings==3,"LFO 2 Hz period");
    p[0].sync=1;p[0].division=5;
    require(std::abs(LfoBank::frequency(p[0],120)-2.)<1e-9 && std::abs(LfoBank::frequency(p[0],60)-1.)<1e-9,"LFO tempo division");
    p={};p[0].target=2;p[0].depth=1;p[0].shape=4;p[0].hz=0.01f;p[0].fade=1;
    lfo.reset();const auto start=lfo.tick(p,120,true).amplitude;
    float half=0,end=0;
    for(int i=1;i<=48000;++i){auto m=lfo.tick(p,120);if(i==24000)half=m.amplitude;end=m.amplitude;}
    require(start>0.999f && half>0.49f && half<0.52f && end<0.004f,"LFO one-second fade-in");
    // A note restarts fade even when phase is free-running.
    for(int i=0;i<1000;++i)end=lfo.tick(p,120,i==0).amplitude;
    require(end>0.96f,"LFO note fade restart");
    for(int shape=0;shape<6;++shape) {
        p={};for(size_t i=0;i<3;++i){p[i].shape=shape;p[i].target=static_cast<int>(i)+1;p[i].depth=-1;p[i].hz=30;p[i].skew=0.05f;}
        lfo.reset();for(int i=0;i<24000;++i){auto m=lfo.tick(p,400,i%1307==0);require(std::isfinite(m.cutoffOctaves) && m.amplitude>=0 && m.amplitude<=1 && std::abs(m.pan)<=1,"LFO bounds");}
    }
    // Identical signal regardless of host segmentation, including an in-block retrigger.
    constexpr int count=48000;std::vector<float> a(count),b(count),c(count),d(count);std::vector<uint8_t> triggers(count);
    for(size_t i=0;i<a.size();++i)a[i]=b[i]=c[i]=d[i]=0.1f*std::sin(0.07f*static_cast<float>(i));
    triggers[499]=1;triggers[16001]=1;
    FxSettings fx;fx.gainDb=0;fx.filterMode=1;fx.cutoff=1200;
    p={};p[0].target=1;p[0].depth=0.6f;p[0].retrigger=1;p[0].hz=3;
    p[1].target=2;p[1].depth=0.8f;p[1].sync=1;p[1].division=2;
    p[2].target=3;p[2].depth=0.5f;p[2].shape=5;
    Effects whole,split;whole.prepare(48000);split.prepare(48000);
    whole.process(a.data(),b.data(),count,fx,120,p,triggers.data());
    for(int start=0;start<count;start+=127)split.process(c.data()+start,d.data()+start,std::min(127,count-start),fx,120,p,triggers.data()+start);
    for(size_t i=0;i<a.size();++i)require(std::abs(a[i]-c[i])<1e-6f && std::abs(b[i]-d[i])<1e-6f,"LFO split-buffer invariance");
    // Actual effects-path tremolo: square LFO must alternate near silence and dry amplitude.
    fx={};fx.gainDb=0;p={};p[0].target=2;p[0].depth=1;p[0].shape=4;p[0].hz=2;
    std::fill(a.begin(),a.end(),0.1f);std::fill(b.begin(),b.end(),0.1f);whole.reset();whole.process(a.data(),b.data(),count,fx,120,p);
    require(std::abs(a[6000])<0.00001f && a[18000]>0.099f,"audible LFO tremolo");
    // Random waveform repeats after panic/reset, but the three generators are independent.
    p={};p[0].target=3;p[0].shape=5;p[0].depth=1;lfo.reset();const float first=lfo.tick(p,120).pan;
    lfo.reset();require(std::abs(first-lfo.tick(p,120).pan)<1e-10f,"LFO deterministic reset");
    p[1]=p[0];p[0].target=0;lfo.reset();require(std::abs(first-lfo.tick(p,120).pan)>1e-7f,"independent random LFOs");
}
}
