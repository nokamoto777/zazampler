#include "Processor.h"
#include "EffectsChecks.h"
#include "LfoChecks.h"
#include "EnvelopeChecks.h"
#include "PerformanceChecks.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
static void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
static void wait(ZaZamplerProcessor& p){
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(15);
    while(p.isLoading() && std::chrono::steady_clock::now()<end)std::this_thread::sleep_for(std::chrono::milliseconds(10));
    check(!p.isLoading() && p.isReady(),"instrument load timed out or failed");
}
#include "BankImportChecks.h"
int main(int argc,char** argv){
    juce::ScopedJuceInitialiser_GUI initialise;
    try {
        if(argc==3 && (juce::String(argv[1])=="--snapshot" || juce::String(argv[1])=="--snapshot-lfo" || juce::String(argv[1])=="--snapshot-env" || juce::String(argv[1])=="--snapshot-fx" || juce::String(argv[1])=="--snapshot-matrix" || juce::String(argv[1])=="--snapshot-seq")) {
            ZaZamplerProcessor p;p.prepareToPlay(48000,128);
            std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
            if(juce::String(argv[1])=="--snapshot-lfo") {
                for(auto* child:editor->getChildren())if(auto* button=dynamic_cast<juce::TextButton*>(child))
                    if(button->getButtonText()=="LFO ROUTING")button->onClick();
            }
            if(juce::String(argv[1])=="--snapshot-env") {
                for(auto* child:editor->getChildren())if(auto* button=dynamic_cast<juce::TextButton*>(child))
                    if(button->getButtonText()=="ENV ROUTING")button->onClick();
            }
            if(juce::String(argv[1])=="--snapshot-fx") {
                for(auto* child:editor->getChildren())if(auto* button=dynamic_cast<juce::TextButton*>(child))
                    if(button->getButtonText()=="EFFECTS")button->onClick();
            }
            if(juce::String(argv[1])=="--snapshot-matrix" || juce::String(argv[1])=="--snapshot-seq") {
                const auto title=juce::String(argv[1])=="--snapshot-matrix"?"MATRIX":"SEQUENCE";
                for(auto* child:editor->getChildren())if(auto* button=dynamic_cast<juce::TextButton*>(child))if(button->getButtonText()==title)button->onClick();
            }
            auto snapshot=editor->createComponentSnapshot(editor->getLocalBounds());
            juce::File file(argv[2]);juce::FileOutputStream output(file);
            check(output.openedOk(),"snapshot output");
            check(output.setPosition(0) && output.truncate().wasOk(),"snapshot truncate");juce::PNGImageFormat png;
            check(png.writeImageToStream(snapshot,output),"snapshot encode");
            std::cout<<"PASS: native editor snapshot\n";return 0;
        }
        fxchecks::run();lfochecks::run();envelopechecks::run();
        check(argc==2,"demo directory required");juce::File root(argv[1]);
        bankchecks::run(root);performancechecks::run(root.getChildFile("Sine.sfz").getFullPathName().toStdString());performancechecks::processorAudio(root);
        ZaZamplerProcessor p;p.prepareToPlay(48000,128);p.setNonRealtime(true);
        juce::MemoryBlock empty;p.getStateInformation(empty);
        p.load(root,{},"Sine.sfz");wait(p);
        juce::AudioBuffer<float> b(2,1024);juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1,69,(juce::uint8)100),700);
        p.processBlock(b,midi);
        check(b.getMagnitude(0,0,700)<1e-7f,"chunked onset too early");check(b.getMagnitude(0,800,224)>0.001f,"large block MIDI lost");
        p.parameters.getParameter("resonance")->setValueNotifyingHost(p.parameters.getParameter("resonance")->convertTo0to1(4.f));
        p.parameters.getParameter("delayMix")->setValueNotifyingHost(0.25f);
        auto set=[&](const char* id,float value){auto* parameter=p.parameters.getParameter(id);parameter->setValueNotifyingHost(parameter->convertTo0to1(value));};
        set("lfo1Target",3);set("lfo1Depth",0.75f);set("lfo1Rate",2.5f);set("lfo2Sync",1);set("lfo2Division",3);set("lfo3Shape",5);set("lfo3Reset",1);
        set("envAmpEnabled",1);set("envAmpRelease",1.2f);set("envFilterAmount",2);set("envModTarget",3);set("envModDepth",0.4f);
        set("keytrack",1);set("voiceMode",1);set("glide",0.15f);set("matrix1Source",9);set("matrix1Target",6);set("matrix1Depth",0.2f);set("seqNote2",7);set("seqVel3",0);
        juce::MemoryBlock state;p.getStateInformation(state);
        ZaZamplerProcessor q;q.prepareToPlay(44100,512);q.setNonRealtime(true);q.setStateInformation(state.getData(),static_cast<int>(state.getSize()));wait(q);
        check(q.parameters.getRawParameterValue("voiceMode")->load()==1 && std::abs(q.parameters.getRawParameterValue("glide")->load()-0.15f)<0.0001f && q.parameters.getRawParameterValue("keytrack")->load()==1,"performance state recall");
        check(q.parameters.getRawParameterValue("matrix1Target")->load()==6 && q.parameters.getRawParameterValue("seqNote2")->load()==7 && q.parameters.getRawParameterValue("seqVel3")->load()==0,"matrix and sequence state recall");
        check(q.parameters.getRawParameterValue("envAmpEnabled")->load()>0.5f && std::abs(q.parameters.getRawParameterValue("envAmpRelease")->load()-1.2f)<0.001f && q.parameters.getRawParameterValue("envModTarget")->load()>2.9f,"envelope state restore");
        check(std::abs(q.parameters.getRawParameterValue("lfo1Depth")->load()-0.75f)<0.001f && q.parameters.getRawParameterValue("lfo2Division")->load()>2.9f && q.parameters.getRawParameterValue("lfo3Shape")->load()>4.9f,"LFO state recall");
        check(std::abs(q.parameters.getRawParameterValue("resonance")->load()-4.f)<0.001f,"resonance restore");
        check(std::abs(q.parameters.getRawParameterValue("delayMix")->load()-0.25f)<0.001f,"delay mix restore");
        midi.addEvent(juce::MidiMessage::noteOn(1,69,(juce::uint8)100),0);q.processBlock(b,midi);check(b.getMagnitude(0,0,1024)>0.001f,"state recall silent");
        q.setStateInformation(empty.getData(),static_cast<int>(empty.getSize()));q.processBlock(b,midi);check(b.getMagnitude(0,0,1024)<1e-12f,"empty state must unload");
        // Rapid superseding loads must publish the latest selection.
        p.load(root,{},"missing.sfz");p.load(root,{},"Sine.sfz");wait(p);
        check(p.statusText().contains("Sine.sfz"),"stale load published");
        auto legacy=juce::ValueTree("QSamplerState");legacy.setProperty("schema",1,nullptr);
        for(const auto& spec:parameterSpecs()) {
            if(juce::StringArray{"gain","cutoff","reverb","channel"}.contains(spec.id)) {
                auto node=juce::ValueTree("PARAM");node.setProperty("id",spec.id,nullptr);
                node.setProperty("value",juce::String(spec.id)=="cutoff" ? 1000.f : spec.initial,nullptr);
                legacy.addChild(node,-1,nullptr);
            }
        }
        juce::MemoryBlock oldState;auto oldXml=legacy.createXml();juce::AudioProcessor::copyXmlToBinary(*oldXml,oldState);
        q.setStateInformation(oldState.getData(),static_cast<int>(oldState.getSize()));
        check(std::abs(q.parameters.getRawParameterValue("filterMode")->load()-7.f)<0.001f,"legacy filter migration");
        check(std::abs(q.parameters.getRawParameterValue("delayMix")->load())<0.001f,"legacy defaults retain previous effect");
        check(std::abs(q.parameters.getRawParameterValue("lfo1Target")->load())<0.001f && std::abs(q.parameters.getRawParameterValue("lfo1Depth")->load())<0.001f,"legacy state must disable new LFOs");
        check(q.parameters.getRawParameterValue("envAmpEnabled")->load()<0.5f && std::abs(q.parameters.getRawParameterValue("envFilterAmount")->load())<0.001f,"legacy envelope bypass");
        check(q.parameters.getRawParameterValue("voiceMode")->load()==0 && q.parameters.getRawParameterValue("seqMode")->load()==0 && q.parameters.getRawParameterValue("matrix1Depth")->load()==0,"legacy performance defaults");
        // No explicit wait: offline render synchronizes with asynchronous load.
        q.load(root,{},"Sine.sfz");midi.addEvent(juce::MidiMessage::noteOn(1,69,(juce::uint8)100),0);q.processBlock(b,midi);
        check(q.isReady() && b.getMagnitude(0,0,1024)>0.001f,"offline immediate load/render must not be silent");
        p.releaseResources();q.releaseResources();
        std::cout<<"PASS: processor large buffers, state recall, empty-state unload, latest-load wins, FX/LFO state, legacy migration, LFO period/fade/sync/audio/split buffers, ADSR timing/gating/pedal/audio/state\n";
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
