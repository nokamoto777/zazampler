#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class PerformanceWheel final : public juce::Component, public juce::SettableTooltipClient {
public:
    explicit PerformanceWheel(bool pitch) : bend(pitch) {
        setComponentID(pitch?"pitchWheel":"modWheel");setTitle(pitch?"Pitch bend":"Modulation");
        setWantsKeyboardFocus(true);setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
        setTooltip(pitch?"Pitch bend: drag; release returns to centre. Arrow keys bend; release centres. Double-click resets.":"Modulation (CC1): drag; position is retained. Arrow keys adjust. Double-click resets.");
    }
    std::function<void(int)> onChange;
    void release(){if(bend && (dragging || keyHeld)){dragging=keyHeld=false;send(8192);}}
    int midiValue() const {return value;}
    void setMidiValue(int v) {v=juce::jlimit(0,bend?16383:127,v);if(value!=v){value=v;repaint();}}
    void mouseDown(const juce::MouseEvent& e) override {grabKeyboardFocus();dragValue=value;dragY=e.position.y;dragging=true;}
    void mouseDrag(const juce::MouseEvent& e) override {send(dragValue+juce::roundToInt((dragY-e.position.y)*(bend?16383.f:127.f)/juce::jmax(1,getHeight()-20)));}
    void mouseUp(const juce::MouseEvent&) override {dragging=false;if(bend)send(8192);}
    void mouseDoubleClick(const juce::MouseEvent&) override {send(bend?8192:0);}
    void focusLost(FocusChangeType) override {release();}
    bool keyPressed(const juce::KeyPress& key) override {
        if(key==juce::KeyPress::upKey || key==juce::KeyPress::downKey){keyHeld=true;send(value+(key==juce::KeyPress::upKey?1:-1)*(bend?512:4));return true;}
        if(key==juce::KeyPress::homeKey || key==juce::KeyPress::escapeKey){send(bend?8192:0);return true;}return false;
    }
    bool keyStateChanged(bool down) override {if(bend && !down && keyHeld){keyHeld=false;send(8192);return true;}return false;}
    void paint(juce::Graphics& g) override {
        auto body=getLocalBounds().toFloat().withTrimmedBottom(17).reduced(5,1);
        g.setColour(juce::Colour(0xff111518));g.fillRoundedRectangle(body,5);
        juce::ColourGradient gradient(juce::Colour(0xff181c20),body.getX(),0,juce::Colour(0xff191d20),body.getRight(),0,false);
        gradient.addColour(.48,juce::Colour(0xff62696d));g.setGradientFill(gradient);g.fillRoundedRectangle(body.reduced(3),4);
        g.setColour(juce::Colour(0xff15191b));for(float y=body.getY()+6;y<body.getBottom()-4;y+=6)g.drawHorizontalLine(static_cast<int>(y),body.getX()+4,body.getRight()-4);
        const float position=static_cast<float>(value)/(bend?16383.f:127.f);
        const float marker=body.getBottom()-6-position*(body.getHeight()-12);
        g.setColour(juce::Colour(0xffe2ad4b));g.fillRoundedRectangle(body.getX()+3,marker-2,body.getWidth()-6,4,1);
        g.setColour(hasKeyboardFocus(true)?juce::Colour(0xffe2ad4b):juce::Colour(0xff73797c));g.drawRoundedRectangle(body,5,1);
        g.setFont(10.f);g.setColour(juce::Colour(0xffc1c7ca));g.drawText(bend?"BEND":"MOD",0,getHeight()-16,getWidth(),16,juce::Justification::centred);
    }
private:
    bool bend,dragging=false,keyHeld=false;int value=bend?8192:0,dragValue=0;float dragY=0;
    void send(int v){setMidiValue(v);if(onChange)onChange(value);}
};
