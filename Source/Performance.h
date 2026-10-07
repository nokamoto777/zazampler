#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>

struct PerformanceFrame {
    float note=60,velocity=0,wheel=0,pressure=0,bend=0;
    void midi(const unsigned char* d,int n) {
        if(n<2)return;
        const int type=d[0]&0xf0,a=d[1]&127,b=n>2?d[2]&127:0;
        if(type==0x90 && n>=3 && b){note=static_cast<float>(a);velocity=b/127.f;}
        if(type==0xd0)pressure=a/127.f;
        if(type==0xe0 && n>=3)bend=((a|(b<<7))-8192)/8192.f;
        if(type==0xb0 && n>=3){if(a==1)wheel=b/127.f;if(a==121){wheel=pressure=bend=0;}}
    }
};
// One MIDI part, same Omni-channel merging convention as sfizz and EnvelopeGate.
class NoteStack {
public:
    void reset(){counts={};latched={};order={};velocities={};serial=0;pedal=false;}
    void midi(const unsigned char* d,int n) {
        if(n<3)return;
        const int type=d[0]&0xf0,k=d[1]&127,v=d[2]&127;
        if(type==0x90 && v){if(counts[k]<65535)++counts[k];latched[k]=false;order[k]=++serial;velocities[k]=v;}
        else if(type==0x80 || (type==0x90 && !v)){if(counts[k]){--counts[k];if(!counts[k] && pedal)latched[k]=true;}}
        else if(type==0xb0) {
            if(k==64){pedal=v>=64;if(!pedal)latched={};}
            if(k==120)reset();
            if(k==121){pedal=false;latched={};}
            if(k==123){for(int i=0;i<128;++i){if(counts[i] && pedal)latched[i]=true;counts[i]=0;}}
        }
    }
    bool held(int key) const{return counts[key] || latched[key];}
    int latest() const {int key=-1;for(int i=0;i<128;++i)if(held(i) && (key<0 || order[i]>order[key]))key=i;return key;}
    int velocity(int key) const{return velocities[key];}
    int count() const {int result=0;for(int i=0;i<128;++i)if(held(i))++result;return result;}
    int nth(int index) const {for(int i=0;i<128;++i)if(held(i) && index--==0)return i;return -1;}
private:
    std::array<uint16_t,128> counts{};
    std::array<bool,128> latched{};
    std::array<uint64_t,128> order{};
    std::array<int,128> velocities{};
    uint64_t serial=0;bool pedal=false;
};

struct SequenceSettings {
    int mode=0,division=2,length=8,octaves=1; // Off, Up, Down, Step
    float gate=0.7f;
    std::array<int,8> notes {0,0,7,0,12,7,3,7};
    std::array<float,8> velocities {1,1,1,1,1,1,1,1};
};
class NoteSequence {
public:
    void prepare(double sr){rate=sr;reset();}
    void reset(){keys.reset();remaining=gateRemaining=0;step=0;sounding=-1;active=false;}
    static double period(const SequenceSettings& p,double sr,float bpm) {
        static constexpr double beats[]={0.125,0.25,0.5,1./3.,0.75,1.,1.5,2.};
        return sr*60./std::clamp(std::isfinite(bpm)?static_cast<double>(bpm):120.,20.,400.)*beats[std::clamp(p.division,0,7)];
    }
    template<class Emit> void midi(const unsigned char* d,int n,const SequenceSettings& p,Emit&& emit) {
        if(!p.mode){emit(d,n);return;}
        keys.midi(d,n);
        const int type=n?d[0]&0xf0:0;
        // The sequencer owns note duration and sustain. Other expression is passed through.
        if(type!=0x80 && type!=0x90 && !(n>=3 && type==0xb0 && d[1]==64))emit(d,n);
        if(n>=3 && type==0xb0 && d[1]==120){sounding=-1;active=false;remaining=0;step=0;}
    }
    template<class Emit> void tick(const SequenceSettings& p,float bpm,Emit&& emit) {
        if(!p.mode)return;
        auto off=[&]{if(sounding>=0){unsigned char d[]={0x80,static_cast<unsigned char>(sounding),0};emit(d,3);sounding=-1;}};
        if(!keys.count()){off();active=false;remaining=0;step=0;return;}
        if(!active){active=true;remaining=0;step=0;}
        if(sounding>=0 && gateRemaining<=0)off();
        if(remaining<=0) {
            off();
            const auto duration=period(p,rate,bpm);
            const int length=std::clamp(p.length,1,8),slot=step%length;
            int key=keys.latest();
            if(p.mode==3)key+=p.notes[slot];
            else {
                const int count=keys.count(),total=count*std::clamp(p.octaves,1,3);
                int index=step%total;if(p.mode==2)index=total-1-index;
                key=keys.nth(index%count)+12*(index/count);
            }
            const int velocity=static_cast<int>(std::round(keys.velocity(keys.latest())*std::clamp(p.velocities[slot],0.f,1.f)));
            if(key>=0 && key<=127 && velocity>0){unsigned char d[]={0x90,static_cast<unsigned char>(key),static_cast<unsigned char>(std::clamp(velocity,1,127))};emit(d,3);sounding=key;}
            gateRemaining=std::max(1.,duration*std::clamp(p.gate,0.05f,0.95f));
            remaining+=duration;++step;if(step>=1000000)step=0;
        }
        --remaining;--gateRemaining;
    }
private:
    NoteStack keys;double rate=44100,remaining=0,gateRemaining=0;int step=0,sounding=-1;bool active=false;
};
class MonoGlide {
public:
    void prepare(double sr){rate=sr;reset();}
    void reset(){keys.reset();sounding=-1;current=target=increment=0;left=0;}
    template<class Emit> void midi(const unsigned char* d,int n,bool mono,float seconds,Emit&& emit) {
        if(!mono){emit(d,n);return;}
        const int old=keys.latest();keys.midi(d,n);const int key=keys.latest();
        const int type=n?d[0]&0xf0:0;
        if(type!=0x80 && type!=0x90 && !(n>=3 && type==0xb0 && d[1]==64))emit(d,n);
        if(n>=3 && type==0xb0 && d[1]==120){reset();return;}
        const bool retrigger=n>=3 && type==0x90 && d[2]>0;
        if(key!=old || retrigger) {
            if(sounding>=0){unsigned char off[]={0x80,static_cast<unsigned char>(sounding),0};emit(off,3);}
            sounding=key;
            if(key>=0) {
                target=static_cast<double>(key);
                if(old<0 || seconds<=0){current=target;left=0;increment=0;}
                else {left=std::max(1,static_cast<int>(std::round(seconds*rate)));increment=(target-current)/left;}
                unsigned char on[]={0x90,static_cast<unsigned char>(key),static_cast<unsigned char>(keys.velocity(key))};emit(on,3);
            }
        }
    }
    float tick(bool mono,bool absolute=false){const float cents=mono?static_cast<float>((absolute?current:current-target)*100.):0.f;if(left>0){current+=increment;if(--left==0)current=target;}return cents;}
private:
    NoteStack keys;double rate=44100,current=0,target=0,increment=0;int left=0,sounding=-1;
};
