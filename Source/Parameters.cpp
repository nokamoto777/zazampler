#include "Parameters.h"
const std::vector<ParameterSpec>& parameterSpecs() {
    static const std::vector<ParameterSpec> specs={
        {"gain","Output (dB)",0,-60,6,0,1,{}},
        {"cutoff","Filter cutoff (Hz)",0,40,20000,20000,0.25f,{}},
        {"reverb","Reverb mix",4,0,1,0,1,{}},
        {"channel","MIDI channel (0 = omni)",0,0,16,1,1,{}},
        {"filterMode","Filter mode",0,0,7,0,1,{"Off","Low-pass 12 dB","Low-pass 24 dB","High-pass 12 dB","High-pass 24 dB","Band-pass","Notch","Legacy low-pass 6 dB"}},
        {"resonance","Resonance (Q)",0,0.5f,10,0.707f,0.5f,{}},
        {"drive","Saturation drive (dB)",0,0,30,0,1,{}},
        {"driveMix","Saturation mix",0,0,1,0,1,{}},
        {"eq1Hz","EQ 1 frequency (Hz)",1,30,18000,200,0.25f,{}},
        {"eq1Db","EQ 1 gain (dB)",1,-18,18,0,1,{}},
        {"eq1Q","EQ 1 bandwidth (Q)",1,0.2f,10,0.707f,0.5f,{}},
        {"eq2Hz","EQ 2 frequency (Hz)",1,30,18000,3000,0.25f,{}},
        {"eq2Db","EQ 2 gain (dB)",1,-18,18,0,1,{}},
        {"eq2Q","EQ 2 bandwidth (Q)",1,0.2f,10,0.707f,0.5f,{}},
        {"chorusMix","Chorus mix",2,0,1,0,1,{}},
        {"chorusRate","Chorus rate (Hz)",2,0.05f,8,0.4f,0.5f,{}},
        {"chorusDepth","Chorus depth",2,0,1,0.5f,1,{}},
        {"phaserMix","Phaser mix",2,0,1,0,1,{}},
        {"phaserRate","Phaser rate (Hz)",2,0.05f,8,0.3f,0.5f,{}},
        {"phaserDepth","Phaser depth",2,0,1,0.7f,1,{}},
        {"phaserFeedback","Phaser feedback",2,-0.8f,0.8f,0.2f,1,{}},
        {"delayMix","Delay mix",3,0,1,0,1,{}},
        {"delayMs","Delay time (ms, free mode)",3,1,2000,350,0.5f,{}},
        {"delayFeedback","Delay feedback",3,0,0.95f,0.3f,1,{}},
        {"delayPingPong","Delay cross-feedback",3,0,1,0,1,{}},
        {"delaySync","Delay timing",3,0,1,0,1,{"Free","Host tempo"}},
        {"delayDivision","Delay note value",3,0,8,4,1,{"1/32","1/16","1/8","1/8 dotted","1/4","1/4 dotted","1/2","1/2 dotted","1 bar (4 beats)"}},
        {"room","Reverb size",4,0,1,0.65f,1,{}},
        {"damping","Reverb damping",4,0,1,0.45f,1,{}},
        {"width","Reverb stereo width",4,0,1,1,1,{}},
        {"lfo1Shape","LFO 1 waveform",5,0,5,0,1,{"Sine","Triangle","Saw up","Saw down","Square","S&H"}},
        {"lfo1Rate","LFO 1 rate (Hz)",5,0.01f,30,1,0.35f,{}},
        {"lfo1Skew","LFO 1 skew",5,0.05f,0.95f,0.5f,1,{}},
        {"lfo1Fade","LFO 1 fade-in (seconds)",5,0,10,0,0.4f,{}},
        {"lfo1Depth","LFO 1 depth",5,-1,1,0,1,{}},
        {"lfo1Target","LFO 1 target",5,0,6,0,1,{"Off","Cutoff","Volume","Pan","Resonance","Drive","Pitch"}},
        {"lfo1Sync","LFO 1 clock",5,0,1,0,1,{"Free Hz","Host tempo"}},
        {"lfo1Division","LFO 1 note value",5,0,10,5,1,{"1/32","1/16","1/8","1/8 triplet","1/8 dotted","1/4","1/4 dotted","1/2","1 bar","2 bars","4 bars"}},
        {"lfo1Reset","LFO 1 phase reset",5,0,1,0,1,{"Free phase","Note retrigger"}},
        {"lfo2Shape","LFO 2 waveform",5,0,5,0,1,{"Sine","Triangle","Saw up","Saw down","Square","S&H"}},
        {"lfo2Rate","LFO 2 rate (Hz)",5,0.01f,30,1,0.35f,{}},
        {"lfo2Skew","LFO 2 skew",5,0.05f,0.95f,0.5f,1,{}},
        {"lfo2Fade","LFO 2 fade-in (seconds)",5,0,10,0,0.4f,{}},
        {"lfo2Depth","LFO 2 depth",5,-1,1,0,1,{}},
        {"lfo2Target","LFO 2 target",5,0,6,0,1,{"Off","Cutoff","Volume","Pan","Resonance","Drive","Pitch"}},
        {"lfo2Sync","LFO 2 clock",5,0,1,0,1,{"Free Hz","Host tempo"}},
        {"lfo2Division","LFO 2 note value",5,0,10,5,1,{"1/32","1/16","1/8","1/8 triplet","1/8 dotted","1/4","1/4 dotted","1/2","1 bar","2 bars","4 bars"}},
        {"lfo2Reset","LFO 2 phase reset",5,0,1,0,1,{"Free phase","Note retrigger"}},
        {"lfo3Shape","LFO 3 waveform",5,0,5,0,1,{"Sine","Triangle","Saw up","Saw down","Square","S&H"}},
        {"lfo3Rate","LFO 3 rate (Hz)",5,0.01f,30,1,0.35f,{}},
        {"lfo3Skew","LFO 3 skew",5,0.05f,0.95f,0.5f,1,{}},
        {"lfo3Fade","LFO 3 fade-in (seconds)",5,0,10,0,0.4f,{}},
        {"lfo3Depth","LFO 3 depth",5,-1,1,0,1,{}},
        {"lfo3Target","LFO 3 target",5,0,6,0,1,{"Off","Cutoff","Volume","Pan","Resonance","Drive","Pitch"}},
        {"lfo3Sync","LFO 3 clock",5,0,1,0,1,{"Free Hz","Host tempo"}},
        {"lfo3Division","LFO 3 note value",5,0,10,5,1,{"1/32","1/16","1/8","1/8 triplet","1/8 dotted","1/4","1/4 dotted","1/2","1 bar","2 bars","4 bars"}},
        {"lfo3Reset","LFO 3 phase reset",5,0,1,0,1,{"Free phase","Note retrigger"}}
,
        {"envAmpAttack","Amp envelope Attack (seconds)",6,0,10,0.01f,0.35f,{}},
        {"envAmpDecay","Amp envelope Decay (seconds)",6,0,10,0.2f,0.35f,{}},
        {"envAmpSustain","Amp envelope Sustain",6,0,1,1,1,{}},
        {"envAmpRelease","Amp envelope Release (seconds)",6,0,20,0.3f,0.35f,{}},
        {"envFilterAttack","Filter envelope Attack (seconds)",6,0,10,0.01f,0.35f,{}},
        {"envFilterDecay","Filter envelope Decay (seconds)",6,0,10,0.2f,0.35f,{}},
        {"envFilterSustain","Filter envelope Sustain",6,0,1,0,1,{}},
        {"envFilterRelease","Filter envelope Release (seconds)",6,0,20,0.3f,0.35f,{}},
        {"envModAttack","Mod envelope Attack (seconds)",6,0,10,0.01f,0.35f,{}},
        {"envModDecay","Mod envelope Decay (seconds)",6,0,10,0.2f,0.35f,{}},
        {"envModSustain","Mod envelope Sustain",6,0,1,0,1,{}},
        {"envModRelease","Mod envelope Release (seconds)",6,0,20,0.3f,0.35f,{}},
        {"envAmpEnabled","Amp envelope",6,0,1,0,1,{"Off","On"}},
        {"envFilterAmount","Filter envelope amount (octaves)",6,-5,5,0,1,{}},
        {"envModTarget","Mod envelope target",6,0,6,0,1,{"Off","Cutoff","Volume","Pan","Resonance","Drive","Pitch"}},
        {"envModDepth","Mod envelope depth",6,-1,1,0,1,{}}
,
        {"keytrack","Filter keytrack (octave/octave, C4 reference)",7,-1,2,0,1,{}},
        {"glide","Mono glide (seconds)",7,0,2,0,0.4f,{}},
        {"voiceMode","Voice mode",7,0,1,0,1,{"Poly","Mono"}},
        {"seqMode","Sequence mode",8,0,3,0,1,{"Off","Arp Up","Arp Down","8-step"}},
        {"seqDivision","Sequence note value",8,0,7,2,1,{"1/32","1/16","1/8","1/8 triplet","1/8 dotted","1/4","1/4 dotted","1/2"}},
        {"seqGate","Sequence gate",8,0.05f,0.95f,0.7f,1,{}},
        {"seqLength","Sequence steps",8,1,8,8,1,{}},
        {"seqOctaves","Arpeggiator octaves",8,1,3,1,1,{}}
,        {"matrix1Source","Matrix 1 source",7,0,11,0,1,{"Off","LFO 1","LFO 2","LFO 3","Amp Env","Filter Env","Mod Env","Velocity","Key","Mod Wheel","Aftertouch","Pitch Bend"}},
        {"matrix1Target","Matrix 1 target",7,0,6,0,1,{"Off","Cutoff","Volume","Pan","Resonance","Drive","Pitch"}},
        {"matrix1Depth","Matrix 1 depth",7,-1,1,0,1,{}}
,        {"matrix2Source","Matrix 2 source",7,0,11,0,1,{"Off","LFO 1","LFO 2","LFO 3","Amp Env","Filter Env","Mod Env","Velocity","Key","Mod Wheel","Aftertouch","Pitch Bend"}},
        {"matrix2Target","Matrix 2 target",7,0,6,0,1,{"Off","Cutoff","Volume","Pan","Resonance","Drive","Pitch"}},
        {"matrix2Depth","Matrix 2 depth",7,-1,1,0,1,{}}
,        {"matrix3Source","Matrix 3 source",7,0,11,0,1,{"Off","LFO 1","LFO 2","LFO 3","Amp Env","Filter Env","Mod Env","Velocity","Key","Mod Wheel","Aftertouch","Pitch Bend"}},
        {"matrix3Target","Matrix 3 target",7,0,6,0,1,{"Off","Cutoff","Volume","Pan","Resonance","Drive","Pitch"}},
        {"matrix3Depth","Matrix 3 depth",7,-1,1,0,1,{}}
,        {"matrix4Source","Matrix 4 source",7,0,11,0,1,{"Off","LFO 1","LFO 2","LFO 3","Amp Env","Filter Env","Mod Env","Velocity","Key","Mod Wheel","Aftertouch","Pitch Bend"}},
        {"matrix4Target","Matrix 4 target",7,0,6,0,1,{"Off","Cutoff","Volume","Pan","Resonance","Drive","Pitch"}},
        {"matrix4Depth","Matrix 4 depth",7,-1,1,0,1,{}}
,        {"seqNote1","Step 1 semitones",8,-24,24,0,1,{}},
        {"seqVel1","Step 1 velocity (0 = rest)",8,0,1,1,1,{}}
,        {"seqNote2","Step 2 semitones",8,-24,24,0,1,{}},
        {"seqVel2","Step 2 velocity (0 = rest)",8,0,1,1,1,{}}
,        {"seqNote3","Step 3 semitones",8,-24,24,7,1,{}},
        {"seqVel3","Step 3 velocity (0 = rest)",8,0,1,1,1,{}}
,        {"seqNote4","Step 4 semitones",8,-24,24,0,1,{}},
        {"seqVel4","Step 4 velocity (0 = rest)",8,0,1,1,1,{}}
,        {"seqNote5","Step 5 semitones",8,-24,24,12,1,{}},
        {"seqVel5","Step 5 velocity (0 = rest)",8,0,1,1,1,{}}
,        {"seqNote6","Step 6 semitones",8,-24,24,7,1,{}},
        {"seqVel6","Step 6 velocity (0 = rest)",8,0,1,1,1,{}}
,        {"seqNote7","Step 7 semitones",8,-24,24,3,1,{}},
        {"seqVel7","Step 7 velocity (0 = rest)",8,0,1,1,1,{}}
,        {"seqNote8","Step 8 semitones",8,-24,24,7,1,{}},
        {"seqVel8","Step 8 velocity (0 = rest)",8,0,1,1,1,{}}
        ,{"seqRhythm","Rhythm engine",9,0,1,0,1,{"Straight","Euclidean"}},
        {"seqHits","Rhythm hits per 16 steps",9,1,16,5,1,{}},
        {"seqRotation","Rhythm rotation",9,0,15,0,1,{}},
        {"seqSwing","Rhythm shuffle (%)",9,50,75,50,1,{}},
        {"seqNoteOrder","Arpeggiator note order",9,0,1,0,1,{"Up / Down mode","Played order"}}
    };return specs;
}
juce::AudioProcessorValueTreeState::ParameterLayout makeParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout result;
    for(const auto& s:parameterSpecs()) {
        const juce::ParameterID id{s.id,1};
        if(!s.choices.isEmpty())result.add(std::make_unique<juce::AudioParameterChoice>(id,s.name,s.choices,static_cast<int>(s.initial)));
        else if(juce::String(s.id)=="channel" || juce::String(s.id)=="seqHits" || juce::String(s.id)=="seqRotation" || juce::String(s.id)=="seqLength" || juce::String(s.id)=="seqOctaves" || juce::String(s.id).startsWith("seqNote"))result.add(std::make_unique<juce::AudioParameterInt>(id,s.name,static_cast<int>(s.low),static_cast<int>(s.high),static_cast<int>(s.initial)));
        else result.add(std::make_unique<juce::AudioParameterFloat>(id,s.name,juce::NormalisableRange<float>(s.low,s.high,0.f,s.skew),s.initial));
    }return result;
}
FxSettings readFxSettings(juce::AudioProcessorValueTreeState& state) {
    FxSettings p;
    auto get=[&state](const char* id){return state.getRawParameterValue(id)->load();};
    p.keytrack=get("keytrack");p.gainDb=get("gain");p.filterMode=static_cast<int>(get("filterMode"));p.cutoff=get("cutoff");p.resonance=get("resonance");
    p.drive=get("drive");p.driveMix=get("driveMix");
    p.eq1Hz=get("eq1Hz");p.eq1Db=get("eq1Db");p.eq1Q=get("eq1Q");p.eq2Hz=get("eq2Hz");p.eq2Db=get("eq2Db");p.eq2Q=get("eq2Q");
    p.chorusMix=get("chorusMix");p.chorusRate=get("chorusRate");p.chorusDepth=get("chorusDepth");
    p.phaserMix=get("phaserMix");p.phaserRate=get("phaserRate");p.phaserDepth=get("phaserDepth");p.phaserFeedback=get("phaserFeedback");
    p.delayMix=get("delayMix");p.delayMs=get("delayMs");p.delayFeedback=get("delayFeedback");p.delayPingPong=get("delayPingPong");
    p.delaySync=static_cast<int>(get("delaySync"));p.delayDivision=static_cast<int>(get("delayDivision"));
    p.reverb=get("reverb");p.room=get("room");p.damping=get("damping");p.width=get("width");return p;
}

LfoSettingsBank readLfoSettings(juce::AudioProcessorValueTreeState& state) {
    LfoSettingsBank result;
    // Read the fixed IDs once per host block.
    static const char* ids[3][9]={
        {"lfo1Shape","lfo1Target","lfo1Sync","lfo1Division","lfo1Reset","lfo1Rate","lfo1Skew","lfo1Fade","lfo1Depth"},
        {"lfo2Shape","lfo2Target","lfo2Sync","lfo2Division","lfo2Reset","lfo2Rate","lfo2Skew","lfo2Fade","lfo2Depth"},
        {"lfo3Shape","lfo3Target","lfo3Sync","lfo3Division","lfo3Reset","lfo3Rate","lfo3Skew","lfo3Fade","lfo3Depth"}};
    for(size_t i=0;i<3;++i) {
        auto get=[&](int j){return state.getRawParameterValue(ids[i][j])->load();};
        auto& p=result[i];p.shape=static_cast<int>(get(0));p.target=static_cast<int>(get(1));p.sync=static_cast<int>(get(2));p.division=static_cast<int>(get(3));p.retrigger=static_cast<int>(get(4));
        p.hz=get(5);p.skew=get(6);p.fade=get(7);p.depth=get(8);
    }
    return result;
}

EnvelopeSettings readEnvelopeSettings(juce::AudioProcessorValueTreeState& state) {
    EnvelopeSettings p;
    auto get=[&](const char* id){return state.getRawParameterValue(id)->load();};
    p.amp={get("envAmpAttack"),get("envAmpDecay"),get("envAmpSustain"),get("envAmpRelease")};
    p.filter={get("envFilterAttack"),get("envFilterDecay"),get("envFilterSustain"),get("envFilterRelease")};
    p.mod={get("envModAttack"),get("envModDecay"),get("envModSustain"),get("envModRelease")};
    p.ampEnabled=get("envAmpEnabled")>0.5f;p.filterOctaves=get("envFilterAmount");
    p.modTarget=static_cast<int>(get("envModTarget"));p.modDepth=get("envModDepth");return p;
}

MatrixSettings readMatrixSettings(juce::AudioProcessorValueTreeState& state) {
    MatrixSettings p;
    static const char* ids[4][3]={{"matrix1Source","matrix1Target","matrix1Depth"},{"matrix2Source","matrix2Target","matrix2Depth"},{"matrix3Source","matrix3Target","matrix3Depth"},{"matrix4Source","matrix4Target","matrix4Depth"}};
    for(size_t i=0;i<4;++i){p[i].source=static_cast<int>(state.getRawParameterValue(ids[i][0])->load());p[i].target=static_cast<int>(state.getRawParameterValue(ids[i][1])->load());p[i].depth=state.getRawParameterValue(ids[i][2])->load();}return p;
}
SequenceSettings readSequenceSettings(juce::AudioProcessorValueTreeState& state) {
    SequenceSettings p;auto get=[&](const char* id){return state.getRawParameterValue(id)->load();};
    p.mode=static_cast<int>(get("seqMode"));p.division=static_cast<int>(get("seqDivision"));p.gate=get("seqGate");p.length=static_cast<int>(get("seqLength"));p.octaves=static_cast<int>(get("seqOctaves"));
    p.rhythm=static_cast<int>(get("seqRhythm"));p.hits=static_cast<int>(get("seqHits"));p.rotation=static_cast<int>(get("seqRotation"));p.swing=(get("seqSwing")-50.f)*0.02f;p.noteOrder=static_cast<int>(get("seqNoteOrder"));
    static const char* notes[]={"seqNote1","seqNote2","seqNote3","seqNote4","seqNote5","seqNote6","seqNote7","seqNote8"};
    static const char* velocities[]={"seqVel1","seqVel2","seqVel3","seqVel4","seqVel5","seqVel6","seqVel7","seqVel8"};
    for(size_t i=0;i<8;++i){p.notes[i]=static_cast<int>(get(notes[i]));p.velocities[i]=get(velocities[i]);}return p;
}
