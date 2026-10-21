#pragma once
#include "Parameters.h"
inline constexpr const char* pushParameterIds[]={"cutoff","resonance","gain","envAmpAttack","envAmpRelease","lfo1Rate","lfo1Depth","reverb"};
inline void registerPushParameter(juce::AudioProcessorValueTreeState& state,size_t index) {
    if(index>=std::size(pushParameterIds))return;
    auto* p=state.getParameter(pushParameterIds[index]);
    // Notify the host without a normalized -> physical -> normalized round-trip.
    p->beginChangeGesture();p->sendValueChangedMessageToListeners(p->getValue());p->endChangeGesture();
}
