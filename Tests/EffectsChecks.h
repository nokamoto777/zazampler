#pragma once
#include "Effects.h"
#include <random>
#include <iostream>
#include <stdexcept>
namespace fxchecks {
inline void require(bool b,const char* m) {if(!b)throw std::runtime_error(m);}
inline float rms(const std::vector<float>& x,size_t start=0) {double s=0;for(size_t i=start;i<x.size();++i){require(std::isfinite(x[i]),"FX produced non-finite samples");s+=x[i]*x[i];}return static_cast<float>(std::sqrt(s/static_cast<double>(x.size()-start)));}
inline float response(FxSettings p,float hz) {
    Effects fx;fx.prepare(48000);p.gainDb=0;
    std::vector<float> l(24000),r(24000);
    for(size_t i=0;i<l.size();++i)l[i]=r[i]=0.1f*std::sin(2*3.14159265358979323846f*hz*static_cast<float>(i)/48000);
    fx.process(l.data(),r.data(),static_cast<int>(l.size()),p,120);return rms(l,12000);
}
inline void run() {
    FxSettings p;p.gainDb=0;
    Effects bypass;bypass.prepare(48000);
    std::vector<float> l(4096),r(4096),original;
    for(size_t i=0;i<l.size();++i)l[i]=r[i]=0.1f*std::sin(static_cast<float>(i)*0.1f);
    original=l;bypass.process(l.data(),r.data(),4096,p,120);
    for(size_t i=0;i<l.size();++i)require(std::abs(l[i]-original[i])<1.e-6f,"FX bypass changes gain or waveform");
    // Neutral EQ remains transparent even for narrow, low-frequency poles,
    // where FMA cancellation residue was amplified on Apple Silicon.
    for(float hz:{30.f,200.f,3000.f}) {
        FxSettings neutral;neutral.gainDb=0;neutral.eq1Hz=neutral.eq2Hz=hz;neutral.eq1Q=neutral.eq2Q=10;
        Effects eq;eq.prepare(48000);l=original;r=original;eq.process(l.data(),r.data(),4096,neutral,120);
        for(size_t i=0;i<l.size();++i)require(std::abs(l[i]-original[i])<1.e-6f && std::abs(r[i]-original[i])<1.e-6f,"neutral EQ FMA regression");
    }
    p.cutoff=1000;p.resonance=0.707f;p.filterMode=1;
    require(response(p,100)>0.06f && response(p,10000)<0.001f,"LP12 response");
    const float lp12=response(p,4000);p.filterMode=2;require(response(p,4000)<lp12*0.2f,"LP24 slope");
    p.filterMode=3;require(response(p,100)<0.001f && response(p,10000)>0.06f,"HP12 response");
    const float hp12=response(p,250);p.filterMode=4;require(response(p,250)<hp12*0.2f,"HP24 slope");
    p.filterMode=5;require(response(p,1000)>response(p,100)*4,"band pass response");
    p.filterMode=6;require(response(p,1000)<response(p,100)*0.01f,"notch rejection");
    p.filterMode=0;p.eq1Hz=1000;p.eq1Db=6;p.eq1Q=1;
    const float boosted=response(p,1000);p.eq1Db=0;const float flat=response(p,1000);
    require(boosted/flat>1.97f && boosted/flat<2.02f,"EQ +6 dB center gain");
    p.eq2Hz=1000;p.eq2Db=-6;p.eq2Q=1;require(response(p,1000)/flat<0.51f,"EQ2 cut");
    p={};p.gainDb=0;p.delayMix=1;p.delaySync=1;p.delayDivision=4;p.delayFeedback=0.5f;
    require(std::abs(Effects::delayMilliseconds(p,120)-500)<0.01f,"quarter delay tempo");
    require(std::abs(Effects::delayMilliseconds(p,60)-1000)<0.01f,"delay follows BPM");
    Effects delay;delay.prepare(48000);l.assign(73000,0);r=l;l[0]=r[0]=1;
    delay.process(l.data(),r.data(),static_cast<int>(l.size()),p,120);
    require(std::abs(l[24000]-1)<1.e-5f && std::abs(l[48000]-0.5f)<1.e-5f && std::abs(l[72000]-0.25f)<1.e-5f,"delay impulse/feedback timing");
    require(rms(l,1)>0.001f,"delay silence");
    delay.reset();l.assign(4096,0);r=l;delay.process(l.data(),r.data(),4096,p,120);require(rms(l)<1.e-10f,"reset leaves delay tail");
    p.delayPingPong=1;delay.reset();l.assign(49000,0);r=l;l[0]=1;delay.process(l.data(),r.data(),49000,p,120);
    require(std::abs(l[24000]-1)<1.e-5f && std::abs(r[48000]-0.5f)<1.e-5f && std::abs(l[48000])<1.e-5f,"ping pong cross feedback");
    p={};p.gainDb=0;p.driveMix=1;p.drive=24;
    Effects saturation;saturation.prepare(48000);l.assign(4096,2);r=l;saturation.process(l.data(),r.data(),4096,p,120);
    require(rms(l)<0.5f,"saturation does not compress large input");
    p={};p.gainDb=0;p.chorusMix=1;p.chorusDepth=0;
    Effects chorus;chorus.prepare(48000);l.assign(2000,0);r=l;l[0]=r[0]=1;chorus.process(l.data(),r.data(),2000,p,120);
    require(std::abs(l[720]-1)<1.e-4f && std::abs(l[0])<1.e-8f,"chorus center delay");
    p={};p.gainDb=0;p.phaserMix=1;p.phaserDepth=1;
    Effects phaser;phaser.prepare(48000);l=original;r=original;phaser.process(l.data(),r.data(),4096,p,120);
    float difference=0;for(size_t i=0;i<l.size();++i)difference+=std::abs(l[i]-original[i]);require(difference>1,"phaser has no effect");
    p={};p.gainDb=0;p.reverb=1;
    Effects space;space.prepare(48000);l.assign(96000,0);r=l;l[0]=r[0]=1;space.process(l.data(),r.data(),96000,p,120);
    require(rms(l,3000)>1.e-6f,"reverb has no tail");
    // Exercise max resonance/drive/feedback at several sample rates, and mode switches.
    for(double rate:{44100.,48000.,96000.}) {
        Effects fx;fx.prepare(rate);p={};p.gainDb=-12;p.resonance=10;p.drive=30;p.driveMix=1;p.eq1Db=18;p.eq2Db=18;
        p.chorusMix=0.5f;p.phaserMix=0.5f;p.phaserFeedback=0.8f;p.delayMix=0.5f;p.delayFeedback=0.95f;p.reverb=0.5f;
        std::mt19937 rng(1234);std::uniform_real_distribution<float> noise(-0.1f,0.1f);
        l.resize(257);r.resize(257);
        for(int block=0;block<200;++block) {
            p.filterMode=(block/25)%8;p.cutoff=(block%2)?40.f:20000.f;
            for(size_t i=0;i<l.size();++i){l[i]=noise(rng);r[i]=noise(rng);}
            fx.process(l.data(),r.data(),257,p,120);require(std::isfinite(rms(l)) && std::isfinite(rms(r)),"stress output not finite");
        }
    }
    // Block splitting must not change steady parameter processing.
    p={};p.gainDb=-6;p.filterMode=2;p.cutoff=4000;p.chorusMix=0.4f;p.phaserMix=0.5f;p.delayMix=0.3f;p.delayMs=30;p.reverb=0.3f;
    Effects a,b;a.prepare(48000);b.prepare(48000);
    std::vector<float> x(8192),y(8192),xx,yy;
    for(size_t i=0;i<x.size();++i)x[i]=y[i]=std::sin(static_cast<float>(i)*0.1f)*0.1f;
    xx=x;yy=y;a.process(x.data(),y.data(),8192,p,120);
    for(int i=0;i<8192;i+=128)b.process(xx.data()+i,yy.data()+i,128,p,120);
    for(size_t i=0;i<x.size();++i)require(std::abs(x[i]-xx[i])<1.e-5f,"FX depends on host block size");
    std::cout<<"PASS: bypass, LP/HP slopes, BP/notch, two EQs, saturation, chorus, phaser, delay sync/feedback/ping-pong/reset, reverb, stress, block invariance\n";
}
}
