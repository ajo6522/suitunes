#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
namespace suitunes::ui {
inline const juce::Colour blue{0xff009dff},cyan{0xff37ddff},ink{0xff090d12},panel{0xff141b23},text{0xffedf4fb},muted{0xff8a9caf};
inline void label(juce::Graphics& g,const juce::String& s,juce::Rectangle<int> bounds,float size,juce::Colour colour=text,int align=juce::Justification::centred) {
    g.setColour(colour); g.setFont(juce::Font(juce::FontOptions(size))); g.drawText(s,bounds,align,true);
}
inline void card(juce::Graphics& g,juce::Rectangle<float> r,bool glow=false,float radius=16) {
    if(glow) for(int i=5;i>0;--i) { g.setColour(blue.withAlpha(.018f*float(6-i))); g.drawRoundedRectangle(r.expanded(float(i)),radius+float(i),2); }
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff202933),r.getX(),r.getY(),juce::Colour(0xff080d12),r.getRight(),r.getBottom(),false));
    g.fillRoundedRectangle(r,radius); g.setColour(glow?blue:juce::Colour(0xff475562)); g.drawRoundedRectangle(r,radius,1.2f);
    g.setColour(juce::Colours::white.withAlpha(.07f)); g.drawHorizontalLine(int(r.getY()+1),r.getX()+radius,r.getRight()-radius);
}
class Theme final : public juce::LookAndFeel_V4 {
public:
    Theme() {
        setColour(juce::ComboBox::backgroundColourId,ink); setColour(juce::ComboBox::outlineColourId,juce::Colour(0xff52606d));
        setColour(juce::ComboBox::textColourId,text); setColour(juce::ComboBox::arrowColourId,text);
        setColour(juce::PopupMenu::backgroundColourId,juce::Colour(0xff111b26)); setColour(juce::PopupMenu::highlightedBackgroundColourId,blue.darker(.45f));
        setColour(juce::Slider::textBoxTextColourId,text); setColour(juce::Slider::textBoxBackgroundColourId,juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
        setColour(juce::TextButton::textColourOffId,text); setColour(juce::TextButton::textColourOnId,juce::Colours::white);
    }
    void drawButtonBackground(juce::Graphics& g,juce::Button& b,const juce::Colour&,bool over,bool down) override {
        auto r=b.getLocalBounds().toFloat().reduced(2); bool on=b.getToggleState();
        if(b.getButtonText()=="Auto Tune"||b.getButtonText()=="Fine Tune") {
            juce::Path tab;tab.startNewSubPath(r.getX()+20,r.getY());tab.lineTo(r.getRight(),r.getY());tab.lineTo(r.getRight()-20,r.getBottom());tab.lineTo(r.getX(),r.getBottom());tab.closeSubPath();
            if(on){g.setColour(blue.withAlpha(.15f));g.strokePath(tab,juce::PathStrokeType(8));}
            g.setGradientFill(juce::ColourGradient(on?juce::Colour(0xff086dbf):juce::Colour(0xff29333d),0,r.getY(),on?juce::Colour(0xff003665):ink,0,r.getBottom(),false));g.fillPath(tab);g.setColour(on?cyan:juce::Colour(0xff65717e));g.strokePath(tab,juce::PathStrokeType(on?1.6f:1.f));return;
        }
        if(on&&b.isEnabled()) for(int i=4;i>0;--i) { g.setColour(blue.withAlpha(.035f*float(5-i))); g.drawRoundedRectangle(r.expanded(float(i)),12,2); }
        auto top=on?juce::Colour(0xff0865b7):juce::Colour(0xff29333d), bottom=on?juce::Colour(0xff003467):juce::Colour(0xff0a0f14);
        if(over) top=top.brighter(.15f); if(down) bottom=bottom.brighter(.15f);
        g.setGradientFill(juce::ColourGradient(top.withAlpha(b.isEnabled()?1.f:.4f),0,r.getY(),bottom,r.getRight(),r.getBottom(),false));
        g.fillRoundedRectangle(r,11); g.setColour((on?cyan:juce::Colour(0xff65717e)).withAlpha(b.isEnabled()?.9f:.3f)); g.drawRoundedRectangle(r,11,on?1.6f:1.f);
    }
    juce::Font getTextButtonFont(juce::TextButton&,int height) override { return juce::Font(juce::FontOptions(std::min(16.f,height*.43f))); }
    void drawComboBox(juce::Graphics& g,int w,int h,bool,int,int,int,int,juce::ComboBox& box) override {
        card(g,{.5f,.5f,float(w-1),float(h-1)},false,float(h)*.45f);
        g.setColour(box.isEnabled()?text:muted.withAlpha(.5f)); juce::Path p;
        p.startNewSubPath(float(w-25),h*.42f); p.lineTo(float(w-19),h*.58f); p.lineTo(float(w-13),h*.42f); g.strokePath(p,juce::PathStrokeType(2));
    }
    juce::Font getComboBoxFont(juce::ComboBox& b) override { return juce::Font(juce::FontOptions(std::min(17.f,b.getHeight()*.48f))); }
    void positionComboBoxText(juce::ComboBox& box,juce::Label& l) override { l.setBounds(14,1,box.getWidth()-45,box.getHeight()-2); l.setFont(getComboBoxFont(box)); }
    juce::Label* createSliderTextBox(juce::Slider& slider) override {
        auto* box=juce::LookAndFeel_V4::createSliderTextBox(slider);
        box->setColour(juce::Label::textColourId,text);box->setColour(juce::Label::backgroundColourId,juce::Colours::transparentBlack);
        box->setColour(juce::Label::outlineColourId,juce::Colours::transparentBlack);box->setColour(juce::Label::outlineWhenEditingColourId,blue);
        box->setFont(juce::Font(juce::FontOptions(16)));return box;
    }
    void drawLabel(juce::Graphics& g,juce::Label& box) override {
        if(box.isBeingEdited())return;
        g.setColour(box.findColour(juce::Label::textColourId).withMultipliedAlpha(box.isEnabled()?1.f:.45f));
        g.setFont(box.getFont());g.drawFittedText(box.getText(),box.getLocalBounds().reduced(2),box.getJustificationType(),1);
    }
    void drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float pos,float start,float end,juce::Slider& slider) override {
        float diameter=std::min(float(w),float(h))-16, radius=diameter*.5f, cx=x+w*.5f,cy=y+h*.5f;
        auto r=juce::Rectangle<float>(cx-radius,cy-radius,diameter,diameter);
        bool enabled=slider.isEnabled(); auto accent=enabled?blue:juce::Colour(0xff52616d);
        juce::Path arc; arc.addCentredArc(cx,cy,radius-2,radius-2,0,start,end,true);
        g.setColour(juce::Colour(0xff080c11)); g.strokePath(arc,juce::PathStrokeType(9));
        juce::Path value; float angle=start+pos*(end-start); value.addCentredArc(cx,cy,radius-2,radius-2,0,start,angle,true);
        if(enabled) { g.setColour(blue.withAlpha(.13f)); g.strokePath(value,juce::PathStrokeType(14)); }
        g.setColour(accent); g.strokePath(value,juce::PathStrokeType(5));
        g.setColour(enabled?cyan:muted); g.strokePath(value,juce::PathStrokeType(1.5f));
        auto face=r.reduced(11); g.setColour(juce::Colours::black.withAlpha(.7f)); g.fillEllipse(face.translated(2,4));
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff59616a),face.getX(),face.getY(),juce::Colour(0xff11161c),face.getRight(),face.getBottom(),false)); g.fillEllipse(face);
        g.setColour(juce::Colour(0xff091019)); g.drawEllipse(face,2);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff11151a),cx-radius*.3f,cy-radius*.3f,juce::Colour(0xff2d353e),cx+radius*.5f,cy+radius*.6f,false)); g.fillEllipse(face.reduced(4));
        g.setColour(juce::Colours::white.withAlpha(.1f)); g.drawEllipse(face.reduced(5),.7f);
        juce::Path tick; tick.addRoundedRectangle(-1.5f,-radius+22,3,diameter*.18f,1.5f);
        g.setColour(enabled?cyan:text.withAlpha(.4f)); g.fillPath(tick,juce::AffineTransform::rotation(angle).translated(cx,cy));
        for(int i=0;i<=10;++i) {
            float a=start+(end-start)*i/10.f, inner=radius+4,outer=radius+8;
            g.setColour(i<=int(pos*10)&&enabled?blue.withAlpha(.8f):muted.withAlpha(.22f));
            g.drawLine(cx+std::sin(a)*inner,cy-std::cos(a)*inner,cx+std::sin(a)*outer,cy-std::cos(a)*outer,1);
        }
    }
};
class Knob final : public juce::Component {
public:
    explicit Knob(juce::String title):name(std::move(title)) {
        addAndMakeVisible(slider); slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setRotaryParameters(juce::MathConstants<float>::pi*1.25f,juce::MathConstants<float>::pi*2.75f,true);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,100,23); slider.setMouseDragSensitivity(220);
    }
    void paint(juce::Graphics& g) override { label(g,name,{0,0,getWidth(),24},getWidth()>150?17.f:13.f,isEnabled()?text:muted); }
    void resized() override { slider.setBounds(8,24,getWidth()-16,getHeight()-26); }
    void title(juce::String title) { name=std::move(title); repaint(); }
    void percent() { slider.textFromValueFunction=[](double v){return juce::String(v*100,0)+"%";}; slider.valueFromTextFunction=[](juce::String s){return s.getDoubleValue()/100;}; }
    juce::Slider slider;
private: juce::String name;
};
inline juce::String noteName(float midi) {
    if(midi<=0||!std::isfinite(midi)) return "--";
    static const char* names[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    int n=int(std::round(midi)); return juce::String(names[(n%12+12)%12])+juce::String(n/12-1);
}
}
