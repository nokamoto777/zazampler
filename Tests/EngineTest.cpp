#include "Engine.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
static void check(bool v,const char* msg) { if (!v) throw std::runtime_error(msg); }
static float energy(const std::vector<float>& v, int a=0, int b=-1) {
    float e=0; if (b<0) b=static_cast<int>(v.size());
    for(int i=a;i<b;++i) { check(std::isfinite(v[i]),"non-finite audio"); e+=v[i]*v[i]; } return e;
}
int main(int argc,char** argv) {
    try {
        check(argc==2,"demo path required"); const std::string root=argv[1];
        Engine e(48000,512); e.offline(true);
        check(e.load(root+"/Sine.sfz"),"load SFZ"); check(e.regions()==1 && e.samples()==1,"region/sample count");
        std::vector<float> l(512),r(512);
        unsigned char on[]={0x90,69,100},off[]={0x80,69,0};
        e.midi(on,3,128); e.render(l.data(),r.data(),512);
        check(energy(l,0,128)<1e-12f,"sample-accurate onset: early audio");
        check(energy(l,200)>0.01f,"note on audible");
        check(std::abs(energy(l)-energy(r))<1e-3f,"stereo balance");
        e.midi(off,3,0); for(int i=0;i<100;++i) e.render(l.data(),r.data(),512);
        check(energy(l)<1e-8f,"release reaches silence");
        unsigned char pedal[]={0xb0,64,127};
        e.midi(pedal,3,0);e.midi(on,3,1);e.render(l.data(),r.data(),512);
        e.midi(off,3,0); for(int i=0;i<30;++i)e.render(l.data(),r.data(),512);
        check(energy(l)>0.01f,"sustain holds note");
        pedal[2]=0;e.midi(pedal,3,0);for(int i=0;i<100;++i)e.render(l.data(),r.data(),512);
        check(energy(l)<1e-8f,"pedal up releases note");
        check(e.load(root+"/Layers.sfz"),"load layers");
        unsigned char lowWrong[]={0x90,60,100},lowRight[]={0x90,60,40};
        e.midi(lowWrong,3,0);e.render(l.data(),r.data(),512);check(energy(l)<1e-8f,"velocity zone excluded");
        e.midi(lowRight,3,0);e.render(l.data(),r.data(),512);check(energy(l)>1e-5f,"velocity zone included");
        check(e.load(root+"/Offset.sfz"),"load offset/end");
        e.midi(on,3,0);e.render(l.data(),r.data(),512);check(energy(l)>0.01f,"offset playback");
        for(int i=0;i<100;++i)e.render(l.data(),r.data(),512);check(energy(l)<1e-8f,"end respected");
        check(!e.load(root+"/missing.sfz"),"missing instrument fails");
        check(e.load(root+"/Sine.sfz"),"reload after failure");
        e.prepare(44100,512);e.offline(true);e.midi(on,3,0);
        std::vector<float> rendered;
        for(int i=0;i<100;++i) { e.render(l.data(),r.data(),512); rendered.insert(rendered.end(),l.begin(),l.end()); }
        int crossings=0; for(size_t i=4411;i<48510;++i) if(rendered[i-1]<=0 && rendered[i]>0)++crossings;
        check(crossings>=438 && crossings<=442,"sample rate conversion maintains 440 Hz");
        e.panic();for(int i=0;i<100;++i)e.render(l.data(),r.data(),512);check(energy(l)<1e-8f,"panic silence");
        e.midi(on,1,0);e.render(l.data(),r.data(),512);check(energy(l)<1e-8f,"truncated MIDI ignored");
        std::cout << "PASS: SFZ/WAV, timing, stereo, release, sustain, velocity zones, offset/end, missing file, resampling, panic, malformed MIDI\n";
    } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n';return 1; }
}
