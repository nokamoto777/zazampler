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
inline void supplied(const juce::File& bank,const juce::File& patch,const juce::File& demo) {
    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("zazampler-bank-layout",{},false);
    struct Cleanup{juce::File f;~Cleanup(){f.deleteRecursively();}} cleanup{root};
    root.getChildFile("Presets").createDirectory();root.getChildFile("Samples").createDirectory();
    bank.copyFileTo(root.getChildFile(bank.getFileName()));patch.copyFileTo(root.getChildFile("Presets").getChildFile(patch.getFileName()));
    demo.getChildFile("Sine.sfz").copyFileTo(root.getChildFile("Samples/DX-SlapBass2.sfz"));
    demo.getChildFile("tone.wav").copyFileTo(root.getChildFile("Samples/tone.wav"));
    ZaZamplerProcessor p;p.prepareToPlay(48000,128);p.setNonRealtime(true);
    p.libraries.scan(root.getFullPathName(),{});waitScan(p.libraries);const auto catalog=p.libraries.snapshot();
    check(catalog->errors.isEmpty() && catalog->entries.size()==1 && catalog->entries[0].patches.size()==1,"supplied FXB and subfolder FXP grouped");
    check(p.openLibrary(catalog->entries[0]).wasOk(),"supplied bank group opens");
    check(p.getBank()->patches.size()>=7 && p.getBank()->patches[0].name=="DX-SlapBass2","supplied bank presets parsed");
    p.loadBankPatch(root,{},0,false);wait(p);
    juce::AudioBuffer<float> audio(2,2048);juce::MidiBuffer midi;midi.addEvent(juce::MidiMessage::noteOn(1,69,(juce::uint8)100),0);p.processBlock(audio,midi);
    check(audio.getMagnitude(0,0,2048)>0.001f,"supplied bank reference resolves sibling sample folder using synthetic audio");
    juce::MemoryBlock state;p.getStateInformation(state);
    ZaZamplerProcessor recalled;recalled.prepareToPlay(48000,128);recalled.setStateInformation(state.getData(),static_cast<int>(state.getSize()));wait(recalled);
    check(recalled.getBank()->patches.size()==p.getBank()->patches.size() && recalled.selectedBankSlot()==0,"group bank and preset survive state recall");
    std::cout<<"PASS: supplied Digital Bass FXB / DX-SlapBass2 FXP grouping, sample resolution and state recall (synthetic audio)\n";
}
inline void cachingAndReload(const juce::File& demo) {
    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("zazampler-cache-regression",{},false);
    struct Cleanup{juce::File f;~Cleanup(){f.deleteRecursively();}} cleanup{root};
    const auto folder=root.getChildFile("Library"),cache=root.getChildFile("Cache");folder.createDirectory();
    const auto bytes=bankfixture::make();folder.getChildFile("Bank.fxb").replaceWithData(bytes.data(),bytes.size());
    demo.getChildFile("Sine.sfz").copyFileTo(folder.getChildFile("Sine.sfz"));demo.getChildFile("tone.wav").copyFileTo(folder.getChildFile("tone.wav"));
    {LibraryCatalog c(cache);c.scan(folder.getFullPathName(),{});waitScan(c);check(c.snapshot()->entries.size()==1,"initial cache scan");}
    // Add a bank after scanning: restoration must not enumerate the directory.
    folder.getChildFile("New.fxb").replaceWithData(bytes.data(),bytes.size());
    {LibraryCatalog c(cache);check(c.snapshot()->cached && !c.snapshot()->scanning && c.snapshot()->entries.size()==1,"new instance restores remembered folder and index without scanning");
     c.scan(folder.getFullPathName(),{});waitScan(c);check(c.snapshot()->entries.size()==2,"explicit rescan discovers new bank");}
    {LibraryCatalog c(cache);check(c.snapshot()->entries.size()==2,"updated cache survives restart");}
    folder.getChildFile("Bank.fxb").deleteFile();folder.getChildFile("New.fxb").deleteFile();
    // A standalone FXP folder is represented by one bank entry.
    auto fxp=std::vector<uint8_t>(100+12780,0);std::copy(bytes.begin(),bytes.begin()+28,fxp.begin());
    std::memcpy(fxp.data()+8,"FPCh",4);bankfixture::be(fxp,4,static_cast<uint32_t>(fxp.size()-8));bankfixture::be(fxp,24,337);bankfixture::be(fxp,56,static_cast<uint32_t>(fxp.size()-60));
    std::copy(bytes.begin()+160,bytes.begin()+200,fxp.begin()+60);bankfixture::le(fxp,64,1);std::copy(bytes.begin()+200,bytes.begin()+200+12780,fxp.begin()+100);
    folder.getChildFile("A.fxp").replaceWithData(fxp.data(),fxp.size());
    ZaZamplerProcessor p;p.prepareToPlay(48000,128);p.libraries.scan(folder.getFullPathName(),{});waitScan(p.libraries);
    check(p.libraries.snapshot()->entries.size()==1 && p.libraries.snapshot()->entries[0].file==folder,"FXP-only folder grouped");
    check(p.openLibrary(p.libraries.snapshot()->entries[0]).wasOk(),"FXP-only group opens");p.loadBankPatch(folder,{},0,false);wait(p);
    // No successful load may be required to recover from a missing selection.
    p.load(folder,{},"Missing.sfz");wait(p);check(p.statusText().contains("Previous instrument retained"),"failed load retains playable instrument");
    juce::AudioBuffer<float> audio(2,128);juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1,69,(juce::uint8)127),0);p.processBlock(audio,midi);
    for(int i=0;i<30;++i) {
        p.load(folder,{},"Missing.sfz");p.load(folder,{},"Sine.sfz");
        const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(10);
        while(p.isLoading() && std::chrono::steady_clock::now()<end){p.processBlock(audio,midi);std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        wait(p);midi.addEvent(juce::MidiMessage::noteOn(1,69,(juce::uint8)127),0);p.processBlock(audio,midi);
    }
    for(int i=0;i<4;++i)p.processBlock(audio,midi);
    check(audio.getMagnitude(0,0,128)>0.0001f,"repeated live preset swaps still produce sound");
    auto patch=ZamplerBank::parse(bytes.data(),bytes.size()).patches[0];patch.sampleFile="absent.sfz";
    check(resolveBankSample(patch,folder.getChildFile("A.fxp"),folder,&p.libraries.snapshot()->instruments,[]{return true;}).error.contains("cancelled"),"sample lookup can be superseded");
    check(p.debugReport().contains("Loader:") && p.debugReport().contains("Loading sample data:"),"loader stages included in debug report");
    std::cout<<"PASS: persistent catalog, explicit rescan, FXP-only banks and 30 live reloads\n";
}

}
