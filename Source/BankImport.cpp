#include "BankImport.h"
#include <cmath>
#include <filesystem>
BankConversion translateZamplerPatch(const ZamplerPatch& patch) {
    BankConversion result;const auto& p=patch.parameters;
    for(const auto& spec:parameterSpecs())if(juce::String(spec.id)!="channel")result.settings.emplace_back(spec.id,spec.initial);
    auto set=[&](const char* id,float value) {for(auto& item:result.settings)if(item.first==id){item.second=value;return;}};
    auto choice=[](float value,int count){return juce::jlimit(0,count-1,static_cast<int>(value*static_cast<float>(count)));};
    auto hz=[](float value){return 20.f*std::pow(1000.f,value);};
    // Slot identities were matched to the public Zampler 3 parameter descriptors.
    // Physical-unit curves below are ZaZampler approximations, not recovered curves.
    const int modes[]={0,1,5,6,3,2};
    set("filterMode",static_cast<float>(modes[choice(p[126],6)]));
    set("cutoff",juce::jlimit(40.f,20000.f,hz(p[28])));set("resonance",0.707f+9.293f*p[29]);
    // Unity output avoids adding a second fixed attenuation to the SFZ instrument.
    result.warnings.add("Approximate FX conversion: unit curves and original DSP are not calibrated. Output starts at 0 dB; original volume taper is not imported.");
    if(p[116]>=0.5f) {
        const int type=choice(p[79],7);
        if(type>=5) {set("chorusMix",p[83]);set("chorusRate",0.05f*std::pow(160.f,p[80]));set("chorusDepth",p[84]);}
        else {set("phaserMix",p[83]);set("phaserRate",0.05f*std::pow(160.f,p[80]));set("phaserDepth",p[84]);set("phaserFeedback",0.8f*p[82]);}
        if(type!=1 && type<5)result.warnings.add("Original phaser/flanger type is approximated by a 6-stage phaser.");
    }
    if(p[117]>=0.5f) {
        set("delayMix",p[88]);set("delayFeedback",0.95f*p[87]);set("delayMs",20.f*std::pow(100.f,p[89]));
        const int type=choice(p[85],8);const float cross[]={0,1,0.75f,0.5f,0.25f,0.1f,0,0};
        set("delayPingPong",cross[type]);
        result.warnings.add("Delay uses approximate left time for both channels; right time, colour, multitap/diffusion are not reproduced.");
    }
    if(p[118]>=0.5f) {
        set("reverb",p[94]);set("damping",p[92]);set("room",p[93]);
        result.warnings.add("Original reverb type, pre-delay and low-cut are not reproduced.");
    }
    if(p[119]>=0.5f) {
        set("drive",30*p[99]);set("driveMix",p[100]);
        result.warnings.add("Distortion type/colour are approximated by tanh saturation.");
    }
    for(int eq=0;eq<2;++eq) {
        const size_t base=static_cast<size_t>(101+5*eq);
        if(p[static_cast<size_t>(120+eq)]<0.5f)continue;
        if(choice(p[base],3)!=0) {result.warnings.add("Shelf EQ is not mapped to a bell: this EQ band remains bypassed.");continue;}
        const auto prefix=juce::String("eq")+juce::String(eq+1);
        set((prefix+"Hz").toRawUTF8(),juce::jlimit(30.f,18000.f,hz(p[base+1])));
        set((prefix+"Q").toRawUTF8(),0.2f*std::pow(50.f,p[base+2]));
        set((prefix+"Db").toRawUTF8(),36*p[base+3]-18);
    }
    result.warnings.add("Amp/filter envelopes, key tracking, modulation matrix, LFOs, glide and mono mode are not imported.");
    if(p[115]>=0.5f)result.warnings.add("Original arpeggiator is enabled but its settings are not imported. Configure the SEQUENCE page manually.");
    return result;
}
SampleResolution resolveBankSample(const ZamplerPatch& p,const juce::File& bank,const juce::File& root) {
    if(p.sampleFile.empty())return {{},"This slot has no sample reference."};
    const auto ref=juce::String::fromUTF8(p.sampleFile.c_str()).replaceCharacter('\\','/');
    const auto name=ref.fromLastOccurrenceOf("/",false,false);
    if(!name.endsWithIgnoreCase(".sfz"))return {{},"The preset references a non-SFZ instrument (REX is not supported)."};
    auto inside=[&](const juce::File& f){
        namespace fs=std::filesystem;std::error_code error;
        const auto canonicalRoot=fs::weakly_canonical(fs::u8path(root.getFullPathName().toStdString()),error);if(error)return false;
        const auto canonicalFile=fs::weakly_canonical(fs::u8path(f.getFullPathName().toStdString()),error);if(error)return false;
        const auto relative=canonicalFile.lexically_relative(canonicalRoot);
        return !relative.empty() && !relative.is_absolute() && *relative.begin()!=".." && f.existsAsFile();
    };
    juce::Array<juce::File> exact;
    auto add=[&](const juce::File& f){if(inside(f) && !exact.contains(f))exact.add(f);};
    // Never follow the bank author's absolute path automatically.
    if(!juce::File::isAbsolutePath(ref) && !ref.contains("../")) {
        add(bank.getParentDirectory().getChildFile(ref));
        add(bank.getParentDirectory().getChildFile(bank.getFileNameWithoutExtension()).getChildFile(ref));
    }
    if(exact.size()==1)return {exact[0],{}};
    if(exact.size()>1)return {{},"Ambiguous SFZ references; select a narrower library folder."};
    juce::Array<juce::File> matches;
    auto search=[&](const juce::File& directory) {
        for(const auto& entry:juce::RangedDirectoryIterator(directory,true,"*",juce::File::findFiles,juce::File::FollowSymlinks::no))
            if(entry.getFile().getFileName().equalsIgnoreCase(name) && inside(entry.getFile()))matches.add(entry.getFile());
    };
    // Banks in a collection often reuse names such as Bass.sfz. Prefer this
    // bank's own directory before considering other libraries under the root.
    const auto bankDirectory=bank.getParentDirectory();
    if(bankDirectory==root || bankDirectory.isAChildOf(root))search(bankDirectory);
    if(matches.size()==1)return {matches[0],{}};
    if(matches.size()>1)return {{},"Multiple files named "+name+" in this bank folder; select a narrower sample folder."};
    if(bankDirectory!=root) {
        if(!juce::File::isAbsolutePath(ref) && !ref.contains("../")) {
            const auto rootRelative=root.getChildFile(ref);
            if(inside(rootRelative))return {rootRelative,{}};
        }
        search(root);
    }
    if(matches.size()==1)return {matches[0],{}};
    if(matches.size()>1)return {{},"Multiple files named "+name+"; select the correct library subfolder."};
    return {{},"Missing SFZ: "+name+". Select the folder containing this bank's SFZ files and samples."};
}
