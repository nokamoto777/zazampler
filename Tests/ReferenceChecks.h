#pragma once
namespace referencechecks {
inline void run(const juce::File& demo) {
    ZamplerPatch patch;patch.parameters[18]=.675f;patch.parameters[21]=.735f;
    patch.parameters[134]=7;patch.parameters[166]=.4969697f;patch.parameters[198]=1;
    const auto mapped=translateZamplerPatch(patch);
    auto get=[&](const char* id){for(const auto& x:mapped.settings)if(x.first==id)return x.second;return -999.f;};
    check(get("envFilterAmount")==0,"zero imported filter envelope must be neutral");
    check(get("matrix7Depth")==0,"Pitch Semi centre must not detune by 7 cents");
    check(get("envCurve")==1 && get("envAmpOwnRelease")==1,"bank envelope mode");
    check(get("envAmpAttack")>.49f && get("envAmpAttack")<.52f,"reference attack time constant");
    Adsr adsr;adsr.prepare(1000);AdsrSettings setting{.1f,0,1,.1f,true};adsr.noteOn();float level=0;
    for(int i=0;i<100;++i)level=adsr.tick(setting);
    check(std::abs(level-(1-std::exp(-1.f)))<1e-5f,"exponential attack time constant");
    adsr.noteOff();for(int i=0;i<100;++i)level=adsr.tick(setting);
    check(std::abs(level-(1-std::exp(-1.f))*std::exp(-1.f))<1e-5f,"exponential release time constant");
    MonoGlide mono;mono.prepare(48000);std::vector<int> notes;
    auto send=[&](int status,int key){unsigned char m[]={static_cast<unsigned char>(status),static_cast<unsigned char>(key),100};mono.midi(m,3,true,0,[&](const unsigned char* d,int){if(d[0]==0x90)notes.push_back(d[1]);},true);};
    send(0x90,69);mono.tick(true,true);send(0x80,69);send(0x90,60);
    check(notes.back()==69 && mono.tick(true,true)==6000,"contiguous mono phrase holds sample but changes pitch");
    send(0x80,60);mono.tick(true,true);send(0x90,81);
    check(notes.back()==81,"silent gap selects a new mono sample");
    VoiceModulation voice;voice.prepare(48000);voice.resize(128);voice.start(69,1);
    std::array<PerformanceFrame,128> expression{};for(auto& frame:expression)frame.note=60;
    VoiceModSettings held;held.expressionKey=true;held.expression=expression.data();held.frames=128;held.matrix[0]={8,6,1};
    voice.generate(&held,128,0);check(std::abs(voice.pitch(127))<1e-6f,"mono Key route uses played key rather than sample anchor");
    held.expressionKey=false;voice.generate(&held,128,0);check(voice.pitch(127)>20,"ordinary voice Key route uses trigger key");
    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("zazampler-release",{},false);root.createDirectory();
    struct Cleanup{juce::File file;~Cleanup(){file.deleteRecursively();}} cleanup{root};
    demo.getChildFile("tone.wav").copyFileTo(root.getChildFile("tone.wav"));
    auto render=[&](bool own,bool explicitRelease) {
        const auto file=root.getChildFile("test.sfz");file.replaceWithText("<region> sample=tone.wav pitch_keycenter=69 loop_mode=loop_continuous loop_start=0 loop_end=47999"+juce::String(explicitRelease?" ampeg_release=0.05":""));
        Engine engine(48000,128);engine.offline(true);check(engine.load(file.getFullPathName().toStdString()),"release fixture load");
        VoiceModSettings settings;settings.envelopes.ampEnabled=true;settings.envelopes.ownRelease=own;settings.envelopes.amp={0,0,1,.1f,true};
        std::vector<float> result(96000),right(128);
        for(int start=0;start<96000;start+=128){if(start==0){unsigned char m[]={0x90,69,100};engine.midi(m,3,0);}if(start==4096){unsigned char m[]={0x80,69,0};engine.midi(m,3,17);}engine.render(result.data()+start,right.data(),128,nullptr,false,&settings);}return result;
    };
    auto normal=render(false,false),owned=render(true,false),explicitNormal=render(false,true),explicitOwned=render(true,true);
    double tail=0,normalTail=0,end=0;
    for(size_t i=8192;i<12288;++i){tail+=owned[i]*owned[i];normalTail+=normal[i]*normal[i];}
    for(size_t i=90000;i<96000;++i)end+=owned[i]*owned[i];
    std::cout<<"release energies "<<tail<<" "<<normalTail<<" "<<end<<"\n";
    check(tail>1e-3 && normalTail<1e-9 && end<1e-10,"imported release outlives default SFZ release and finishes");
    for(size_t i=0;i<explicitOwned.size();++i)check(std::abs(explicitOwned[i]-explicitNormal[i])<1e-6,"explicit SFZ release preserved");
}
}
