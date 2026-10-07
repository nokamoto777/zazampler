#pragma once
#include "Effects.h"
#include <stdexcept>
#include <vector>
namespace envelopechecks {
inline void require(bool v,const char* msg){if(!v)throw std::runtime_error(msg);}
inline void run() {
    Adsr a;a.prepare(1000);AdsrSettings p{0.1f,0.2f,0.5f,0.1f};a.noteOn();
    float v=0;for(int i=0;i<50;++i)v=a.tick(p);require(std::abs(v-0.5f)<0.001f,"ADSR attack midpoint");
    for(int i=0;i<251;++i)v=a.tick(p);require(std::abs(v-0.5f)<0.01f,"ADSR decay/sustain");
    a.noteOff();for(int i=0;i<50;++i)v=a.tick(p);require(std::abs(v-0.25f)<0.01f,"ADSR release midpoint");
    for(int i=0;i<52;++i)v=a.tick(p);require(v<1e-7f,"ADSR release completion");
    p={0,0,0.8f,0};a.noteOn();require(std::abs(a.tick(p)-0.8f)<1e-6f,"instant attack/decay");a.noteOff();require(a.tick(p)<1e-7f,"instant release");
    p={1,1,0.5f,1};a.noteOn();for(int i=0;i<100;++i)v=a.tick(p);a.noteOn();require(std::abs(a.tick(p)-v)<0.002f,"retrigger continuity");
    EnvelopeGate gate;
    auto midi=[&](int status,int note,int value){unsigned char b[]={static_cast<unsigned char>(status),static_cast<unsigned char>(note),static_cast<unsigned char>(value)};return gate.midi(b,3);};
    require(midi(0x90,60,100)==1 && midi(0x90,64,100)==1,"gate note trigger");
    require(midi(0x80,60,0)==0 && gate.held(),"overlapping notes release too early");
    midi(0xb0,64,127);require(midi(0x90,64,0)==0 && gate.held(),"sustain pedal / velocity zero");
    require(midi(0xb0,64,0)==2 && !gate.held(),"pedal release");
    midi(0x90,60,100);midi(0x90,60,100);require(midi(0x80,60,0)==0,"repeated key release");require(midi(0x80,60,0)==2,"repeated key final release");
    midi(0x90,60,100);midi(0xb0,64,127);require(midi(0xb0,123,0)==0 && gate.held(),"CC123 must honour sustain");require(midi(0xb0,121,0)==2,"CC121 releases pedal latch");
    midi(0x90,60,100);require(midi(0xb0,120,0)==4 && !gate.held(),"CC120 hard reset");
    EnvelopeEvent e;e.merge(2);e.merge(1);require(e.flags==1,"same-sample off/on ordering");e.merge(2);require(e.flags==3,"same-sample on/off ordering");
    // Direct audio test with constant input isolates the envelope from SFZ release behavior.
    constexpr size_t n=48000;std::vector<float> l(n,0.1f),r(n,0.1f),l2=l,r2=r;std::vector<EnvelopeEvent> events(n);
    events[100].flags=1;events[24100].flags=2;
    FxSettings fx;fx.gainDb=0;EnvelopeSettings settings;settings.ampEnabled=true;settings.amp={0.1f,0.1f,0.5f,0.1f};
    Effects whole,split;whole.prepare(48000);split.prepare(48000);
    whole.process(l.data(),r.data(),static_cast<int>(n),fx,120,{},nullptr,settings,events.data());
    for(size_t start=0;start<n;start+=127)split.process(l2.data()+start,r2.data()+start,static_cast<int>(std::min(size_t(127),n-start)),fx,120,{},nullptr,settings,events.data()+start);
    require(l[99]<1e-7f && std::abs(l[2500]-0.05f)<0.001f && std::abs(l[16000]-0.05f)<0.001f && l[30000]<1e-7f,"ADSR audio amplitude/timing");
    for(size_t i=0;i<n;++i)require(std::abs(l[i]-l2[i])<1e-6f && std::abs(r[i]-r2[i])<1e-6f,"ADSR block segmentation");
    // Cutoff envelope opens the real filter; Mod envelope moves the stereo balance.
    settings={};settings.filter={0,0,1,0};settings.filterOctaves=3;fx.filterMode=1;fx.cutoff=600;
    for(size_t i=0;i<n;++i)l[i]=r[i]=l2[i]=r2[i]=0.1f*std::sin(6.2831853f*4000.f*static_cast<float>(i)/48000.f);
    whole.reset();split.reset();events.assign(n,{});events[0].flags=1;
    whole.process(l.data(),r.data(),static_cast<int>(n),fx,120,{},nullptr,settings,events.data());
    split.process(l2.data(),r2.data(),static_cast<int>(n),fx,120);
    double opened=0,closed=0;for(size_t i=1000;i<n;++i){opened+=l[i]*l[i];closed+=l2[i]*l2[i];}
    require(opened>closed*10.,"filter envelope must affect audio");
    settings={};settings.mod={0,0,1,0};settings.modTarget=3;settings.modDepth=1;fx={};fx.gainDb=0;
    std::fill(l.begin(),l.end(),0.1f);std::fill(r.begin(),r.end(),0.1f);whole.reset();
    whole.process(l.data(),r.data(),static_cast<int>(n),fx,120,{},nullptr,settings,events.data());
    require(l.back()<0.0001f && r.back()>0.099f,"mod envelope audio pan");
    // Filter and mod envelopes are independent from amp bypass.
    EnvelopeBank bank;bank.prepare(1000);settings.filter={0,0,0.3f,0};settings.mod={0,0,0.7f,0};
    auto values=bank.tick(settings,{1});require(std::abs(values.filter-0.3f)<1e-6f && std::abs(values.mod-0.7f)<1e-6f,"independent filter/mod envelopes");
}
}
