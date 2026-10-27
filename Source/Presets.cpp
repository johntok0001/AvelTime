#include "Presets.h"
#include "Params.h"

namespace avel::time
{
    namespace
    {
        // division indices (see Params.h)
        enum Div { d64, d32T, d32, d16T, d16, d16D, d8T, d8, d8D, d4T, d4, d4D, d2T, d2, d2D, d1, d1D, d21, d41 };
        enum Tgt { tNone, tTime, tTimeL, tTimeR, tFeedback, tCross, tF1, tF2, tF3, tF4, tDrive, tDiff, tPitch, tPan, tWidth, tMix };
        enum Ft { lowCut, highCut, bell, lowShelf, highShelf, bandPass, notch };
        enum Shape { sine, tri, sawUp, sawDown, square, sh, smooth };

        using V = std::vector<std::pair<juce::String, float>>;

        V filt (int i, bool on, int type, float freq, float gain = 0.0f, float q = 0.707f)
        {
            return { { pid::filt (i, "On"), on ? 1.0f : 0.0f }, { pid::filt (i, "Type"), (float) type }, { pid::filt (i, "Freq"), freq },
                     { pid::filt (i, "Gain"), gain }, { pid::filt (i, "Q"), q } };
        }
        V slot (int i, int src, int dst, float amt)
        {
            return { { pid::slot (i, "Src"), (float) src }, { pid::slot (i, "Dst"), (float) dst }, { pid::slot (i, "Amt"), amt * 100.0f } };
        }
        V lfo (int i, float rate, int shape, float phase = 0.0f, bool sync = false, int div = d1)
        {
            return { { pid::lfo (i, "Rate"), rate }, { pid::lfo (i, "Shape"), (float) shape }, { pid::lfo (i, "Phase"), phase },
                     { pid::lfo (i, "Sync"), sync ? 1.0f : 0.0f }, { pid::lfo (i, "Div"), (float) div } };
        }
        V cat (std::initializer_list<V> parts)
        {
            V out;
            for (auto& p : parts) out.insert (out.end(), p.begin(), p.end());
            return out;
        }
        V syncTimes (int l, int r) { return { { pid::sync, 1 }, { pid::divL, (float) l }, { pid::divR, (float) r }, { pid::link, l == r ? 1.0f : 0.0f } }; }
        V msTimes (float l, float r) { return { { pid::sync, 0 }, { pid::timeL, l }, { pid::timeR, r }, { pid::link, std::abs (l - r) < 0.001f ? 1.0f : 0.0f } }; }
    }

    const std::vector<PresetManager::Factory>& PresetManager::factory()
    {
        static const std::vector<Factory> f {
            { "Quarter Note", "Basics", cat ({ syncTimes (d4, d4), { { pid::feedback, 40 }, { pid::mix, 28 } } }) },
            { "Dotted Eighth", "Basics", cat ({ syncTimes (d8D, d8D), { { pid::feedback, 45 }, { pid::mix, 30 } }, filt (1, true, highCut, 6000) }) },
            { "Wide Stereo 1/8 + 1/8D", "Basics", cat ({ syncTimes (d8, d8D), { { pid::feedback, 38 }, { pid::mix, 30 }, { pid::width, 140 } } }) },
            { "Slapback", "Basics", cat ({ msTimes (95, 95), { { pid::feedback, 8 }, { pid::mix, 35 }, { pid::drive, 18 } }, filt (1, true, highCut, 5200) }) },
            { "Ping-Pong 1/8", "Stereo", cat ({ syncTimes (d8, d8), { { pid::routing, 1 }, { pid::feedback, 55 }, { pid::mix, 30 } }, filt (0, true, lowCut, 220) }) },
            { "Ping-Pong Triplets", "Stereo", cat ({ syncTimes (d8T, d8T), { { pid::routing, 1 }, { pid::feedback, 50 }, { pid::mix, 28 } }, filt (1, true, highCut, 5000) }) },
            { "Haas Widener", "Stereo", cat ({ msTimes (1, 16), { { pid::feedback, 0 }, { pid::mix, 50 }, { pid::width, 160 } } }) },
            { "Tape Echo", "Character", cat ({ syncTimes (d8D, d8D), { { pid::feedback, 58 }, { pid::mix, 30 }, { pid::drive, 35 }, { pid::driveStyle, 0 } },
                                             filt (0, true, lowCut, 160), filt (1, true, highCut, 3800),
                                             lfo (0, 0.7f, sine), lfo (1, 6.0f, smooth), slot (0, 1, tTime, 0.06f), slot (1, 2, tTime, 0.025f) }) },
            { "Dub Siren", "Character", cat ({ syncTimes (d4D, d4D), { { pid::feedback, 92 }, { pid::mix, 40 }, { pid::drive, 45 }, { pid::driveStyle, 1 } },
                                             filt (0, true, lowCut, 280, 0, 1.2f), filt (1, true, highCut, 2400, 0, 1.1f), filt (2, true, bell, 900, 4, 1.2f) }) },
            { "Lo-Fi Telephone", "Character", cat ({ syncTimes (d8, d8), { { pid::feedback, 50 }, { pid::mix, 35 }, { pid::drive, 60 }, { pid::driveStyle, 2 } },
                                                   filt (0, true, lowCut, 600, 0, 1.4f), filt (1, true, highCut, 2600, 0, 1.4f) }) },
            { "Analog Wobble", "Modulated", cat ({ msTimes (320, 340), { { pid::feedback, 50 }, { pid::mix, 30 }, { pid::drive, 22 } },
                                                 filt (1, true, highCut, 4500), lfo (0, 0.45f, sine, 90), slot (0, 1, tTime, 0.15f) }) },
            { "Chorus Echo", "Modulated", cat ({ msTimes (24, 31), { { pid::feedback, 18 }, { pid::mix, 40 }, { pid::width, 170 } },
                                               lfo (0, 0.8f, tri, 120), slot (0, 1, tTime, 0.35f) }) },
            { "Filter Sweep 1/8", "Modulated", cat ({ syncTimes (d8, d8D), { { pid::feedback, 62 }, { pid::mix, 32 } },
                                                     filt (2, true, bandPass, 1200, 0, 2.5f),
                                                     lfo (0, 0.2f, tri, 60, true, d21), slot (0, 1, tF3, 0.55f) }) },
            { "Shimmer Up", "Space", cat ({ syncTimes (d4, d4D), { { pid::feedback, 70 }, { pid::mix, 35 }, { pid::pitch, 12 }, { pid::diffusion, 60 }, { pid::diffSize, 70 } },
                                           filt (0, true, lowCut, 400), filt (1, true, highCut, 9000) }) },
            { "Octave Down Dub", "Space", cat ({ syncTimes (d4, d4), { { pid::feedback, 55 }, { pid::mix, 32 }, { pid::pitch, -12 }, { pid::drive, 20 } },
                                                filt (0, true, lowCut, 90), filt (1, true, highCut, 3000) }) },
            { "Ambient Cloud", "Space", cat ({ msTimes (620, 780), { { pid::feedback, 78 }, { pid::mix, 40 }, { pid::diffusion, 90 }, { pid::diffSize, 85 }, { pid::width, 150 } },
                                              filt (0, true, lowCut, 250), filt (1, true, highCut, 6500),
                                              lfo (1, 0.12f, smooth, 90), slot (0, 2, tTime, 0.2f) }) },
            { "Ducked Vocal 1/4", "Mixing", cat ({ syncTimes (d4, d4), { { pid::feedback, 45 }, { pid::mix, 38 }, { pid::duck, 70 }, { pid::duckRelease, 300 } },
                                                  filt (0, true, lowCut, 300), filt (1, true, highCut, 6000) }) },
            { "Envelope Drive", "Mixing", cat ({ syncTimes (d8D, d8D), { { pid::feedback, 50 }, { pid::mix, 30 }, { pid::drive, 15 } },
                                                filt (1, true, highCut, 2500), slot (0, 3, tF2, 0.5f), slot (1, 3, tDrive, 0.4f) }) },
            { "Runaway Oscillation", "Experimental", cat ({ syncTimes (d8D, d8D), { { pid::feedback, 108 }, { pid::mix, 35 }, { pid::drive, 30 } },
                                                            filt (0, true, lowCut, 200), filt (1, true, highCut, 3500),
                                                            lfo (0, 0.08f, sine), slot (0, 1, tTime, 0.4f) }) },
            { "Glitch Jumps", "Experimental", cat ({ syncTimes (d16, d16D), { { pid::feedback, 55 }, { pid::mix, 35 }, { pid::timeMode, 2 } },
                                                    lfo (0, 4.0f, sh, 90, true, d8), slot (0, 1, tTime, 0.6f), slot (1, 1, tPan, 0.5f) }) },
        };
        return f;
    }

    juce::File PresetManager::userFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Avel").getChildFile ("Avel Time").getChildFile ("Presets");
    }

    juce::Array<juce::File> PresetManager::userPresets() const
    {
        auto files = userFolder().findChildFiles (juce::File::findFiles, false, "*.xml");
        files.sort();
        return files;
    }

    void PresetManager::resetToDefaults()
    {
        for (auto* p : apvts.processor.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                rp->setValueNotifyingHost (rp->getDefaultValue());
    }

    void PresetManager::applyValues (const std::vector<std::pair<juce::String, float>>& v)
    {
        resetToDefaults();
        for (auto& [id, val] : v)
            if (auto* p = apvts.getParameter (id))
                p->setValueNotifyingHost (p->convertTo0to1 (val));
    }

    void PresetManager::loadFactory (int index)
    {
        const auto& f = factory();
        if (index < 0 || index >= (int) f.size()) { resetToDefaults(); currentName = "Default"; currentFactory = -1; return; }
        applyValues (f[(size_t) index].values);
        currentName = f[(size_t) index].name;
        currentFactory = index;
        currentFile = juce::File();
    }

    bool PresetManager::loadUser (const juce::File& file)
    {
        auto xml = juce::XmlDocument::parse (file);
        if (xml == nullptr) return false;
        auto tree = juce::ValueTree::fromXml (*xml);
        if (! tree.isValid()) return false;
        for (auto* p : apvts.processor.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            {
                const auto child = tree.getChildWithProperty ("id", rp->getParameterID());
                const float v = child.isValid() ? (float) child.getProperty ("value") : rp->convertFrom0to1 (rp->getDefaultValue());
                rp->setValueNotifyingHost (rp->convertTo0to1 (v));
            }
        currentName = file.getFileNameWithoutExtension();
        currentFactory = -1;
        currentFile = file;
        return true;
    }

    bool PresetManager::saveUser (const juce::String& name)
    {
        const auto clean = juce::File::createLegalFileName (name.trim());
        if (clean.isEmpty()) return false;
        userFolder().createDirectory();
        auto state = apvts.copyState();
        juce::ValueTree t ("AvelTimePreset");
        for (int i = 0; i < state.getNumChildren(); ++i)
            if (state.getChild (i).hasType ("PARAM")) t.appendChild (state.getChild (i).createCopy(), nullptr);
        auto file = userFolder().getChildFile (clean + ".xml");
        if (auto xml = t.createXml(); xml != nullptr && xml->writeTo (file))
        {
            currentName = clean; currentFactory = -1; currentFile = file;
            return true;
        }
        return false;
    }

    void PresetManager::step (int dir)
    {
        const int nf = (int) factory().size();
        const auto users = userPresets();
        const int total = nf + users.size();
        int cur = currentFactory;
        if (cur < 0 && currentFile.existsAsFile()) cur = nf + users.indexOf (currentFile);
        cur = cur < 0 ? (dir > 0 ? 0 : total - 1) : (cur + dir + total) % total;
        if (cur < nf) loadFactory (cur);
        else loadUser (users[cur - nf]);
    }
}
