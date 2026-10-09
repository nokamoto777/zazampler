#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "WaveformCapture.h"

class WaveformDisplay final : public juce::Component {
public:
    explicit WaveformDisplay(WaveformCapture& source) : capture(source) {
        setComponentID("outputVisualizer");setInterceptsMouseClicks(false,false);
    }
    void update() {
        const int count=capture.read(incoming.data(),static_cast<int>(incoming.size()));
        if(count>0) {
            lastUpdate=juce::Time::getMillisecondCounter();
            for(int i=0;i<count;++i) {history[next]=incoming[static_cast<size_t>(i)];next=(next+1)%history.size();}
        } else if(juce::Time::getMillisecondCounter()-lastUpdate>250)history.fill({});
        if(isVisible())repaint();
    }
    void paint(juce::Graphics& g) override {
        const auto area=getLocalBounds().toFloat();
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff182b35),0,0,juce::Colour(0xff0c171e),0,area.getHeight(),false));g.fillRect(area);
        g.setColour(juce::Colour(0xff46606d));g.drawRect(area,1);
        g.setFont(juce::FontOptions(9.f));g.setColour(juce::Colour(0xffa7bac4));
        g.drawText("OUTPUT WAVEFORM",7,1,180,12,juce::Justification::centredLeft);
        float maximum=0;
        for(const auto& f:history)maximum=std::max({maximum,std::abs(f.lowL),std::abs(f.highL),std::abs(f.lowR),std::abs(f.highR)});
        const float zoom=maximum>0.00001f?juce::jlimit(1.f,32.f,0.9f/maximum):1.f;
        g.drawText("L / R    |    DISPLAY x"+juce::String(zoom,1)+"    |    RED = CLIP",getWidth()-290,1,283,12,juce::Justification::centredRight);
        const float x0=22.f,width=area.getWidth()-29.f,row=(area.getHeight()-16.f)*0.5f;
        for(int channel=0;channel<2;++channel) {
            const float centre=14.f+row*(static_cast<float>(channel)+0.5f),height=row*0.44f;
            const auto colour=channel==0?juce::Colour(0xffe7ae4b):juce::Colour(0xff73d3e6);
            g.setColour(juce::Colour(0xff2a424e));g.drawHorizontalLine(static_cast<int>(centre),x0,x0+width);
            for(int i=1;i<8;++i)g.drawVerticalLine(static_cast<int>(x0+width*static_cast<float>(i)/8.f),centre-height,centre+height);
            g.setColour(colour);g.drawText(channel==0?"L":"R",5,static_cast<int>(centre)-6,12,12,juce::Justification::centred);
            for(size_t i=0;i<history.size();++i) {
                const auto& f=history[(next+i)%history.size()];
                const float low=channel==0?f.lowL:f.lowR,high=channel==0?f.highL:f.highR;
                const float x=x0+width*static_cast<float>(i)/static_cast<float>(history.size()-1);
                const float top=centre-juce::jlimit(-1.f,1.f,high*zoom)*height,bottom=centre-juce::jlimit(-1.f,1.f,low*zoom)*height;
                g.setColour(low< -1.f || high>1.f?juce::Colour(0xffff684d):colour);
                g.drawLine(x,top-0.35f,x,bottom+0.35f,1.15f);
            }
        }
    }
private:
    WaveformCapture& capture;
    std::array<WaveformCapture::Frame,512> history {};
    std::array<WaveformCapture::Frame,WaveformCapture::capacity> incoming {};
    size_t next=0;
    juce::uint32 lastUpdate=0;
};
