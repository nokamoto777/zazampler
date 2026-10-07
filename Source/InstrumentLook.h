#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
class InstrumentLook final : public juce::LookAndFeel_V4 {
public:
    InstrumentLook() {
        setColour(juce::TextButton::buttonColourId,juce::Colour(0xff96bad3));
        setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xffbfdcef));
        setColour(juce::TextButton::textColourOffId,juce::Colour(0xff203746));
        setColour(juce::TextButton::textColourOnId,juce::Colour(0xff152c3b));
        setColour(juce::ComboBox::backgroundColourId,juce::Colour(0xffafd2e8));
        setColour(juce::ComboBox::textColourId,juce::Colour(0xff193443));
        setColour(juce::ComboBox::outlineColourId,juce::Colour(0xff64859c));
        setColour(juce::ComboBox::arrowColourId,juce::Colour(0xff193443));
        setColour(juce::PopupMenu::backgroundColourId,juce::Colour(0xff292929));
        setColour(juce::PopupMenu::highlightedBackgroundColourId,juce::Colour(0xff765924));
        setColour(juce::ToggleButton::textColourId,juce::Colour(0xff173747));
        setColour(juce::ToggleButton::tickColourId,juce::Colour(0xffb68126));
    }
    juce::Font getTextButtonFont(juce::TextButton&,int) override {return juce::Font(juce::FontOptions(11.f,juce::Font::bold));}
    juce::Font getComboBoxFont(juce::ComboBox&) override {return juce::Font(juce::FontOptions(12.f));}
    void drawButtonBackground(juce::Graphics& g,juce::Button& b,const juce::Colour& colour,bool over,bool down) override {
        const auto r=b.getLocalBounds().toFloat().reduced(0.5f);
        auto base=colour;if(over)base=base.brighter(0.1f);if(down)base=base.darker(0.1f);if(!b.isEnabled())base=base.withAlpha(0.4f);
        g.setGradientFill(juce::ColourGradient(base.brighter(0.08f),0,0,base.darker(0.08f),0,r.getHeight(),false));g.fillRect(r);
        g.setColour(juce::Colour(0xff617f92));g.drawRect(r,0.7f);
        if(b.getToggleState()){g.setColour(juce::Colour(0xffe1ab4e));g.fillRect(r.getX()+2,r.getBottom()-3,r.getWidth()-4,2.f);}
    }
    void drawComboBox(juce::Graphics& g,int w,int h,bool,int,int,int,int,juce::ComboBox& box) override {
        auto r=juce::Rectangle<float>(0.5f,0.5f,static_cast<float>(w)-1,static_cast<float>(h)-1);
        const auto base=box.findColour(juce::ComboBox::backgroundColourId);
        g.setGradientFill(juce::ColourGradient(base.brighter(0.04f),0,0,base.darker(0.04f),0,static_cast<float>(h),false));g.fillRect(r);
        g.setColour(box.findColour(juce::ComboBox::outlineColourId));g.drawRect(r,0.8f);
        juce::Path arrow;const float x=static_cast<float>(w)-12, y=static_cast<float>(h)*0.5f;
        arrow.addTriangle(x-3,y-2,x+3,y-2,x,y+2);g.setColour(box.findColour(juce::ComboBox::arrowColourId));g.fillPath(arrow);
    }
    void drawToggleButton(juce::Graphics& g,juce::ToggleButton& b,bool,bool) override {
        const auto ink=b.findColour(juce::ToggleButton::textColourId).withMultipliedAlpha(b.isEnabled()?1.f:0.45f);
        const float y=static_cast<float>(b.getHeight())*0.5f-4;
        g.setColour(ink);g.drawRect(2.f,y,8.f,8.f,0.7f);
        if(b.getToggleState()){g.setColour(juce::Colour(0xffbd8d38));g.fillRect(3.f,y+1,6.f,6.f);}
        g.setColour(ink);g.setFont(juce::FontOptions(11.f));g.drawText(b.getButtonText(),16,0,b.getWidth()-16,b.getHeight(),juce::Justification::centredLeft);
    }
    void drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float position,float start,float end,juce::Slider& s) override {
        const float radius=juce::jmin(23.f,static_cast<float>(juce::jmin(w,h))*0.5f-4.f);
        const float cx=static_cast<float>(x+w/2),cy=static_cast<float>(y+h/2);
        const bool enabled=s.isEnabled();
        const auto amber=enabled?juce::Colour(0xffe7a83f):juce::Colour(0xff555555);
        g.setColour(juce::Colours::black.withAlpha(0.55f));g.fillEllipse(cx-radius-1,cy-radius+1,2*radius+2,2*radius+3);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff777777),cx-radius,cy-radius,juce::Colour(0xff202020),cx+radius,cy+radius,false));
        g.fillEllipse(cx-radius,cy-radius,2*radius,2*radius);
        const float r=radius-3.f;
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff434343),cx,cy-r,juce::Colour(0xff272727),cx,cy+r,false));
        g.fillEllipse(cx-r,cy-r,2*r,2*r);g.setColour(juce::Colour(0xff131313));g.drawEllipse(cx-r,cy-r,2*r,2*r,1.f);
        juce::Path arc;arc.addCentredArc(cx,cy,radius,radius,0,start,start+position*(end-start),true);
        g.setColour(amber);g.strokePath(arc,juce::PathStrokeType(1.8f));
        const float angle=start+position*(end-start);
        g.setColour(enabled?juce::Colour(0xffc9c9c9):juce::Colour(0xff626262));
        g.drawLine(cx+std::sin(angle)*r*0.47f,cy-std::cos(angle)*r*0.47f,cx+std::sin(angle)*r*0.93f,cy-std::cos(angle)*r*0.93f,1.5f);
    }
};
