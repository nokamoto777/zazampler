#pragma once
#include "BankImport.h"

// --render-fx BANK SLOT INPUT OUTPUT EFFECT [id=value ...]
// Original dry audio through only our FX: separates sampler and FX errors.
static int renderFxReference(int argc,char** argv) {
    check(argc>=7,"render-fx arguments");
    juce::MemoryBlock bytes;check(juce::File(argv[2]).loadFileAsData(bytes),"FX bank read");
    auto bank=ZamplerBank::parse(bytes.getData(),bytes.getSize());
    const int slot=juce::String(argv[3]).getIntValue()-1;
    check(slot>=0 && slot<static_cast<int>(bank.patches.size()),"FX slot");
    const juce::String effect(argv[6]);
    const juce::StringArray names{"none","drive","phaser","delay","reverb"};
    const int selected=names.indexOf(effect);check(selected>=0,"FX selection");
    auto patch=bank.patches[static_cast<size_t>(slot)];
    for(size_t i=116;i<=121;++i)patch.parameters[i]=0;
    const size_t flags[]={0,119,116,117,118};
    if(selected>0)patch.parameters[flags[selected]]=1;
    const auto conversion=translateZamplerPatch(patch);
    ZaZamplerProcessor processor;
    for(const auto& setting:conversion.settings) {
        auto* parameter=processor.parameters.getParameter(setting.first);
        check(parameter!=nullptr,"FX import parameter");
        parameter->setValueNotifyingHost(parameter->convertTo0to1(setting.second));
    }
    for(int i=7;i<argc;++i) {
        const juce::String arg(argv[i]);
        auto* parameter=processor.parameters.getParameter(arg.upToFirstOccurrenceOf("=",false,false));
        check(parameter!=nullptr && arg.contains("="),"FX override");
        parameter->setValueNotifyingHost(parameter->convertTo0to1(arg.fromFirstOccurrenceOf("=",false,false).getFloatValue()));
    }
    auto settings=readFxSettings(processor.parameters);
    settings.filterMode=0;settings.gainDb=0;settings.keytrack=0;
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(juce::File(argv[4])));
    check(reader!=nullptr && reader->numChannels==2,"FX input must be stereo audio");
    juce::File output(argv[5]);check(output!=juce::File(argv[4]),"FX output must differ from input");
    output.deleteFile();auto stream=output.createOutputStream();check(stream!=nullptr,"FX output");
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),reader->sampleRate,2,24,{},0));
    check(writer!=nullptr,"FX writer");
    Effects fx;fx.prepare(reader->sampleRate);
    constexpr int block=128;juce::AudioBuffer<float> audio(2,block);
    for(juce::int64 start=0;start<reader->lengthInSamples;start+=block) {
        const int count=static_cast<int>(std::min<juce::int64>(block,reader->lengthInSamples-start));
        check(reader->read(&audio,0,count,start,true,true),"FX audio read");
        fx.process(audio.getWritePointer(0),audio.getWritePointer(1),count,settings,120);
        check(writer->writeFromAudioSampleBuffer(audio,0,count),"FX audio write");
    }
    output.withFileExtension("txt").replaceWithText("Isolated FX: "+effect+"\nOriginal dry audio; filter, envelopes and output gain bypassed.\n"+conversion.warnings.joinIntoString("\n"));
    return 0;
}
