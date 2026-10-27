#include "Views.h"
#include <queue>
#include <map>

namespace avel::time::ui
{
    namespace
    {
        float paramValue (AvelTimeProcessor& p, const juce::String& id) { return p.apvts.getRawParameterValue (id)->load(); }

        void setParamValue (AvelTimeProcessor& p, const juce::String& id, float v)
        {
            if (auto* prm = p.apvts.getParameter (id))
            {
                const float n = prm->convertTo0to1 (v);
                if (std::abs (prm->getValue() - n) > 1.0e-6f) prm->setValueNotifyingHost (n);
            }
        }

        void beginGesture (AvelTimeProcessor& p, const juce::String& id) { if (auto* prm = p.apvts.getParameter (id)) prm->beginChangeGesture(); }
        void endGesture (AvelTimeProcessor& p, const juce::String& id)   { if (auto* prm = p.apvts.getParameter (id)) prm->endChangeGesture(); }

        juce::String timeLabel (float ms)
        {
            if (ms >= 1000.0f)
            {
                const float s = ms / 1000.0f;
                return std::abs (s - std::round (s)) < 0.01f ? juce::String ((int) std::round (s)) + " s" : juce::String (s, 2).trimCharactersAtEnd ("0") + " s";
            }
            return juce::String (juce::roundToInt (ms)) + " ms";
        }

        // average amplitude loss per repeat through the enabled feedback filters (300 Hz - 4 kHz)
        float filterLossPerRepeat (AvelTimeProcessor& p)
        {
            const double sr = p.getSampleRateSafe();
            dsp::SvfCoeffs cs[numFilters];
            bool on[numFilters];
            for (int i = 0; i < numFilters; ++i)
            {
                on[i] = paramValue (p, pid::filt (i, "On")) > 0.5f;
                if (on[i]) cs[i] = dsp::designSvf ((FilterType) (int) paramValue (p, pid::filt (i, "Type")), paramValue (p, pid::filt (i, "Freq")),
                                                   paramValue (p, pid::filt (i, "Gain")), paramValue (p, pid::filt (i, "Q")), sr);
            }
            double acc = 0.0;
            const int N = 24;
            for (int k = 0; k < N; ++k)
            {
                const double f = 300.0 * std::pow (4000.0 / 300.0, (double) k / (N - 1));
                double m = 1.0;
                for (int i = 0; i < numFilters; ++i) if (on[i]) m *= dsp::svfMagnitude (cs[i], f, sr);
                acc += m * m;
            }
            return (float) std::sqrt (acc / N);
        }
    }

    //==============================================================================================
    // TapView
    //==============================================================================================
    juce::Rectangle<float> TapView::graph() const
    {
        auto r = getLocalBounds().toFloat();
        r.removeFromTop (insetTop);
        r.removeFromBottom (insetBottom);
        return r.reduced (28.0f, 4.0f).withTrimmedRight (26.0f);
    }

    float TapView::xFor (float tMs) const { auto g = graph(); return g.getX() + g.getWidth() * tMs / spanMs; }
    float TapView::tFor (float x) const   { auto g = graph(); return (x - g.getX()) / g.getWidth() * spanMs; }

    void TapView::computeTaps()
    {
        taps.clear();
        auto [tl, tr] = proc.resolvedTimesMs();
        const bool frz = paramValue (proc, pid::freeze) > 0.5f;
        const float fb = frz ? 1.0f : paramValue (proc, pid::feedback) * 0.01f;
        const int routing = (int) paramValue (proc, pid::routing);
        const float cross = routing == 1 ? 1.0f : paramValue (proc, pid::cross) * 0.01f;
        const float loss = frz ? 1.0f : filterLossPerRepeat (proc);
        const float T[2] { tl, tr };

        struct Ev { float t; float g; int ch; int n; bool operator< (const Ev& o) const { return t > o.t; } };
        std::priority_queue<Ev> q;
        q.push ({ tl, loss, 0, 1 });
        if (routing != 1) q.push ({ tr, loss, 1, 1 });

        const float horizon = 30000.0f;
        std::map<std::pair<int, int>, size_t> index;
        int pops = 0;
        while (! q.empty() && pops < 600)
        {
            auto e = q.top(); q.pop(); ++pops;
            if (e.t > horizon) break;
            const auto key = std::make_pair (e.ch, (int) std::round (e.t * 2.0f));
            if (auto it = index.find (key); it != index.end()) { taps[it->second].gain += e.g; continue; }
            index[key] = taps.size();
            taps.push_back ({ e.t, e.g, e.ch, e.n });
            const float gs = e.g * fb * (1.0f - cross) * loss, go = e.g * fb * cross * loss;
            if (gs > 0.001f && taps.size() < 300) q.push ({ e.t + T[e.ch], std::min (gs, 4.0f), e.ch, e.n + 1 });
            if (go > 0.001f && taps.size() < 300) q.push ({ e.t + T[1 - e.ch], std::min (go, 4.0f), 1 - e.ch, e.n + 1 });
        }

        // span: until the echoes fade below -48 dB, at least a few repeats
        float last = 0.0f;
        for (auto& t : taps) if (t.gain > 0.03f) last = std::max (last, t.t);
        const float tmax = std::max (tl, tr);
        spanTarget = juce::jlimit (tmax * 2.3f, std::max (tmax * 2.3f, std::min (8000.0f, tmax * 14.0f)), std::max (last * 1.1f, tmax * 4.2f));
        if (frz) spanTarget = std::max (tmax * 6.0f, 2000.0f);
    }

    void TapView::tick()
    {
        computeTaps();
        if (std::abs (spanMs - spanTarget) / spanTarget > 0.002f) spanMs += (spanTarget - spanMs) * 0.18f; else spanMs = spanTarget;

        const auto onset = proc.engine.lastOnset.load();
        const auto now = proc.engine.sampleCounter.load();
        const double sr = proc.getSampleRateSafe();
        liveMs = (float) ((double) (now - onset) / sr * 1000.0);
        if (liveMs < 0.0f || liveMs > spanMs * 1.05f) liveMs = -1.0f;
        for (int i = 0; i < 2; ++i) approach (handleAnim[i], (hoverHandle == i || dragHandle == i) ? 1.0f : 0.0f, 0.3f);
        repaint();
    }

    juce::Point<float> TapView::handlePos (int ch) const
    {
        auto g = graph();
        const float cy = g.getCentreY();
        const float half = (g.getHeight() * 0.5f - 6.0f) * 0.9f;
        auto [tl, tr] = proc.resolvedTimesMs();
        const float t = ch == 0 ? tl : tr;
        float gain = 0.0f;
        for (auto& tp : taps) if (tp.ch == ch && tp.n == 1) gain = tp.gain;
        const float h = juce::jlimit (0.06f, 1.0f, (juce::Decibels::gainToDecibels (std::min (gain, 1.0f), -60.0f) + 60.0f) / 60.0f) * half;
        return { xFor (t), ch == 0 ? cy - h : cy + h };
    }

    void TapView::paint (juce::Graphics& g)
    {
        auto gr = graph();
        const float cy = gr.getCentreY();
        const float half = (gr.getHeight() * 0.5f - 6.0f) * 0.9f;
        const bool sync = paramValue (proc, pid::sync) > 0.5f;
        const int routing = (int) paramValue (proc, pid::routing);

        // grid
        const float steps[] { 5, 10, 20, 25, 50, 100, 200, 250, 500, 1000, 2000, 2500, 5000, 10000 };
        float step = 10000.0f;
        for (float s : steps) if (spanMs / s <= 9.0f) { step = s; break; }
        g.setFont (font (10.5f, Weight::medium));
        for (float t = 0.0f; t <= spanMs + 0.1f; t += step)
        {
            const float x = xFor (t);
            g.setColour (juce::Colours::white.withAlpha (t == 0.0f ? 0.075f : 0.035f));
            g.fillRect (x - 0.5f, gr.getY(), 1.0f, gr.getHeight());
            g.setColour (col::label3);
            g.drawText (t == 0.0f ? "0" : timeLabel (t), juce::Rectangle<float> (x - 40.0f, gr.getBottom() + 4.0f, 80.0f, 16.0f), juce::Justification::centred, false);
        }
        if (sync)
        {
            const float qms = (float) (60000.0 / juce::jlimit (20.0, 999.0, proc.getBpm()));
            if (spanMs / qms < 48.0f)
                for (float t = qms; t < spanMs; t += qms)
                {
                    const float x = xFor (t);
                    g.setColour (product::accentA.withAlpha (0.07f));
                    juce::Path dash; dash.startNewSubPath (x, gr.getY()); dash.lineTo (x, gr.getBottom());
                    juce::Path out; const float d[] { 2.0f, 4.0f };
                    juce::PathStrokeType (1.0f).createDashedStroke (out, dash, d, 2);
                    g.fillPath (out);
                }
        }
        // dB lines
        for (float db : { -12.0f, -24.0f, -36.0f, -48.0f })
        {
            const float h = (db + 60.0f) / 60.0f * half;
            g.setColour (juce::Colours::white.withAlpha (0.025f));
            g.fillRect (gr.getX(), cy - h, gr.getWidth(), 1.0f);
            g.fillRect (gr.getX(), cy + h, gr.getWidth(), 1.0f);
            if (db == -12.0f || db == -36.0f)
            {
                g.setColour (col::label4);
                g.drawText (juce::String ((int) db), juce::Rectangle<float> (gr.getRight() + 4.0f, cy - h - 7.0f, 30.0f, 14.0f), juce::Justification::centredLeft, false);
                g.drawText (juce::String ((int) db), juce::Rectangle<float> (gr.getRight() + 4.0f, cy + h - 7.0f, 30.0f, 14.0f), juce::Justification::centredLeft, false);
            }
        }
        g.setColour (juce::Colours::white.withAlpha (0.09f));
        g.fillRect (gr.getX(), cy - 0.5f, gr.getWidth(), 1.0f);

        // dry impulse
        g.setColour (col::label2);
        g.fillRoundedRectangle (juce::Rectangle<float> (3.0f, half * 2.0f * 0.5f).withCentre ({ xFor (0.0f), cy }), 1.5f);

        // live play position
        if (liveMs >= 0.0f)
        {
            const float x = xFor (liveMs);
            const float a = juce::jlimit (0.0f, 1.0f, 1.0f - liveMs / spanMs);
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.0f), x - 60.0f, 0, juce::Colours::white.withAlpha (0.07f * a), x, 0, false));
            g.fillRect (juce::Rectangle<float> (x - 60.0f, gr.getY(), 60.0f, gr.getHeight()));
            g.setColour (juce::Colours::white.withAlpha (0.25f * a));
            g.fillRect (x - 0.5f, gr.getY(), 1.0f, gr.getHeight());
        }

        // taps
        const float barW = juce::jlimit (2.5f, 7.0f, gr.getWidth() / std::max (8.0f, (float) taps.size()) * 0.5f);
        for (auto& tp : taps)
        {
            if (tp.t > spanMs * 1.02f) continue;
            const float db = juce::Decibels::gainToDecibels (std::min (tp.gain, 1.0f), -60.0f);
            const float hn = (db + 60.0f) / 60.0f;
            if (hn <= 0.005f) continue;
            const float h = hn * half;
            const float x = xFor (tp.t);
            const auto c = tp.ch == 0 ? product::left : product::right;
            float live = 0.0f;
            if (liveMs >= 0.0f) live = std::exp (-std::abs (liveMs - tp.t) / 45.0f);
            auto bar = tp.ch == 0 ? juce::Rectangle<float> (x - barW * 0.5f, cy - h, barW, h) : juce::Rectangle<float> (x - barW * 0.5f, cy, barW, h);
            if (live > 0.02f)
            {
                g.setColour (c.withAlpha (0.25f * live));
                g.fillRoundedRectangle (bar.expanded (barW * 1.2f, barW * 0.8f), barW);
            }
            const float a = 0.35f + 0.65f * hn;
            g.setGradientFill (juce::ColourGradient (c.withAlpha (a), x, tp.ch == 0 ? cy - h : cy + h, c.withAlpha (a * 0.25f), x, cy, false));
            g.fillRoundedRectangle (bar, barW * 0.5f);
            g.setColour (c.interpolatedWith (juce::Colours::white, live * 0.6f).withAlpha (std::min (1.0f, a + 0.2f)));
            g.fillEllipse (juce::Rectangle<float> (barW + 2.0f, barW + 2.0f).withCentre ({ x, tp.ch == 0 ? cy - h : cy + h }));
        }

        // draggable first-tap handles with labels
        auto [tl, tr] = proc.resolvedTimesMs();
        for (int ch = 0; ch < 2; ++ch)
        {
            if (ch == 1 && routing == 1)
            {
                bool any = false; for (auto& tp : taps) if (tp.ch == 1) any = true;
                if (! any) continue;
            }
            auto hp = handlePos (ch);
            if (ch == 1 && routing == 1) continue;
            const auto c = ch == 0 ? product::left : product::right;
            const float r = 7.0f + 2.0f * handleAnim[ch];
            g.setColour (c.withAlpha (0.25f + 0.15f * handleAnim[ch]));
            g.fillEllipse (juce::Rectangle<float> (r * 2.6f, r * 2.6f).withCentre (hp));
            g.setGradientFill (juce::ColourGradient (c.brighter (0.3f), hp.x, hp.y - r, c.darker (0.2f), hp.x, hp.y + r, false));
            g.fillEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (hp));
            g.setColour (juce::Colours::white.withAlpha (0.6f + 0.4f * handleAnim[ch]));
            g.drawEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (hp), 1.4f);

            juce::String txt = juce::String (ch == 0 ? "LEFT   " : "RIGHT   ") + timeLabel (ch == 0 ? tl : tr);
            if (sync) txt = juce::String (ch == 0 ? "LEFT   " : "RIGHT   ")
                            + divisions()[(size_t) (int) paramValue (proc, paramValue (proc, pid::link) > 0.5f ? pid::divL : (ch == 0 ? pid::divL : pid::divR))].name
                            + "  " + juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")) + "  " + timeLabel (ch == 0 ? tl : tr);
            const auto f = font (11.5f, Weight::semibold);
            const float tw = textWidth (f, txt) + 22.0f;
            auto pill = juce::Rectangle<float> (gr.getRight() - tw, ch == 0 ? gr.getY() - 2.0f : gr.getBottom() - 22.0f, tw, 24.0f);
            drawPill (g, pill, 12.0f);
            g.setColour (c.interpolatedWith (juce::Colours::white, 0.45f));
            g.setFont (f);
            g.drawText (txt, pill, juce::Justification::centred, false);
        }
    }

    void TapView::mouseMove (const juce::MouseEvent& e)
    {
        int h = -1;
        for (int ch = 0; ch < 2; ++ch)
            if (handlePos (ch).getDistanceFrom (e.position) < 14.0f && ! (ch == 1 && (int) paramValue (proc, pid::routing) == 1)) h = ch;
        if (h != hoverHandle) { hoverHandle = h; repaint(); }
        setMouseCursor (h >= 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
    }

    void TapView::mouseDown (const juce::MouseEvent& e)
    {
        mouseMove (e);
        dragHandle = hoverHandle;
        if (dragHandle < 0) return;
        proc.undo.beginNewTransaction();
        const bool link = paramValue (proc, pid::link) > 0.5f;
        if (dragHandle == 1 && link) setParamValue (proc, pid::link, 0.0f);
        const bool sync = paramValue (proc, pid::sync) > 0.5f;
        beginGesture (proc, sync ? (dragHandle == 0 ? pid::divL : pid::divR) : (dragHandle == 0 ? pid::timeL : pid::timeR));
    }

    void TapView::mouseDrag (const juce::MouseEvent& e)
    {
        if (dragHandle < 0) return;
        const float ms = juce::jlimit (1.0f, maxDelayMs, tFor (e.position.x));
        const bool sync = paramValue (proc, pid::sync) > 0.5f;
        if (sync)
        {
            const double qms = 60000.0 / juce::jlimit (20.0, 999.0, proc.getBpm());
            int best = 0; double bestD = 1e9;
            for (int i = 0; i < (int) divisions().size(); ++i)
            {
                const double d = std::abs (std::log ((divisions()[(size_t) i].quarters * qms) / ms));
                if (d < bestD && divisions()[(size_t) i].quarters * qms <= maxDelayMs) { bestD = d; best = i; }
            }
            setParamValue (proc, dragHandle == 0 ? pid::divL : pid::divR, (float) best);
        }
        else
            setParamValue (proc, dragHandle == 0 ? pid::timeL : pid::timeR, e.mods.isShiftDown() ? std::round (ms) : ms);
    }

    void TapView::mouseUp (const juce::MouseEvent&)
    {
        if (dragHandle >= 0)
        {
            const bool sync = paramValue (proc, pid::sync) > 0.5f;
            endGesture (proc, sync ? (dragHandle == 0 ? pid::divL : pid::divR) : (dragHandle == 0 ? pid::timeL : pid::timeR));
        }
        dragHandle = -1;
    }

    //==============================================================================================
    // FilterView
    //==============================================================================================
    FilterView::FilterView (AvelTimeProcessor& p) : proc (p)
    {
        fftBuf.assign (4096, 0.0f);
        window.resize (2048);
        for (int i = 0; i < 2048; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / 2047.0f);
        specIn.assign (240, -120.0f);
        specWet.assign (240, -120.0f);
    }

    juce::Rectangle<float> FilterView::graph() const
    {
        auto r = getLocalBounds().toFloat();
        r.removeFromTop (insetTop);
        r.removeFromBottom (22.0f);
        return r.withTrimmedRight (34.0f);
    }

    float FilterView::xForF (float f) const { auto g = graph(); return g.getX() + g.getWidth() * std::log (f / 20.0f) / std::log (1000.0f); }
    float FilterView::fForX (float x) const { auto g = graph(); return 20.0f * std::pow (1000.0f, (x - g.getX()) / g.getWidth()); }
    float FilterView::yForDb (float db) const
    {
        auto g = graph();
        const float usable = g.getHeight() - insetBottom;   // keep the curve above the floating card
        return g.getY() + usable * 0.5f - db / 24.0f * usable * 0.46f;
    }
    float FilterView::dbForY (float y) const
    {
        auto g = graph();
        const float usable = g.getHeight() - insetBottom;
        return (g.getY() + usable * 0.5f - y) / (usable * 0.46f) * 24.0f;
    }

    bool FilterView::hasGain (int i) const
    {
        const auto t = (FilterType) (int) paramValue (proc, pid::filt (i, "Type"));
        return t == FilterType::bell || t == FilterType::lowShelf || t == FilterType::highShelf;
    }

    float FilterView::bandMag (int i, float f) const
    {
        const auto c = dsp::designSvf ((FilterType) (int) paramValue (proc, pid::filt (i, "Type")), paramValue (proc, pid::filt (i, "Freq")),
                                       paramValue (proc, pid::filt (i, "Gain")), paramValue (proc, pid::filt (i, "Q")), proc.getSampleRateSafe());
        return dsp::svfMagnitude (c, f, proc.getSampleRateSafe());
    }

    juce::Point<float> FilterView::nodePos (int i) const
    {
        const float f = paramValue (proc, pid::filt (i, "Freq"));
        float db;
        if (hasGain (i)) db = paramValue (proc, pid::filt (i, "Gain"));
        else
        {
            const auto t = (FilterType) (int) paramValue (proc, pid::filt (i, "Type"));
            db = t == FilterType::bandPass ? 0.0f : t == FilterType::notch ? -18.0f : juce::Decibels::gainToDecibels (bandMag (i, f), -24.0f);
        }
        return { xForF (f), yForDb (juce::jlimit (-23.0f, 23.0f, db)) };
    }

    void FilterView::tick()
    {
        // analyzer
        const double sr = proc.getSampleRateSafe();
        const int N = 2048;
        auto run = [&] (const ScopeFifo& fifo, std::vector<float>& spec, float smooth)
        {
            std::fill (fftBuf.begin(), fftBuf.end(), 0.0f);
            fifo.copyLatest (fftBuf.data(), N);
            for (int i = 0; i < N; ++i) fftBuf[(size_t) i] *= window[(size_t) i];
            fft.performFrequencyOnlyForwardTransform (fftBuf.data());
            const int P = (int) spec.size();
            for (int k = 0; k < P; ++k)
            {
                const float f0 = 20.0f * std::pow (1000.0f, (float) k / (float) P), f1 = 20.0f * std::pow (1000.0f, (float) (k + 1) / (float) P);
                int b0 = (int) (f0 / sr * N), b1 = std::max (b0 + 1, (int) (f1 / sr * N));
                b0 = juce::jlimit (1, N / 2 - 1, b0); b1 = juce::jlimit (b0 + 1, N / 2, b1);
                float m = 0.0f;
                for (int b = b0; b < b1; ++b) m = std::max (m, fftBuf[(size_t) b]);
                const float fc = std::sqrt (f0 * f1);
                float db = juce::Decibels::gainToDecibels (m * 4.0f / (float) N, -140.0f) + 4.5f * std::log2 (fc / 1000.0f);
                spec[(size_t) k] = db > spec[(size_t) k] ? db : spec[(size_t) k] + (db - spec[(size_t) k]) * smooth;
            }
        };
        run (proc.scopeIn, specIn, 0.12f);
        run (proc.scopeWet, specWet, 0.12f);
        for (int i = 0; i < numFilters; ++i)
        {
            const bool on = paramValue (proc, pid::filt (i, "On")) > 0.5f;
            approach (nodeAnim[i], (hover == i || drag == i) ? 1.0f : (i == selected ? 0.6f : 0.0f) + (on ? 0.0f : 0.0f), 0.3f);
        }
        repaint();
    }

    void FilterView::paint (juce::Graphics& g)
    {
        auto gr = graph();
        const double sr = proc.getSampleRateSafe();

        // grid
        g.setFont (font (10.5f, Weight::medium));
        for (float dec : { 20.0f, 100.0f, 1000.0f, 10000.0f })
            for (int m = 1; m < 10; ++m)
            {
                const float f = dec * (float) m;
                if (f > 20000.0f || f < 20.0f) continue;
                const float x = xForF (f);
                g.setColour (juce::Colours::white.withAlpha (m == 1 ? 0.075f : 0.03f));
                g.fillRect (x - 0.5f, gr.getY(), 1.0f, gr.getHeight());
            }
        for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
        {
            g.setColour (col::label3);
            g.drawText (f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + "k" : juce::String ((int) f),
                        juce::Rectangle<float> (xForF (f) - 30.0f, gr.getBottom() + 3.0f, 60.0f, 16.0f), juce::Justification::centred, false);
        }
        for (float db : { -18.0f, -12.0f, -6.0f, 0.0f, 6.0f, 12.0f, 18.0f })
        {
            const float y = yForDb (db);
            g.setColour (juce::Colours::white.withAlpha (db == 0.0f ? 0.08f : 0.03f));
            g.fillRect (gr.getX(), y - 0.5f, gr.getWidth(), 1.0f);
            g.setColour (col::label4);
            g.drawText ((db > 0 ? "+" : "") + juce::String ((int) db), juce::Rectangle<float> (gr.getRight() + 4.0f, y - 7.0f, 30.0f, 14.0f), juce::Justification::centredLeft, false);
        }

        // analyzer: -96..0 dB over the full graph height
        auto specPath = [&] (const std::vector<float>& s, bool close)
        {
            juce::Path p;
            const int P = (int) s.size();
            for (int k = 0; k < P; ++k)
            {
                const float f = 20.0f * std::pow (1000.0f, ((float) k + 0.5f) / (float) P);
                const float y = juce::jmap (juce::jlimit (-96.0f, 6.0f, s[(size_t) k]), -96.0f, 6.0f, gr.getBottom(), gr.getY() + gr.getHeight() * 0.08f);
                if (k == 0) p.startNewSubPath (gr.getX(), y);
                p.lineTo (xForF (f), y);
            }
            if (close) { p.lineTo (gr.getRight(), gr.getBottom()); p.lineTo (gr.getX(), gr.getBottom()); p.closeSubPath(); }
            return p;
        };
        g.setColour (juce::Colour (0xff3a4a66).withAlpha (0.35f));
        g.fillPath (specPath (specIn, true));
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.16f), 0, gr.getY(), juce::Colours::white.withAlpha (0.02f), 0, gr.getBottom(), false));
        g.fillPath (specPath (specWet, true));
        g.setColour (juce::Colours::white.withAlpha (0.28f));
        g.strokePath (specPath (specWet, false), juce::PathStrokeType (1.0f));

        // curves
        dsp::SvfCoeffs cs[numFilters];
        bool on[numFilters];
        for (int i = 0; i < numFilters; ++i)
        {
            on[i] = paramValue (proc, pid::filt (i, "On")) > 0.5f;
            cs[i] = dsp::designSvf ((FilterType) (int) paramValue (proc, pid::filt (i, "Type")), paramValue (proc, pid::filt (i, "Freq")),
                                    paramValue (proc, pid::filt (i, "Gain")), paramValue (proc, pid::filt (i, "Q")), sr);
        }
        const int P = juce::jmax (64, (int) gr.getWidth() / 2);
        std::vector<float> total ((size_t) P, 1.0f);
        const float y0 = yForDb (0.0f);
        for (int i = 0; i < numFilters; ++i)
        {
            juce::Path band;
            for (int k = 0; k < P; ++k)
            {
                const float x = gr.getX() + gr.getWidth() * (float) k / (float) (P - 1);
                const float m = dsp::svfMagnitude (cs[i], fForX (x), sr);
                if (on[i]) total[(size_t) k] *= m;
                const float y = juce::jlimit (gr.getY(), gr.getBottom(), yForDb (juce::Decibels::gainToDecibels (m, -60.0f)));
                if (k == 0) band.startNewSubPath (x, y); else band.lineTo (x, y);
            }
            if (! on[i]) continue;
            const auto c = col::band (i);
            const float emph = i == selected ? 1.0f : 0.4f + 0.6f * nodeAnim[i];
            juce::Path fillP (band);
            fillP.lineTo (gr.getRight(), y0); fillP.lineTo (gr.getX(), y0); fillP.closeSubPath();
            g.setColour (c.withAlpha (0.07f * emph));
            g.fillPath (fillP);
            g.setColour (c.withAlpha (0.35f * emph));
            g.strokePath (band, juce::PathStrokeType (1.0f));
        }

        // after several repeats (feedback included)
        const float fb = juce::jlimit (0.0f, 1.2f, paramValue (proc, pid::feedback) * 0.01f);
        for (int n : { 4 })
        {
            juce::Path rep;
            for (int k = 0; k < P; ++k)
            {
                const float x = gr.getX() + gr.getWidth() * (float) k / (float) (P - 1);
                const float m = std::pow (total[(size_t) k], (float) n) * std::pow (std::max (fb, 1.0e-4f), (float) (n - 1));
                const float y = juce::jlimit (gr.getY(), gr.getBottom() - 1.0f, yForDb (juce::Decibels::gainToDecibels (m, -60.0f)));
                if (k == 0) rep.startNewSubPath (x, y); else rep.lineTo (x, y);
            }
            juce::Path dashed; const float d[] { 4.0f, 4.0f };
            juce::PathStrokeType (1.2f).createDashedStroke (dashed, rep, d, 2);
            g.setColour (product::accentB.withAlpha (0.45f));
            g.fillPath (dashed);
        }

        juce::Path tp;
        for (int k = 0; k < P; ++k)
        {
            const float x = gr.getX() + gr.getWidth() * (float) k / (float) (P - 1);
            const float y = juce::jlimit (gr.getY(), gr.getBottom(), yForDb (juce::Decibels::gainToDecibels (total[(size_t) k], -60.0f)));
            if (k == 0) tp.startNewSubPath (x, y); else tp.lineTo (x, y);
        }
        g.setColour (product::accentA.withAlpha (0.25f));
        g.strokePath (tp, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (product::accentA);
        g.strokePath (tp, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // legend
        {
            auto lg = juce::Rectangle<float> (gr.getX() + 14.0f, gr.getY() + 12.0f, 250.0f, 24.0f);
            g.setFont (font (11.0f, Weight::medium));
            g.setColour (product::accentA);
            g.fillRoundedRectangle (lg.getX(), lg.getCentreY() - 1.0f, 16.0f, 2.0f, 1.0f);
            g.setColour (col::label2);
            g.drawText ("Each repeat", lg.withTrimmedLeft (22.0f), juce::Justification::centredLeft, false);
            g.setColour (product::accentB.withAlpha (0.7f));
            for (int k = 0; k < 3; ++k) g.fillRect (lg.getX() + 110.0f + (float) k * 6.0f, lg.getCentreY() - 0.6f, 3.5f, 1.2f);
            g.setColour (col::label2);
            g.drawText ("After 4 repeats", lg.withTrimmedLeft (134.0f), juce::Justification::centredLeft, false);
        }

        // nodes
        for (int i = 0; i < numFilters; ++i)
        {
            auto np = nodePos (i);
            const auto c = col::band (i);
            const float a = nodeAnim[i];
            const float r = 9.0f + 1.5f * a;
            if (on[i])
            {
                g.setColour (c.withAlpha (0.18f + 0.18f * a));
                g.fillEllipse (juce::Rectangle<float> (r * 2.8f, r * 2.8f).withCentre (np));
                g.setGradientFill (juce::ColourGradient (c.brighter (0.35f), np.x, np.y - r, c.darker (0.25f), np.x, np.y + r, false));
                g.fillEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (np));
                if (i == selected)
                {
                    g.setColour (juce::Colours::white);
                    g.drawEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (np), 1.6f);
                }
                g.setColour (juce::Colours::white);
            }
            else
            {
                g.setColour (col::bg1.withAlpha (0.8f));
                g.fillEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (np));
                g.setColour (c.withAlpha (0.5f + 0.3f * a));
                g.drawEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (np), i == selected ? 1.6f : 1.2f);
                g.setColour (c.withAlpha (0.7f));
            }
            g.setFont (font (11.0f, Weight::semibold));
            g.drawText (juce::String (i + 1), juce::Rectangle<float> (r * 2, r * 2).withCentre (np), juce::Justification::centred, false);

            if (a > 0.65f && (hover == i || drag == i))
            {
                const auto txt = formatHz (paramValue (proc, pid::filt (i, "Freq"))) + (hasGain (i) ? "   " + juce::String (paramValue (proc, pid::filt (i, "Gain")), 1) + " dB" : juce::String())
                                 + "   Q " + juce::String (paramValue (proc, pid::filt (i, "Q")), 2);
                const auto f = font (11.5f, Weight::semibold);
                auto pill = juce::Rectangle<float> (textWidth (f, txt) + 22.0f, 24.0f).withCentre ({ np.x, np.y - 30.0f });
                pill = pill.withX (juce::jlimit (gr.getX(), gr.getRight() - pill.getWidth(), pill.getX())).withY (std::max (gr.getY(), pill.getY()));
                drawPill (g, pill);
                g.setColour (col::label);
                g.setFont (f);
                g.drawText (txt, pill, juce::Justification::centred, false);
            }
        }
    }

    void FilterView::setParam (const juce::String& id, float v) { setParamValue (proc, id, v); }

    void FilterView::mouseMove (const juce::MouseEvent& e)
    {
        int h = -1; float best = 16.0f;
        for (int i = 0; i < numFilters; ++i)
        {
            const float d = nodePos (i).getDistanceFrom (e.position);
            if (d < best) { best = d; h = i; }
        }
        if (h != hover) { hover = h; repaint(); }
        setMouseCursor (h >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
    }

    void FilterView::mouseDown (const juce::MouseEvent& e)
    {
        mouseMove (e);
        drag = hover;
        if (drag < 0) return;
        selected = drag;
        if (onSelect) onSelect (drag);
        if (e.mods.isPopupMenu()) { showBandMenu (drag); drag = -1; return; }
        proc.undo.beginNewTransaction();
        dragStart = nodePos (drag);
        dragStartQ = paramValue (proc, pid::filt (drag, "Q"));
        for (auto* w : { "Freq", "Gain", "Q" }) beginGesture (proc, pid::filt (drag, w));
        if (paramValue (proc, pid::filt (drag, "On")) < 0.5f && e.getNumberOfClicks() == 1) {}
    }

    void FilterView::mouseDrag (const juce::MouseEvent& e)
    {
        if (drag < 0) return;
        auto off = e.getOffsetFromDragStart().toFloat();
        if (e.mods.isShiftDown()) off *= 0.25f;
        auto p = dragStart + off;
        auto gr = graph();
        setParam (pid::filt (drag, "Freq"), juce::jlimit (20.0f, 20000.0f, fForX (juce::jlimit (gr.getX(), gr.getRight(), p.x))));
        if (hasGain (drag)) setParam (pid::filt (drag, "Gain"), juce::jlimit (-24.0f, 24.0f, dbForY (p.y)));
        else setParam (pid::filt (drag, "Q"), juce::jlimit (0.1f, 18.0f, dragStartQ * std::pow (2.0f, -off.y / 40.0f)));
        if (paramValue (proc, pid::filt (drag, "On")) < 0.5f && e.getDistanceFromDragStart() > 3) setParam (pid::filt (drag, "On"), 1.0f);
    }

    void FilterView::mouseUp (const juce::MouseEvent&)
    {
        if (drag >= 0) for (auto* w : { "Freq", "Gain", "Q" }) endGesture (proc, pid::filt (drag, w));
        drag = -1;
    }

    void FilterView::mouseDoubleClick (const juce::MouseEvent& e)
    {
        proc.undo.beginNewTransaction();
        if (hover >= 0)
        {
            setParam (pid::filt (hover, "On"), paramValue (proc, pid::filt (hover, "On")) > 0.5f ? 0.0f : 1.0f);
            return;
        }
        for (int i = 0; i < numFilters; ++i)
            if (paramValue (proc, pid::filt (i, "On")) < 0.5f)
            {
                setParam (pid::filt (i, "Type"), (float) (int) FilterType::bell);
                setParam (pid::filt (i, "Freq"), juce::jlimit (20.0f, 20000.0f, fForX (e.position.x)));
                setParam (pid::filt (i, "Gain"), juce::jlimit (-24.0f, 24.0f, dbForY (e.position.y)));
                setParam (pid::filt (i, "Q"), 1.0f);
                setParam (pid::filt (i, "On"), 1.0f);
                selected = i;
                if (onSelect) onSelect (i);
                return;
            }
    }

    void FilterView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
    {
        mouseMove (e);
        const int i = hover >= 0 ? hover : -1;
        if (i < 0) return;
        const float q = paramValue (proc, pid::filt (i, "Q"));
        setParam (pid::filt (i, "Q"), juce::jlimit (0.1f, 18.0f, q * std::pow (2.0f, w.deltaY * 2.0f)));
    }

    void FilterView::showBandMenu (int i)
    {
        juce::PopupMenu m;
        const bool on = paramValue (proc, pid::filt (i, "On")) > 0.5f;
        m.addItem (1, on ? "Disable band" : "Enable band");
        m.addSeparator();
        const auto names = filterTypeNames();
        const int cur = (int) paramValue (proc, pid::filt (i, "Type"));
        for (int t = 0; t < names.size(); ++t) m.addItem (10 + t, names[t], true, t == cur);
        m.addSeparator();
        m.addItem (2, "Reset band");
        m.showMenuAsync (juce::PopupMenu::Options(), [this, i] (int r)
        {
            if (r <= 0) return;
            proc.undo.beginNewTransaction();
            if (r == 1) setParam (pid::filt (i, "On"), paramValue (proc, pid::filt (i, "On")) > 0.5f ? 0.0f : 1.0f);
            else if (r == 2)
            {
                for (auto* w : { "Gain", "Q" })
                    if (auto* prm = proc.apvts.getParameter (pid::filt (i, w))) prm->setValueNotifyingHost (prm->getDefaultValue());
            }
            else setParam (pid::filt (i, "Type"), (float) (r - 10));
        });
    }

    //==============================================================================================
    void LfoPreview::paint (juce::Graphics& g)
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.22f));
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (col::stroke);
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
        r = r.reduced (10.0f, 12.0f);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillRect (r.getX(), r.getCentreY() - 0.5f, r.getWidth(), 1.0f);

        const auto shape = (LfoShape) (int) paramValue (proc, pid::lfo (idx, "Shape"));
        const float stereo = paramValue (proc, pid::lfo (idx, "Phase")) / 360.0f;
        const int seed = 17 + idx * 31;
        const float cycles = 2.0f;
        bool used = false;
        for (int s = 0; s < numSlots; ++s)
            if ((int) paramValue (proc, pid::slot (s, "Src")) == idx + 1 && (int) paramValue (proc, pid::slot (s, "Dst")) != 0
                && std::abs (paramValue (proc, pid::slot (s, "Amt"))) > 0.0f) used = true;

        const float phase = proc.engine.lfoPhaseOut[idx].load();
        auto path = [&] (float offset)
        {
            juce::Path p;
            const int N = (int) r.getWidth();
            for (int k = 0; k <= N; ++k)
            {
                const double ph = (double) cycles * k / N + offset;
                const float v = dsp::Engine::lfoDisplay (shape, ph, seed);
                const float x = r.getX() + (float) k, y = r.getCentreY() - v * r.getHeight() * 0.5f;
                if (k == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            return p;
        };
        const auto cl = used ? product::left : col::label3;
        if (stereo > 0.001f)
        {
            g.setColour ((used ? product::right : col::label3).withAlpha (0.55f));
            g.strokePath (path (stereo), juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setColour (cl.withAlpha (0.25f));
        g.strokePath (path (0.0f), juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (cl);
        g.strokePath (path (0.0f), juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // live dot in the first cycle
        const float v = dsp::Engine::lfoDisplay (shape, phase, seed);
        juce::Point<float> dot (r.getX() + r.getWidth() * phase / cycles, r.getCentreY() - v * r.getHeight() * 0.5f);
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.fillEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre (dot));
        g.setColour (juce::Colours::white);
        g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (dot));

        if (! used)
        {
            g.setFont (font (11.0f, Weight::medium));
            g.setColour (col::label3);
            g.drawText ("Not assigned", r.removeFromTop (14.0f), juce::Justification::topRight, false);
        }
    }

    void EnvPreview::paint (juce::Graphics& g)
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.22f));
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (col::stroke);
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
        r = r.reduced (10.0f, 12.0f);
        juce::Path p;
        const int N = (int) hist.size();
        for (int k = 0; k < N; ++k)
        {
            const float x = r.getX() + r.getWidth() * (float) k / (float) (N - 1);
            const float y = r.getBottom() - hist[(size_t) k] * r.getHeight();
            if (k == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        juce::Path f (p);
        f.lineTo (r.getRight(), r.getBottom()); f.lineTo (r.getX(), r.getBottom()); f.closeSubPath();
        g.setGradientFill (juce::ColourGradient (col::band (1).withAlpha (0.35f), 0, r.getY(), col::band (1).withAlpha (0.02f), 0, r.getBottom(), false));
        g.fillPath (f);
        g.setColour (col::band (1));
        g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    //==============================================================================================
    void Meter::paint (juce::Graphics& g)
    {
        auto r = getLocalBounds().toFloat();
        g.setFont (caption());
        g.setColour (col::label3);
        g.drawText (captionText, r.removeFromTop (14.0f), juce::Justification::centred, false);
        r.removeFromTop (4.0f);
        const float w = (r.getWidth() - 4.0f) * 0.5f;
        for (int i = 0; i < 2; ++i)
        {
            auto b = juce::Rectangle<float> (r.getX() + (float) i * (w + 4.0f), r.getY(), w, r.getHeight());
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.fillRoundedRectangle (b, 2.5f);
            auto norm = [] (float db) { return juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f); };
            const float h = norm (level[i]) * b.getHeight();
            auto lv = b.withTop (b.getBottom() - h);
            juce::ColourGradient cg (col::green, 0, b.getBottom(), col::red, 0, b.getY(), false);
            cg.addColour (0.75, col::green);
            cg.addColour (0.88, col::yellow);
            g.setGradientFill (cg);
            g.fillRoundedRectangle (lv, 2.5f);
            const float py = b.getBottom() - norm (peak[i]) * b.getHeight();
            if (peak[i] > -60.0f)
            {
                g.setColour (peak[i] > 0.0f ? col::red : col::label);
                g.fillRect (b.getX(), py - 1.0f, b.getWidth(), 1.5f);
            }
        }
    }
}
