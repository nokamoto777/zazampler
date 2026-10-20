#pragma once
#include "PushMapping.h"
namespace modulationdisplaychecks {
inline void run(const juce::File& root) {
    LfoSettingsBank settings;settings[0].phaseLock=true;settings[0].sync=1;settings[0].division=5;
    LfoBank a,b;a.prepare(48000);b.prepare(48000);
    for(int i=0;i<171;++i)b.tick(settings,120);
    for(double ppq:{0.,.25,1.25,-.75,123.25}) {
        const auto x=a.tick(settings,120,false,true,ppq),y=b.tick(settings,120,true,true,ppq);
        check(std::abs(x.waves[0]-y.waves[0])<1e-6f,"PPQ locks phase across retrigger and seek");
        check(std::abs(x.waves[0]-std::sin(2*juce::MathConstants<double>::pi*ppq))<1e-5f,"PPQ sine phase");
    }
    settings[0].shape=5;const auto random=a.tick(settings,120,false,true,1.25).waves[0];
    a.tick(settings,120,false,true,5.25);
    check(random==a.tick(settings,120,false,true,1.25).waves[0],"PPQ S&H repeats on loop");
    ZaZamplerProcessor p;p.prepareToPlay(48000,128);p.setNonRealtime(true);
    p.load(root,{},"Sine.sfz");waitLoaded(p);
    auto set=[&](const char* id,float value){auto* x=p.parameters.getParameter(id);x->setValueNotifyingHost(x->convertTo0to1(value));};
    struct Listener:juce::AudioProcessorParameter::Listener {
        int begins=0,ends=0,changes=0;
        void parameterValueChanged(int,float) override{++changes;}
        void parameterGestureChanged(int,bool start) override{if(start)++begins;else ++ends;}
    } listener;
    for(size_t i=0;i<std::size(pushParameterIds);++i) {
        auto* param=p.parameters.getParameter(pushParameterIds[i]);const auto before=param->getValue();
        param->addListener(&listener);registerPushParameter(p.parameters,i);param->removeListener(&listener);
        check(param->getValue()==before,"Push registration preserves sound parameters");
    }
    check(listener.begins==8 && listener.ends==8 && listener.changes==8,"8 balanced host configuration gestures");
    set("lfo1Target",1);set("lfo1Depth",.8f);set("lfo1Rate",3);set("envAmpEnabled",1);set("envAmpRelease",2);set("envCurve",1);
    const float base=p.parameters.getParameter("cutoff")->getValue();
    for(int scope=0;scope<2;++scope) {
        set("modScope",static_cast<float>(scope));float minimum=100,maximum=-100;
        for(int block=0;block<150;++block){juce::AudioBuffer<float> audio(2,128);juce::MidiBuffer midi;if(block==0)midi.addEvent(juce::MidiMessage::noteOn(1,69,static_cast<juce::uint8>(100)),0);p.processBlock(audio,midi);
            check(p.modulationMeter.active.load(),"global/voice audio telemetry active");
            const auto mod=p.modulationMeter.read();minimum=std::min(minimum,mod.cutoffOctaves);maximum=std::max(maximum,mod.cutoffOctaves);
        }
        check(maximum-minimum>3,"display follows real LFO destination");
        check(p.parameters.getParameter("cutoff")->getValue()==base,"telemetry preserves host base value");
    }
    check(p.getTailLengthSeconds()>=2*std::log(1.e9),"exponential release tail reported to host");
    p.panic();for(int i=0;i<20;++i){juce::AudioBuffer<float> audio(2,128);juce::MidiBuffer midi;p.processBlock(audio,midi);}
    check(!p.modulationMeter.active.load(),"voice meter clears after panic");
}
}
