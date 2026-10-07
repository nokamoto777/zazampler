#pragma once
#include "Effects.h"
#include "Lfo.h"
#include <juce_audio_processors/juce_audio_processors.h>
struct ParameterSpec {
    const char* id; const char* name; int page;
    float low,high,initial,skew=1;
    juce::StringArray choices;
};
const std::vector<ParameterSpec>& parameterSpecs();
juce::AudioProcessorValueTreeState::ParameterLayout makeParameterLayout();
FxSettings readFxSettings(juce::AudioProcessorValueTreeState&);

LfoSettingsBank readLfoSettings(juce::AudioProcessorValueTreeState&);

EnvelopeSettings readEnvelopeSettings(juce::AudioProcessorValueTreeState&);

MatrixSettings readMatrixSettings(juce::AudioProcessorValueTreeState&);
SequenceSettings readSequenceSettings(juce::AudioProcessorValueTreeState&);
