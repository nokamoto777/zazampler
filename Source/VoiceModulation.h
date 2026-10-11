#pragma once
#include "ModMatrix.h"
#include <vector>

// Shared with the pinned sfizz extension. Borrowed settings/CC frames live for
// one render call; every sfizz region voice owns its oscillator/envelope/filter.
struct VoiceModSettings {
    LfoSettingsBank lfos;
    EnvelopeSettings envelopes;
    MatrixSettings matrix;
    const PerformanceFrame* expression=nullptr;
    int frames=0,filterMode=0;
    float bpm=120,cutoff=20000,resonance=0.707f,keytrack=0;
};
class VoiceModulation {
public:
    void prepare(double sr){rate=sr;lfos.prepare(sr);envelopes.prepare(sr);}
    void resize(size_t size){frames.resize(size);}
    void start(float note,float velocity){key=note;vel=velocity;lfos.reset();envelopes.reset();filters={};first=true;releaseAt=-1;routes={};routeTails={};}
    void release(int delay){if(releaseAt<0)releaseAt=delay;}
    void generate(const VoiceModSettings* p,size_t count,size_t delay) {
        enabled=p!=nullptr;
        if(!enabled)return;
        for(size_t i=0;i<count;++i) {
            frames[i]={};
            if(i<delay)continue;
            EnvelopeEvent event;
            if(first){event.flags=1;first=false;}
            if(releaseAt>=0 && static_cast<int>(i)>=releaseAt){event.flags|=2;releaseAt=-1;}
            auto mod=lfos.tick(p->lfos,p->bpm,(event.flags&1)!=0);
            const auto env=envelopes.tick(p->envelopes,event);
            if(p->envelopes.ampEnabled)mod.amplitude*=env.amp;
            mod.cutoffOctaves+=env.filter*p->envelopes.filterOctaves;
            addMatrixTarget(mod,p->envelopes.modTarget,p->envelopes.modDepth*env.mod);
            PerformanceFrame performance=p->expression && static_cast<int>(i)<p->frames?p->expression[i]:PerformanceFrame{};
            performance.note=key;performance.velocity=vel;
            const float slew=static_cast<float>(1-std::exp(-1/(rate*0.002)));
            for(size_t row=0;row<p->matrix.size();++row) {
                const auto& route=p->matrix[row];
                if((route.source==0 || route.target==0 || route.depth==0) && !routeTails[row])continue;
                bool tail=false;
                const float value=matrixSource(route.source,mod,env,performance)*route.depth;
                for(int t=0;t<6;++t) {
                    auto& smoothed=routes[row][static_cast<size_t>(t)];
                    smoothed+=slew*((route.target==t+1?value:0)-smoothed);
                    if(std::abs(smoothed)<1e-8f)smoothed=0;
                    else {addMatrixTarget(mod,t+1,smoothed);tail=true;}
                }
                routeTails[row]=tail;
            }
            mod.cutoffOctaves=std::clamp(mod.cutoffOctaves+p->keytrack*(key-60)/12,-10.f,10.f);
            mod.pan=std::clamp(mod.pan,-1.f,1.f);mod.pitchCents=std::clamp(mod.pitchCents,-4800.f,4800.f);
            frames[i]=mod;
        }
        if(releaseAt>=0)releaseAt=std::max(0,releaseAt-static_cast<int>(count));
    }
    float pitch(size_t i) const{return enabled && i<frames.size()?frames[i].pitchCents:0;}
    void process(float* l,float* r,size_t count,const VoiceModSettings* p) {
        if(!p)return;
        for(size_t i=0;i<count;++i) {
            const auto& m=frames[i];
            const float hz=std::clamp(p->cutoff*std::exp2(m.cutoffOctaves),20.f,static_cast<float>(rate*0.45));
            const float g=std::tan(3.14159265358979323846f*hz/static_cast<float>(rate));
            const float k=1/std::clamp(p->resonance+m.resonance,0.5f,10.f);
            float x[]={l[i],r[i]};
            for(size_t c=0;c<2;++c) {
                if(p->filterMode) {
                    x[c]=filters[c][0].tick(x[c],g,k,p->filterMode);
                    if(p->filterMode==2 || p->filterMode==4)x[c]=filters[c][1].tick(x[c],g,k,p->filterMode);
                }
                if(m.driveDb>0){const float d=std::pow(10.f,std::min(m.driveDb,48.f)/20);x[c]=std::tanh(x[c]*d)/std::tanh(d);}
                x[c]*=m.amplitude*(c==0?std::min(1.f,1-m.pan):std::min(1.f,1+m.pan));
            }
            l[i]=x[0];r[i]=x[1];
        }
    }
private:
    struct Filter {
        float s1=0,s2=0;
        float tick(float x,float g,float k,int mode){const float a=1/(1+g*(g+k)),v1=a*s1+g*a*(x-s2),v2=s2+g*v1;s1=2*v1-s1;s2=2*v2-s2;const float hp=x-k*v1-v2;return mode==3 || mode==4?hp:mode==5?k*v1:mode==6?hp+v2:v2;}
    };
    double rate=44100;float key=60,vel=1;bool first=true,enabled=false;int releaseAt=-1;
    LfoBank lfos;EnvelopeBank envelopes;
    std::vector<LfoModulation> frames;
    std::array<std::array<Filter,2>,2> filters{};
    std::array<std::array<float,6>,32> routes{};
    std::array<bool,32> routeTails{};
};
