#pragma once
#include "Theme.h"
#include "FineTune.h"
namespace suitunes::ui {
class PitchTimeline final : public juce::Component {
public:
    std::shared_ptr<FineTuneSession> session;
    std::function<void()> edited,selectionChanged,viewChanged;
    std::function<void(double)> seek;
    std::function<float(float)> snapPitch;
    double start=0,span=1,playhead=0;
    bool splitTool=false,snap=true;
    void changed(bool reset=false) {
        if(reset&&session) { start=0; span=std::max(.1,session->duration()); }
        low=48; high=76;
        if(session&&!session->notes.empty()) {
            low=100;high=0; for(auto n:session->notes) { low=std::min(low,int(std::floor(std::min(n.source,n.target)))-3); high=std::max(high,int(std::ceil(std::max(n.source,n.target)))+3); }
            if(high-low<12) { high+=6;low-=6; }
        }
        dirty=true; repaint();
    }
    void resized() override { dirty=true; }
    void paint(juce::Graphics& g) override {
        if(dirty||cache.getWidth()!=getWidth()||cache.getHeight()!=getHeight()) {
            cache=juce::Image(juce::Image::ARGB,std::max(1,getWidth()),std::max(1,getHeight()),true);
            juce::Graphics cached(cache); drawTimeline(cached); dirty=false;
        }
        g.drawImageAt(cache,0,0);
        if(session&&playhead>=start&&playhead<=start+span) {
            float px=x(playhead); g.setColour(cyan); g.drawVerticalLine(int(px),28.f,float(getHeight()-8));
            juce::Path marker; marker.addTriangle(px-7,28,px+7,28,px,37); g.fillPath(marker);
        }
    }
    void mouseDown(const juce::MouseEvent& e) override {
        if(!session) return; cursor=time(e.position.x);
        if(splitTool) { session->splitAt(cursor); if(edited) edited(); changed(); return; }
        active=-1;
        if(!e.mods.isShiftDown()) for(size_t i=0;i<session->notes.size();++i) if(noteBounds(session->notes[i]).expanded(2).contains(e.position)) { active=int(i); break; }
        if(active>=0) { dragRemembered=false;dragTarget=session->notes[size_t(active)].target; dragY=e.position.y; }
        else { selectionAnchor=cursor; session->selectionStart=cursor; session->selectionEnd=cursor; if(seek) seek(cursor); changed(); }
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        if(!session) return;
        if(active>=0) {
            if(!dragRemembered){session->remember();dragRemembered=true;}
            auto& n=session->notes[size_t(active)]; float target=dragTarget-(e.position.y-dragY)/plot().getHeight()*(high-low);
            n.target=std::clamp(snap?(snapPitch?snapPitch(target):std::round(target)):target,std::max(0.f,n.source-24),std::min(127.f,n.source+24)); n.edited=true;
        } else {
            double t=time(e.position.x); session->selectionStart=std::min(t,selectionAnchor);session->selectionEnd=std::max(t,selectionAnchor);
        }
        changed();
    }
    void mouseUp(const juce::MouseEvent& e) override {
        if(!session) return;
        if(active>=0 && dragRemembered) { if(edited) edited(); }
        else if(selectionChanged) selectionChanged();
        active=-1;
    }
    void mouseDoubleClick(const juce::MouseEvent& e) override { if(session) { session->splitAt(time(e.position.x)); if(edited) edited(); changed(); } }
    void mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& wheel) override {
        if(!session) return;
        double anchor=time(e.position.x),fraction=std::clamp((e.position.x-plot().getX())/plot().getWidth(),0.f,1.f);
        span=std::clamp(span*std::pow(1.3,-wheel.deltaY*5),std::min(.25,session->duration()),std::max(.25,session->duration()));
        start=std::clamp(anchor-fraction*span,0.0,std::max(0.0,session->duration()-span)); changed();if(viewChanged)viewChanged();
    }
    double cursor=0;
private:
    juce::Rectangle<float> plot() const { return {43,29,float(std::max(1,getWidth()-53)),float(std::max(1,getHeight()-87))}; }
    float x(double t) const { return plot().getX()+float((t-start)/span)*plot().getWidth(); }
    float y(float pitch) const { return plot().getBottom()-(pitch-low)/float(std::max(1,high-low))*plot().getHeight(); }
    double time(float px) const { return std::clamp(start+double((px-plot().getX())/plot().getWidth())*span,0.0,session?session->duration():0); }
    juce::Rectangle<float> noteBounds(NoteRegion n) const { float row=plot().getHeight()/float(high-low); return {x(n.start),y(n.edited?n.target:n.source)-row*.42f,std::max(3.f,x(n.end)-x(n.start)),std::max(8.f,row*.85f)}; }
    void drawTimeline(juce::Graphics& g) {
        g.fillAll(ink); auto p=plot();
        if(!session) {
            label(g,"IMPORT A VOCAL WAV",getLocalBounds().withTrimmedBottom(12),22,text);
            label(g,"Select a range, then use Auto Fine Tune",getLocalBounds().translated(0,30),13,muted); return;
        }
        juce::Graphics::ScopedSaveState saved(g); g.reduceClipRegion(getLocalBounds());
        for(int n=low;n<=high;++n) {
            int yy=int(y(float(n))); g.setColour(juce::Colour(0xff20303e).withAlpha(n%12==0?.9f:.45f));g.drawHorizontalLine(yy,p.getX(),p.getRight());
            if((high-low<18||n%2==0)&&yy>=p.getY()) label(g,noteName(float(n)),{0,yy-8,36,16},11,muted,juce::Justification::centredRight);
        }
        for(int i=0;i<=8;++i) {
            double t=start+span*i/8; float xx=x(t); g.setColour(juce::Colour(0xff1b2b38)); g.drawVerticalLine(int(xx),p.getY(),float(getHeight()-7));
            label(g,(span<=20?juce::String(t,1):juce::String(int(std::round(t))))+"s",{std::clamp(int(xx)-25,0,std::max(0,getWidth()-50)),2,50,22},11,muted);
        }
        if(session->selectionEnd>session->selectionStart) {
            g.setColour(blue.withAlpha(.10f));g.fillRect(juce::Rectangle<float>(x(session->selectionStart),p.getY(),x(session->selectionEnd)-x(session->selectionStart),float(getHeight())-p.getY()));
        }
        for(auto n:session->notes) {
            if(n.end<start||n.start>start+span) continue;
            auto r=noteBounds(n); g.setGradientFill(juce::ColourGradient(blue.withAlpha(n.edited?.7f:.35f),0,r.getY(),juce::Colour(0xff064073).withAlpha(.7f),0,r.getBottom(),false));
            g.fillRoundedRectangle(r,5); g.setColour(n.edited?cyan:blue);g.drawRoundedRectangle(r,5,1);
        }
        juce::Path trace; bool drawing=false;float previousX=-100;
        for(auto f:session->frames) {
            if(f.time<start)continue;if(f.time>start+span)break;
            if(f.midi<=0||f.confidence<.85f) { drawing=false;continue; }
            float xx=x(f.time), yy=y(f.midi);
            if(xx-previousX<.65f&&drawing)continue;
            if(!drawing) { trace.startNewSubPath(xx,yy); drawing=true; } else trace.lineTo(xx,yy); previousX=xx;
        }
        g.setColour(blue.withAlpha(.22f)); g.strokePath(trace,juce::PathStrokeType(4));g.setColour(juce::Colour(0xffd8f6ff));g.strokePath(trace,juce::PathStrokeType(1.3f));
        float base=float(getHeight()-29);
        for(int px=int(p.getX());px<int(p.getRight());++px) {
            double t=start+(px-p.getX())/p.getWidth()*span;
            size_t index=size_t(std::max(0.0,t*session->rate/256));
            float peak=index<session->peaks.size()?session->peaks[index]:0;
            float height=std::min(22.f,peak*64);g.setColour(blue.withAlpha(.8f));g.drawVerticalLine(px,base-height,base+height);
        }
    }
    bool dirty=true,dragRemembered=false;
    juce::Image cache;
    int low=48,high=76,active=-1;
    float dragTarget=0,dragY=0;
    double selectionAnchor=0;
};
}
