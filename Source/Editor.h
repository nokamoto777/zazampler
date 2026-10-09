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
    struct Canvas final : juce::Component {
        explicit Canvas(ZaZamplerEditor& e) : owner(e) {}
        void paint(juce::Graphics& g) override { owner.paintPanel(g); }
        ZaZamplerEditor& owner;
    };
    void paintPanel(juce::Graphics&);
    InstrumentLook look;
    Canvas canvas {*this};
    juce::TooltipWindow tooltips {this,700};
    ZaZamplerProcessor& processor;
    juce::TextButton folderButton {"LIBRARY FOLDER"}, scanButton {"RESCAN"}, panicButton {"PANIC"},bankButton {"LOAD BANK / FXP"},sfzButton {"SFZ MODE"},detailsButton {"IMPORT DETAILS"};
    juce::TextButton previous {"<"},next {">"},keysButton {"KEYBOARD"},effectsButton {"EFFECTS"},mainButton {"MAIN"},lfoButton {"LFO ROUTING"},envButton {"ENV ROUTING"},matrixButton {"MATRIX"},sequenceButton {"SEQUENCE"},rhythmButton {"RHYTHM"};
    juce::ToggleButton approximateFx {"Approximate bank FX (not identical)"};
    juce::ComboBox patches,libraries;
    juce::TextEditor presetSearch;
    juce::Label libraryStatus;
    std::shared_ptr<const LibraryCatalog::Snapshot> visibleCatalog;
    bool selectAfterScan=false;
    void refreshLibraries();
    void selectLibrary();
    juce::Label status,patchInfo;
    juce::MidiKeyboardComponent keyboard;
    std::vector<std::unique_ptr<PanelControl>> controls;
    juce::Component fxPanel,routingPanel,envRoutingPanel,matrixPanel,sequencePanel,rhythmPanel;
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
