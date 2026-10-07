#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include "Lfo.h"
#include "Envelope.h"
#include "ModMatrix.h"
#include <vector>

struct FxSettings {
    int filterMode=0;
    float cutoff=20000, resonance=0.707f, keytrack=0;
    float drive=0, driveMix=0;
    float eq1Hz=200, eq1Db=0, eq1Q=0.707f, eq2Hz=3000, eq2Db=0, eq2Q=0.707f;
    float chorusMix=0, chorusRate=0.4f, chorusDepth=0.5f;
    float phaserMix=0, phaserRate=0.3f, phaserDepth=0.7f, phaserFeedback=0.2f;
    float delayMix=0, delayMs=350, delayFeedback=0.3f, delayPingPong=0;
    int delaySync=0, delayDivision=4;
    float reverb=0, room=0.65f, damping=0.45f, width=1;
    float gainDb=-6;
};

// Stereo master effects, not a model of Zampler's proprietary/per-voice DSP.
// Ring buffers are allocated only in prepare(). All processing is in-place.
class Effects {
public:
    void prepare(double sampleRate);
    void reset();
    void process(float* left,float* right,int count,const FxSettings&,float bpm,const LfoSettingsBank& lfoSettings={},const uint8_t* noteTriggers=nullptr,const EnvelopeSettings& envelopeSettings={},const EnvelopeEvent* envelopeEvents=nullptr,const LfoModulation* modulation=nullptr);
    LfoModulation modulationTick(const FxSettings&,float bpm,const LfoSettingsBank&,bool noteOn,const EnvelopeSettings&,EnvelopeEvent,const MatrixSettings& = {},const PerformanceFrame& = {});
    static float delayMilliseconds(const FxSettings&,float bpm);
private:
    static constexpr float pi=3.14159265358979323846f;
    struct Svf {
        float s1=0,s2=0;
        float tick(float x,float g,float k,int mode);
    };
    struct Eq {
        float b0=1,b1=0,b2=0,a1=0,a2=0,z1=0,z2=0;
        void coefficients(double sr,float hz,float db,float q);
        float tick(float x);
    };
    struct Smooth {
        float value=0;
        float tick(float target,float a) { value+=a*(target-value); return value; }
    };
    double rate=44100,chorusPhase=0,phaserPhase=0;
    float smoothing=0.001f;
    bool initial=true;
    int eqCounter=0;
    std::array<std::array<Svf,3>,2> filters;
    std::array<std::array<Eq,2>,2> equalizers;
    float legacy[2] {};
    float allpass[2][6] {}, phaserLast[2] {};
    std::array<std::vector<float>,2> chorusBuffer,delayBuffer;
    size_t chorusWrite=0,delayWrite=0;
    std::array<Smooth,25> smooth;
    Smooth outputGain;
    std::array<float,8> filterWeights {};
    juce::Reverb reverb;
    LfoBank lfos;
    EnvelopeBank envelopes;
    Smooth ampEnvelopeBlend,filterEnvelopeDepth;
    std::array<Smooth,6> modEnvelopeRoutes;
    std::array<std::array<Smooth,6>,4> matrixRoutes;
    Smooth keytrackAmount;
    bool modulationInitial=true;
    static float read(const std::vector<float>&,size_t write,float delay);
};
