#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "BuildInfo.h"

inline juce::String zazamplerBuildInfo() {
    return juce::String("ZaZampler ")+ZAZAMPLER_VERSION+"\nCommit: "+ZAZAMPLER_COMMIT+"\nBuild: "+ZAZAMPLER_BUILD_ID+
        "\nBuilt (UTC): "+ZAZAMPLER_BUILD_TIME+"\n"+juce::SystemStats::getOperatingSystemName()+
        " / "+juce::String(sizeof(void*)==8?"64-bit":"32-bit")+"\nhttps://github.com/nokamoto777/zazampler";
}
class LogPanel final : public juce::Component {
public:
    explicit LogPanel(std::function<juce::String()> read):reader(std::move(read)) {
        addAndMakeVisible(text);addAndMakeVisible(copy);addAndMakeVisible(refresh);
        text.setMultiLine(true);text.setReadOnly(true);text.setScrollbarsShown(true);
        text.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(),12.f,juce::Font::plain));
        refresh.onClick=[this]{text.setText(reader(),false);text.setCaretPosition(0);};
        copy.onClick=[this]{juce::SystemClipboard::copyTextToClipboard(text.getText());};
        refresh.onClick();setSize(800,520);
    }
    void resized() override {auto area=getLocalBounds().reduced(12);auto buttons=area.removeFromTop(32);copy.setBounds(buttons.removeFromLeft(140));refresh.setBounds(buttons.removeFromLeft(140));area.removeFromTop(8);text.setBounds(area);}
private:
    std::function<juce::String()> reader;juce::TextEditor text;juce::TextButton copy{"COPY ALL"},refresh{"REFRESH"};
};
class AboutPanel final : public juce::Component {
public:
    explicit AboutPanel(std::function<void()> showDebug) {
        addAndMakeVisible(info);addAndMakeVisible(link);addAndMakeVisible(debug);
        info.setText(zazamplerBuildInfo(),juce::dontSendNotification);info.setJustificationType(juce::Justification::topLeft);
        info.setColour(juce::Label::textColourId,juce::Colours::white);
        debug.onClick=std::move(showDebug);setSize(540,280);
    }
    void resized() override {info.setBounds(20,20,getWidth()-40,180);link.setBounds(20,202,getWidth()-40,24);debug.setBounds(20,240,140,28);}
private:
    juce::Label info;
    juce::HyperlinkButton link{"GitHub repository",juce::URL("https://github.com/nokamoto777/zazampler")};
    juce::TextButton debug{"DEBUG LOG"};
};
class InfoWindow final : public juce::DocumentWindow {
public:
    InfoWindow(const juce::String& name,juce::Component* content):DocumentWindow(name,juce::Colour(0xff252525),DocumentWindow::closeButton){setUsingNativeTitleBar(true);setContentOwned(content,true);setResizable(true,false);centreWithSize(content->getWidth(),content->getHeight());setVisible(true);}
    void closeButtonPressed() override {setVisible(false);}
};
