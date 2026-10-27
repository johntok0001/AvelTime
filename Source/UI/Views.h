#pragma once

#include "Theme.h"
#include "../PluginProcessor.h"
#include <juce_dsp/juce_dsp.h>

namespace avel::time::ui
{
    using namespace avel::ui;

    namespace product
    {
        const juce::Colour accentA { 0xff30d158 };   // mint green
        const juce::Colour accentB { 0xff64d2ff };   // cyan
        const juce::Colour left    { 0xff30d158 };
        const juce::Colour right   { 0xff64d2ff };
    }

    //==============================================================================================
    /** Pill showing a choice parameter's value; click opens a menu. */
    class ChoicePill : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
    {
    public:
        ChoicePill() { startTimerHz (60); }

        void bind (juce::RangedAudioParameter& p, juce::UndoManager* um, juce::StringArray displayNames = {})
        {
            param = &p; undo = um;
            names = displayNames.isEmpty() ? (dynamic_cast<juce::AudioParameterChoice*> (&p) != nullptr ? dynamic_cast<juce::AudioParameterChoice*> (&p)->choices : juce::StringArray()) : displayNames;
            attachment = std::make_unique<juce::ParameterAttachment> (p, [this] (float v) { index = juce::roundToInt (v); repaint(); if (onValue) onValue (index); }, um);
            attachment->sendInitialUpdate();
        }

        void setSections (std::vector<std::pair<int, juce::String>> s) { sections = std::move (s); }
        int getIndex() const { return index; }
        juce::String getText() const { return names[index]; }
        int idealWidth (float size = 12.0f) const
        {
            float w = 0;
            for (auto& n : names) w = juce::jmax (w, textWidth (font (size, Weight::semibold), n));
            return (int) w + 34;
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (0.5f);
            g.setColour (col::fill.withAlpha (0.45f + 0.35f * hover));
            g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
            g.setColour (col::stroke);
            g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
            g.setFont (font (fontSize, Weight::semibold));
            g.setColour (dimmed ? col::label3 : col::label);
            g.drawText (names[index], r.withTrimmedLeft (10.0f).withTrimmedRight (20.0f), juce::Justification::centredLeft, true);
            icon::draw (g, icon::chevronDown(), r.removeFromRight (22.0f).withSizeKeepingCentre (14.0f, 14.0f).translated (-3.0f, 0), col::label2, 1.6f);
        }

        void mouseDown (const juce::MouseEvent&) override
        {
            if (param == nullptr) return;
            juce::PopupMenu m;
            size_t sec = 0;
            for (int i = 0; i < names.size(); ++i)
            {
                while (sec < sections.size() && sections[sec].first == i)
                {
                    if (i > 0) m.addSeparator();
                    m.addSectionHeader (sections[sec].second);
                    ++sec;
                }
                m.addItem (i + 1, names[i], true, i == index);
            }
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (getWidth()),
                             [sp = juce::Component::SafePointer<ChoicePill> (this)] (int r)
                             {
                                 if (sp == nullptr || r <= 0) return;
                                 if (sp->undo != nullptr) sp->undo->beginNewTransaction();
                                 sp->attachment->setValueAsCompleteGesture ((float) (r - 1));
                             });
        }

        float fontSize = 12.0f;
        bool dimmed = false;
        std::function<void (int)> onValue;

    private:
        void timerCallback() override { if (approach (hover, isMouseOver() ? 1.0f : 0.0f, 0.3f)) repaint(); }
        juce::RangedAudioParameter* param = nullptr;
        juce::UndoManager* undo = nullptr;
        std::unique_ptr<juce::ParameterAttachment> attachment;
        juce::StringArray names;
        std::vector<std::pair<int, juce::String>> sections;
        int index = 0;
        float hover = 0.0f;
    };

    //==============================================================================================
    /** Compact "CAPTION value" field, drag vertically to change. */
    class MiniValue : public juce::Slider, private juce::Timer
    {
    public:
        MiniValue (const juce::String& cap) : captionText (cap.toUpperCase())
        {
            setSliderStyle (juce::Slider::LinearBarVertical);
            setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            setMouseDragSensitivity (160);
            setVelocityBasedMode (false);
            startTimerHz (60);
        }
        void bind (juce::RangedAudioParameter& p, juce::UndoManager* um)
        {
            undo = um;
            attachment = std::make_unique<juce::SliderParameterAttachment> (p, *this, um);
            setDoubleClickReturnValue (true, p.convertFrom0to1 (p.getDefaultValue()));
        }
        int idealWidth() const
        {
            return (int) (textWidth (caption(), captionText) + textWidth (font (11.5f, Weight::semibold), "0000 ms") + 26);
        }
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (0.5f);
            g.setColour (col::fill.withAlpha (0.35f + 0.35f * hover));
            g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
            g.setFont (caption());
            g.setColour (col::label3);
            auto t = r.reduced (10.0f, 0);
            g.drawText (captionText, t, juce::Justification::centredLeft, false);
            g.setFont (font (11.5f, Weight::semibold));
            g.setColour (dimmed ? col::label.withAlpha (0.45f) : col::label);
            g.drawText (getTextFromValue (getValue()), t, juce::Justification::centredRight, false);
        }
        void mouseDown (const juce::MouseEvent& e) override { if (undo) undo->beginNewTransaction(); juce::Slider::mouseDown (e); }
        bool dimmed = false;
    private:
        void timerCallback() override { if (approach (hover, isMouseOverOrDragging() ? 1.0f : 0.0f, 0.3f)) repaint(); }
        juce::String captionText;
        juce::UndoManager* undo = nullptr;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
        float hover = 0.0f;
    };

    //==============================================================================================
    /** Echo timeline: L taps above the centre line, R taps below. Drag the first tap to set the time. */
    class TapView : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit TapView (AvelTimeProcessor& p) : proc (p) {}

        void setGraphInsets (float top, float bottom) { insetTop = top; insetBottom = bottom; }
        void tick();
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override { hoverHandle = -1; repaint(); }

        struct Tap { float t; float gain; int ch; int n; };

    private:
        void computeTaps();
        juce::Rectangle<float> graph() const;
        float xFor (float tMs) const;
        float tFor (float x) const;
        juce::Point<float> handlePos (int ch) const;

        AvelTimeProcessor& proc;
        std::vector<Tap> taps;
        float spanMs = 3000.0f, spanTarget = 3000.0f;
        float insetTop = 52.0f, insetBottom = 190.0f;
        int hoverHandle = -1, dragHandle = -1;
        float handleAnim[2] {};
        int64_t lastOnsetSeen = -1;
        float liveMs = -1.0f;
    };

    //==============================================================================================
    /** Feedback-filter graph with nodes and analyzer. */
    class FilterView : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit FilterView (AvelTimeProcessor& p);
        void setGraphInsets (float top, float bottom) { insetTop = top; insetBottom = bottom; }
        void tick();
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

        int selected = 0;
        std::function<void (int)> onSelect;

    private:
        juce::Rectangle<float> graph() const;
        float xForF (float f) const;
        float fForX (float x) const;
        float yForDb (float db) const;
        float dbForY (float y) const;
        juce::Point<float> nodePos (int i) const;
        bool hasGain (int i) const;
        float bandMag (int i, float f) const;
        void setParam (const juce::String& id, float v);
        void showBandMenu (int i);

        AvelTimeProcessor& proc;
        juce::dsp::FFT fft { 11 };
        std::vector<float> fftBuf, window;
        std::vector<float> specIn, specWet;
        float insetTop = 12.0f, insetBottom = 150.0f;
        int hover = -1, drag = -1;
        float nodeAnim[numFilters] {};
        juce::Point<float> dragStart;
        float dragStartQ = 1.0f;
    };

    //==============================================================================================
    /** Live LFO waveform preview. */
    class LfoPreview : public juce::Component
    {
    public:
        LfoPreview (AvelTimeProcessor& p, int index) : proc (p), idx (index) {}
        void paint (juce::Graphics&) override;
    private:
        AvelTimeProcessor& proc;
        int idx;
    };

    /** Scrolling envelope-follower trace. */
    class EnvPreview : public juce::Component
    {
    public:
        explicit EnvPreview (AvelTimeProcessor& p) : proc (p) { hist.assign (160, 0.0f); }
        void tick() { hist.erase (hist.begin()); hist.push_back (proc.engine.envValue.load()); repaint(); }
        void paint (juce::Graphics&) override;
    private:
        AvelTimeProcessor& proc;
        std::vector<float> hist;
    };

    //==============================================================================================
    class Meter : public juce::Component
    {
    public:
        void set (float l, float r)
        {
            for (int i = 0; i < 2; ++i)
            {
                const float v = i == 0 ? l : r;
                const float db = juce::Decibels::gainToDecibels (v, -70.0f);
                level[i] = db > level[i] ? db : level[i] - 1.1f;
                if (db > peak[i]) { peak[i] = db; holdFrames[i] = 50; }
                else if (--holdFrames[i] < 0) peak[i] -= 0.6f;
            }
            repaint();
        }
        void paint (juce::Graphics& g) override;
        juce::String captionText;
    private:
        float level[2] { -70.0f, -70.0f }, peak[2] { -70.0f, -70.0f };
        int holdFrames[2] {};
    };
}
