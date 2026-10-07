#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
struct AdsrSettings {float attack=0.01f,decay=0.2f,sustain=1.f,release=0.3f;};
struct EnvelopeSettings {
    AdsrSettings amp,filter,mod;
    bool ampEnabled=false;
    float filterOctaves=0,modDepth=0;
    int modTarget=0;
};
// Event flags are sample-offset events: note retrigger, last-note release, hard panic.
struct EnvelopeEvent {
    uint8_t flags=0;
    void merge(uint8_t value){if(value)flags=value==4?value:static_cast<uint8_t>((flags&1)|value);}
};
// The sfizz wrapper is one MIDI part, including Omni mode. Sustain/CC act on that part.
class EnvelopeGate {
public:
    void reset(){keys={};latched={};pedal=false;}
    bool held() const {for(size_t i=0;i<128;++i)if(keys[i] || latched[i])return true;return false;}
    uint8_t midi(const unsigned char* bytes,int size) {
        if(size<3)return 0;
        const int type=bytes[0]&0xf0;const auto key=static_cast<size_t>(bytes[1]&127);const int value=bytes[2]&127;
        const bool wasHeld=held();
        if(type==0x90 && value>0){if(keys[key]<65535)++keys[key];latched[key]=false;return 1;}
        if(type==0x80 || (type==0x90 && value==0)) {
            if(keys[key]>0){--keys[key];if(keys[key]==0 && pedal)latched[key]=true;}
        } else if(type==0xb0) {
            if(key==64){pedal=value>=64;if(!pedal)latched={};}
            else if(key==120){reset();return 4;}
            else if(key==123){for(size_t i=0;i<128;++i){if(keys[i] && pedal)latched[i]=true;keys[i]=0;}}
            else if(key==121){pedal=false;latched={};}
        }
        return wasHeld && !held()?2:0;
    }
private:
    std::array<uint16_t,128> keys{};
    std::array<bool,128> latched{};
    bool pedal=false;
};
class Adsr {
public:
    void prepare(double sr){rate=sr>0?sr:44100.;reset();}
    void reset(){level=releaseStart=0;stage=Idle;}
    void noteOn(){stage=Attack;}
    void noteOff(){if(stage!=Idle && stage!=Release){releaseStart=level;stage=Release;}}
    float tick(const AdsrSettings& p) {
        const double sustain=std::clamp(static_cast<double>(p.sustain),0.,1.);
        for(int transitions=0;transitions<3;++transitions) {
            switch(stage) {
                case Idle:return 0;
                case Attack:
                    if(p.attack<=0){level=1;stage=Decay;continue;}
                    level=std::min(1.,level+1./(rate*p.attack));if(level>=1.-1e-9){level=1;stage=Decay;}
                    break;
                case Decay:
                    if(p.decay<=0){level=sustain;stage=Sustain;continue;}
                    level=std::max(sustain,level-(1.-sustain)/(rate*p.decay));
                    if(level<=sustain+1e-9){level=sustain;stage=Sustain;}
                    break;
                case Sustain:
                    // Live sustain edits approach the new value without a discontinuity.
                    level+=std::clamp(sustain-level,-1./(rate*0.005),1./(rate*0.005));break;
                case Release:
                    if(p.release<=0){reset();return 0;}
                    level=std::max(0.,level-releaseStart/(rate*p.release));if(level<=1e-9){reset();return 0;}break;
            }
            break;
        }
        return static_cast<float>(std::clamp(level,0.,1.));
    }
private:
    enum Stage {Idle,Attack,Decay,Sustain,Release};Stage stage=Idle;
    double rate=44100,level=0,releaseStart=0;
};
struct EnvelopeValues {float amp=0,filter=0,mod=0;};
class EnvelopeBank {
public:
    void prepare(double sr){amp.prepare(sr);filter.prepare(sr);mod.prepare(sr);}
    void reset(){amp.reset();filter.reset();mod.reset();}
    EnvelopeValues tick(const EnvelopeSettings& p,EnvelopeEvent event={}) {
        if(event.flags&4)reset();
        else {
            if(event.flags&1){amp.noteOn();filter.noteOn();mod.noteOn();}
            if(event.flags&2){amp.noteOff();filter.noteOff();mod.noteOff();}
        }
        return {amp.tick(p.amp),filter.tick(p.filter),mod.tick(p.mod)};
    }
private:Adsr amp,filter,mod;
};
