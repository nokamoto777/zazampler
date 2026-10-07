#pragma once
#include "Processor.h"
#include "InstrumentLook.h"
class PanelControl;
class ZaZamplerEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit ZaZamplerEditor(ZaZamplerProcessor&);
    ~ZaZamplerEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    InstrumentLook look;
    juce::TooltipWindow tooltips {this,700};
    ZaZamplerProcessor& processor;
    juce::TextButton folderButton {"SAMPLE FOLDER"}, panicButton {"PANIC"},bankButton {"LOAD BANK / FXP"},sfzButton {"SFZ MODE"},detailsButton {"IMPORT DETAILS"};
    juce::TextButton previous {"<"},next {">"},keysButton {"KEYBOARD"},effectsButton {"EFFECTS"},mainButton {"MAIN"},lfoButton {"LFO ROUTING"},envButton {"ENV ROUTING"},matrixButton {"MATRIX"},sequenceButton {"SEQUENCE"};
    juce::ToggleButton approximateFx {"Approximate bank FX (not identical)"};
    juce::ComboBox patches;
    juce::Label status,patchInfo;
    juce::MidiKeyboardComponent keyboard;
    std::vector<std::unique_ptr<PanelControl>> controls;
    juce::Component fxPanel,routingPanel,envRoutingPanel,matrixPanel,sequencePanel;
    bool showRouting=false;
    int centrePageIndex=0;
    bool showEffects=false;
    PanelControl& knob(const char* id,const char* title,int x,int y,int w=60,int h=74,bool fx=false,bool routing=false,bool envRouting=false,int extra=0);
    void centrePage(int page);
    void bottomPage(bool effects);
    void movePreset(int direction);
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<FolderAccess> folderAccess;
    juce::Array<juce::File> files;
    std::shared_ptr<const ZamplerBank> visibleBank;
    void chooseFolder();
    void chooseBank();
    void refreshPresets();
    void selectPreset();
    void timerCallback() override;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ZaZamplerEditor)
};
