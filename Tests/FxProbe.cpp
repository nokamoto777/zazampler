// Small headless harness for the production Effects implementation.
// Native-endian interleaved float32 stereo input/output; no gain adjustment.
#include "EffectsChecks.h"
#include <fstream>
#include <string>
int main(int argc,char** argv) {
    try {
        if(argc==2 && std::string(argv[1])=="--self-test"){fxchecks::run();return 0;}
        if(argc<4)throw std::runtime_error("fx_probe INPUT.f32 OUTPUT.f32 SAMPLE_RATE [field=value ...]");
        if(std::string(argv[1])==argv[2])throw std::runtime_error("Input and output must differ");
        const double rate=std::stod(argv[3]);
        if(!std::isfinite(rate)||rate<8000||rate>192000)throw std::runtime_error("Invalid sample rate");
        FxSettings p;
        for(int i=4;i<argc;++i) {
            const std::string arg(argv[i]);const auto pos=arg.find('=');
            if(pos==std::string::npos)throw std::runtime_error("Expected field=value");
            const auto key=arg.substr(0,pos);const float value=std::stof(arg.substr(pos+1));
            if(!std::isfinite(value))throw std::runtime_error("Non-finite parameter");
#define FIELD(name) if(key==#name){p.name=value;continue;}
            FIELD(cutoff) FIELD(resonance) FIELD(gainDb) FIELD(drive) FIELD(driveMix)
            FIELD(chorusMix) FIELD(chorusRate) FIELD(chorusDepth)
            FIELD(phaserMix) FIELD(phaserRate) FIELD(phaserDepth) FIELD(phaserFeedback)
            FIELD(delayMix) FIELD(delayMs) FIELD(delayFeedback) FIELD(delayPingPong)
            FIELD(reverb) FIELD(room) FIELD(damping) FIELD(width)
#undef FIELD
            if(key=="spaceSquaredMix"){p.spaceSquaredMix=value!=0;continue;}
            throw std::runtime_error("Unsupported field: "+key);
        }
        std::ifstream input(argv[1],std::ios::binary);std::ofstream output(argv[2],std::ios::binary);
        if(!input||!output)throw std::runtime_error("Audio file open failed");
        Effects effects;effects.prepare(rate);
        float interleaved[256],left[128],right[128];
        while(input.read(reinterpret_cast<char*>(interleaved),sizeof(interleaved))||input.gcount()) {
            const auto bytes=input.gcount();
            if(bytes%static_cast<int>(2*sizeof(float)))throw std::runtime_error("Incomplete stereo frame");
            const int n=static_cast<int>(bytes/(2*sizeof(float)));
            for(int j=0;j<n;++j){left[j]=interleaved[2*j];right[j]=interleaved[2*j+1];}
            effects.process(left,right,n,p,120);
            for(int j=0;j<n;++j){interleaved[2*j]=left[j];interleaved[2*j+1]=right[j];}
            output.write(reinterpret_cast<char*>(interleaved),bytes);
        }
        if(input.bad()||!output)throw std::runtime_error("Audio I/O failed");
        return 0;
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
