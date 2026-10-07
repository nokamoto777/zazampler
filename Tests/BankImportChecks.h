#pragma once
#include "BankFixture.h"
namespace bankchecks {
inline void run(const juce::File& demo) {
    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("zazampler-bank",{},false);
    root.createDirectory();
    struct Cleanup {juce::File f;~Cleanup(){f.deleteRecursively();}} cleanup{root};
    const auto data=bankfixture::make();const auto bank=root.getChildFile("Synthetic.fxb");
    check(bank.replaceWithData(data.data(),data.size()),"write bank fixture");
    const auto parsed=ZamplerBank::parse(data.data(),data.size());
    check(resolveBankSample(parsed.patches[0],bank,root).error.contains("Missing SFZ"),"missing sample report");
    const auto a=root.getChildFile("a"),b=root.getChildFile("b");a.createDirectory();b.createDirectory();
    demo.getChildFile("Sine.sfz").copyFileTo(a.getChildFile("Sine.sfz"));
    demo.getChildFile("tone.wav").copyFileTo(a.getChildFile("tone.wav"));
    check(resolveBankSample(parsed.patches[0],bank,root).file==a.getChildFile("Sine.sfz"),"recursive relink");
    a.getChildFile("Sine.sfz").copyFileTo(b.getChildFile("Sine.sfz"));
    check(resolveBankSample(parsed.patches[0],bank,root).error.contains("Multiple"),"duplicate sample must not guess");
    b.deleteRecursively();
    ZaZamplerProcessor p;p.prepareToPlay(48000,128);
    check(p.openBank(bank).wasOk(),"bank open");p.loadBankPatch(root,{},0,true);wait(p);
    check(std::abs(p.parameters.getRawParameterValue("filterMode")->load()-2.f)<0.001f,"bank LP24 conversion");
    check(std::abs(p.parameters.getRawParameterValue("delayMix")->load()-0.3f)<0.001f,"bank delay conversion");
    auto* cutoff=p.parameters.getParameter("cutoff");cutoff->setValueNotifyingHost(cutoff->convertTo0to1(1234.f));
    juce::MemoryBlock state;p.getStateInformation(state);bank.deleteFile();
    ZaZamplerProcessor q;q.prepareToPlay(48000,128);q.setStateInformation(state.getData(),static_cast<int>(state.getSize()));wait(q);
    check(q.getBank() && q.selectedBankSlot()==0,"embedded bank restored without original bank file");
    check(std::abs(q.parameters.getRawParameterValue("cutoff")->load()-1234.f)<0.01f,"manual FX override restored");
    q.loadBankPatch(root,{},0,false);wait(q);
    check(std::abs(q.parameters.getRawParameterValue("filterMode")->load())<0.001f && std::abs(q.parameters.getRawParameterValue("delayMix")->load())<0.001f,"SFZ only bypass");
    check(q.importNotes().contains("SFZ only"),"SFZ only disclosure");
}
}
