#pragma once
#include "PerformanceWheel.h"
namespace wheelchecks {
inline void run(const juce::File& demo) {
    // Real-time callbacks preserve host sample offsets. Compare the entire onset
    // start against offline rendering, at several block sizes and note positions.
    // sfizz uses different interpolation quality in offline mode, so amplitudes
    // need not be bit-identical; event-to-onset timing must be identical.
    for(int block:{64,128,512})for(int offset:{0,block-1})for(int scope:{0,1}) {
        auto render=[&](bool offline) {
            ZaZamplerProcessor p;p.prepareToPlay(48000,block);p.setNonRealtime(offline);
            auto set=[&](const char* id,float value){auto* x=p.parameters.getParameter(id);x->setValueNotifyingHost(x->convertTo0to1(value));};
            set("modScope",static_cast<float>(scope));set("envAmpEnabled",1);set("envAmpAttack",0);set("envAmpDecay",0);set("envAmpSustain",1);
            p.load(demo,{},"Sine.sfz");wait(p);
            juce::AudioBuffer<float> audio(2,block);juce::MidiBuffer midi;
            // Allow the loader to finish its final non-audio bookkeeping.
            p.processBlock(audio,midi);
            std::vector<float> out(2048);
            for(int start=0;start<2048;start+=block) {
                if(start==0)midi.addEvent(juce::MidiMessage::noteOn(1,69,(juce::uint8)127),offset);
                p.processBlock(audio,midi);std::copy(audio.getReadPointer(0),audio.getReadPointer(0)+block,out.begin()+start);
            }
            check(p.getLatencySamples()==0,"sampler reports zero algorithmic latency");
            check(p.debugReport().contains("quiet-note onset="),"onset diagnostic included");
            return out;
        };
        const auto live=render(false),offline=render(true);int a=-1,b=-1;float error=0;
        for(size_t i=0;i<live.size();++i){if(a<0 && std::abs(live[i])>1e-5f)a=static_cast<int>(i);if(b<0 && std::abs(offline[i])>1e-5f)b=static_cast<int>(i);error=std::max(error,std::abs(live[i]-offline[i]));}
        std::cout<<"onset block="<<block<<" offset="<<offset<<" scope="<<scope<<" live="<<a<<" offline="<<b<<" error="<<error<<"\n";
        check(a>=offset && a<=offset+4 && a==b,"live MIDI onset equals offline onset without additional block delay");
    }
    ZaZamplerProcessor p;p.prepareToPlay(48000,128);p.setNonRealtime(true);p.load(demo,{},"Sine.sfz");wait(p);
    auto set=[&](const char* id,float value){auto* x=p.parameters.getParameter(id);x->setValueNotifyingHost(x->convertTo0to1(value));};
    set("matrix1Source",9);set("matrix1Target",3);set("matrix1Depth",1);set("modScope",1);
    p.setUiMod(127);p.setUiPitch(16383);
    juce::AudioBuffer<float> audio(2,128);juce::MidiBuffer midi;midi.addEvent(juce::MidiMessage::noteOn(1,69,(juce::uint8)127),0);
    int crossings=0;float last=0;double left=0,right=0;
    for(int k=0;k<200;++k){p.processBlock(audio,midi);if(k>20)for(int i=0;i<128;++i){const float v=audio.getSample(1,i);if(last<=0 && v>0)++crossings;last=v;left+=std::abs(audio.getSample(0,i));right+=std::abs(v);}}
    const double hz=crossings*48000.0/(179*128);
    check(right>1 && left<right*.001,"GUI modulation reaches per-voice CC1 matrix");
    check(hz>485 && hz<505,"GUI pitch wheel bends actual audio by default SFZ range");
    midi.addEvent(juce::MidiMessage::pitchWheel(1,6000),13);midi.addEvent(juce::MidiMessage::controllerEvent(1,1,42),31);p.processBlock(audio,midi);
    check(p.pitchWheelValue()==6000 && p.modWheelValue()==42,"hardware wheel state mirrored without resending");
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
        auto* canvas=editor->findChildWithID("instrumentCanvas");
        auto* pitch=dynamic_cast<PerformanceWheel*>(canvas->findChildWithID("pitchWheel"));
        auto* mod=dynamic_cast<PerformanceWheel*>(canvas->findChildWithID("modWheel"));
        check(pitch && mod && pitch->midiValue()==6000 && mod->midiValue()==42,"editor mirrors hardware controls");
        pitch->keyPressed(juce::KeyPress(juce::KeyPress::upKey));pitch->keyStateChanged(false);
        check(p.pitchWheelValue()==8192,"spring wheel returns to centre on key release");
        mod->keyPressed(juce::KeyPress(juce::KeyPress::upKey));mod->keyStateChanged(false);check(p.modWheelValue()==46,"mod wheel holds its position");
    }
    p.processBlock(audio,midi);
    midi.addEvent(juce::MidiMessage::controllerEvent(1,121,0),0);p.processBlock(audio,midi);
    check(p.pitchWheelValue()==8192 && p.modWheelValue()==0,"reset controllers updates wheels");
    std::cout<<"PASS: live/offline onset at 64/128/512 frames, GUI wheel audio and hardware mirroring\n";
}
}
