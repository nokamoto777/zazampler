#pragma once
#include "ZamplerBank.h"
#include "Parameters.h"
struct BankConversion {
    std::vector<std::pair<juce::String,float>> settings;
    juce::StringArray warnings;
};
// Deliberately an approximate translation, never advertised as identical DSP.
BankConversion translateZamplerPatch(const ZamplerPatch&);
struct SampleResolution {juce::File file;juce::String error;};
SampleResolution resolveBankSample(const ZamplerPatch&,const juce::File& bankFile,const juce::File& permittedRoot);
