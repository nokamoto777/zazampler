#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
// Keep precise numeric entry without permanent text fields on the instrument panel.
class KnobSlider final : public juce::Slider {
public:
    void mouseDown(const juce::MouseEvent& event) override {
        if(!event.mods.isPopupMenu()){juce::Slider::mouseDown(event);return;}
        if(!isEnabled())return;
        auto* dialog=new juce::AlertWindow("Set value",getName(),juce::MessageBoxIconType::NoIcon);
        dialog->addTextEditor("value",getTextFromValue(getValue()),"Value");
        dialog->addButton("Apply",1,juce::KeyPress(juce::KeyPress::returnKey));
        dialog->addButton("Cancel",0,juce::KeyPress(juce::KeyPress::escapeKey));
        juce::Component::SafePointer<KnobSlider> safe(this);
        dialog->enterModalState(true,juce::ModalCallbackFunction::create([safe,dialog](int result){
            if(safe && result==1){juce::Slider::ScopedDragNotification gesture(*safe);safe->setValue(safe->getValueFromText(dialog->getTextEditorContents("value")),juce::sendNotificationSync);}
        }),true);
    }
};
