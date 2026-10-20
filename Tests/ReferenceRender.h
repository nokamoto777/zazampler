#pragma once
// Reproducible bank rendering through the actual processor, without a DAW.
// --render-bank BANK ROOT SLOT(1-based) MIDI OUTPUT.wav SECONDS [id=value ...]
static int renderReference(int argc,char** argv) {
    check(argc>=8,"render-bank arguments");
    juce::MidiFile file;juce::FileInputStream input{juce::File(argv[5])};check(file.readFrom(input),"MIDI read");
    file.convertTimestampTicksToSeconds();juce::MidiMessageSequence events;
    for(int t=0;t<file.getNumTracks();++t)events.addSequence(*file.getTrack(t),0);
    events.sort();constexpr double rate=44100;constexpr int block=128;
    ZaZamplerProcessor p;p.prepareToPlay(rate,block);p.setNonRealtime(true);
    check(p.openBank(juce::File(argv[2])).wasOk(),"bank read");
    p.loadBankPatch(juce::File(argv[3]),{},juce::String(argv[4]).getIntValue()-1,true);wait(p);
    for(int a=8;a<argc;++a){juce::String s(argv[a]);auto* v=p.parameters.getParameter(s.upToFirstOccurrenceOf("=",false,false));check(v!=nullptr,"override parameter");v->setValueNotifyingHost(v->convertTo0to1(s.fromFirstOccurrenceOf("=",false,false).getFloatValue()));}
    const int frames=juce::roundToInt(juce::String(argv[7]).getDoubleValue()*rate);check(frames>0,"duration");
    juce::File output(argv[6]);output.deleteFile();auto stream=output.createOutputStream();check(stream!=nullptr,"output");
    juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(),rate,2,24,{},0));check(writer!=nullptr,"writer");
    int next=0;for(int start=0;start<frames;start+=block){int n=std::min(block,frames-start);juce::AudioBuffer<float> audio(2,n);juce::MidiBuffer midi;
        while(next<events.getNumEvents()){const auto& m=events.getEventPointer(next)->message;int at=juce::roundToInt(m.getTimeStamp()*rate);if(at>=start+n)break;if(!m.isMetaEvent())midi.addEvent(m,std::max(0,at-start));++next;}
        p.processBlock(audio,midi);check(writer->writeFromAudioSampleBuffer(audio,0,n),"write");}
    output.withFileExtension("txt").replaceWithText(p.debugReport());return 0;
}
