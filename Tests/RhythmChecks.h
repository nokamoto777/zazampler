#pragma once
#include "Performance.h"
#include <vector>
#include <stdexcept>
namespace rhythmchecks {
inline void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
inline void run() {
    for(int hits=1;hits<=16;++hits)for(int rotation=0;rotation<16;++rotation) {
        std::vector<int> positions;
        for(int i=0;i<16;++i)if(NoteSequence::rhythmHit(i,hits,rotation))positions.push_back(i);
        require(static_cast<int>(positions.size())==hits,"rhythm hit count");
        int shortest=16,longest=0;
        for(size_t i=0;i<positions.size();++i) {
            int gap=(positions[(i+1)%positions.size()]-positions[i]+16)%16;if(!gap)gap=16;
            shortest=std::min(shortest,gap);longest=std::max(longest,gap);
        }
        require(longest-shortest<=1,"rhythm onsets must be maximally even");
    }
    struct Event{int time,key,type;};
    auto render=[](int block,int hits,int rotation,float swing,int order,int engine=1) {
        NoteSequence seq;seq.prepare(1000);SequenceSettings p;p.mode=1;p.rhythm=engine;p.hits=hits;p.rotation=rotation;p.swing=swing;p.noteOrder=order;p.gate=0.5f;
        std::vector<Event> events;int time=0;
        auto emit=[&](const unsigned char* d,int n){if(n>=3)events.push_back({time,d[1],d[0]&0xf0});};
        for(int key:{67,60,64}){unsigned char on[]={0x90,static_cast<unsigned char>(key),100};seq.midi(on,3,p,emit);}
        for(int start=0;start<4000;start+=block)for(time=start;time<std::min(start+block,4000);++time)seq.tick(p,120,emit);
        const auto rendered=events;
        unsigned char off[]={0xb0,120,0};seq.midi(off,3,p,emit);events.clear();seq.tick(p,120,emit);require(events.empty(),"rhythm panic must stop notes");
        return rendered;
    };
    for(int hits:{3,5,7,16}) {
        const auto a=render(128,hits,0,0,0),b=render(257,hits,0,0,0);
        require(a.size()==b.size(),"rhythm block-size event count");int notes=0;
        for(size_t i=0;i<a.size();++i){require(a[i].time==b[i].time && a[i].key==b[i].key && a[i].type==b[i].type,"rhythm block-size timing");if(a[i].type==0x90)++notes;}
        require(notes==hits*2,"odd rhythm must repeat within four beats");
    }
    // Independent one-based transcription of the requested step maps.
    const std::vector<std::vector<int>> maps={
        {1},{1,9},{1,7,13},{1,5,9,13},{1,5,8,11,14},
        {1,4,6,9,12,14},{1,3,5,7,9,11,13},{1,3,5,7,9,11,13,15},
        {1,3,5,7,9,11,13,15,16},{1,3,4,6,7,9,10,12,13,15},
        {1,3,4,6,7,9,10,12,13,15,16},{1,3,4,5,7,8,9,11,12,13,15,16},
        {1,3,4,5,6,8,9,10,12,13,14,16},{1,3,4,5,6,7,8,9,11,12,13,14,15,16},
        {1,3,4,5,6,7,8,9,10,11,12,13,14,15,16},{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16}
    };
    for(int pattern=1;pattern<=16;++pattern) {
        const auto& expected=maps[static_cast<size_t>(pattern-1)];
        for(int rotation=0;rotation<16;++rotation)for(int i=0;i<16;++i) {
            const int original=(i-rotation+16)%16+1;
            require(NoteSequence::fixedPatternHit(i,pattern,rotation)==(std::find(expected.begin(),expected.end(),original)!=expected.end()),"fixed map must match supplied steps under rotation");
        }
        for(int block:{128,257}) {
            const auto events=render(block,pattern,0,0,0,2);std::vector<int> actual,times;
            for(const auto& e:events)if(e.type==0x90)actual.push_back(e.time);
            for(int cycle=0;cycle<2;++cycle)for(int step:expected)times.push_back(cycle*2000+(step-1)*125);
            require(actual==times,"fixed map sample-accurate onsets over two bars");
        }
    }
    const auto fixedThree=render(128,3,0,0,0,2);
    require(fixedThree[1].type==0x80 && fixedThree[1].time==375,"fixed-map gate spans next hit");
    const auto fixedShuffle=render(128,16,0,0.5f,0,2);
    std::vector<int> fixedTimes;for(const auto& e:fixedShuffle)if(e.type==0x90)fixedTimes.push_back(e.time);
    require(fixedTimes[1]==188 && fixedTimes[16]==2000,"fixed map shuffle preserves bar length");
    auto three=render(128,3,0,0,0);std::vector<int> onsets;
    for(auto e:three)if(e.type==0x90)onsets.push_back(e.time);
    require(onsets==std::vector<int>({0,750,1375,2000,2750,3375}),"3/16 sample-accurate onsets");
    require(three[1].type==0x80 && three[1].time==375,"gate spans gaps between hits");
    auto swung=render(128,16,0,0.5f,0);onsets.clear();for(auto e:swung)if(e.type==0x90)onsets.push_back(e.time);
    require(onsets[0]==0 && onsets[1]==188 && onsets[2]==250 && onsets[16]==2000,"shuffle must preserve cycle duration");
    auto rotated=render(128,3,1,0,0);require(rotated.front().time==125,"rotation shifts by one grid step");
    NoteSequence seq;seq.prepare(1000);SequenceSettings p;p.mode=1;p.rhythm=1;p.hits=3;p.noteOrder=1;
    std::vector<int> keys;auto emit=[&](const unsigned char* d,int n){if(n>=3 && (d[0]&0xf0)==0x90)keys.push_back(d[1]);};
    for(int key:{67,60,64}){unsigned char on[]={0x90,static_cast<unsigned char>(key),100};seq.midi(on,3,p,emit);}
    for(int t=0;t<2000;++t)seq.tick(p,120,emit);
    require(keys==std::vector<int>({67,60,64}),"played order advances only on rhythm hits");
    unsigned char pedal[]={0xb0,64,127};seq.midi(pedal,3,p,emit);
    for(int key:{67,60,64}){unsigned char off[]={0x80,static_cast<unsigned char>(key),0};seq.midi(off,3,p,emit);}
    keys.clear();for(int t=0;t<2000;++t)seq.tick(p,120,emit);
    require(keys.size()==3,"rhythm sustain pedal keeps held notes");
    pedal[2]=0;seq.midi(pedal,3,p,emit);keys.clear();for(int t=0;t<2000;++t)seq.tick(p,120,emit);
    require(keys.empty(),"rhythm stops when all notes and pedal release");
    unsigned char on[]={0x90,72,100};seq.midi(on,3,p,emit);seq.tick(p,120,emit);
    require(keys==std::vector<int>({72}),"rhythm restarts from first hit after release");
    p.rhythm=0;keys.clear();seq.tick(p,120,emit);
    require(keys==std::vector<int>({72}),"switching rhythm retains held keys and restarts");
}
}
