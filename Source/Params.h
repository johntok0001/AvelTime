#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

namespace avel::time
{
    constexpr int numFilters = 4;
    constexpr int numLfos = 2;
    constexpr int numSlots = 6;
    constexpr float maxDelayMs = 4000.0f;

    namespace pid
    {
        inline const juce::String timeL = "timeL", timeR = "timeR", link = "link", sync = "sync", divL = "divL", divR = "divR";
        inline const juce::String feedback = "feedback", cross = "cross", routing = "routing", timeMode = "timeMode";
        inline const juce::String width = "width", pan = "pan";
        inline const juce::String drive = "drive", driveStyle = "driveStyle", diffusion = "diffusion", diffSize = "diffSize";
        inline const juce::String pitch = "pitch", duck = "duck", duckRelease = "duckRelease", freeze = "freeze";
        inline const juce::String input = "input", mix = "mix", output = "output";
        inline const juce::String envAttack = "envAttack", envRelease = "envRelease", envSens = "envSens";

        inline juce::String filt (int i, const char* what) { return "f" + juce::String (i + 1) + what; } // On, Type, Freq, Gain, Q
        inline juce::String lfo (int i, const char* what)  { return "lfo" + juce::String (i + 1) + what; } // Rate, Sync, Div, Shape, Phase
        inline juce::String slot (int i, const char* what) { return "m" + juce::String (i + 1) + what; } // Src, Dst, Amt
    }

    // Note values, shared by the delay and the LFOs. Value in quarter notes.
    struct Division { const char* name; double quarters; };
    inline const std::array<Division, 19>& divisions()
    {
        static const std::array<Division, 19> d { {
            { "1/64", 1.0 / 16 }, { "1/32T", 1.0 / 12 }, { "1/32", 1.0 / 8 }, { "1/16T", 1.0 / 6 }, { "1/16", 0.25 }, { "1/16D", 0.375 },
            { "1/8T", 1.0 / 3 }, { "1/8", 0.5 }, { "1/8D", 0.75 }, { "1/4T", 2.0 / 3 }, { "1/4", 1.0 }, { "1/4D", 1.5 },
            { "1/2T", 4.0 / 3 }, { "1/2", 2.0 }, { "1/2D", 3.0 }, { "1/1", 4.0 }, { "1/1D", 6.0 }, { "2/1", 8.0 }, { "4/1", 16.0 } } };
        return d;
    }
    inline juce::StringArray divisionNames()
    {
        juce::StringArray s;
        for (auto& d : divisions()) s.add (d.name);
        return s;
    }

    enum class FilterType { lowCut, highCut, bell, lowShelf, highShelf, bandPass, notch };
    inline juce::StringArray filterTypeNames() { return { "Low Cut", "High Cut", "Bell", "Low Shelf", "High Shelf", "Band Pass", "Notch" }; }

    enum class LfoShape { sine, triangle, sawUp, sawDown, square, sampleHold, smoothRandom };
    inline juce::StringArray lfoShapeNames() { return { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Sample & Hold", "Smooth Random" }; }

    inline juce::StringArray modSourceNames() { return { "None", "LFO 1", "LFO 2", "Envelope" }; }

    enum class ModTarget { none, time, timeL, timeR, feedback, cross, f1, f2, f3, f4, drive, diffusion, pitch, pan, width, mix, count };
    inline juce::StringArray modTargetNames()
    {
        return { "None", "Time", "Time L", "Time R", "Feedback", "Cross", "Filter 1 Freq", "Filter 2 Freq", "Filter 3 Freq", "Filter 4 Freq",
                 "Drive", "Diffusion", "Pitch", "Pan", "Width", "Mix" };
    }

    inline juce::String formatMs (float ms)
    {
        if (ms >= 1000.0f) return juce::String (ms / 1000.0f, 2) + " s";
        if (ms >= 100.0f) return juce::String (juce::roundToInt (ms)) + " ms";
        return juce::String (ms, 1) + " ms";
    }

    inline juce::String formatHz (float hz)
    {
        if (hz >= 1000.0f) return juce::String (hz / 1000.0f, hz >= 10000.0f ? 1 : 2) + " kHz";
        if (hz >= 100.0f) return juce::String (juce::roundToInt (hz)) + " Hz";
        return juce::String (hz, 1) + " Hz";
    }

    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;
        AudioProcessorValueTreeState::ParameterLayout layout;
        int v = 1;
        auto pctAttr = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int) { return String (roundToInt (x)) + " %"; })
                                                      .withValueFromStringFunction ([] (const String& s) { return s.retainCharacters ("-0123456789.").getFloatValue(); });
        auto dbAttr = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int) { return (x > 0.05f ? "+" : "") + String (x, 1) + " dB"; })
                                                     .withValueFromStringFunction ([] (const String& s) { return s.retainCharacters ("-0123456789.").getFloatValue(); });
        auto msAttr = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int) { return formatMs (x); })
                                                     .withValueFromStringFunction ([] (const String& s)
                                                     {
                                                         const auto t = s.trim().toLowerCase();
                                                         const float n = t.retainCharacters ("0123456789.").getFloatValue();
                                                         return (t.endsWith ("s") && ! t.endsWith ("ms")) ? n * 1000.0f : n;
                                                     });
        auto hzAttr = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int) { return formatHz (x); })
                                                     .withValueFromStringFunction ([] (const String& s)
                                                     {
                                                         const auto t = s.trim().toLowerCase();
                                                         const float n = t.retainCharacters ("0123456789.").getFloatValue();
                                                         return t.contains ("k") ? n * 1000.0f : n;
                                                     });

        NormalisableRange<float> timeRange (1.0f, maxDelayMs, 0.0f);
        timeRange.setSkewForCentre (400.0f);

        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::timeL, v }, "Time L", timeRange, 375.0f, msAttr));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::timeR, v }, "Time R", timeRange, 375.0f, msAttr));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::link, v }, "Link L/R", StringArray { "Off", "On" }, 1));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::sync, v }, "Sync", StringArray { "Off", "On" }, 1));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::divL, v }, "Note L", divisionNames(), 10));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::divR, v }, "Note R", divisionNames(), 10));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::feedback, v }, "Feedback", NormalisableRange<float> (0.0f, 120.0f), 45.0f, pctAttr));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::cross, v }, "Cross Feedback", NormalisableRange<float> (0.0f, 100.0f), 0.0f, pctAttr));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::routing, v }, "Routing", StringArray { "Stereo", "Ping-Pong", "Mono" }, 0));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::timeMode, v }, "Time Change", StringArray { "Tape", "Fade", "Jump" }, 0));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::width, v }, "Width", NormalisableRange<float> (0.0f, 200.0f), 100.0f, pctAttr));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::pan, v }, "Pan", NormalisableRange<float> (-100.0f, 100.0f), 0.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int)
            {
                const int p = roundToInt (x);
                return p == 0 ? String ("C") : p < 0 ? String (-p) + " L" : String (p) + " R";
            }).withValueFromStringFunction ([] (const String& s)
            {
                const auto t = s.trim().toUpperCase();
                const float n = t.retainCharacters ("-0123456789.").getFloatValue();
                return t.endsWith ("L") ? -std::abs (n) : n;
            })));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::drive, v }, "Drive", NormalisableRange<float> (0.0f, 100.0f), 0.0f, pctAttr));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::driveStyle, v }, "Drive Style", StringArray { "Tape", "Tube", "Clip" }, 0));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::diffusion, v }, "Diffusion", NormalisableRange<float> (0.0f, 100.0f), 0.0f, pctAttr));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::diffSize, v }, "Diffusion Size", NormalisableRange<float> (0.0f, 100.0f), 50.0f, pctAttr));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::pitch, v }, "Pitch", NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int)
            {
                if (std::abs (x) < 0.005f) return String ("0 st");
                return (x > 0 ? "+" : "") + String (x, std::abs (x - (float) roundToInt (x)) < 0.005f ? 0 : 2) + " st";
            }).withValueFromStringFunction ([] (const String& s) { return s.retainCharacters ("-0123456789.").getFloatValue(); })));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::duck, v }, "Ducking", NormalisableRange<float> (0.0f, 100.0f), 0.0f, pctAttr));
        {
            NormalisableRange<float> r (20.0f, 2000.0f); r.setSkewForCentre (250.0f);
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::duckRelease, v }, "Duck Release", r, 250.0f, msAttr));
        }
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::freeze, v }, "Freeze", StringArray { "Off", "On" }, 0));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::input, v }, "Input", NormalisableRange<float> (-24.0f, 24.0f), 0.0f, dbAttr));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::mix, v }, "Mix", NormalisableRange<float> (0.0f, 100.0f), 30.0f, pctAttr));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::output, v }, "Output", NormalisableRange<float> (-24.0f, 24.0f), 0.0f, dbAttr));

        // feedback filters
        const FilterType defType[] { FilterType::lowCut, FilterType::highCut, FilterType::bell, FilterType::bell };
        const float defFreq[] { 120.0f, 7000.0f, 600.0f, 2500.0f };
        const bool defOn[] { true, true, false, false };
        for (int i = 0; i < numFilters; ++i)
        {
            const String n = "Filter " + String (i + 1) + " ";
            NormalisableRange<float> fr (20.0f, 20000.0f); fr.setSkewForCentre (632.0f);
            NormalisableRange<float> qr (0.1f, 18.0f); qr.setSkewForCentre (1.0f);
            layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::filt (i, "On"), v }, n + "On", StringArray { "Off", "On" }, defOn[i] ? 1 : 0));
            layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::filt (i, "Type"), v }, n + "Type", filterTypeNames(), (int) defType[i]));
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::filt (i, "Freq"), v }, n + "Freq", fr, defFreq[i], hzAttr));
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::filt (i, "Gain"), v }, n + "Gain", NormalisableRange<float> (-24.0f, 24.0f), 0.0f, dbAttr));
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::filt (i, "Q"), v }, n + "Q", qr, 0.707f,
                AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int) { return String (x, 2); })));
        }

        // modulation
        for (int i = 0; i < numLfos; ++i)
        {
            const String n = "LFO " + String (i + 1) + " ";
            NormalisableRange<float> rr (0.01f, 20.0f); rr.setSkewForCentre (1.0f);
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::lfo (i, "Rate"), v }, n + "Rate", rr, i == 0 ? 0.6f : 0.15f, hzAttr));
            layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::lfo (i, "Sync"), v }, n + "Sync", StringArray { "Off", "On" }, 0));
            layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::lfo (i, "Div"), v }, n + "Note", divisionNames(), 15));
            layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::lfo (i, "Shape"), v }, n + "Shape", lfoShapeNames(), i == 0 ? 0 : 6));
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::lfo (i, "Phase"), v }, n + "Stereo Phase", NormalisableRange<float> (0.0f, 180.0f), 0.0f,
                AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int) { return String (roundToInt (x)) + juce::String (juce::CharPointer_UTF8 ("\xc2\xb0")); })));
        }
        {
            NormalisableRange<float> ar (0.1f, 200.0f); ar.setSkewForCentre (10.0f);
            NormalisableRange<float> rr (5.0f, 2000.0f); rr.setSkewForCentre (150.0f);
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::envAttack, v }, "Envelope Attack", ar, 5.0f, msAttr));
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::envRelease, v }, "Envelope Release", rr, 150.0f, msAttr));
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::envSens, v }, "Envelope Sensitivity", NormalisableRange<float> (-24.0f, 24.0f), 0.0f, dbAttr));
        }
        for (int i = 0; i < numSlots; ++i)
        {
            const String n = "Mod " + String (i + 1) + " ";
            layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::slot (i, "Src"), v }, n + "Source", modSourceNames(), 0));
            layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::slot (i, "Dst"), v }, n + "Target", modTargetNames(), 0));
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::slot (i, "Amt"), v }, n + "Amount", NormalisableRange<float> (-100.0f, 100.0f), 0.0f, pctAttr));
        }
        return layout;
    }
}
