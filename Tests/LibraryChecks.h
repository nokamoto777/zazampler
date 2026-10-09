#pragma once
namespace librarychecks {
inline void waitScan(LibraryCatalog& catalog) {
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(15);
    while(catalog.snapshot()->scanning && std::chrono::steady_clock::now()<end)std::this_thread::sleep_for(std::chrono::milliseconds(5));
    check(!catalog.snapshot()->scanning,"catalog scan timeout");
}
inline void run(const juce::File& demo,const juce::File& screenshot={}) {
    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("zazampler-libraries",{},false);
    struct Cleanup {juce::File file;~Cleanup(){file.deleteRecursively();}} cleanup{root};
    const auto a=root.getChildFile("Library 1"),b=root.getChildFile("Library 2");
    a.getChildFile("Samples").createDirectory();b.getChildFile("Samples").createDirectory();
    const auto bytes=bankfixture::make();
    const auto bankA=a.getChildFile("Library.fxb"),bankB=b.getChildFile("Library.FXB");
    bankA.replaceWithData(bytes.data(),bytes.size());bankB.replaceWithData(bytes.data(),bytes.size());
    root.getChildFile("Broken.fxb").replaceWithText("invalid bank");
    for(const auto& dir:{a,b}) {
        demo.getChildFile("Sine.sfz").copyFileTo(dir.getChildFile("Samples/Sine.sfz"));
        demo.getChildFile("tone.wav").copyFileTo(dir.getChildFile("Samples/tone.wav"));
    }
    ZaZamplerProcessor p;p.prepareToPlay(48000,128);p.setNonRealtime(true);
    p.libraries.scan(root.getFullPathName(),{});waitScan(p.libraries);
    auto catalog=p.libraries.snapshot();
    check(catalog->entries.size()==2 && catalog->errors.size()==1 && catalog->instruments.size()==2,"nested libraries, uppercase extension, corrupt bank isolation");
    check(catalog->entries[0].name!=catalog->entries[1].name,"relative paths distinguish same bank names");
    const auto parsed=ZamplerBank::parse(bytes.data(),bytes.size());
    check(resolveBankSample(parsed.patches[0],bankA,root).file==a.getChildFile("Samples/Sine.sfz"),"bank-local sample wins over other library");
    check(resolveBankSample(parsed.patches[0],bankB,root).file==b.getChildFile("Samples/Sine.sfz"),"second library sample isolation");
    demo.getChildFile("Sine.sfz").copyFileTo(root.getChildFile("Sine.sfz"));
    check(resolveBankSample(parsed.patches[0],bankA,root).file==a.getChildFile("Samples/Sine.sfz"),"root-level names cannot shadow bank-local samples");
    root.getChildFile("Sine.sfz").deleteFile();
    check(p.openBank(bankB).wasOk(),"open second library");p.loadBankPatch(root,{},0,false);wait(p);
    check(std::abs(p.parameters.getRawParameterValue("gain")->load())<0.0001f,"new bank preset defaults to unity output");
    // Compare actual rendering at unity vs the previous -6 dB default.
    auto measure=[&](float db) {
        auto* gain=p.parameters.getParameter("gain");gain->setValueNotifyingHost(gain->convertTo0to1(db));
        p.panic();juce::AudioBuffer<float> audio(2,2048);juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1,69,(juce::uint8)127),0);p.processBlock(audio,midi);
        std::array<WaveformCapture::Frame,WaveformCapture::capacity> captured {};
        const auto count=p.waveform.read(captured.data(),WaveformCapture::capacity);
        check(count>0,"processor publishes output to visualizer");
        return audio.getRMSLevel(0,512,1536);
    };
    const auto unity=measure(0),old=measure(-6);
    check(old>0 && unity/old>1.98f && unity/old<2.01f,"unity output removes 6 dB attenuation");
    juce::MemoryBlock state;p.getStateInformation(state);
    ZaZamplerProcessor restored;restored.prepareToPlay(48000,128);restored.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
    wait(restored);waitScan(restored.libraries);
    check(restored.libraries.snapshot()->root==root.getFullPathName() && restored.libraries.snapshot()->entries.size()==2,"library root restored and rescanned");
    check(restored.currentBankPath()==bankB.getFullPathName() && restored.selectedBankSlot()==0,"selected library and preset restored");
    check(std::abs(restored.parameters.getRawParameterValue("gain")->load()+6.f)<0.0001f,"saved output gain preserved");
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor(restored.createEditor());
        auto* canvas=editor->findChildWithID("instrumentCanvas");check(canvas!=nullptr,"browser canvas");
        auto* banks=dynamic_cast<juce::ComboBox*>(canvas->findChildWithID("librarySelector"));
        auto* presets=dynamic_cast<juce::ComboBox*>(canvas->findChildWithID("presetSelector"));
        auto* search=dynamic_cast<juce::TextEditor*>(canvas->findChildWithID("presetSearch"));
        check(banks && presets && search && banks->getNumItems()==2 && banks->getSelectedId()==2,"browser recalls second library");
        search->setText("does not exist",false);search->onTextChange();check(presets->getNumItems()==0,"preset search filters names");
        search->setText({},false);search->onTextChange();check(presets->getNumItems()>0,"clear preset search");
        banks->setSelectedId(1,juce::dontSendNotification);banks->onChange();wait(restored);
        check(restored.currentBankPath()==bankA.getFullPathName() && restored.selectedBankSlot()==0,"browser library selection loads preset");
    }
    restored.clearBank();restored.load(root,{},"Library 2/Samples/Sine.sfz");wait(restored);
    juce::MemoryBlock sfzState;restored.getStateInformation(sfzState);
    restored.setStateInformation(sfzState.getData(),static_cast<int>(sfzState.getSize()));wait(restored);waitScan(restored.libraries);
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor(restored.createEditor());
        check(!restored.getBank(),"reopening SFZ mode does not replace it with first bank");
        auto* canvas=editor->findChildWithID("instrumentCanvas");
        auto* presets=dynamic_cast<juce::ComboBox*>(canvas->findChildWithID("presetSelector"));
        check(presets && presets->getText().contains("Library 2"),"SFZ browser restores selected file");
    }
    if(screenshot!=juce::File()) {
        juce::AudioBuffer<float> audio(2,16384);juce::MidiBuffer midi;p.processBlock(audio,midi);
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
        juce::FileOutputStream output(screenshot);check(output.openedOk(),"browser screenshot output");
        check(output.setPosition(0) && output.truncate().wasOk(),"browser screenshot truncate");
        juce::PNGImageFormat png;check(png.writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds()),output),"browser screenshot encode");
    }
    p.libraries.scan(root.getChildFile("missing").getFullPathName(),{});waitScan(p.libraries);
    check(p.libraries.snapshot()->entries.empty() && !p.libraries.snapshot()->errors.isEmpty(),"missing root report");
    p.libraries.scan(root.getFullPathName(),{});p.libraries.scan(b.getFullPathName(),{});waitScan(p.libraries);
    check(p.libraries.snapshot()->root==b.getFullPathName() && p.libraries.snapshot()->entries.size()==1,"latest scan supersedes previous root");
}
}
