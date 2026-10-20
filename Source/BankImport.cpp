#include "BankImport.h"
#include <cmath>
#include <filesystem>
#include <chrono>
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
    // Empirical time constants from RX reference audio; not an exact DSP clone.
    auto seconds=[](float value){return value<=0?0.f:0.001f*std::pow(10000.f,value);};
    set("monoSampleHold",1);set("envCurve",1);set("envAmpOwnRelease",1);
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
    set("envFilterAmount",5*p[30]);set("keytrack",p[26]);
    set("voiceMode",p[127]>=0.5f?1.f:0.f);set("glide",2*p[78]*p[78]);
    for(int i=0;i<3;++i) {
        const auto prefix="lfo"+juce::String(i+1);const auto base=static_cast<size_t>(33+9*i);
        set((prefix+"Shape").toRawUTF8(),static_cast<float>(choice(p[base],6)));
        set((prefix+"Rate").toRawUTF8(),0.01f*std::pow(3000.f,p[base+1]));
        set((prefix+"Division").toRawUTF8(),static_cast<float>(choice(p[base+1],11)));
        set((prefix+"Skew").toRawUTF8(),juce::jlimit(0.05f,0.95f,p[base+2]));
        set((prefix+"Fade").toRawUTF8(),10.f*std::pow(p[base+3],3.f));
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
        float depth=2*p[160+i]-1;
        // Pitch Semi is discrete; treating its near-centre stored value as a
        // fractional semitone detuned the supplied RX reference presets.
        if(dst==1)depth=std::round(12*depth)/12;
        set((prefix+"Depth").toRawUTF8(),depth*(dst==2?1.f/12.f:1.f));
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
    result.warnings.add("Mono phrase sample selection and discrete Pitch Semi were calibrated on four RX references; modulation depth ranges remain approximate.");
    result.warnings.add("LFO/envelope/glide/keytrack and supported matrix routes imported with approximate curves. Per-voice envelopes multiply SFZ envelopes. Envelope-owned release extends only the near-zero default SFZ release; explicit longer SFZ release and sample endings still apply. Exponential ADSR times are empirical time constants, not exact recovered curves. LFO shape order and synced division scaling need auditory calibration against Zampler.");
    return result;
}
SampleResolution resolveBankSample(const ZamplerPatch& p,const juce::File& bank,const juce::File& root,
    const juce::Array<juce::File>* index,const std::function<bool()>& cancelled) {
    if(p.sampleFile.empty())return {{},"This slot has no sample reference."};
    const auto ref=juce::String::fromUTF8(p.sampleFile.c_str()).replaceCharacter('\\','/');
    const auto name=ref.fromLastOccurrenceOf("/",false,false);
    if(!name.endsWithIgnoreCase(".sfz") && !name.endsWithIgnoreCase(".rex") && !name.endsWithIgnoreCase(".rx2") && !name.endsWithIgnoreCase(".rcy"))return {{},"The preset references an unsupported instrument type."};
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    auto interrupted=[&]{return (cancelled && cancelled()) || std::chrono::steady_clock::now()>deadline;};
    namespace fs=std::filesystem;std::error_code ec;
    const auto canonicalRoot=fs::weakly_canonical(fs::u8path(root.getFullPathName().toStdString()),ec);
    if(ec)return {{},"Library folder is unavailable."};
    auto inside=[&](const juce::File& f){
        std::error_code error;
        const auto canonicalFile=fs::weakly_canonical(fs::u8path(f.getFullPathName().toStdString()),error);if(error)return false;
        const auto relative=canonicalFile.lexically_relative(canonicalRoot);
        return !relative.empty() && !relative.is_absolute() && *relative.begin()!=".." && f.existsAsFile();
    };
    if(interrupted())return {{},"Sample search cancelled or timed out."};
    // Prefer the saved relative reference, including a bank-named sample folder.
    if(!juce::File::isAbsolutePath(ref) && !ref.contains("../")) {
        juce::Array<juce::File> exact;
        for(const auto& f:{bank.getParentDirectory().getChildFile(ref),bank.getParentDirectory().getChildFile(bank.getFileNameWithoutExtension()).getChildFile(ref)})
            if(inside(f) && !exact.contains(f))exact.add(f);
        if(exact.size()==1)return {exact[0],{}};
        if(exact.size()>1)return {{},"Ambiguous sample references; select a narrower library folder."};
    }
    juce::Array<juce::File> candidates;
    auto consider=[&](const juce::File& f){if(f.getFileName().equalsIgnoreCase(name) && inside(f) && !candidates.contains(f))candidates.add(f);};
    if(index) {
        for(const auto& f:*index){if(interrupted())return {{},"Sample search cancelled or timed out."};consider(f);}
    } else {
        // Unindexed direct imports have a bounded, cancellable fallback. Never
        // leave all subsequent preset requests behind a complete library walk.
        int visited=0;
        for(const auto& entry:juce::RangedDirectoryIterator(root,true,"*",juce::File::findFiles,juce::File::FollowSymlinks::no)) {
            if(interrupted() || ++visited>20000)return {{},"Sample search limit reached. Choose LIBRARY FOLDER and RESCAN to build the index."};
            consider(entry.getFile());
        }
    }
    // FXPs may live in Presets/, with sibling Samples/. Widen one ancestor at a
    // time, stopping at the explicitly permitted root; do not cross libraries.
    for(auto directory=bank.getParentDirectory();directory==root || directory.isAChildOf(root);directory=directory.getParentDirectory()) {
        juce::Array<juce::File> matches;
        for(const auto& f:candidates)if(f.isAChildOf(directory))matches.add(f);
        if(matches.size()==1)return {matches[0],{}};
        if(matches.size()>1)return {{},"Multiple files named "+name+" in this library; select a narrower sample folder."};
        if(directory==root)break;
    }
    if(candidates.size()==1)return {candidates[0],{}};
    if(candidates.size()>1)return {{},"Multiple files named "+name+"; select the correct library subfolder."};
    return {{},"Missing sample instrument: "+name+". Reconnect the library drive, or RESCAN after adding files. FXB/FXP do not contain sample audio."};
}
