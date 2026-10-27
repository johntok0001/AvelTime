#pragma once

// Avel design system: shared look for every Avel product.
// Colours, fonts, glass panels, icons, LookAndFeel and the standard controls
// (Dial, Segmented, Switch, IconButton). Keep in sync across products.

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "BinaryData.h"

namespace avel::ui
{
    namespace col
    {
        const juce::Colour bg0       { 0xff0b0b0d };
        const juce::Colour bg1       { 0xff151518 };
        const juce::Colour bg2       { 0xff1c1c1f };
        const juce::Colour glass     { 0xe6202023 };
        const juce::Colour glassHi   { 0xff2c2c2f };
        const juce::Colour stroke    { 0x17ffffff };
        const juce::Colour strokeHi  { 0x26ffffff };
        const juce::Colour fill      { 0x33767680 };
        const juce::Colour fillHi    { 0x59767680 };
        const juce::Colour thumb     { 0xff636366 };
        const juce::Colour label     { 0xfff5f5f7 };
        const juce::Colour label2    { 0x99ebebf5 };
        const juce::Colour label3    { 0x4debebf5 };
        const juce::Colour label4    { 0x29ebebf5 };
        const juce::Colour accent    { 0xff0a84ff };
        const juce::Colour green     { 0xff30d158 };
        const juce::Colour red       { 0xffff453a };
        const juce::Colour yellow    { 0xffffd60a };
        const juce::Colour pill      { 0xf0232326 };

        inline juce::Colour band (int index)
        {
            static const juce::Colour c[] {
                juce::Colour (0xff0a84ff), juce::Colour (0xffff9f0a), juce::Colour (0xff30d158), juce::Colour (0xffbf5af2),
                juce::Colour (0xffff375f), juce::Colour (0xff64d2ff), juce::Colour (0xffffd60a), juce::Colour (0xff5e5ce6),
                juce::Colour (0xff66d4cf), juce::Colour (0xffff6961), juce::Colour (0xffac8e68), juce::Colour (0xffda8fff)
            };
            return c[(size_t) ((index % 12) + 12) % 12];
        }
    }

    enum class Weight { regular, medium, semibold };

    inline juce::Typeface::Ptr typeface (Weight w)
    {
        static juce::Typeface::Ptr r = juce::Typeface::createSystemTypefaceFor (BinaryData::InterRegular_otf, BinaryData::InterRegular_otfSize);
        static juce::Typeface::Ptr m = juce::Typeface::createSystemTypefaceFor (BinaryData::InterMedium_otf, BinaryData::InterMedium_otfSize);
        static juce::Typeface::Ptr s = juce::Typeface::createSystemTypefaceFor (BinaryData::InterSemiBold_otf, BinaryData::InterSemiBold_otfSize);
        return w == Weight::regular ? r : w == Weight::medium ? m : s;
    }

    inline juce::Font font (float size, Weight w = Weight::medium, float kerning = 0.0f)
    {
        return juce::Font (juce::FontOptions (typeface (w)).withHeight (size).withKerningFactor (kerning));
    }

    inline juce::Font caption() { return font (10.0f, Weight::semibold, 0.06f); }

    inline float textWidth (const juce::Font& f, const juce::String& s)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (f, s, 0.0f, 0.0f);
        return ga.getBoundingBox (0, -1, true).getWidth();
    }

    inline void drawGlass (juce::Graphics& g, juce::Rectangle<float> r, float radius, float shadow = 1.0f)
    {
        if (shadow > 0.0f)
        {
            juce::Path p;
            p.addRoundedRectangle (r, radius);
            juce::DropShadow (juce::Colours::black.withAlpha (0.55f * shadow), (int) (30 * shadow), { 0, (int) (12 * shadow) }).drawForPath (g, p);
            juce::DropShadow (juce::Colours::black.withAlpha (0.35f * shadow), 4, { 0, 1 }).drawForPath (g, p);
        }
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xf0262629), r.getX(), r.getY(), juce::Colour (0xf01b1b1e), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, radius);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.06f), r.getX(), r.getY(),
                                                 juce::Colours::transparentWhite, r.getX(), r.getY() + juce::jmin (40.0f, r.getHeight() * 0.5f), false));
        g.fillRoundedRectangle (r.reduced (1.0f), radius - 1.0f);
        g.setColour (col::stroke);
        g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawLine (r.getX() + radius, r.getY() + 0.75f, r.getRight() - radius, r.getY() + 0.75f, 1.0f);
    }

    inline void drawPill (juce::Graphics& g, juce::Rectangle<float> r, float radius = 13.0f)
    {
        g.setColour (col::pill);
        g.fillRoundedRectangle (r, radius);
        g.setColour (col::stroke);
        g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
    }

    inline bool approach (float& v, float target, float speed = 0.25f)
    {
        if (std::abs (v - target) < 0.002f) { const bool changed = std::abs (v - target) > 0.0f; v = target; return changed; }
        v += (target - v) * speed;
        return true;
    }

    namespace icon
    {
        using P = juce::Path;
        inline P undo() { P p; p.startNewSubPath (9, 6); p.lineTo (5, 10); p.lineTo (9, 14); p.startNewSubPath (5, 10); p.lineTo (14, 10);
                          p.cubicTo (17.5f, 10, 20, 12.5f, 20, 15); p.cubicTo (20, 17.5f, 17.5f, 20, 14, 20); p.lineTo (11, 20); return p; }
        inline P redo() { auto p = undo(); p.applyTransform (juce::AffineTransform::scale (-1, 1).translated (25, 0)); return p; }
        inline P trash() { P p; p.startNewSubPath (4.5f, 7); p.lineTo (19.5f, 7); p.startNewSubPath (9.5f, 7); p.lineTo (10, 4.5f); p.lineTo (14, 4.5f); p.lineTo (14.5f, 7);
                           p.startNewSubPath (6.5f, 7); p.lineTo (7.5f, 19.5f); p.lineTo (16.5f, 19.5f); p.lineTo (17.5f, 7);
                           p.startNewSubPath (10.25f, 10.5f); p.lineTo (10.5f, 16.5f); p.startNewSubPath (13.75f, 10.5f); p.lineTo (13.5f, 16.5f); return p; }
        inline P power() { P p; p.addCentredArc (12, 13, 7, 7, 0, juce::degreesToRadians (35.0f), juce::degreesToRadians (325.0f), true);
                           p.startNewSubPath (12, 4); p.lineTo (12, 11.5f); return p; }
        inline P chevronDown()  { P p; p.startNewSubPath (7, 10); p.lineTo (12, 15); p.lineTo (17, 10); return p; }
        inline P chevronLeft()  { P p; p.startNewSubPath (14, 6); p.lineTo (8, 12); p.lineTo (14, 18); return p; }
        inline P chevronRight() { P p; p.startNewSubPath (10, 6); p.lineTo (16, 12); p.lineTo (10, 18); return p; }
        inline P copy() { P p; p.addRoundedRectangle (8.5f, 8.5f, 11.0f, 11.0f, 2.5f); p.startNewSubPath (5, 15); p.lineTo (5, 7); p.quadraticTo (5, 5, 7, 5); p.lineTo (15, 5); return p; }
        inline P close() { P p; p.startNewSubPath (7, 7); p.lineTo (17, 17); p.startNewSubPath (17, 7); p.lineTo (7, 17); return p; }
        inline P snowflake()
        {
            P p;
            for (int i = 0; i < 3; ++i)
            {
                const float a = juce::MathConstants<float>::pi * (float) i / 3.0f;
                const float dx = std::sin (a), dy = -std::cos (a);
                p.startNewSubPath (12 - dx * 8, 12 - dy * 8); p.lineTo (12 + dx * 8, 12 + dy * 8);
                for (float s : { -1.0f, 1.0f })
                {
                    const float bx = 12 + s * dx * 5.2f, by = 12 + s * dy * 5.2f;
                    const float px = -dy, py = dx;
                    p.startNewSubPath (bx + px * 2.4f + s * dx * 2.2f, by + py * 2.4f + s * dy * 2.2f);
                    p.lineTo (bx, by);
                    p.lineTo (bx - px * 2.4f + s * dx * 2.2f, by - py * 2.4f + s * dy * 2.2f);
                }
            }
            return p;
        }
        inline P tap() { P p; p.addEllipse (8.5f, 8.5f, 7.0f, 7.0f); p.addCentredArc (12, 12, 7.5f, 7.5f, 0, -0.9f, 0.9f, true);
                         p.addCentredArc (12, 12, 7.5f, 7.5f, 0, juce::MathConstants<float>::pi - 0.9f, juce::MathConstants<float>::pi + 0.9f, true); return p; }
        inline P link() { P p; p.addRoundedRectangle (3.5f, 9.0f, 9.5f, 6.0f, 3.0f); p.addRoundedRectangle (11.0f, 9.0f, 9.5f, 6.0f, 3.0f); return p; }
        inline P save() { P p; p.startNewSubPath (5, 5); p.lineTo (16, 5); p.lineTo (19, 8); p.lineTo (19, 19); p.lineTo (5, 19); p.closeSubPath();
                          p.startNewSubPath (8, 5); p.lineTo (8, 9.5f); p.lineTo (15, 9.5f); p.lineTo (15, 5); p.addRoundedRectangle (8.0f, 13.0f, 8.0f, 6.0f, 1.0f); return p; }
        inline P note() { P p; p.startNewSubPath (10, 17); p.lineTo (10, 5); p.lineTo (18, 4); p.lineTo (18, 7); p.lineTo (10, 8); p.addEllipse (4.5f, 14.5f, 5.5f, 5.0f); return p; }

        inline void draw (juce::Graphics& g, const P& p, juce::Rectangle<float> area, juce::Colour c, float stroke = 1.6f, float designSize = 24.0f)
        {
            const float s = juce::jmin (area.getWidth(), area.getHeight()) / designSize;
            auto t = juce::AffineTransform::scale (s).translated (area.getCentreX() - designSize * 0.5f * s, area.getCentreY() - designSize * 0.5f * s);
            g.setColour (c);
            g.strokePath (p, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), t);
        }
    }

    //==================================================================================================
    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel()
        {
            setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xf7222225));
            setColour (juce::PopupMenu::textColourId, col::label);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent);
            setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
            setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xf7262629));
            setColour (juce::TooltipWindow::textColourId, col::label);
            setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff1c1c1f));
            setColour (juce::TextEditor::textColourId, col::label);
            setColour (juce::TextEditor::highlightColourId, col::accent.withAlpha (0.5f));
            setColour (juce::TextEditor::outlineColourId, col::strokeHi);
            setColour (juce::TextEditor::focusedOutlineColourId, col::accent);
            setColour (juce::CaretComponent::caretColourId, col::accent);
            setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xff1e1e21));
            setColour (juce::AlertWindow::textColourId, col::label);
            setColour (juce::AlertWindow::outlineColourId, col::strokeHi);
            setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2c2c2f));
            setColour (juce::TextButton::textColourOffId, col::label);
            setColour (juce::ResizableWindow::backgroundColourId, col::bg0);
        }

        juce::Typeface::Ptr getTypefaceForFont (const juce::Font& f) override
        {
            if (f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName())
                return typeface (f.isBold() ? Weight::semibold : Weight::medium);
            return juce::LookAndFeel_V4::getTypefaceForFont (f);
        }

        juce::Font getPopupMenuFont() override { return font (13.0f, Weight::medium); }

        void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
        {
            g.fillAll (juce::Colour (0xf7222225));
            g.setColour (col::strokeHi);
            g.drawRect (0, 0, w, h, 1);
        }

        void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator, int standardHeight, int& w, int& h) override
        {
            if (isSeparator) { w = 50; h = 9; return; }
            juce::ignoreUnused (standardHeight);
            h = 26;
            w = (int) textWidth (getPopupMenuFont(), text) + 52;
        }

        void drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted,
                                bool isTicked, bool hasSubMenu, const juce::String& text, const juce::String&, const juce::Drawable*, const juce::Colour*) override
        {
            auto r = area.toFloat();
            if (isSeparator)
            {
                g.setColour (col::stroke);
                g.fillRect (r.reduced (10.0f, 0).withHeight (1.0f).withCentre (r.getCentre()));
                return;
            }
            if (isHighlighted && isActive)
            {
                g.setColour (col::accent);
                g.fillRoundedRectangle (r.reduced (4.0f, 1.0f), 5.0f);
            }
            const auto tc = ! isActive ? col::label3 : isHighlighted ? juce::Colours::white : col::label;
            if (isTicked)
            {
                juce::Path p; p.startNewSubPath (8, 13); p.lineTo (11, 16); p.lineTo (17, 8);
                icon::draw (g, p, r.withWidth (26.0f).reduced (5.0f), tc, 1.8f);
            }
            g.setColour (tc);
            g.setFont (getPopupMenuFont());
            g.drawText (text, r.withTrimmedLeft (28.0f).withTrimmedRight (hasSubMenu ? 22.0f : 10.0f), juce::Justification::centredLeft, true);
            if (hasSubMenu)
                icon::draw (g, icon::chevronRight(), r.removeFromRight (22.0f).reduced (4.0f), tc, 1.6f);
        }

        int getPopupMenuBorderSize() override { return 5; }

        juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override
        {
            const auto f = font (12.0f, Weight::medium);
            const int w = juce::jmin (320, (int) textWidth (f, tipText) + 22);
            const int lines = (int) std::ceil ((textWidth (f, tipText) + 22) / 320.0f);
            const int h = 10 + 16 * juce::jmax (1, lines);
            return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                         screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6, w, h)
                       .constrainedWithin (parentArea);
        }

        void drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h) override
        {
            auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
            g.setColour (juce::Colour (0xf7262629));
            g.fillRoundedRectangle (r, 7.0f);
            g.setColour (col::strokeHi);
            g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);
            g.setColour (col::label);
            g.setFont (font (12.0f, Weight::medium));
            g.drawFittedText (text, r.reduced (11.0f, 5.0f).toNearestInt(), juce::Justification::centredLeft, 6, 1.0f);
        }

        void fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor&) override
        {
            g.setColour (juce::Colour (0xff1c1c1f));
            g.fillRoundedRectangle (0, 0, (float) w, (float) h, 6.0f);
        }

        void drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& te) override
        {
            g.setColour (te.hasKeyboardFocus (true) ? col::accent : col::strokeHi);
            g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f, 6.0f, 1.0f);
        }

        void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
        {
            auto r = b.getLocalBounds().toFloat().reduced (0.5f);
            g.setColour (down ? col::accent.darker (0.2f) : over ? col::accent.brighter (0.1f) : col::accent);
            g.fillRoundedRectangle (r, 7.0f);
        }

        juce::Font getTextButtonFont (juce::TextButton&, int) override { return font (13.0f, Weight::semibold); }
        juce::Font getAlertWindowTitleFont() override { return font (15.0f, Weight::semibold); }
        juce::Font getAlertWindowMessageFont() override { return font (13.0f, Weight::regular); }
        juce::Font getAlertWindowFont() override { return font (13.0f, Weight::regular); }

        void drawAlertBox (juce::Graphics& g, juce::AlertWindow& w, const juce::Rectangle<int>& textArea, juce::TextLayout& tl) override
        {
            auto r = w.getLocalBounds().toFloat();
            g.setColour (juce::Colour (0xff1e1e21));
            g.fillRoundedRectangle (r, 12.0f);
            g.setColour (col::strokeHi);
            g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);
            tl.draw (g, textArea.toFloat());
        }
    };

    //==================================================================================================
    /** Ring dial with caption above and value below. Double-click resets, click the value to type. */
    class Dial : public juce::Slider, private juce::Timer
    {
    public:
        Dial (const juce::String& captionText = {}, juce::Colour arcColour = col::accent)
            : captionStr (captionText.toUpperCase()), colour (arcColour)
        {
            setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
            setVelocityBasedMode (false);
            setMouseDragSensitivity (220);
            setRepaintsOnMouseActivity (true);
            setWantsKeyboardFocus (false);
            startTimerHz (60);
        }

        void bind (juce::RangedAudioParameter& p, juce::UndoManager* um)
        {
            param = &p;
            undo = um;
            attachment = std::make_unique<juce::SliderParameterAttachment> (p, *this, um);
            setDoubleClickReturnValue (true, p.convertFrom0to1 (p.getDefaultValue()));
            if (auto* f = dynamic_cast<juce::AudioParameterFloat*> (&p))
                juce::ignoreUnused (f);
        }

        /** For values that are not plugin parameters. */
        void bindPlain (double minV, double maxV, double def, std::function<juce::String (double)> fmt, double skewMid = 0.0)
        {
            setRange (minV, maxV, 0.0);
            if (skewMid > 0.0) setSkewFactorFromMidPoint (skewMid);
            setDoubleClickReturnValue (true, def);
            setValue (def, juce::dontSendNotification);
            textFromValueFunction = std::move (fmt);
        }

        void setCaption (const juce::String& s) { captionStr = s.toUpperCase(); repaint(); }
        void setColour (juce::Colour c) { colour = c; repaint(); }
        void setBipolar (bool b) { bipolar = b; repaint(); }
        void setDimmed (bool d) { dimmed = d; repaint(); }
        void setValueOverride (std::function<juce::String()> f) { valueOverride = std::move (f); repaint(); }
        float getDialDiameter() const { return diameter; }

        juce::String getValueText()
        {
            if (valueOverride) return valueOverride();
            return getTextFromValue (getValue());
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            const bool hasCaption = captionStr.isNotEmpty();
            auto capR = hasCaption ? r.removeFromTop (15.0f) : juce::Rectangle<float>();
            valueR = r.removeFromBottom (18.0f);
            r.removeFromBottom (2.0f);
            diameter = juce::jmin (r.getWidth(), r.getHeight());
            auto ring = r.withSizeKeepingCentre (diameter, diameter).reduced (3.0f);

            if (hasCaption)
            {
                g.setFont (caption());
                g.setColour (col::label3.interpolatedWith (col::label2, hover));
                g.drawText (captionStr, capR, juce::Justification::centred, false);
            }

            const auto rp = getRotaryParameters();
            const float pos = (float) valueToProportionOfLength (getValue());
            const float a0 = rp.startAngleRadians, a1 = rp.endAngleRadians;
            const float av = a0 + pos * (a1 - a0);
            const float aStart = bipolar ? (a0 + a1) * 0.5f : a0;
            const auto cx = ring.getCentreX(), cy = ring.getCentreY();
            const float rad = ring.getWidth() * 0.5f;
            const float th = juce::jmax (2.5f, rad * 0.085f);
            const auto c = dimmed ? col::label3 : colour;

            juce::Path track;
            track.addCentredArc (cx, cy, rad - th * 0.5f, rad - th * 0.5f, 0, a0, a1, true);
            g.setColour (col::fill);
            g.strokePath (track, juce::PathStrokeType (th, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            if (std::abs (av - aStart) > 0.001f)
            {
                juce::Path arc;
                arc.addCentredArc (cx, cy, rad - th * 0.5f, rad - th * 0.5f, 0, juce::jmin (aStart, av), juce::jmax (aStart, av), true);
                if (! dimmed)
                {
                    g.setColour (c.withAlpha (0.16f + 0.1f * hover));
                    g.strokePath (arc, juce::PathStrokeType (th * 3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                }
                g.setColour (c);
                g.strokePath (arc, juce::PathStrokeType (th, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }

            auto face = ring.reduced (th * 2.2f);
            juce::DropShadow (juce::Colours::black.withAlpha (0.6f), (int) (face.getWidth() * 0.18f), { 0, 2 }).drawForRectangle (g, face.toNearestInt());
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff36363a).interpolatedWith (juce::Colour (0xff3e3e42), hover), face.getX(), face.getY(),
                                                     juce::Colour (0xff1a1a1d), face.getX(), face.getBottom(), false));
            g.fillEllipse (face);
            g.setColour (juce::Colours::white.withAlpha (0.09f));
            g.drawEllipse (face.reduced (0.5f), 1.0f);

            const float fr = face.getWidth() * 0.5f;
            const float sx = std::sin (av), sy = -std::cos (av);
            g.setColour (dimmed ? col::label3 : juce::Colours::white);
            g.drawLine (cx + sx * fr * 0.38f, cy + sy * fr * 0.38f, cx + sx * fr * 0.8f, cy + sy * fr * 0.8f, juce::jmax (1.6f, fr * 0.09f));

            g.setFont (font (12.0f, Weight::semibold));
            g.setColour ((dimmed ? col::label : col::label.interpolatedWith (juce::Colours::white, hover)).withAlpha (dimmed ? 0.45f : 1.0f));
            g.drawText (getValueText(), valueR, juce::Justification::centred, false);
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (undo != nullptr) undo->beginNewTransaction();
            if (valueR.contains (e.position) && e.getNumberOfClicks() == 1 && ! e.mods.isPopupMenu())
            {
                typing = true;
                return;
            }
            typing = false;
            juce::Slider::mouseDown (e);
        }

        void mouseDrag (const juce::MouseEvent& e) override { if (! typing) juce::Slider::mouseDrag (e); }

        void mouseUp (const juce::MouseEvent& e) override
        {
            if (typing)
            {
                typing = false;
                if (valueR.contains (e.position) && e.getDistanceFromDragStart() < 3) showEditor();
                return;
            }
            juce::Slider::mouseUp (e);
        }

        void showEditor()
        {
            editor = std::make_unique<juce::TextEditor>();
            editor->setFont (font (12.0f, Weight::semibold));
            editor->setJustification (juce::Justification::centred);
            editor->setText (getValueText(), false);
            editor->selectAll();
            editor->setBounds (valueR.toNearestInt().expanded (4, 2));
            editor->onReturnKey = [this] { commitEditor(); };
            editor->onEscapeKey = [this] { juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<Dial> (this)] { if (sp) sp->editor.reset(); }); };
            editor->onFocusLost = [this] { commitEditor(); };
            addAndMakeVisible (*editor);
            editor->grabKeyboardFocus();
        }

        std::function<void()> onHoverChange;

    private:
        void commitEditor()
        {
            if (editor == nullptr) return;
            const auto txt = editor->getText();
            if (undo != nullptr) undo->beginNewTransaction();
            setValue (getValueFromText (txt), juce::sendNotificationSync);
            juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<Dial> (this)] { if (sp) sp->editor.reset(); });
        }

        void timerCallback() override
        {
            const bool over = isMouseOverOrDragging() || isMouseButtonDown();
            if (approach (hover, over ? 1.0f : 0.0f, 0.3f)) repaint();
        }

        juce::String captionStr;
        juce::Colour colour;
        bool bipolar = false, dimmed = false, typing = false;
        float hover = 0.0f, diameter = 0.0f;
        juce::Rectangle<float> valueR;
        juce::RangedAudioParameter* param = nullptr;
        juce::UndoManager* undo = nullptr;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
        std::unique_ptr<juce::TextEditor> editor;
        std::function<juce::String()> valueOverride;
    };

    //==================================================================================================
    /** iOS segmented control with an animated thumb. */
    class Segmented : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
    {
    public:
        Segmented (juce::StringArray opts = {}) : options (std::move (opts)) { startTimerHz (60); }

        void setOptions (juce::StringArray o) { options = std::move (o); repaint(); }
        int getSelected() const { return selected; }

        void setSelected (int i, juce::NotificationType n = juce::sendNotificationSync)
        {
            i = juce::jlimit (0, juce::jmax (0, options.size() - 1), i);
            if (i == selected) return;
            selected = i;
            if (n != juce::dontSendNotification && onChange) onChange (i);
            repaint();
        }

        void bind (juce::RangedAudioParameter& p, juce::UndoManager* um)
        {
            undo = um;
            attachment = std::make_unique<juce::ParameterAttachment> (p, [this] (float v) { setSelected (juce::roundToInt (v), juce::dontSendNotification); }, um);
            onChange = [this] (int i) { attachment->setValueAsCompleteGesture ((float) i); };
            attachment->sendInitialUpdate();
            thumbPos = (float) selected;
        }

        int idealWidth (float size = 12.0f, float pad = 22.0f) const
        {
            float w = 4.0f;
            const auto f = font (size, Weight::semibold);
            float widest = 0.0f;
            for (auto& o : options) widest = juce::jmax (widest, textWidth (f, o));
            w += (widest + pad) * (float) options.size();
            return (int) std::ceil (w);
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            const float rad = juce::jmin (9.0f, r.getHeight() * 0.32f);
            g.setColour (col::fill.withAlpha (0.55f));
            g.fillRoundedRectangle (r, rad);
            if (options.isEmpty()) return;
            const float segW = (r.getWidth() - 4.0f) / (float) options.size();
            auto thumb = juce::Rectangle<float> (r.getX() + 2.0f + thumbPos * segW, r.getY() + 2.0f, segW, r.getHeight() - 4.0f);
            juce::DropShadow (juce::Colours::black.withAlpha (0.4f), 6, { 0, 2 }).drawForRectangle (g, thumb.toNearestInt());
            g.setColour (col::thumb);
            g.fillRoundedRectangle (thumb, rad - 2.0f);
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.drawRoundedRectangle (thumb.reduced (0.5f), rad - 2.0f, 1.0f);

            g.setFont (font (fontSize, Weight::semibold));
            for (int i = 0; i < options.size(); ++i)
            {
                auto sr = juce::Rectangle<float> (r.getX() + 2.0f + (float) i * segW, r.getY(), segW, r.getHeight());
                const float sel = juce::jlimit (0.0f, 1.0f, 1.0f - std::abs (thumbPos - (float) i));
                g.setColour (col::label2.interpolatedWith (col::label, juce::jmax (sel, i == hoverIndex ? 0.5f : 0.0f)));
                g.drawText (options[i], sr, juce::Justification::centred, false);
                if (i > 0 && std::abs (thumbPos - (float) i) > 0.95f && std::abs (thumbPos - (float) (i - 1)) > 0.95f)
                {
                    g.setColour (col::stroke);
                    g.fillRect (sr.getX() - 0.5f, sr.getY() + sr.getHeight() * 0.28f, 1.0f, sr.getHeight() * 0.44f);
                }
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (options.isEmpty()) return;
            if (undo != nullptr) undo->beginNewTransaction();
            const float segW = ((float) getWidth() - 4.0f) / (float) options.size();
            setSelected ((int) ((e.position.x - 2.0f) / segW));
        }
        void mouseMove (const juce::MouseEvent& e) override
        {
            if (options.isEmpty()) return;
            const float segW = ((float) getWidth() - 4.0f) / (float) options.size();
            const int h = juce::jlimit (0, options.size() - 1, (int) ((e.position.x - 2.0f) / segW));
            if (h != hoverIndex) { hoverIndex = h; repaint(); }
        }
        void mouseExit (const juce::MouseEvent&) override { hoverIndex = -1; repaint(); }

        std::function<void (int)> onChange;
        float fontSize = 12.0f;

    private:
        void timerCallback() override { if (approach (thumbPos, (float) selected, 0.3f)) repaint(); }

        juce::StringArray options;
        int selected = 0, hoverIndex = -1;
        float thumbPos = 0.0f;
        juce::UndoManager* undo = nullptr;
        std::unique_ptr<juce::ParameterAttachment> attachment;
    };

    //==================================================================================================
    /** iOS toggle switch, optionally with a caption on the left. */
    class Switch : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
    {
    public:
        Switch (const juce::String& text = {}, juce::Colour on = col::green) : captionText (text), onColour (on) { startTimerHz (60); }

        bool getState() const { return state; }
        void setState (bool s, juce::NotificationType n = juce::sendNotificationSync)
        {
            if (s == state) return;
            state = s;
            if (n != juce::dontSendNotification && onChange) onChange (s);
            repaint();
        }

        void bind (juce::RangedAudioParameter& p, juce::UndoManager* um)
        {
            undo = um;
            attachment = std::make_unique<juce::ParameterAttachment> (p, [this] (float v) { setState (v > 0.5f, juce::dontSendNotification); }, um);
            onChange = [this] (bool s) { attachment->setValueAsCompleteGesture (s ? 1.0f : 0.0f); };
            attachment->sendInitialUpdate();
            anim = state ? 1.0f : 0.0f;
        }

        int idealWidth() const { return 38 + (captionText.isEmpty() ? 0 : (int) textWidth (caption(), captionText.toUpperCase()) + 10); }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            auto sw = r.removeFromRight (38.0f).withSizeKeepingCentre (38.0f, 22.0f);
            if (captionText.isNotEmpty())
            {
                g.setFont (caption());
                g.setColour (col::label3.interpolatedWith (col::label2, juce::jmax (hover, anim * 0.6f)));
                g.drawText (captionText.toUpperCase(), r.withTrimmedRight (8.0f), juce::Justification::centredRight, false);
            }
            g.setColour (col::fill.interpolatedWith (onColour, anim));
            g.fillRoundedRectangle (sw, 11.0f);
            auto knob = juce::Rectangle<float> (18.0f, 18.0f).withCentre ({ sw.getX() + 11.0f + anim * 16.0f, sw.getCentreY() });
            juce::DropShadow (juce::Colours::black.withAlpha (0.35f), 4, { 0, 1 }).drawForRectangle (g, knob.toNearestInt());
            g.setColour (juce::Colours::white);
            g.fillEllipse (knob);
        }

        void mouseDown (const juce::MouseEvent&) override
        {
            if (undo != nullptr) undo->beginNewTransaction();
            setState (! state);
        }

        std::function<void (bool)> onChange;

    private:
        void timerCallback() override
        {
            bool c = approach (anim, state ? 1.0f : 0.0f, 0.3f);
            c |= approach (hover, isMouseOver() ? 1.0f : 0.0f, 0.3f);
            if (c) repaint();
        }

        juce::String captionText;
        juce::Colour onColour;
        bool state = false;
        float anim = 0.0f, hover = 0.0f;
        juce::UndoManager* undo = nullptr;
        std::unique_ptr<juce::ParameterAttachment> attachment;
    };

    //==================================================================================================
    /** Stroke icon or text button with hover fade, toggled tint and a primary (filled) style. */
    class IconButton : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
    {
    public:
        IconButton() { startTimerHz (60); }
        IconButton (juce::Path p) : path (std::move (p)) { startTimerHz (60); }
        IconButton (const juce::String& t) : text (t) { startTimerHz (60); }

        void setIcon (juce::Path p) { path = std::move (p); repaint(); }
        void setText (const juce::String& t) { text = t; repaint(); }
        void setPrimary (bool p) { primary = p; repaint(); }
        void setToggled (bool t) { toggled = t; repaint(); }
        bool isToggled() const { return toggled; }
        void setTint (juce::Colour c) { tint = c; repaint(); }
        void setFilledBackground (bool b) { filledBg = b; repaint(); }
        void setEnabledLook (bool e) { enabledLook = e; repaint(); }

        int idealWidth() const
        {
            int w = 0;
            if (! path.isEmpty()) w += 28;
            if (text.isNotEmpty()) w += (int) textWidth (font (12.0f, Weight::semibold), text) + (path.isEmpty() ? 22 : 10);
            return juce::jmax (28, w);
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            const float rad = juce::jmin (8.0f, r.getHeight() * 0.3f);
            if (primary)
            {
                g.setColour (tint.withMultipliedBrightness (1.0f + 0.12f * hover - 0.15f * press));
                g.fillRoundedRectangle (r, rad);
            }
            else
            {
                const float bgA = (filledBg ? 0.5f : 0.0f) + 0.5f * hover + 0.6f * tog;
                if (bgA > 0.001f)
                {
                    g.setColour (col::fill.withAlpha (juce::jmin (1.0f, bgA) * 0.8f).interpolatedWith (tint.withAlpha (0.22f), tog));
                    g.fillRoundedRectangle (r, rad);
                }
            }
            const auto fg = primary ? juce::Colours::white
                                    : (enabledLook ? col::label2.interpolatedWith (col::label, hover).interpolatedWith (tint, tog) : col::label4);
            auto content = r;
            if (! path.isEmpty())
            {
                auto ir = text.isEmpty() ? content.withSizeKeepingCentre (22.0f, 22.0f) : content.removeFromLeft (28.0f).withSizeKeepingCentre (20.0f, 20.0f).translated (4.0f, 0);
                icon::draw (g, path, ir, fg, 1.6f);
            }
            if (text.isNotEmpty())
            {
                g.setFont (font (12.0f, Weight::semibold));
                g.setColour (fg);
                g.drawText (text, content.withTrimmedRight (path.isEmpty() ? 0.0f : 6.0f), path.isEmpty() ? juce::Justification::centred : juce::Justification::centredLeft, false);
            }
        }

        void mouseDown (const juce::MouseEvent&) override { pressed = true; }
        void mouseUp (const juce::MouseEvent& e) override
        {
            pressed = false;
            if (getLocalBounds().contains (e.getPosition()) && onClick) onClick();
        }

        std::function<void()> onClick;

    private:
        void timerCallback() override
        {
            bool c = approach (hover, isMouseOver() ? 1.0f : 0.0f, 0.3f);
            c |= approach (tog, toggled ? 1.0f : 0.0f, 0.3f);
            c |= approach (press, pressed ? 1.0f : 0.0f, 0.4f);
            if (c) repaint();
        }

        juce::Path path;
        juce::String text;
        bool primary = false, toggled = false, pressed = false, filledBg = false, enabledLook = true;
        juce::Colour tint = col::accent;
        float hover = 0.0f, tog = 0.0f, press = 0.0f;
    };
}
