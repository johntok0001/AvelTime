#pragma once

#include "PluginProcessor.h"
#include "UI/Theme.h"
#include "UI/Views.h"

namespace avel::time::ui
{
    /** Floating glass card; the shadow margin is part of the bounds, so it is never clipped. */
    class GlassPanel : public juce::Component
    {
    public:
        static constexpr int margin = 36;
        GlassPanel() { setInterceptsMouseClicks (false, false); }
        void setGlassBounds (juce::Rectangle<int> r) { setBounds (r.expanded (margin)); }
        juce::Rectangle<int> glassBounds() const { return getBounds().reduced (margin); }
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced ((float) margin);
            drawGlass (g, r, radius);
            if (title.isNotEmpty())
            {
                g.setFont (font (12.5f, Weight::semibold));
                g.setColour (titleColour);
                g.drawText (title, r.reduced (16.0f, 0).withHeight (34.0f).translated (0, 4.0f), juce::Justification::centredLeft, false);
            }
        }
        juce::String title;
        juce::Colour titleColour = col::label;
        float radius = 18.0f;
    };

    class SmallText : public juce::Component
    {
    public:
        SmallText() { setInterceptsMouseClicks (false, false); }
        void setText (const juce::String& t, juce::Colour c = col::label3) { if (t != text || c != colour) { text = t; colour = c; repaint(); } }
        void paint (juce::Graphics& g) override
        {
            g.setFont (font (fontSize, Weight::medium));
            g.setColour (colour);
            g.drawText (text, getLocalBounds(), just, false);
        }
        juce::String text;
        juce::Colour colour = col::label3;
        float fontSize = 11.0f;
        juce::Justification just = juce::Justification::centred;
    };

    class PresetPill : public juce::Component, private juce::Timer
    {
    public:
        PresetPill() { startTimerHz (60); }
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (0.5f);
            g.setColour (col::fill.withAlpha (0.35f + 0.35f * hover));
            g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
            g.setColour (col::stroke);
            g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
            g.setFont (font (12.5f, Weight::semibold));
            g.setColour (col::label);
            g.drawText (name, r.withTrimmedLeft (14.0f).withTrimmedRight (26.0f), juce::Justification::centredLeft, true);
            icon::draw (g, icon::chevronDown(), r.removeFromRight (26.0f).withSizeKeepingCentre (14.0f, 14.0f).translated (-4.0f, 0), col::label2, 1.6f);
        }
        void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }
        juce::String name;
        std::function<void()> onClick;
    private:
        void timerCallback() override { if (approach (hover, isMouseOver() ? 1.0f : 0.0f, 0.3f)) repaint(); }
        float hover = 0.0f;
    };
}

class AvelTimeEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AvelTimeEditor (AvelTimeProcessor&);
    ~AvelTimeEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void setView (int v);
    int getView() const { return view; }
    void selectBand (int b);

private:
    void timerCallback() override;
    void layoutDelayView (juce::Rectangle<int> display);
    void layoutFilterView (juce::Rectangle<int> display);
    void layoutModView (juce::Rectangle<int> display);
    void bindTimeDials();
    void showPresetMenu();
    void savePresetDialog();
    void tapTempo();
    void updateDynamic();

    AvelTimeProcessor& proc;
    avel::ui::LookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    int view = 0;
    juce::Rectangle<int> displayR;

    // top bar
    avel::time::ui::PresetPill presetPill;
    avel::ui::IconButton presetPrev { avel::ui::icon::chevronLeft() }, presetNext { avel::ui::icon::chevronRight() };
    avel::ui::IconButton undoBtn { avel::ui::icon::undo() }, redoBtn { avel::ui::icon::redo() };
    avel::ui::Segmented viewSeg { { "Delay", "Filters", "Modulation" } };
    avel::ui::IconButton freezeBtn { "Freeze" };
    avel::ui::IconButton abA { "A" }, abB { "B" }, abCopy { avel::ui::icon::copy() };

    // delay view
    avel::time::ui::TapView tapView;
    avel::time::ui::GlassPanel delayCard;
    avel::ui::Switch syncSw { "Sync", avel::time::ui::product::accentA }, linkSw { "Link L/R", avel::time::ui::product::accentA };
    avel::ui::IconButton tapBtn { "Tap" };
    avel::ui::Segmented routingSeg, timeModeSeg;
    avel::ui::Dial timeL { "Time L", avel::time::ui::product::left }, timeR { "Time R", avel::time::ui::product::right };
    avel::ui::Dial feedback { "Feedback", avel::time::ui::product::accentA }, cross { "Cross", avel::time::ui::product::accentA };
    avel::ui::Dial width { "Width", avel::time::ui::product::accentB }, pan { "Pan", avel::time::ui::product::accentB };
    avel::ui::Dial drive { "Drive", avel::ui::col::band (1) }, diffusion { "Diffusion", avel::ui::col::band (3) };
    avel::ui::Dial pitch { "Pitch", avel::ui::col::band (4) }, duck { "Ducking", avel::ui::col::band (6) };
    avel::time::ui::ChoicePill driveStyle;
    avel::time::ui::MiniValue diffSize { "Size" }, duckRelease { "Rel" };
    avel::time::ui::SmallText timeAltL, timeAltR, tempoText;
    int boundSync = -1;
    bool showOverlayCaptions = true;

    // filter view
    avel::time::ui::FilterView filterView;
    avel::time::ui::GlassPanel filterCard;
    std::array<avel::ui::IconButton, avel::time::numFilters> bandBtns;
    avel::ui::Switch bandOn { "On" };
    avel::time::ui::ChoicePill bandType;
    avel::ui::Dial bandFreq { "Freq" }, bandGain { "Gain" }, bandQ { "Q" };
    int band = 0;

    // modulation view
    std::array<avel::time::ui::GlassPanel, 3> srcCards;
    std::array<std::unique_ptr<avel::time::ui::LfoPreview>, avel::time::numLfos> lfoPreview;
    avel::time::ui::EnvPreview envPreview;
    std::array<avel::ui::Dial, avel::time::numLfos> lfoRate, lfoPhase;
    std::array<avel::time::ui::ChoicePill, avel::time::numLfos> lfoShape, lfoDiv;
    std::array<std::unique_ptr<avel::ui::Switch>, avel::time::numLfos> lfoSync;
    avel::ui::Dial envAttack { "Attack", avel::ui::col::band (1) }, envRelease { "Release", avel::ui::col::band (1) }, envSens { "Sensitivity", avel::ui::col::band (1) };
    std::array<avel::time::ui::GlassPanel, avel::time::numSlots> slotCards;
    std::array<avel::time::ui::ChoicePill, avel::time::numSlots> slotSrc, slotDst;
    std::array<avel::ui::Dial, avel::time::numSlots> slotAmt;

    // right column
    avel::time::ui::Meter inMeter, outMeter;
    avel::ui::Dial inputDial { "Input" }, mixDial { "Mix", avel::time::ui::product::accentA }, outputDial { "Output" };

    std::vector<double> tapTimes;
    juce::ComponentBoundsConstrainer constrainer;
    std::unique_ptr<juce::AlertWindow> alert;
    bool restoring = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AvelTimeEditor)
};
