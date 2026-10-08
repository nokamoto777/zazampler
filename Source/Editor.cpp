#include "Editor.h"
#include "KnobSlider.h"
class PanelControl final : public juce::Component {
public:
    juce::Label label;
    KnobSlider slider;
    juce::ComboBox choice;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sa;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> ca;
    bool isChoice=false;
    PanelControl(juce::AudioProcessorValueTreeState& state,const char* id,const char* title) {
        label.setText(title,juce::dontSendNotification);label.setFont(juce::FontOptions(11.f));
        label.setJustificationType(juce::Justification::centred);label.setColour(juce::Label::textColourId,juce::Colour(0xffd4dde1));addAndMakeVisible(label);
        const ParameterSpec* spec=nullptr;
        if(id)for(const auto& p:parameterSpecs())if(juce::String(p.id)==id)spec=&p;
        isChoice=spec && !spec->choices.isEmpty();
        if(isChoice) {
            choice.addItemList(spec->choices,1);addAndMakeVisible(choice);
            ca=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state,id,choice);
            choice.setTooltip(spec->name);
        } else {
            slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setRotaryParameters(juce::MathConstants<float>::pi*1.22f,juce::MathConstants<float>::pi*2.78f,true);
            slider.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
            slider.setPopupDisplayEnabled(true,true,nullptr);slider.setNumDecimalPlacesToDisplay(2);addAndMakeVisible(slider);
            if(spec) {
                sa=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state,id,slider);
                slider.setName(spec->name);slider.setDoubleClickReturnValue(true,spec->initial);
                const bool integer=juce::String(id)=="channel" || juce::String(id)=="cutoff" || juce::String(id).endsWith("Hz") || juce::String(id).startsWith("seqNote") || juce::String(id)=="seqLength" || juce::String(id)=="seqOctaves";
                slider.textFromValueFunction=[integer](double v){return juce::String(v,integer?0:2);};
                slider.updateText();
                slider.setTooltip(juce::String(spec->name)+" | Drag to edit; right-click to enter a value; double-click to reset");
            } else {
                slider.setRange(0,1);slider.setValue(0.4);slider.setEnabled(false);
                slider.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
                label.setColour(juce::Label::textColourId,juce::Colour(0xff879197));
            }
        }
    }
    void resized() override {
        label.setBounds(0,getHeight()-17,getWidth(),16);
        slider.setBounds(0,0,getWidth(),getHeight()-17);
        choice.setBounds(3,label.getText().isEmpty()?0:4,getWidth()-6,label.getText().isEmpty()?getHeight():24);
    }
};
PanelControl& ZaZamplerEditor::knob(const char* id,const char* title,int x,int y,int w,int h,bool fx,bool routing,bool envRouting,int extra) {
    auto c=std::make_unique<PanelControl>(processor.parameters,id,title);
    auto& parent=extra==3?matrixPanel:extra==4?sequencePanel:envRouting?envRoutingPanel:routing?routingPanel:(fx?fxPanel:static_cast<juce::Component&>(canvas));
    parent.addAndMakeVisible(*c);c->setBounds(x,y,w,h);
    if(routing || envRouting || extra)c->label.setColour(juce::Label::textColourId,juce::Colour(0xff244758));
    auto& result=*c;controls.push_back(std::move(c));return result;
}
ZaZamplerEditor::ZaZamplerEditor(ZaZamplerProcessor& p)
    : AudioProcessorEditor(p),processor(p),keyboard(p.keyboard,juce::MidiKeyboardComponent::horizontalKeyboard) {
    setLookAndFeel(&look);
    addAndMakeVisible(canvas);
    canvas.setComponentID("instrumentCanvas");
    for(auto* c:std::initializer_list<juce::Component*>{&folderButton,&panicButton,&patches,&status,&patchInfo,&keyboard,&fxPanel,&bankButton,&sfzButton,&detailsButton,&approximateFx,&previous,&next,&keysButton,&effectsButton,&mainButton,&lfoButton,&routingPanel,&envButton,&envRoutingPanel,&matrixButton,&sequenceButton,&matrixPanel,&sequencePanel})canvas.addAndMakeVisible(c);
    // Main-panel locations follow the familiar Zampler arrangement.
    knob("cutoff","Cutoff",800,48,72,78);knob("resonance","Reso",872,48,66,78);
    knob("envFilterAmount","Env / oct",938,48,64,78);knob("keytrack","KTrack",1002,48,64,78);
    knob("filterMode","",809,133,247,25);
    knob("channel","MIDI CH",805,190,74,60);knob("glide","Glide / s",887,190,74,60);knob("gain","Volume",979,190,80,60);
    for(int row=0;row<3;++row) {
        const int y=278+row*80;
        const char* names[]={"Attack","Decay","Sustain","Release"};
        const char* prefixes[]={"envMod","envFilter","envAmp"};
        for(int i=0;i<4;++i)knob((juce::String(prefixes[row])+names[i]).toRawUTF8(),names[i],802+i*67,y,60,60);
        const auto prefix=juce::String("lfo")+juce::String(row+1);
        auto add=[&](const char* suffix,const char* label,int x,int top,int width,int height,bool routing=false)->PanelControl& {
            return knob((prefix+suffix).toRawUTF8(),label,x,top,width,height,false,routing);
        };
        add("Shape","",87,194+row*102,154,21);
        add("Rate","Rate / Hz",42,215+row*102,63,72);add("Skew","Skew",110,215+row*102,63,72);add("Fade","Fade / s",178,215+row*102,63,72);
        const int top=row*83;
        add("Target",("LFO "+juce::String(row+1)+" target").toRawUTF8(),0,top+10,145,49,true);
        add("Depth","Depth",147,top,67,77,true);
        add("Sync","Clock",214,top+1,112,47,true);add("Division","Division",326,top+1,133,47,true);
        add("Reset","",217,top+53,237,21,true);
    }
    knob("envAmpEnabled","",992,423,75,19);
    knob("envModTarget","Mod envelope target",5,15,241,50,false,false,true);
    knob("envModDepth","Depth",274,8,92,86,false,false,true);
    knob("voiceMode","Voice mode",5,79,200,49,false,false,true);
    for(int row=0;row<4;++row) {
        const auto prefix=juce::String("matrix")+juce::String(row+1);
        knob((prefix+"Source").toRawUTF8(),("Source "+juce::String(row+1)).toRawUTF8(),0,row*61,160,50,false,false,false,3);
        knob((prefix+"Target").toRawUTF8(),"Destination",165,row*61,166,50,false,false,false,3);
        knob((prefix+"Depth").toRawUTF8(),"Depth",354,row*61,76,58,false,false,false,3);
    }
    knob("seqMode","Mode",0,0,119,51,false,false,false,4);
    knob("seqDivision","Division",122,0,115,51,false,false,false,4);
    knob("seqGate","Gate",239,0,72,70,false,false,false,4);
    knob("seqLength","Length",313,0,72,70,false,false,false,4);
    knob("seqOctaves","Octaves",387,0,72,70,false,false,false,4);
    for(int step=1;step<=8;++step) {
        knob(("seqNote"+juce::String(step)).toRawUTF8(),(juce::String(step)+" / st").toRawUTF8(),(step-1)*57,83,56,74,false,false,false,4);
        knob(("seqVel"+juce::String(step)).toRawUTF8(),"Velocity",(step-1)*57,166,56,74,false,false,false,4);
    }
    // Bottom effects rack: every enabled control is attached to the real parameter.
    knob("drive","Drive",5,23,66,76,true);knob("driveMix","Amount",5,102,66,76,true);
    knob("eq1Hz","Freq",90,25,64,75,true);knob("eq1Q","Q",151,25,56,75,true);knob("eq1Db","Gain",120,103,64,75,true);
    knob("eq2Hz","Freq",224,25,64,75,true);knob("eq2Q","Q",285,25,56,75,true);knob("eq2Db","Gain",254,103,64,75,true);
    const char* modIds[]={"phaserRate","phaserDepth","phaserFeedback","phaserMix","chorusRate","chorusDepth","chorusMix"};
    const char* modNames[]={"Rate","Depth","Feedback","Amount","Rate","Depth","Amount"};
    for(int i=0;i<7;++i)knob(modIds[i],modNames[i],352+(i<4?i:i-4)*57, i<4?25:103,57,75,true);
    knob("delayMs","Time / ms",597,25,62,75,true);knob("delayFeedback","Feedback",661,25,62,75,true);knob("delayMix","Amount",725,25,62,75,true);
    knob("delaySync","Timing",592,111,105,48,true);knob("delayDivision","Division",701,111,115,48,true);
    knob("delayPingPong","Cross",786,25,57,75,true);
    knob("room","Size",858,25,83,75,true);knob("damping","Damp",944,25,83,75,true);
    knob("reverb","Amount",858,103,83,75,true);knob("width","Width",944,103,83,75,true);
    patches.setTextWhenNothingSelected("Select an instrument");patches.onChange=[this]{selectPreset();};
    bankButton.onClick=[this]{chooseBank();};sfzButton.onClick=[this]{processor.clearBank();refreshPresets();};
    approximateFx.onClick=[this]{if(visibleBank)selectPreset();};
    detailsButton.onClick=[this]{juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,"Bank import details",processor.importNotes().isNotEmpty()?processor.importNotes():"No bank preset selected.");};
    previous.onClick=[this]{movePreset(-1);};next.onClick=[this]{movePreset(1);};
    mainButton.onClick=[this]{centrePage(false);};lfoButton.onClick=[this]{centrePage(1);};envButton.onClick=[this]{centrePage(2);};matrixButton.onClick=[this]{centrePage(3);};sequenceButton.onClick=[this]{centrePage(4);};
    keysButton.onClick=[this]{bottomPage(false);};effectsButton.onClick=[this]{bottomPage(true);};
    folderButton.onClick=[this]{chooseFolder();};panicButton.onClick=[this]{processor.keyboard.allNotesOff(0);processor.panic();};
    const auto location=processor.libraryLocation();
    if(location.first.isNotEmpty())folderAccess=std::make_unique<FolderAccess>(location.first.toStdString(),location.second.toStdString());
    refreshPresets();
    for(auto* b:{&keysButton,&effectsButton,&panicButton}) {
        b->setColour(juce::TextButton::buttonColourId,juce::Colour(0xff292929));
        b->setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xff3b3b3b));
        b->setColour(juce::TextButton::textColourOffId,juce::Colour(0xffbcbcbc));
        b->setColour(juce::TextButton::textColourOnId,juce::Colour(0xffeee4d0));
    }
    keyboard.setAvailableRange(24,108);keyboard.setLowestVisibleKey(36);keyboard.setKeyWidth(20.f);
    keyboard.setColour(juce::MidiKeyboardComponent::whiteNoteColourId,juce::Colour(0xffe9edf0));
    keyboard.setColour(juce::MidiKeyboardComponent::blackNoteColourId,juce::Colour(0xff181d21));
    keyboard.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId,juce::Colour(0xffd8a141));
    status.setFont(juce::FontOptions(12.f));status.setColour(juce::Label::textColourId,juce::Colour(0xffaabcc5));
    patchInfo.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(),13.f,juce::Font::plain));patchInfo.setColour(juce::Label::textColourId,juce::Colour(0xff244758));
    patchInfo.setJustificationType(juce::Justification::topLeft);
    setResizable(true,true);
    setResizeLimits(660,456,2200,1520);
    setSize(1100,760);bottomPage(false);centrePage(false);timerCallback();startTimerHz(10);
}
ZaZamplerEditor::~ZaZamplerEditor() {stopTimer();setLookAndFeel(nullptr);}
void ZaZamplerEditor::bottomPage(bool effects) {
    showEffects=effects;fxPanel.setVisible(effects);keyboard.setVisible(!effects);
    effectsButton.setToggleState(effects,juce::dontSendNotification);keysButton.setToggleState(!effects,juce::dontSendNotification);canvas.repaint();
}
void ZaZamplerEditor::centrePage(int page) {
    centrePageIndex=page;const bool routing=page!=0;showRouting=routing;routingPanel.setVisible(page==1);envRoutingPanel.setVisible(page==2);matrixPanel.setVisible(page==3);sequencePanel.setVisible(page==4);
    for(auto* c:std::initializer_list<juce::Component*>{&bankButton,&folderButton,&sfzButton,&previous,&patches,&next,&patchInfo,&approximateFx,&detailsButton})c->setVisible(!routing);
    mainButton.setToggleState(!routing,juce::dontSendNotification);lfoButton.setToggleState(page==1,juce::dontSendNotification);envButton.setToggleState(page==2,juce::dontSendNotification);matrixButton.setToggleState(page==3,juce::dontSendNotification);sequenceButton.setToggleState(page==4,juce::dontSendNotification);canvas.repaint();
}
void ZaZamplerEditor::movePreset(int direction) {
    const int count=patches.getNumItems();if(count==0)return;
    const int index=patches.getSelectedItemIndex();patches.setSelectedItemIndex((index+direction+count)%count);
}
void ZaZamplerEditor::chooseFolder() {
    chooser=std::make_unique<juce::FileChooser>("Choose the folder containing SFZ files AND their samples",juce::File(),"",true);
    auto safe=juce::Component::SafePointer<ZaZamplerEditor>(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectDirectories,[safe](const juce::FileChooser& fc){
        if (!safe) return;
        const auto root=fc.getResult(); if (!root.isDirectory()) return;
        const auto bookmark=FolderAccess::makeBookmark(root.getFullPathName().toStdString());
        safe->folderAccess=std::make_unique<FolderAccess>(root.getFullPathName().toStdString(),bookmark);
        safe->refreshPresets();
        if(safe->patches.getNumItems()>0) {
            if(safe->patches.getSelectedId()==0)safe->patches.setSelectedItemIndex(0,juce::dontSendNotification);
            safe->selectPreset();
        }
    });
}
void ZaZamplerEditor::chooseBank() {
    chooser=std::make_unique<juce::FileChooser>("Open a Zampler bank or preset",juce::File(),"*.fxb;*.fxp",true);
    auto safe=juce::Component::SafePointer<ZaZamplerEditor>(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe](const juce::FileChooser& fc){
        if(!safe || !fc.getResult().existsAsFile())return;
        const auto result=safe->processor.openBank(fc.getResult());
        if(result.failed()) {juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Cannot open bank",result.getErrorMessage());return;}
        safe->refreshPresets();
        if(safe->folderAccess && safe->patches.getNumItems()>0)safe->patches.setSelectedItemIndex(0);
        else juce::MessageManager::callAsync([safe]{if(safe)safe->chooseFolder();});
    });
}
void ZaZamplerEditor::refreshPresets() {
    visibleBank=processor.getBank();patches.clear(juce::dontSendNotification);files.clear();
    if(visibleBank) {
        approximateFx.setToggleState(processor.bankUsesApproximateFx(),juce::dontSendNotification);
        for(const auto& p:visibleBank->patches)if(p.hasSample())patches.addItem(juce::String(p.slot+1).paddedLeft('0',3)+"  "+juce::String::fromUTF8(p.name.c_str()),p.slot+1);
        const auto selected=processor.selectedBankSlot();if(selected>=0)patches.setSelectedId(selected+1,juce::dontSendNotification);
        patches.setTextWhenNothingSelected(patches.getNumItems()?"Select a bank preset":"This bank has no sample references");
    } else if(folderAccess) {
        const juce::File root(juce::String::fromUTF8(folderAccess->path.c_str()));
        root.findChildFiles(files,juce::File::findFiles,true,"*.sfz");files.sort();
        for(int i=0;i<files.size();++i)patches.addItem(files[i].getRelativePathFrom(root),i+1);
        patches.setTextWhenNothingSelected(files.isEmpty()?"No SFZ files in this folder":"Select an SFZ instrument");
    }
    approximateFx.setEnabled(visibleBank!=nullptr);detailsButton.setEnabled(visibleBank!=nullptr);
    previous.setEnabled(patches.getNumItems()>1);next.setEnabled(patches.getNumItems()>1);
}
void ZaZamplerEditor::selectPreset() {
    if(!folderAccess)return;
    const int slot=patches.getSelectedId()-1;if(slot<0)return;
    const juce::File root(juce::String::fromUTF8(folderAccess->path.c_str()));
    const auto bookmark=juce::String::fromUTF8(folderAccess->bookmark.c_str());
    if(visibleBank)processor.loadBankPatch(root,bookmark,slot,approximateFx.getToggleState());
    else if(juce::isPositiveAndBelow(slot,files.size()))processor.load(root,bookmark,files[slot].getRelativePathFrom(root));
}
void ZaZamplerEditor::timerCallback() {
    if(visibleBank!=processor.getBank()) {
        const auto location=processor.libraryLocation();
        if(location.first.isNotEmpty())folderAccess=std::make_unique<FolderAccess>(location.first.toStdString(),location.second.toStdString());
        refreshPresets();
    }
    keyboard.setMidiChannel(juce::jmax(1,static_cast<int>(processor.parameters.getRawParameterValue("channel")->load())));
    status.setText(processor.statusText(),juce::dontSendNotification);
    juce::String info;
    if(visibleBank) {
        info="BANK  "+juce::String::fromUTF8(visibleBank->name.c_str());
        const int slot=processor.selectedBankSlot();
        if(slot>=0 && static_cast<size_t>(slot)<visibleBank->patches.size()) {
            const auto& patch=visibleBank->patches[static_cast<size_t>(slot)];
            info+="\n\n"+juce::String::fromUTF8(patch.comment.c_str())+"\n\nSFZ  "+juce::String::fromUTF8(patch.sampleFile.c_str());
        } else info+="\n\nSelect a bank preset and its sample folder.";
    } else info="SFZ INSTRUMENT\n\nLoad a bank or choose a folder containing\nSFZ instruments and their samples.";
    patchInfo.setText(info,juce::dontSendNotification);canvas.repaint(990,727,85,12);if(showEffects)canvas.repaint(24,532,1052,24);
}
void ZaZamplerEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff181818));
}
void ZaZamplerEditor::paintPanel(juce::Graphics& g) {
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff383838),0,0,juce::Colour(0xff222222),0,760,false));g.fillAll();
    auto text=[&](const juce::String& s,int x,int y,int w,int h,float size,juce::Colour colour=juce::Colour(0xffc9c9c9)){
        g.setColour(colour);g.setFont(juce::FontOptions(size,juce::Font::bold));g.drawText(s,x,y,w,h,juce::Justification::centredLeft);
    };
    auto panel=[&](int x,int y,int w,int h,const juce::String& title){
        g.setColour(juce::Colour(0xff272727));g.fillRect(x,y,w,h);
        g.setColour(juce::Colour(0xff4c4c4c));g.drawRect(static_cast<float>(x)+0.5f,static_cast<float>(y)+0.5f,static_cast<float>(w)-1.f,static_cast<float>(h)-1.f,0.8f);
        g.setColour(juce::Colours::black.withAlpha(0.5f));g.drawHorizontalLine(y+h-2,static_cast<float>(x+1),static_cast<float>(x+w-1));
        text(title,x+12,y+5,w-24,20,11.f);
    };
    panel(24,20,749,128,"");
    // Original vector artwork: dark perforated metal and amber indicators.
    g.setColour(juce::Colour(0xff181818));for(int y=28;y<144;y+=5)for(int x=30;x<770;x+=5)g.fillEllipse(static_cast<float>(x),static_cast<float>(y),1.5f,1.5f);
    g.setFont(juce::FontOptions(52.f,juce::Font::bold|juce::Font::italic));
    g.setColour(juce::Colours::black);g.drawText("ZaZampler",49,37,510,68,juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xffececec));g.drawText("ZaZampler",46,34,510,68,juce::Justification::centredLeft);
    text("SAMPLE WORKSTATION",50,108,335,22,12.f,juce::Colour(0xffdea03d));
    text("SFZ  /  FXB  /  128 VOICES",531,111,225,18,12.f);
    for(int i=0;i<3;++i) {
        panel(24,190+i*102,226,100,"LFO "+juce::String(i+1));

    }
    panel(791,20,285,142,"FILTER");panel(791,169,285,83,"OUTPUT");
    for(int i=0;i<3;++i) {
        const char* names[]={"MOD ENVELOPE","FILTER ENVELOPE","AMP ENVELOPE"};
        panel(791,259+i*80,285,80,names[i]);
    }
    // Vent strip above the inset central display.
    g.setColour(juce::Colour(0xff131313));for(int x=270;x<770;x+=7)g.fillRect(x,154,3,14);
    // Central blue LCD with the bank actions and patch browser.
    g.setColour(juce::Colour(0xff141b20));g.fillRect(266,172,507,331);
    g.setColour(juce::Colour(0xff747474));g.drawRect(269,175,501,325,2);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xffb6d8e9),274,181,juce::Colour(0xff7eacc7),274,494,false));g.fillRect(274,180,491,315);
    const auto ink=juce::Colour(0xff244758);

    g.setColour(juce::Colour(0xff547d95));g.drawLine(286,215,753,215,1.f);
    if(!showRouting)text("PATCH",289,254,95,17,10.f,ink);
    if(!showRouting) {
        g.setColour(juce::Colour(0xff6b98b3));g.drawLine(287,420,750,420,1.f);
        text("POLYPHONY  128     /     SFZ SAMPLE ENGINE",289,426,440,19,11.f,ink);
    } else if(centrePageIndex==1) {
        text("GLOBAL LFOs / BEFORE FX / DEPTH 0 = BYPASS",289,477,462,15,10.f,ink);
    } else if(centrePageIndex==2) {
        text("AMP: enable On in the AMP ENVELOPE header.",300,352,440,22,12.f,ink);
        text("FILTER: set Env / oct and enable the filter.",300,377,440,22,12.f,ink);
        text("GLIDE: choose Mono, then turn Glide / s.",300,402,445,22,12.f,ink);
        text("All envelopes retrigger on each note-on.",300,432,440,22,11.f,ink);
        text("Shared by all voices / sustain pedal supported.",300,455,440,22,11.f,ink);
    } else if(centrePageIndex==3) {
        text("4 GLOBAL ROUTES / PITCH DEPTH 1 = 12 SEMITONES",289,477,462,15,10.f,ink);
    } else {
        text("HOLD KEYS TO RUN / VELOCITY 0 = REST / HOST BPM",289,477,462,15,10.f,ink);
    }
    panel(24,532,1052,184,"");
    if(showEffects) {
        const int positions[]={24,110,244,376,619,876};
        const char* names[]={"SATURATION","EQ 1","EQ 2","PHASER / CHORUS","DELAY","REVERB"};
        auto active=[&](const char* id){return std::abs(processor.parameters.getRawParameterValue(id)->load())>0.001f;};
        const bool enabled[]={active("driveMix"),active("eq1Db"),active("eq2Db"),active("phaserMix") || active("chorusMix"),active("delayMix"),active("reverb")};
        for(int i=0;i<6;++i) {g.setColour(enabled[i]?juce::Colour(0xffe4ae49):juce::Colour(0xff555555));g.fillRect(positions[i]+5,542,4,4);text(names[i],positions[i]+12,536,i==0?76:200,17,i==0?9.f:11.f);if(i>0){g.setColour(juce::Colour(0xff4a4a4a));g.drawLine(static_cast<float>(positions[i]),537,static_cast<float>(positions[i]),708,1.f);}}
    }
    text("ZaZampler",28,153,155,25,20.f);text("0.8",200,158,45,18,11.f,juce::Colour(0xffdea03d));
    text("ARM64  /  AUv3 / VST3",848,505,235,21,11.f);
    g.setColour(juce::Colour(0xff12191e));g.fillRect(990,728,85,7);
    g.setColour(processor.peak.load()>1.f?juce::Colours::orange:juce::Colour(0xffdea03d));g.fillRect(990,728,static_cast<int>(85.f*juce::jlimit(0.f,1.f,processor.peak.load())),7);
    // Panel fasteners.
    for(auto p:{juce::Point<int>(12,12),{1088,12},{12,748},{1088,748}}){g.setColour(juce::Colour(0xff161c20));g.fillEllipse(static_cast<float>(p.x-3),static_cast<float>(p.y-3),6,6);g.setColour(juce::Colour(0xff67737b));g.drawLine(static_cast<float>(p.x-2),static_cast<float>(p.y),static_cast<float>(p.x+2),static_cast<float>(p.y),1);}
}
void ZaZamplerEditor::resized() {
    constexpr float designWidth=1100.f,designHeight=760.f;
    const float scale=std::min(getWidth()/designWidth,getHeight()/designHeight);
    canvas.setBounds(0,0,1100,760);
    canvas.setTransform(juce::AffineTransform::scale(scale).translated(
        (getWidth()-designWidth*scale)*0.5f,(getHeight()-designHeight*scale)*0.5f));
    mainButton.setBounds(288,184,59,26);lfoButton.setBounds(351,184,104,26);envButton.setBounds(459,184,104,26);matrixButton.setBounds(567,184,86,26);sequenceButton.setBounds(657,184,94,26);
    routingPanel.setBounds(287,221,461,251);envRoutingPanel.setBounds(287,221,461,251);matrixPanel.setBounds(287,221,461,251);sequencePanel.setBounds(287,221,461,251);
    bankButton.setBounds(289,221,144,26);folderButton.setBounds(442,221,147,26);sfzButton.setBounds(598,221,150,26);
    previous.setBounds(288,274,28,30);patches.setBounds(321,274,395,30);next.setBounds(721,274,28,30);
    patchInfo.setBounds(288,317,460,99);approximateFx.setBounds(286,454,305,29);detailsButton.setBounds(603,455,145,25);
    keysButton.setBounds(24,505,115,23);effectsButton.setBounds(144,505,115,23);panicButton.setBounds(679,505,94,23);
    fxPanel.setBounds(32,532,1040,183);keyboard.setBounds(40,552,1020,141);status.setBounds(24,721,950,28);
}
