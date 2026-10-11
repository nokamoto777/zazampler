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
    // Time/rate tapers are explicitly approximate, while slot and route IDs
    // come from the manufacturer's parameter descriptors and route-name tables.
    auto seconds=[](float value){return 10.f*value*value*value;};
    const char* envelopes[]={"envAmp","envFilter","envMod"};
    const int bases[]={18,22,57};
    for(int e=0;e<3;++e) {
        const auto prefix=juce::String(envelopes[e]);const auto base=static_cast<size_t>(bases[e]);
        set((prefix+"Attack").toRawUTF8(),seconds(p[base]));
        set((prefix+"Decay").toRawUTF8(),seconds(p[base+1]));
        set((prefix+"Sustain").toRawUTF8(),p[base+2]);
        set((prefix+"Release").toRawUTF8(),seconds(p[base+3]));
    }
    set("envAmpEnabled",1);set("modScope",1);
    set("envFilterAmount",10*p[30]-5);set("keytrack",p[26]);
    set("voiceMode",p[127]>=0.5f?1.f:0.f);set("glide",2*p[78]*p[78]);
    for(int i=0;i<3;++i) {
        const auto prefix="lfo"+juce::String(i+1);const auto base=static_cast<size_t>(33+9*i);
        set((prefix+"Shape").toRawUTF8(),static_cast<float>(choice(p[base],6)));
        set((prefix+"Rate").toRawUTF8(),0.01f*std::pow(3000.f,p[base+1]));
        set((prefix+"Division").toRawUTF8(),static_cast<float>(choice(p[base+1],11)));
        set((prefix+"Skew").toRawUTF8(),juce::jlimit(0.05f,0.95f,p[base+2]));
        set((prefix+"Fade").toRawUTF8(),seconds(p[base+3]));
        set((prefix+"Sync").toRawUTF8(),p[base+4]>=0.5f?1.f:0.f);
        set((prefix+"Reset").toRawUTF8(),p[base+5]>=0.5f?1.f:0.f);
    }
    auto source=[](int id) {
        switch(id){case 1:return 7;case 2:return 8;case 3:return 9;case 4:return 11;case 5:return 10;
            case 7:return 12;case 11:return 13;case 12:return 14;case 13:return 1;case 14:return 2;case 15:return 3;
            case 16:return 15;case 17:return 16;case 18:return 17;case 19:return 18;case 20:return 19;case 21:return 20;
            case 22:return 4;case 23:return 5;case 25:return 6;default:return 0;}
    };
    auto target=[](int id){switch(id){case 1:case 2:return 6;case 3:case 88:return 2;case 4:return 3;case 5:return 1;case 26:return 4;default:return 0;}};
    for(size_t i=0;i<32;++i) {
        const int src=static_cast<int>(std::lround(p[128+i])),dst=static_cast<int>(std::lround(p[192+i]));
        if(!src || !dst)continue;
        const int mappedSource=source(src),mappedTarget=target(dst);
        if(!mappedSource || !mappedTarget || p[224+i]!=0) {
            result.warnings.add("Matrix row "+juce::String(static_cast<int>(i)+1)+" skipped: source ID "+juce::String(src)+", destination ID "+juce::String(dst)+", voice selector "+juce::String(p[224+i])+" is not supported.");continue;
        }
        const auto prefix="matrix"+juce::String(static_cast<int>(i)+1);
        set((prefix+"Source").toRawUTF8(),static_cast<float>(mappedSource));
        set((prefix+"Target").toRawUTF8(),static_cast<float>(mappedTarget));
        set((prefix+"Depth").toRawUTF8(),(2*p[160+i]-1)*(dst==2?1.f/12.f:1.f));
    }
    set("seqRhythm",0);set("seqHostSync",p[114]>=0.5f?1.f:0.f);
    set("seqOctaves",static_cast<float>(choice(p[70],3)+1));
    set("seqDivision",static_cast<float>(choice(p[71],8)));
    set("seqGate",juce::jlimit(0.05f,0.95f,p[72]));set("seqSwing",50+25*p[73]);
    if(p[115]>=0.5f) {
        const int mode=static_cast<int>(std::lround(p[125]));
        set("seqMode",mode==1?2.f:1.f);
        result.warnings.add("Arpeggiator timing/range imported approximately. Original mode "+juce::String(mode)+" uses "+(mode==1?juce::String("Down"):juce::String("Up"))+". Packed Arp/ArpFinger step data, original ties and free-running rate are not decoded; edit SEQUENCE as needed.");
    }
    result.warnings.add("LFO/envelope/glide/keytrack and supported matrix routes imported with approximate curves. Per-voice envelopes multiply the SFZ envelopes; source sample/voice lifetime still applies. LFO shape order and synced division scaling need auditory calibration against Zampler.");
    return result;
}
SampleResolution resolveBankSample(const ZamplerPatch& p,const juce::File& bank,const juce::File& root) {
    if(p.sampleFile.empty())return {{},"This slot has no sample reference."};
    const auto ref=juce::String::fromUTF8(p.sampleFile.c_str()).replaceCharacter('\\','/');
    const auto name=ref.fromLastOccurrenceOf("/",false,false);
    if(!name.endsWithIgnoreCase(".sfz") && !name.endsWithIgnoreCase(".rex") && !name.endsWithIgnoreCase(".rx2") && !name.endsWithIgnoreCase(".rcy"))return {{},"The preset references an unsupported instrument type."};
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
