#pragma once
#include "AboutPanel.h"
namespace advancedchecks {
inline void run(const juce::File& demo) {
    // Host position must produce identical onsets with irregular block sizes,
    // including an initial position between grid points.
    auto sequence=[](int block) {
        NoteSequence seq;seq.prepare(1000);SequenceSettings p;p.mode=1;p.rhythm=2;p.hits=16;p.hostSync=true;
        std::vector<int> onsets;int time=0;auto emit=[&](const unsigned char* d,int){if(d[0]==0x90)onsets.push_back(time);};
        unsigned char on[]={0x90,60,100};seq.midi(on,3,p,emit);
        for(int start=0;start<1800;start+=block){seq.synchronize(p,120,0.1+start*.002,0,start==0);for(time=start;time<std::min(1800,start+block);++time)seq.tick(p,120,emit);}
        return onsets;
    };
    const auto a=sequence(37),b=sequence(128);check(a==b && a.front()==75,"PPQ onset and block invariance");
    NoteSequence seq;seq.prepare(1000);SequenceSettings p;p.mode=3;p.division=1;p.length=2;p.gate=.5f;p.ties[0]=true;
    int ons=0,offs=0;auto emit=[&](const unsigned char* d,int){if(d[0]==0x90)++ons;if(d[0]==0x80)++offs;};
    unsigned char on[]={0x90,60,100};seq.midi(on,3,p,emit);
    for(int i=0;i<180;++i)seq.tick(p,120,emit);
    check(ons==1 && offs==0,"Tie suppresses gate-off and next note retrigger");
    for(int i=180;i<250;++i)seq.tick(p,120,emit);check(offs==1,"Tie chain ends with gate release");
    p.hostSync=true;seq.synchronize(p,120,8,8,true);seq.tick(p,120,emit);check(ons==2,"seek restarts on actual bar head");

    // Sum of individually rendered parts must equal polyphonic per-voice DSP.
    // A later note must not reset the first note's pitch LFO or amplitude ADSR.
    auto render=[&](int mask) {
        Engine engine(48000,128);engine.offline(true);check(engine.load(demo.getChildFile("Sine.sfz").getFullPathName().toStdString()),"voice test load");
        VoiceModSettings settings;settings.envelopes.ampEnabled=true;settings.envelopes.amp={.02f,.01f,.7f,.03f};
        settings.lfos[0].target=6;settings.lfos[0].depth=.1f;settings.lfos[0].hz=7;settings.lfos[0].retrigger=1;
        std::vector<float> out(8192),right(128);
        for(int start=0;start<8192;start+=128) {
            if(start==0 && (mask&1)){unsigned char note[]={0x90,60,100};engine.midi(note,3,13);}
            if(start==1536 && (mask&2)){unsigned char note[]={0x90,67,90};engine.midi(note,3,37);}
            if(start==4096 && (mask&1)){unsigned char off[]={0x80,60,0};engine.midi(off,3,51);}
            engine.render(out.data()+start,right.data(),128,nullptr,false,&settings);
        }return out;
    };
    const auto first=render(1),second=render(2),both=render(3);double error=0,energy=0;
    for(size_t i=0;i<both.size();++i){error+=std::pow(both[i]-first[i]-second[i],2);energy+=both[i]*both[i];}
    check(energy>.01 && error<1e-8,"independent per-voice audio equals sum of isolated notes");

    auto bytes=bankfixture::make();bankfixture::param(bytes,127,1);bankfixture::param(bytes,78,.5f);
    bankfixture::param(bytes,33,.5f);bankfixture::param(bytes,34,.75f);bankfixture::param(bytes,37,1);
    bankfixture::param(bytes,159,13);bankfixture::param(bytes,191,.75f);bankfixture::param(bytes,223,5);
    bankfixture::param(bytes,115,1);bankfixture::param(bytes,114,1);
    auto bank=ZamplerBank::parse(bytes.data(),bytes.size());const auto imported=translateZamplerPatch(bank.patches[0]);
    auto value=[&](const char* id){for(const auto& item:imported.settings)if(item.first==id)return item.second;return -999.f;};
    check(value("matrix32Source")==1 && value("matrix32Target")==1 && std::abs(value("matrix32Depth")-.5f)<1e-5f,"FXB extended matrix IDs");
    check(value("voiceMode")==1 && value("modScope")==1 && value("lfo1Sync")==1 && value("seqHostSync")==1 && value("seqMode")==1,"FXB performance settings");
    check(std::abs(value("glide")-.5f)<1e-5f && value("envAmpEnabled")==1,"FXB envelopes/glide");

    DiagnosticLog log;log.add("entry-initial");log.audio(DiagnosticLog::seek,1250);log.add("entry-latest");
    auto text=log.snapshot();check(text.indexOf("entry-latest")<text.indexOf("entry-initial") && text.contains("1.250"),"debug log reverse order and realtime drain");
    for(int i=0;i<4100;++i)log.add("bounded");check(log.snapshot().contains("retained 4096 / 4096"),"bounded diagnostic retention");
    check(zazamplerBuildInfo().contains("github.com/nokamoto777/zazampler"),"build provenance URL");
}
inline void rex(const juce::File& temp) {
    const auto root=temp.getNonexistentChildFile("zazampler-rex-test",{},false);root.createDirectory();
    struct Cleanup{juce::File file;~Cleanup(){file.deleteRecursively();}} cleanup{root};
    std::vector<float> left(2400),right(2400);for(int i=0;i<2400;++i){left[static_cast<size_t>(i)]=.4f*std::sin(i*.057f);right[static_cast<size_t>(i)]=.2f*std::sin(i*.08f);}
    VLError error;VLFile raw=vl_create_new(2,48000,120000,&error);check(raw && error==VL_OK,"create synthetic REX2");
    check(vl_add_slice(raw,0,left.data(),right.data(),2400)==VL_OK,"REX2 slice encode");
    check(vl_add_slice(raw,3840,left.data(),right.data(),2400)==1,"REX2 second slice encode");
    const auto file=root.getChildFile("test.rx2");check(vl_save(raw,file.getFullPathName().toRawUTF8())==VL_OK,"write synthetic REX2");vl_close(raw);
    juce::File cache;
    {RexImport rex(file);cache=rex.directory;check(rex.description.contains("2 slices") && rex.sfz.existsAsFile(),"REX2 native import");
        Engine engine(48000,128);engine.offline(true);check(engine.load(rex.sfz.getFullPathName().toStdString()),"REX2 SFZ slices load");
        unsigned char note[]={0x90,0,100};engine.midi(note,3,0);float l[128]{},r[128]{};engine.render(l,r,128);float energy=0;for(float x:l)energy+=x*x;check(energy>1e-4f,"REX2 slice audio");}
    check(!cache.exists(),"REX decode cache lifetime cleanup");
    // Legacy .rex is an AIFF-backed PCM container. Generate our own audio.
    const auto legacy=root.getChildFile("legacy.rex");juce::AiffAudioFormat aiff;
    auto output=legacy.createOutputStream();std::unique_ptr<juce::AudioFormatWriter> writer(aiff.createWriterFor(output.get(),48000,1,16,{},0));check(writer!=nullptr,"legacy AIFF writer");output.release();
    juce::AudioBuffer<float> audio(1,2400);audio.copyFrom(0,0,left.data(),2400);check(writer->writeFromAudioSampleBuffer(audio,0,2400),"legacy PCM write");writer.reset();
    // Add the legacy ReCycle application/tempo chunk (plain AIFF is not REX).
    juce::MemoryBlock legacyBytes;check(legacy.loadFileAsData(legacyBytes),"read legacy fixture");
    auto* begin=static_cast<const uint8_t*>(legacyBytes.getData());std::vector<uint8_t> legacyData(begin,begin+legacyBytes.getSize());
    const size_t chunk=legacyData.size();legacyData.resize(chunk+28,0);
    std::memcpy(legacyData.data()+chunk,"APPL",4);bankfixture::be(legacyData,chunk+4,20);std::memcpy(legacyData.data()+chunk+8,"REX ",4);
    bankfixture::be(legacyData,chunk+24,120000);bankfixture::be(legacyData,4,static_cast<uint32_t>(legacyData.size()-8));
    check(legacy.replaceWithData(legacyData.data(),legacyData.size()),"write REX metadata");
    {RexImport rex(legacy);check(rex.sfz.existsAsFile(),"legacy REX PCM import");}
    std::memcpy(legacyData.data()+chunk+8,"ReCy",4);legacyData[chunk+20]=1;bankfixture::be(legacyData,chunk+22,120000);
    const auto rcy=root.getChildFile("legacy.rcy");check(rcy.replaceWithData(legacyData.data(),legacyData.size()),"write RCY metadata");
    {RexImport rex(rcy);check(rex.sfz.existsAsFile(),"legacy RCY PCM import");}
    ZaZamplerProcessor processor;processor.prepareToPlay(48000,128);processor.setNonRealtime(true);processor.load(root,{},"test.rx2");wait(processor);
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    std::function<juce::MidiKeyboardComponent*(juce::Component&)> findKeyboard=[&](juce::Component& parent)->juce::MidiKeyboardComponent* {
        for(auto* child:parent.getChildren()){if(auto* keyboard=dynamic_cast<juce::MidiKeyboardComponent*>(child))return keyboard;if(auto* keyboard=findKeyboard(*child))return keyboard;}return nullptr;
    };
    auto* keyboard=findKeyboard(*editor);check(keyboard && keyboard->getRangeStart()==0 && keyboard->getRangeEnd()==127 && keyboard->getLowestVisibleKey()==0,"REX slices reachable from on-screen keyboard");
    juce::AudioBuffer<float> out(2,128);juce::MidiBuffer midi;midi.addEvent(juce::MidiMessage::noteOn(1,0,(juce::uint8)100),0);processor.processBlock(out,midi);check(out.getMagnitude(0,128)>0.001f,"processor REX audio");
    struct Head final:juce::AudioPlayHead {
        double beat=0;bool playing=true;
        juce::Optional<PositionInfo> getPosition() const override {PositionInfo p;p.setPpqPosition(beat);p.setBpm(120);p.setIsPlaying(playing);return p;}
    } head;
    processor.setPlayHead(&head);
    for(const auto* id:{"rexPlayback","midiOutput"})processor.parameters.getParameter(id)->setValueNotifyingHost(1);
    midi.clear();processor.processBlock(out,midi);bool slice0=false;
    for(const auto message:midi)if(message.getMessage().isNoteOn() && message.getMessage().getNoteNumber()==0 && message.samplePosition==0)slice0=true;
    check(slice0 && out.getMagnitude(0,128)>1e-5f,"REX host loop without MIDI input");
    head.beat=1;midi.clear();processor.processBlock(out,midi);bool slice1=false;
    for(const auto message:midi)if(message.getMessage().isNoteOn() && message.getMessage().getNoteNumber()==1 && message.samplePosition==0)slice1=true;
    check(slice1,"REX host seek follows slice PPQ");
    head.playing=false;midi.clear();processor.processBlock(out,midi);bool stopped=false;
    for(const auto message:midi)if(message.getMessage().isNoteOff())stopped=true;
    check(stopped,"REX host stop releases MIDI");processor.setPlayHead(nullptr);
    juce::MemoryBlock state;processor.getStateInformation(state);ZaZamplerProcessor recalled;recalled.prepareToPlay(48000,128);recalled.setStateInformation(state.getData(),static_cast<int>(state.getSize()));wait(recalled);check(recalled.statusText().contains("REX"),"REX source recall rebuilds cache");
}
}
