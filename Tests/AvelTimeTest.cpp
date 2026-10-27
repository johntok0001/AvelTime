// Offline tests for Avel Time: DSP behaviour, stability, state, presets, CPU and UI snapshots.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <iostream>
#include <random>

using namespace avel::time;

static int failures = 0;
static void check (bool ok, const juce::String& what)
{
    std::cout << (ok ? "  [ok]   " : "  [FAIL] ") << what << std::endl;
    if (! ok) ++failures;
}

static void setP (AvelTimeProcessor& p, const juce::String& id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    jassert (prm != nullptr);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

static std::unique_ptr<AvelTimeProcessor> make (double sr = 48000.0, int block = 512)
{
    auto p = std::make_unique<AvelTimeProcessor>();
    p->setPlayConfigDetails (2, 2, sr, block);
    p->presets.resetToDefaults();
    p->prepareToPlay (sr, block);
    return p;
}

/** Runs the processor over an input signal (stereo), returns output. */
static juce::AudioBuffer<float> run (AvelTimeProcessor& p, const juce::AudioBuffer<float>& in, int block = 512)
{
    juce::AudioBuffer<float> out (in);
    juce::MidiBuffer midi;
    for (int pos = 0; pos < out.getNumSamples(); pos += block)
    {
        const int n = std::min (block, out.getNumSamples() - pos);
        juce::AudioBuffer<float> view (out.getArrayOfWritePointers(), 2, pos, n);
        p.processBlock (view, midi);
    }
    return out;
}

static juce::AudioBuffer<float> impulse (int len, bool l = true, bool r = true)
{
    juce::AudioBuffer<float> b (2, len);
    b.clear();
    if (l) b.setSample (0, 0, 1.0f);
    if (r) b.setSample (1, 0, 1.0f);
    return b;
}

static int peakIndex (const float* x, int from, int to)
{
    int bi = from; float bv = 0.0f;
    for (int i = from; i < to; ++i) if (std::abs (x[i]) > bv) { bv = std::abs (x[i]); bi = i; }
    return bi;
}
static float peakIn (const float* x, int from, int to)
{
    float bv = 0.0f;
    for (int i = std::max (0, from); i < to; ++i) bv = std::max (bv, std::abs (x[i]));
    return bv;
}
static float rmsIn (const float* x, int from, int to)
{
    double a = 0.0;
    for (int i = from; i < to; ++i) a += (double) x[i] * x[i];
    return (float) std::sqrt (a / std::max (1, to - from));
}
static bool finite (const juce::AudioBuffer<float>& b)
{
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i) if (! std::isfinite (b.getSample (c, i))) return false;
    return true;
}
static void wetOnly (AvelTimeProcessor& p)
{
    setP (p, pid::mix, 100.0f);
    setP (p, pid::sync, 0.0f);
    for (int i = 0; i < numFilters; ++i) setP (p, pid::filt (i, "On"), 0.0f);
}

static void testTiming()
{
    std::cout << "Echo timing" << std::endl;
    const double sr = 48000.0;
    for (float ms : { 1.0f, 12.5f, 250.0f, 1234.0f, 4000.0f })
    {
        auto p = make (sr);
        wetOnly (*p);
        setP (*p, pid::timeL, ms); setP (*p, pid::link, 1.0f); setP (*p, pid::feedback, 0.0f);
        p->prepareToPlay (sr, 512);
        const int len = (int) (sr * (ms / 1000.0 + 0.3));
        auto out = run (*p, impulse (len));
        const int expect = (int) std::round (ms * 0.001 * sr);
        const int got = peakIndex (out.getReadPointer (0), 0, len);
        check (std::abs (got - expect) <= 1, "time " + juce::String (ms) + " ms: echo at sample " + juce::String (got) + " (expected " + juce::String (expect) + ")");
    }
    // compensation with pitch and diffusion active
    {
        auto p = make (sr);
        wetOnly (*p);
        setP (*p, pid::timeL, 300.0f); setP (*p, pid::link, 1.0f); setP (*p, pid::feedback, 0.0f); setP (*p, pid::pitch, 7.0f);
        p->prepareToPlay (sr, 512);
        auto out = run (*p, impulse ((int) (sr * 0.6)));
        const int got = peakIndex (out.getReadPointer (0), 0, out.getNumSamples());
        check (std::abs (got - (int) (0.3 * sr)) < (int) (0.025 * sr), "pitch +7: echo energy centred near 300 ms (" + juce::String (got / sr * 1000.0, 1) + " ms)");
    }
    // tempo sync: 120 BPM default, 1/4 = 500 ms, 1/8D = 375 ms
    {
        auto p = make (sr);
        wetOnly (*p);
        setP (*p, pid::sync, 1.0f); setP (*p, pid::link, 0.0f); setP (*p, pid::divL, 10.0f); setP (*p, pid::divR, 8.0f); setP (*p, pid::feedback, 0.0f);
        p->prepareToPlay (sr, 512);
        auto [l, r] = p->resolvedTimesMs();
        check (std::abs (l - 500.0f) < 0.01f && std::abs (r - 375.0f) < 0.01f, "sync 1/4 and 1/8D at 120 BPM = " + juce::String (l) + " / " + juce::String (r) + " ms");
        auto out = run (*p, impulse ((int) (sr * 0.8)));
        check (std::abs (peakIndex (out.getReadPointer (0), 0, out.getNumSamples()) - 24000) <= 1
            && std::abs (peakIndex (out.getReadPointer (1), 0, out.getNumSamples()) - 18000) <= 1, "sync echoes land on 500 / 375 ms");
    }
}

static void testFeedback()
{
    std::cout << "Feedback and routing" << std::endl;
    const double sr = 48000.0;
    {
        auto p = make (sr);
        wetOnly (*p);
        setP (*p, pid::timeL, 100.0f); setP (*p, pid::link, 1.0f); setP (*p, pid::feedback, 50.0f);
        p->prepareToPlay (sr, 512);
        auto out = run (*p, impulse ((int) (sr * 0.6)));
        const float* x = out.getReadPointer (0);
        const int T = 4800;
        float e[4];
        for (int k = 0; k < 4; ++k) e[k] = peakIn (x, (k + 1) * T - 20, (k + 1) * T + 20);
        const float r1 = e[1] / e[0], r2 = e[2] / e[1], r3 = e[3] / e[2];
        check (std::abs (r1 - 0.5f) < 0.02f && std::abs (r2 - 0.5f) < 0.02f && std::abs (r3 - 0.5f) < 0.02f,
               "50 % feedback halves every repeat (" + juce::String (r1, 3) + ", " + juce::String (r2, 3) + ", " + juce::String (r3, 3) + ")");
    }
    {
        auto p = make (sr);
        wetOnly (*p);
        setP (*p, pid::timeL, 100.0f); setP (*p, pid::link, 1.0f); setP (*p, pid::feedback, 70.0f); setP (*p, pid::routing, 1.0f);
        p->prepareToPlay (sr, 512);
        auto out = run (*p, impulse ((int) (sr * 0.5)));
        const float* L = out.getReadPointer (0); const float* R = out.getReadPointer (1);
        const bool ok = peakIn (L, 4790, 4810) > 0.4f && peakIn (R, 4790, 4810) < 0.01f
                     && peakIn (R, 9590, 9610) > 0.2f && peakIn (L, 9590, 9610) < 0.01f
                     && peakIn (L, 14390, 14410) > 0.1f;
        check (ok, "ping-pong: L, then R, then L");
    }
    {
        auto p = make (sr);
        wetOnly (*p);
        setP (*p, pid::timeL, 100.0f); setP (*p, pid::timeR, 150.0f); setP (*p, pid::link, 0.0f); setP (*p, pid::feedback, 60.0f); setP (*p, pid::cross, 100.0f);
        p->prepareToPlay (sr, 512);
        auto out = run (*p, impulse ((int) (sr * 0.5), true, false));
        const float* R = out.getReadPointer (1);
        check (peakIn (R, 4800 + 7200 - 4, 4800 + 7200 + 4) > 0.3f, "cross feedback 100 %: L echo arrives on R after L + R time");
    }
}

static void testStability()
{
    std::cout << "Stability" << std::endl;
    const double sr = 48000.0;
    std::mt19937 rng (7);
    std::uniform_real_distribution<float> u (-1.0f, 1.0f);
    struct Case { const char* name; std::vector<std::pair<juce::String, float>> v; };
    std::vector<Case> cases {
        { "feedback 120 %", { { pid::feedback, 120 } } },
        { "feedback 120 % + drive clip", { { pid::feedback, 120 }, { pid::drive, 100 }, { pid::driveStyle, 2 } } },
        { "feedback 120 % + resonant bell +24 dB", { { pid::feedback, 120 }, { pid::filt (2, "On"), 1 }, { pid::filt (2, "Gain"), 24 }, { pid::filt (2, "Q"), 18 } } },
        { "feedback 110 % + pitch + diffusion", { { pid::feedback, 110 }, { pid::pitch, 12 }, { pid::diffusion, 100 } } },
        { "short time 1 ms, feedback 120 %", { { pid::timeL, 1 }, { pid::feedback, 120 } } },
        { "everything modulated", { { pid::feedback, 115 }, { pid::slot (0, "Src"), 1 }, { pid::slot (0, "Dst"), 1 }, { pid::slot (0, "Amt"), 100 },
                                    { pid::slot (1, "Src"), 2 }, { pid::slot (1, "Dst"), 6 }, { pid::slot (1, "Amt"), -100 },
                                    { pid::slot (2, "Src"), 3 }, { pid::slot (2, "Dst"), 12 }, { pid::slot (2, "Amt"), 100 },
                                    { pid::lfo (0, "Rate"), 20 }, { pid::lfo (1, "Shape"), 5 } } },
    };
    for (auto& c : cases)
    {
        auto p = make (sr);
        setP (*p, pid::sync, 0.0f);
        setP (*p, pid::mix, 100.0f);
        for (auto& [id, v] : c.v) setP (*p, id, v);
        p->prepareToPlay (sr, 512);
        juce::AudioBuffer<float> in (2, (int) sr * 12);
        in.clear();
        for (int i = 0; i < (int) sr; ++i) { in.setSample (0, i, 0.7f * u (rng)); in.setSample (1, i, 0.7f * u (rng)); }
        auto out = run (*p, in);
        const float pk = std::max (peakIn (out.getReadPointer (0), 0, out.getNumSamples()), peakIn (out.getReadPointer (1), 0, out.getNumSamples()));
        check (finite (out) && pk < 3.0f, juce::String (c.name) + ": finite, peak " + juce::String (juce::Decibels::gainToDecibels (pk), 1) + " dBFS");
    }
}

static void testFreeze()
{
    std::cout << "Freeze" << std::endl;
    const double sr = 48000.0;
    auto p = make (sr);
    wetOnly (*p);
    setP (*p, pid::timeL, 400.0f); setP (*p, pid::link, 1.0f); setP (*p, pid::feedback, 60.0f);
    p->prepareToPlay (sr, 512);
    std::mt19937 rng (3);
    std::uniform_real_distribution<float> u (-0.5f, 0.5f);
    juce::AudioBuffer<float> in (2, (int) sr);
    for (int i = 0; i < in.getNumSamples(); ++i) { in.setSample (0, i, u (rng)); in.setSample (1, i, u (rng)); }
    run (*p, in);
    setP (*p, pid::freeze, 1.0f);
    juce::AudioBuffer<float> more (2, (int) sr * 20);
    for (int i = 0; i < more.getNumSamples(); ++i) { more.setSample (0, i, u (rng)); more.setSample (1, i, u (rng)); }
    auto out = run (*p, more);
    const float* x = out.getReadPointer (0);
    const float a = rmsIn (x, (int) sr * 1, (int) sr * 3), b = rmsIn (x, (int) sr * 17, (int) sr * 19);
    check (a > 0.01f && std::abs (juce::Decibels::gainToDecibels (b / a)) < 1.0f,
           "loop holds for 20 s while new input is ignored (level change " + juce::String (juce::Decibels::gainToDecibels (b / a), 2) + " dB)");
}

static float zeroCrossFreq (const float* x, int from, int to, double sr)
{
    int zc = 0;
    for (int i = from + 1; i < to; ++i) if ((x[i - 1] < 0.0f) != (x[i] < 0.0f)) ++zc;
    return (float) (zc * 0.5 * sr / (to - from));
}

static void testPitch()
{
    std::cout << "Pitch shift" << std::endl;
    const double sr = 48000.0;
    for (float st : { 12.0f, -12.0f, 7.0f })
    {
        auto p = make (sr);
        wetOnly (*p);
        setP (*p, pid::timeL, 200.0f); setP (*p, pid::link, 1.0f); setP (*p, pid::feedback, 0.0f); setP (*p, pid::pitch, st);
        p->prepareToPlay (sr, 512);
        juce::AudioBuffer<float> in (2, (int) sr * 2);
        for (int i = 0; i < in.getNumSamples(); ++i)
        {
            const float v = 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi * 440.0f * (float) i / (float) sr);
            in.setSample (0, i, v); in.setSample (1, i, v);
        }
        auto out = run (*p, in);
        const float f = zeroCrossFreq (out.getReadPointer (0), (int) sr / 2, (int) sr * 2, sr);
        const float expect = 440.0f * std::pow (2.0f, st / 12.0f);
        check (std::abs (f / expect - 1.0f) < 0.03f, juce::String (st) + " st: echo at " + juce::String (f, 1) + " Hz (expected " + juce::String (expect, 1) + ")");
    }
}

static void testFilters()
{
    std::cout << "Feedback filters" << std::endl;
    const double sr = 48000.0;
    const FilterType types[] { FilterType::lowCut, FilterType::highCut, FilterType::bell, FilterType::lowShelf, FilterType::highShelf, FilterType::bandPass, FilterType::notch };
    double worst = 0.0;
    for (auto t : types)
        for (float fc : { 100.0f, 1000.0f, 8000.0f })
        {
            const auto c = dsp::designSvf (t, fc, 9.0f, 1.3f, sr);
            for (float f : { 50.0f, 300.0f, 1000.0f, 3000.0f, 12000.0f })
            {
                // measure the real filter with a sine
                dsp::Svf s;
                double acc = 0.0, accIn = 0.0;
                const int N = (int) sr;
                for (int i = 0; i < N; ++i)
                {
                    const float x = std::sin (2.0f * juce::MathConstants<float>::pi * f * (float) i / (float) sr);
                    const float y = s.process (x, c);
                    if (i > N / 2) { acc += (double) y * y; accIn += (double) x * x; }
                }
                const double measured = std::sqrt (acc / accIn);
                const double predicted = dsp::svfMagnitude (c, f, sr);
                const double err = std::abs (juce::Decibels::gainToDecibels (measured, -200.0) - juce::Decibels::gainToDecibels (predicted, -200.0));
                if (predicted > 0.01) worst = std::max (worst, err);
            }
        }
    check (worst < 0.1, "drawn curves match the real filters (worst error " + juce::String (worst, 3) + " dB)");

    auto p = make (sr);
    wetOnly (*p);
    setP (*p, pid::timeL, 100.0f); setP (*p, pid::link, 1.0f); setP (*p, pid::feedback, 90.0f);
    setP (*p, pid::filt (1, "On"), 1.0f); setP (*p, pid::filt (1, "Type"), 1.0f); setP (*p, pid::filt (1, "Freq"), 2000.0f);
    p->prepareToPlay (sr, 512);
    auto out = run (*p, impulse ((int) sr));
    // high frequencies fade faster: compare HF content of echo 1 vs echo 6 via first difference energy ratio
    const float* x = out.getReadPointer (0);
    auto hfRatio = [&] (int k)
    {
        const int a = k * 4800 - 200, b = k * 4800 + 400;
        double d = 0, e = 0;
        for (int i = a + 1; i < b; ++i) { d += (double) (x[i] - x[i - 1]) * (x[i] - x[i - 1]); e += (double) x[i] * x[i]; }
        return d / std::max (1.0e-12, e);
    };
    check (hfRatio (6) < hfRatio (1) * 0.5, "high cut in the loop: later echoes are darker");
}

static void testStateAndPresets()
{
    std::cout << "State and presets" << std::endl;
    std::mt19937 rng (11);
    std::uniform_real_distribution<float> u (0.0f, 1.0f);
    auto a = make();
    for (auto* prm : a->getParameters()) prm->setValueNotifyingHost (u (rng));
    juce::MemoryBlock mb;
    a->getStateInformation (mb);
    auto b = make();
    b->setStateInformation (mb.getData(), (int) mb.getSize());
    int mismatches = 0;
    for (int i = 0; i < a->getParameters().size(); ++i)
    {
        auto* pa = dynamic_cast<juce::RangedAudioParameter*> (a->getParameters()[i]);
        auto* pb = dynamic_cast<juce::RangedAudioParameter*> (b->getParameters()[i]);
        if (std::abs (pa->convertFrom0to1 (pa->getValue()) - pb->convertFrom0to1 (pb->getValue())) > 1.0e-3f * (pa->getNormalisableRange().getRange().getLength())) ++mismatches;
    }
    check (mismatches == 0, "all " + juce::String (a->getParameters().size()) + " parameters survive save / load (" + juce::String (mismatches) + " mismatches)");

    // every factory preset: finite and sensible level on noise bursts
    const double sr = 48000.0;
    std::uniform_real_distribution<float> n (-0.25f, 0.25f);
    float worst = 0.0f; juce::String worstName; bool allFinite = true;
    for (int i = 0; i < (int) PresetManager::factory().size(); ++i)
    {
        auto p = make (sr);
        p->presets.loadFactory (i);
        p->prepareToPlay (sr, 512);
        juce::AudioBuffer<float> in (2, (int) sr * 6);
        in.clear();
        for (int s = 0; s < (int) sr * 3; ++s) if ((s / 4800) % 4 == 0) { in.setSample (0, s, n (rng)); in.setSample (1, s, n (rng)); }
        auto out = run (*p, in);
        allFinite &= finite (out);
        const float pk = std::max (peakIn (out.getReadPointer (0), 0, out.getNumSamples()), peakIn (out.getReadPointer (1), 0, out.getNumSamples()));
        if (pk > worst) { worst = pk; worstName = PresetManager::factory()[(size_t) i].name; }
    }
    check (allFinite && worst < 2.0f, juce::String ((int) PresetManager::factory().size()) + " factory presets run clean (loudest: " + worstName + ", "
           + juce::String (juce::Decibels::gainToDecibels (worst), 1) + " dBFS for -12 dBFS input)");

    // user preset round trip
    auto p = make();
    setP (*p, pid::feedback, 77.0f);
    const bool saved = p->presets.saveUser ("__AvelTimeTest");
    setP (*p, pid::feedback, 10.0f);
    const bool loaded = p->presets.loadUser (PresetManager::userFolder().getChildFile ("__AvelTimeTest.xml"));
    check (saved && loaded && std::abs (p->apvts.getRawParameterValue (pid::feedback)->load() - 77.0f) < 0.01f, "user preset save / load");
    PresetManager::userFolder().getChildFile ("__AvelTimeTest.xml").deleteFile();
}

static void testCpu()
{
    std::cout << "CPU" << std::endl;
    const double sr = 48000.0;
    auto p = make (sr);
    setP (*p, pid::sync, 0.0f);
    setP (*p, pid::feedback, 70.0f); setP (*p, pid::drive, 40.0f); setP (*p, pid::diffusion, 60.0f); setP (*p, pid::pitch, 12.0f);
    for (int i = 0; i < numFilters; ++i) setP (*p, pid::filt (i, "On"), 1.0f);
    for (int i = 0; i < numSlots; ++i) { setP (*p, pid::slot (i, "Src"), (float) (1 + i % 3)); setP (*p, pid::slot (i, "Dst"), (float) (1 + i * 2)); setP (*p, pid::slot (i, "Amt"), 30.0f); }
    p->prepareToPlay (sr, 512);
    juce::AudioBuffer<float> in (2, (int) sr * 10);
    std::mt19937 rng (5);
    std::uniform_real_distribution<float> u (-0.3f, 0.3f);
    for (int i = 0; i < in.getNumSamples(); ++i) { in.setSample (0, i, u (rng)); in.setSample (1, i, u (rng)); }
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    run (*p, in);
    const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
    std::cout << "    everything on: " << juce::String (ms / 100.0, 2) << " % of one core at 48 kHz" << std::endl;
    check (ms < 10000.0 * 0.15, "real-time factor well below 15 % (" + juce::String (ms / 100.0, 2) + " %)");
}

static void snapshots (const juce::File& dir)
{
    std::cout << "UI snapshots" << std::endl;
    dir.createDirectory();
    auto p = make();
    p->presets.loadFactory (7); // Tape Echo
    setP (*p, pid::filt (2, "On"), 1.0f); setP (*p, pid::filt (2, "Gain"), 5.0f);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p->createEditor());
    auto* e = dynamic_cast<AvelTimeEditor*> (ed.get());
    ed->setVisible (true);

    std::mt19937 rng (9);
    std::uniform_real_distribution<float> u (-0.4f, 0.4f);
    auto feed = [&] (int frames)
    {
        for (int f = 0; f < frames; ++f)
        {
            juce::AudioBuffer<float> b (2, 816);
            for (int i = 0; i < 816; ++i)
            {
                const float burst = (f % 30) < 4 ? 1.0f : 0.0f;
                const float v = burst * u (rng) + 0.15f * std::sin ((float) (f * 816 + i) * 0.03f);
                b.setSample (0, i, v); b.setSample (1, i, v * 0.9f);
            }
            juce::MidiBuffer m;
            p->processBlock (b, m);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (17);
        }
    };
    const char* names[] { "delay", "filters", "modulation" };
    for (auto [w, h] : { std::pair<int, int> { 1180, 720 }, std::pair<int, int> { 1000, 640 }, std::pair<int, int> { 1500, 900 } })
    {
        ed->setSize (w, h);
        for (int v = 0; v < 3; ++v)
        {
            e->setView (v);
            feed (50);
            auto img = ed->createComponentSnapshot (ed->getLocalBounds());
            auto f = dir.getChildFile (juce::String (names[v]) + "_" + juce::String (w) + "x" + juce::String (h) + ".png");
            f.deleteFile();
            juce::FileOutputStream os (f);
            juce::PNGImageFormat().writeImageToStream (img, os);
            std::cout << "    " << f.getFileName() << std::endl;
        }
    }
    // ping-pong + sync view
    ed->setSize (1180, 720);
    p->presets.loadFactory (4);
    e->setView (0);
    feed (40);
    {
        auto img = ed->createComponentSnapshot (ed->getLocalBounds());
        auto f = dir.getChildFile ("delay_pingpong.png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (img, os);
    }
    ed.reset();
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::String only = argc > 1 ? juce::String (argv[1]) : juce::String();
    if (only.isEmpty() || only == "dsp")
    {
        testTiming();
        testFeedback();
        testStability();
        testFreeze();
        testPitch();
        testFilters();
        testStateAndPresets();
        testCpu();
    }
    if (only.isEmpty() || only == "ui")
        snapshots (juce::File::getCurrentWorkingDirectory().getChildFile ("shots"));
    std::cout << (failures == 0 ? "ALL TESTS PASSED" : juce::String (failures) + " TEST(S) FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}
