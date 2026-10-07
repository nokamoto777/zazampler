#pragma once
#include "Performance.h"
#include "Effects.h"
#include "Engine.h"
#include <vector>
#include <stdexcept>
namespace performancechecks {
inline void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Event {int time,type,key,velocity;};
inline void run(const std::string& sfz) {
    // Mono last-note priority, return to a held key, pedal and duplicate notes.
    MonoGlide mono;mono.prepare(1000);int time=0;std::vector<Event> events;
    auto emit=[&](const unsigned char* d,int n){if(n>=3)events.push_back({time,d[0]&0xf0,d[1],d[2]});};
    unsigned char a[]={0x90,60,100},b[]={0x90,72,100},off[]={0x80,72,0},pedal[]={0xb0,64,127},up[]={0xb0,64,0};
    mono.midi(a,3,true,0.1f,emit);require(mono.tick(true)==0,"first mono note must not glide from zero");
    mono.midi(b,3,true,0.1f,emit);require(std::abs(mono.tick(true)+1200)<0.001f,"glide starts at previous note");
    for(int i=1;i<100;++i)mono.tick(true);
    require(std::abs(mono.tick(true))<0.001f,"glide reaches target on time");
    mono.midi(pedal,3,true,0.1f,emit);mono.midi(off,3,true,0.1f,emit);
    require(events.back().key==72 && events.back().type==0x90,"pedal holds mono note");
    mono.midi(up,3,true,0.1f,emit);require(events.back().key==60 && events.back().type==0x90,"mono returns to held key after pedal up");
    require(std::abs(mono.tick(true)-1200)<0.001f,"return glide starts at current pitch");
    NoteStack keys;keys.midi(a,3);keys.midi(a,3);unsigned char offA[]={0x80,60,0};keys.midi(offA,3);require(keys.count()==1,"duplicate note count");keys.midi(offA,3);require(keys.count()==0,"duplicate note release");
    // Exact sequencer event timing at 1 kHz: eighth notes @120 = 250 samples.
    NoteSequence seq;seq.prepare(1000);SequenceSettings p;p.mode=3;p.gate=0.5f;p.notes={0,7,12,0,0,0,0,0};p.velocities[2]=0;
    events.clear();seq.midi(a,3,p,emit);
    for(time=0;time<750;++time)seq.tick(p,120,emit);
    require(events.size()==4,"sequence rest must not emit note-on");
    require(events[0].time==0 && events[0].key==60 && events[1].time==125 && events[1].type==0x80,"sequence gate timing");
    require(events[2].time==250 && events[2].key==67 && events[3].time==375,"sequence transpose timing");
    seq.midi(offA,3,p,emit);seq.tick(p,120,emit);require(events.size()==4,"rest note release");
    p.mode=1;p.velocities.fill(1);p.octaves=2;seq.reset();events.clear();seq.midi(a,3,p,emit);seq.midi(b,3,p,emit);
    for(time=0;time<1000;++time)seq.tick(p,120,emit);
    require(events[0].key==60 && events[2].key==72 && events[4].key==72 && events[6].key==84,"arpeggio ordering and octaves");
    // Shared modulation sources, independent rows, inversion and key tracking.
    Effects effects;effects.prepare(48000);FxSettings fx;fx.keytrack=1;PerformanceFrame frame;frame.note=72;frame.wheel=1;
    MatrixSettings matrix;matrix[0]={9,6,0.5f};matrix[1]={9,3,-0.75f};LfoModulation mod;
    for(int i=0;i<48000;++i)mod=effects.modulationTick(fx,120,{},false,{}, {},matrix,frame);
    require(std::abs(mod.cutoffOctaves-1)<0.001f,"keytrack octave per octave");
    require(std::abs(mod.pitchCents-600)<0.1f && std::abs(mod.pan+0.75f)<0.001f,"matrix simultaneous pitch and inverted pan");
    matrix={};for(int i=0;i<48000;++i)mod=effects.modulationTick(fx,120,{},false,{}, {},matrix,frame);
    require(std::abs(mod.pitchCents)<0.001f && std::abs(mod.pan)<0.001f,"matrix route-off releases modulation");
    LfoSettingsBank lfo;lfo[0].target=6;lfo[0].depth=1;lfo[0].shape=4;lfo[0].hz=0.01f;
    for(int i=0;i<4800;++i)mod=effects.modulationTick(fx,120,lfo,false,{},{});
    require(mod.pitchCents>1199,"direct LFO pitch modulation");
    // Actual sample playback, not only control values: 440 Hz becomes 880 Hz.
    auto audio=[&](int block,float cents,bool absolute=false,int key=69) {
        Engine engine(48000,block);require(engine.load(sfz),"pitch test load");engine.offline(true);
        std::vector<float> output(24000),right(static_cast<size_t>(block)),pitch(static_cast<size_t>(block),cents);
        unsigned char note[]={0x90,static_cast<unsigned char>(key),100};engine.midi(note,3,37);
        for(int start=0;start<24000;start+=block)engine.render(output.data()+start,right.data(),std::min(block,24000-start),pitch.data(),absolute);
        return output;
    };
    auto low=audio(128,0),high=audio(128,1200),split=audio(257,1200);
    auto frequency=[](const std::vector<float>& x){int crossings=0;for(size_t i=4801;i<x.size();++i)if(x[i-1]<=0 && x[i]>0)++crossings;return crossings*48000./(x.size()-4800);};
    require(std::abs(frequency(low)-440)<4 && std::abs(frequency(high)-880)<4,"sfizz pitch buffer must change measured audio frequency");
    double error=0;for(size_t i=0;i<high.size();++i)error+=std::abs(high[i]-split[i]);
    require(error/high.size()<0.001,"pitch render must not depend on host block size");
    const auto absolute=audio(128,6900,true,81);
    require(std::abs(frequency(absolute)-440)<4,"absolute mono pitch must compensate each voice trigger note");
    require(std::all_of(high.begin(),high.begin()+37,[](float x){return std::abs(x)<1e-7;}),"pitch extension must preserve delayed onset");
}
inline void processorAudio(const juce::File& root) {
    auto render=[&](int block,bool sequenced) {
        ZaZamplerProcessor p;p.prepareToPlay(48000,block);p.setNonRealtime(true);p.load(root,{},"Sine.sfz");
        auto set=[&](const char* id,float value){auto* param=p.parameters.getParameter(id);param->setValueNotifyingHost(param->convertTo0to1(value));};
        if(sequenced){set("seqMode",3);set("seqDivision",1);set("seqLength",2);set("seqNote1",0);set("seqNote2",12);set("seqGate",0.5f);}
        else {set("voiceMode",1);set("glide",0.1f);}
        std::vector<float> output(24000);
        for(int start=0;start<24000;start+=block) {
            const int n=std::min(block,24000-start);juce::AudioBuffer<float> audio(2,n);juce::MidiBuffer midi;
            for(const auto event:std::array<std::pair<int,int>,3>{{{37,69},{6000,81},{18000,-1}}}) {
                if(event.first>=start && event.first<start+n) {
                    if(event.second==-1)midi.addEvent(juce::MidiMessage::allSoundOff(1),event.first-start);
                    else if(!sequenced || event.second==69)midi.addEvent(juce::MidiMessage::noteOn(1,event.second,juce::uint8(100)),event.first-start);
                }
            }
            p.processBlock(audio,midi);std::copy_n(audio.getReadPointer(0),n,output.begin()+start);
        }
        return output;
    };
    auto mono=render(128,false),split=render(257,false),seq=render(128,true),seqSplit=render(257,true);
    auto difference=[](const std::vector<float>& a,const std::vector<float>& b){double error=0;for(size_t i=0;i<a.size();++i)error+=std::abs(a[i]-b[i]);return error/a.size();};
    require(difference(mono,split)<0.002,"processor glide block-size invariance");
    require(difference(seq,seqSplit)<0.002,"processor sequencer block-size invariance");
    int crossings=0;for(int i=12001;i<17000;++i)if(mono[static_cast<size_t>(i-1)]<=0 && mono[static_cast<size_t>(i)]>0)++crossings;
    require(std::abs(crossings*48000./4999.-880)<12,"processor mono glide must reach octave target");
    require(std::all_of(mono.begin()+19000,mono.end(),[](float x){return std::abs(x)<1e-6f;}),"mono CC120 stops voices");
    require(std::all_of(seq.begin()+19000,seq.end(),[](float x){return std::abs(x)<1e-6f;}),"sequencer CC120 stops voices");
}
}
