#pragma once
namespace remainingchecks {
inline void run(const juce::File& demo){
    NoteSequence seq;seq.prepare(1000);SequenceSettings s;s.mode=1;s.latch=true;s.division=1;
    std::vector<int> notes;auto emit=[&](const unsigned char* d,int){if(d[0]==0x90)notes.push_back(d[1]);};
    unsigned char on[]={0x90,60,100},off[]={0x80,60,0},next[]={0x90,67,100};
    seq.midi(on,3,s,emit);seq.tick(s,120,emit);seq.midi(off,3,s,emit);
    for(int i=0;i<250;++i)seq.tick(s,120,emit);check(notes.size()>1 && notes.back()==60,"latch retains released note");
    seq.midi(next,3,s,emit);for(int i=0;i<250;++i)seq.tick(s,120,emit);check(notes.back()==67,"new chord replaces latch");
    off[1]=67;seq.midi(off,3,s,emit);s.latch=false;const auto count=notes.size();for(int i=0;i<300;++i)seq.tick(s,120,emit);check(notes.size()==count,"latch off releases chord");
    OutputLimiter limiter;limiter.prepare(48000);float l=4,r=-2;limiter.process(l,r);check(std::abs(l)<=.980001f && std::abs(r/l+.5f)<1e-6f,"stereo-linked limiter ceiling");
    for(int i=0;i<48000;++i){l=.1f;r=.05f;limiter.process(l,r);}check(l>.099f,"limiter recovers");
    const std::vector<RexSliceEvent> events{{0,0},{.5,1},{1.25,129}};
    auto render=[&](int block){std::vector<int> result;RexClock clock;for(int start=0;start<4000;start+=block){clock.begin(events,2,start*.001);for(int i=start;i<std::min(4000,start+block);++i)clock.tick(events,i*.001,.001,[&](int slice){result.push_back(i*1000+slice);});}return result;};
    check(render(37)==render(128) && render(37).size()==6,"REX block invariance and loop boundaries");
    const auto temp=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("zazampler-missing",{},false);temp.createDirectory();
    struct Cleanup{juce::File f;~Cleanup(){f.deleteRecursively();}} cleanup{temp};
    demo.getChildFile("tone.wav").copyFileTo(temp.getChildFile("tone.wav"));
    auto sfz=temp.getChildFile("test.sfz");sfz.replaceWithText("<region> sample=tone.wav key=69\n<region> sample=absent-one.wav key=70\n<region> sample=absent-two.wav key=71\n");
    Engine engine(48000,128);engine.load(sfz.getFullPathName().toStdString());const auto missing=engine.missingSamples(sfz.getFullPathName().toStdString());
    check(missing.size()==2,"all missing region samples reported");
    ZaZamplerProcessor p;p.prepareToPlay(48000,128);p.setNonRealtime(true);p.load(demo,{},"Sine.sfz");wait(p);
    auto set=[&](const char* id,float v){auto* x=p.parameters.getParameter(id);x->setValueNotifyingHost(x->convertTo0to1(v));};
    set("channel",0);set("midiOutput",1);juce::AudioBuffer<float> audio(2,128);juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(2,69,(juce::uint8)100),37);p.processBlock(audio,midi);
    bool found=false;for(const auto x:midi)if(x.getMessage().isNoteOn())found=x.samplePosition==37 && x.getMessage().getChannel()==2;
    check(found,"MIDI out preserves channel and sample offset");
    set("midiOutput",0);midi.clear();p.processBlock(audio,midi);found=false;
    for(const auto x:midi)if(x.getMessage().isNoteOff() && x.getMessage().getChannel()==2 && x.getMessage().getNoteNumber()==69)found=true;
    check(found,"MIDI output disable releases external notes");
    p.load(demo,{},"Layers.sfz");wait(p);midi.clear();p.processBlock(audio,midi);
    check(audio.getMagnitude(0,128)>1e-5f,"old voice audible during crossfade");
    for(int b=0;b<20;++b){midi.clear();p.processBlock(audio,midi);}
    check(audio.getMagnitude(0,128)<1e-7f,"crossfade ends after 20 ms");
}
}
