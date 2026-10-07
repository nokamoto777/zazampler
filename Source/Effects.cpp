#include "Effects.h"
#include <cmath>
#include <algorithm>

float Effects::Svf::tick(float x,float g,float k,int mode) {
    const float a1=1.f/(1.f+g*(g+k)), a2=g*a1, a3=g*a2;
    const float v3=x-s2,v1=a1*s1+a2*v3,v2=s2+a2*s1+a3*v3;
    s1=2*v1-s1;s2=2*v2-s2;
    const float hp=x-k*v1-v2;
    switch(mode) {case 3:case 4:return hp;case 5:return k*v1;case 6:return hp+v2;default:return v2;}
}
void Effects::Eq::coefficients(double sr,float hz,float db,float q) {
    // A neutral bell is exactly the identity. Running its recursive form can
    // accumulate cancellation residue when Apple Clang contracts multiply/add.
    bypass = db == 0.f;
    if(bypass) {z1=z2=0.f;return;}
    const float w=2*pi*std::min(hz,static_cast<float>(sr*0.45))/static_cast<float>(sr);
    const float a=std::pow(10.f,db/40),alpha=std::sin(w)/(2*q),c=std::cos(w),a0=1+alpha/a;
    b0=(1+alpha*a)/a0;b1=-2*c/a0;b2=(1-alpha*a)/a0;a1=-2*c/a0;a2=(1-alpha/a)/a0;
}
float Effects::Eq::tick(float x) { if(bypass)return x;const float y=b0*x+z1;z1=b1*x-a1*y+z2;z2=b2*x-a2*y;return y; }
float Effects::read(const std::vector<float>& data,size_t write,float samples) {
    float pos=static_cast<float>(write)-samples;
    const float size=static_cast<float>(data.size());
    while(pos<0)pos+=size;
    const auto a=static_cast<size_t>(pos)%data.size();const auto b=(a+1)%data.size();
    const float f=pos-std::floor(pos);return data[a]+f*(data[b]-data[a]);
}
void Effects::prepare(double sr) {
    rate=juce::jmax(8000.,sr);smoothing=1.f-std::exp(-1.f/static_cast<float>(rate*0.02));
    for(auto& b:chorusBuffer)b.assign(static_cast<size_t>(rate*0.05)+4,0.f);
    for(auto& b:delayBuffer)b.assign(static_cast<size_t>(rate*12.1)+4,0.f);
    juce::Reverb::Parameters neutral;neutral.wetLevel=0;neutral.dryLevel=0.5f;
    reverb.setParameters(neutral);reverb.setSampleRate(rate);lfos.prepare(rate);envelopes.prepare(rate);reset();
}
void Effects::reset() {
    filters={};equalizers={};
    for(auto& c:allpass)for(auto& x:c)x=0;
    for(auto& x:legacy)x=0;
    for(auto& x:phaserLast)x=0;
    for(auto& b:chorusBuffer)std::fill(b.begin(),b.end(),0.f);
    for(auto& b:delayBuffer)std::fill(b.begin(),b.end(),0.f);
    chorusWrite=delayWrite=0;chorusPhase=phaserPhase=0;eqCounter=0;
    initial=true;filterWeights={};reverb.reset();lfos.reset();envelopes.reset();ampEnvelopeBlend={};filterEnvelopeDepth={};modEnvelopeRoutes={};matrixRoutes={};keytrackAmount={};modulationInitial=true;
}
float Effects::delayMilliseconds(const FxSettings& p,float bpm) {
    static constexpr float beats[]={0.125f,0.25f,0.5f,0.75f,1.f,1.5f,2.f,3.f,4.f};
    const float tempo=std::isfinite(bpm) ? juce::jlimit(20.f,400.f,bpm) : 120.f;
    return juce::jlimit(1.f,12000.f,p.delaySync ? 60000.f/tempo*beats[juce::jlimit(0,8,p.delayDivision)] : p.delayMs);
}
void Effects::process(float* l,float* r,int n,const FxSettings& p,float bpm,const LfoSettingsBank& lfoSettings,const uint8_t* noteTriggers,const EnvelopeSettings& envelopeSettings,const EnvelopeEvent* envelopeEvents,const LfoModulation* modulation) {
    if(n<=0 || delayBuffer[0].empty())return;
    const float targets[]={p.cutoff,p.resonance,p.drive,p.driveMix,p.eq1Hz,p.eq1Db,p.eq1Q,p.eq2Hz,p.eq2Db,p.eq2Q,
        p.chorusMix,p.chorusRate,p.chorusDepth,p.phaserMix,p.phaserRate,p.phaserDepth,p.phaserFeedback,
        p.delayMix,static_cast<float>(static_cast<double>(delayMilliseconds(p,bpm))*rate/1000.),p.delayFeedback,p.delayPingPong};
    const int mode=juce::jlimit(0,7,p.filterMode);
    if(initial) {for(size_t j=0;j<std::size(targets);++j)smooth[j].value=targets[j];filterWeights[static_cast<size_t>(mode)]=1;outputGain.value=std::pow(10.f,p.gainDb/20.f);initial=false;}
    // Account for JUCE reverb internal dry x2 / wet x3 scaling.
    juce::Reverb::Parameters rp;rp.roomSize=p.room;rp.damping=p.damping;rp.width=p.width;
    rp.wetLevel=p.reverb/3.f;rp.dryLevel=(1.f-p.reverb)*0.5f;reverb.setParameters(rp);
    for(int i=0;i<n;++i) {
        float v[22];for(size_t j=0;j<std::size(targets);++j)v[j]=smooth[j].tick(targets[j],smoothing);
        auto mod=modulation?modulation[i]:modulationTick(p,bpm,lfoSettings,noteTriggers && noteTriggers[i]!=0,envelopeSettings,envelopeEvents?envelopeEvents[i]:EnvelopeEvent{});
        v[0]*=std::exp2(mod.cutoffOctaves);v[1]+=mod.resonance;v[2]=juce::jlimit(0.f,30.f,v[2]+mod.driveDb);
        for(size_t j=0;j<filterWeights.size();++j)filterWeights[j]+=smoothing*((static_cast<int>(j)==mode?1.f:0.f)-filterWeights[j]);
        const float hz=juce::jlimit(20.f,static_cast<float>(rate*0.45),v[0]),g=std::tan(pi*hz/static_cast<float>(rate));
        const float k=1.f/juce::jlimit(0.5f,10.f,v[1]);
        if(eqCounter==0) for(auto& c:equalizers) {c[0].coefficients(rate,v[4],v[5],v[6]);c[1].coefficients(rate,v[7],v[8],v[9]);}
        eqCounter=(eqCounter+1)&31;
        float x[2]={l[i]*mod.amplitude*(mod.pan>0?1.f-mod.pan:1.f),r[i]*mod.amplitude*(mod.pan<0?1.f+mod.pan:1.f)};
        for(int c=0;c<2;++c) {
            // Derive simultaneous LP/HP/BP/notch outputs from one TPT state update.
            auto& f=filters[static_cast<size_t>(c)];
            const float beforeS1=f[0].s1,beforeS2=f[0].s2;
            float responses[8];responses[0]=x[c];
            responses[1]=f[0].tick(x[c],g,k,1);
            Svf copy;copy.s1=beforeS1;copy.s2=beforeS2;responses[3]=copy.tick(x[c],g,k,3);
            copy.s1=beforeS1;copy.s2=beforeS2;responses[5]=copy.tick(x[c],g,k,5);
            responses[6]=responses[1]+responses[3];
            responses[2]=f[1].tick(responses[1],g,k,1);
            responses[4]=f[2].tick(responses[3],g,k,3);
            legacy[c]+=(1-std::exp(-2*pi*hz/static_cast<float>(rate)))*(x[c]-legacy[c]);
            responses[7]=v[0]>=19999.f?x[c]:legacy[c];
            x[c]=0;for(size_t j=0;j<filterWeights.size();++j)x[c]+=filterWeights[j]*responses[j];
            const float drive=std::pow(10.f,v[2]/20.f);
            const float saturated=std::tanh(x[c]*drive)/std::sqrt(drive);
            x[c]+=v[3]*(saturated-x[c]);
            for(auto& eq:equalizers[static_cast<size_t>(c)])x[c]=eq.tick(x[c]);
            const float phase=static_cast<float>(chorusPhase)+(c?pi/2:0);
            const float delay=static_cast<float>(rate/1000.*static_cast<double>(15.f+v[12]*10.f*std::sin(phase)));
            const float delayed=read(chorusBuffer[static_cast<size_t>(c)],chorusWrite,delay);
            chorusBuffer[static_cast<size_t>(c)][chorusWrite]=x[c];x[c]+=v[10]*(delayed-x[c]);
            const float sweep=static_cast<float>(phaserPhase)+(c?pi/2:0);
            const float freq=700.f*std::pow(2.f,2.5f*v[15]*std::sin(sweep));
            const float pg=std::tan(pi*juce::jmin(freq,static_cast<float>(rate*0.4))/static_cast<float>(rate)),a=(pg-1)/(pg+1);
            float phased=x[c]+v[16]*phaserLast[c];
            for(float& z:allpass[c]) {const float y=a*phased+z;z=phased-a*y;phased=y;}
            phaserLast[c]=phased;
            x[c]+=v[13]*(0.5f*(x[c]+phased)-x[c]);
        }
        const float dl=read(delayBuffer[0],delayWrite,v[18]),dr=read(delayBuffer[1],delayWrite,v[18]);
        delayBuffer[0][delayWrite]=x[0]+v[19]*(dl+(dr-dl)*v[20]);
        delayBuffer[1][delayWrite]=x[1]+v[19]*(dr+(dl-dr)*v[20]);
        l[i]=x[0]+v[17]*(dl-x[0]);r[i]=x[1]+v[17]*(dr-x[1]);
        chorusWrite=(chorusWrite+1)%chorusBuffer[0].size();delayWrite=(delayWrite+1)%delayBuffer[0].size();
        chorusPhase+=2*pi*v[11]/rate;phaserPhase+=2*pi*v[14]/rate;
        if(chorusPhase>=2*pi)chorusPhase-=2*pi;
        if(phaserPhase>=2*pi)phaserPhase-=2*pi;
    }
    reverb.processStereo(l,r,n);
    const float targetGain=std::pow(10.f,p.gainDb/20.f);
    for(int i=0;i<n;++i) {const float gain=outputGain.tick(targetGain,smoothing);l[i]*=gain;r[i]*=gain;}
}

LfoModulation Effects::modulationTick(const FxSettings& p,float bpm,const LfoSettingsBank& lfoSettings,bool noteOn,
    const EnvelopeSettings& envelopeSettings,EnvelopeEvent event,const MatrixSettings& matrix,const PerformanceFrame& performance) {
    if(modulationInitial){ampEnvelopeBlend.value=envelopeSettings.ampEnabled?1.f:0.f;filterEnvelopeDepth.value=envelopeSettings.filterOctaves;keytrackAmount.value=p.keytrack;modulationInitial=false;}
        auto mod=lfos.tick(lfoSettings,bpm,noteOn);
        const auto eg=envelopes.tick(envelopeSettings,event);
        const float ampBlend=ampEnvelopeBlend.tick(envelopeSettings.ampEnabled?1.f:0.f,smoothing);
        mod.amplitude*=1.f+ampBlend*(eg.amp-1.f);
        mod.cutoffOctaves+=filterEnvelopeDepth.tick(envelopeSettings.filterOctaves,smoothing)*eg.filter;
        for(size_t target=0;target<6;++target) {
            const float goal=envelopeSettings.modTarget==static_cast<int>(target)+1?envelopeSettings.modDepth*eg.mod:0.f;
            modEnvelopeRoutes[target].tick(goal,smoothing);
        }
        mod.cutoffOctaves+=5.f*modEnvelopeRoutes[0].value;
        mod.amplitude*=std::pow(10.f,12.f*modEnvelopeRoutes[1].value/20.f);
        mod.pan=juce::jlimit(-1.f,1.f,mod.pan+modEnvelopeRoutes[2].value);
        mod.resonance+=4.f*modEnvelopeRoutes[3].value;mod.driveDb+=24.f*modEnvelopeRoutes[4].value;mod.pitchCents+=1200.f*modEnvelopeRoutes[5].value;

        const std::array<float,12> sources {0,mod.waves[0],mod.waves[1],mod.waves[2],eg.amp,eg.filter,eg.mod,
            performance.velocity,(performance.note-60.f)/60.f,performance.wheel,performance.pressure,performance.bend};
        for(size_t row=0;row<matrix.size();++row) {
            const auto& route=matrix[row];
            const float value=sources[static_cast<size_t>(juce::jlimit(0,11,route.source))]*route.depth;
            for(size_t t=0;t<6;++t)matrixRoutes[row][t].tick(route.target==static_cast<int>(t)+1?value:0.f,smoothing);
            mod.cutoffOctaves+=5.f*matrixRoutes[row][0].value;
            mod.amplitude*=std::pow(10.f,12.f*matrixRoutes[row][1].value/20.f);
            mod.pan+=matrixRoutes[row][2].value;mod.resonance+=4.f*matrixRoutes[row][3].value;
            mod.driveDb+=24.f*matrixRoutes[row][4].value;mod.pitchCents+=1200.f*matrixRoutes[row][5].value;
        }
        mod.cutoffOctaves+=keytrackAmount.tick(p.keytrack,smoothing)*(performance.note-60.f)/12.f;
        mod.cutoffOctaves=juce::jlimit(-10.f,10.f,mod.cutoffOctaves);
        mod.pan=juce::jlimit(-1.f,1.f,mod.pan);mod.pitchCents=juce::jlimit(-4800.f,4800.f,mod.pitchCents);
        return mod;
}
