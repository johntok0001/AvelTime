#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace avel::time;

AvelTimeProcessor::AvelTimeProcessor()
    : juce::AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                             .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undo, "AvelTime", createLayout())
{
    auto get = [this] (const juce::String& id) { auto* v = apvts.getRawParameterValue (id); jassert (v != nullptr); return v; };
    c.timeL = get (pid::timeL); c.timeR = get (pid::timeR); c.link = get (pid::link); c.sync = get (pid::sync);
    c.divL = get (pid::divL); c.divR = get (pid::divR); c.feedback = get (pid::feedback); c.cross = get (pid::cross);
    c.routing = get (pid::routing); c.timeMode = get (pid::timeMode); c.width = get (pid::width); c.pan = get (pid::pan);
    c.drive = get (pid::drive); c.driveStyle = get (pid::driveStyle); c.diffusion = get (pid::diffusion); c.diffSize = get (pid::diffSize);
    c.pitch = get (pid::pitch); c.duck = get (pid::duck); c.duckRelease = get (pid::duckRelease); c.freeze = get (pid::freeze);
    c.input = get (pid::input); c.mix = get (pid::mix); c.output = get (pid::output);
    c.envAttack = get (pid::envAttack); c.envRelease = get (pid::envRelease); c.envSens = get (pid::envSens);
    for (int i = 0; i < numFilters; ++i)
    {
        c.fOn[i] = get (pid::filt (i, "On")); c.fType[i] = get (pid::filt (i, "Type")); c.fFreq[i] = get (pid::filt (i, "Freq"));
        c.fGain[i] = get (pid::filt (i, "Gain")); c.fQ[i] = get (pid::filt (i, "Q"));
    }
    for (int i = 0; i < numLfos; ++i)
    {
        c.lRate[i] = get (pid::lfo (i, "Rate")); c.lSync[i] = get (pid::lfo (i, "Sync")); c.lDiv[i] = get (pid::lfo (i, "Div"));
        c.lShape[i] = get (pid::lfo (i, "Shape")); c.lPhase[i] = get (pid::lfo (i, "Phase"));
    }
    for (int i = 0; i < numSlots; ++i)
    {
        c.mSrc[i] = get (pid::slot (i, "Src")); c.mDst[i] = get (pid::slot (i, "Dst")); c.mAmt[i] = get (pid::slot (i, "Amt"));
    }

    uiState.setProperty ("w", 1180, nullptr);
    uiState.setProperty ("h", 720, nullptr);
    uiState.setProperty ("view", 0, nullptr);
    uiState.setProperty ("band", 0, nullptr);
    engine.setSettings (readSettings());
}

bool AvelTimeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    const auto in = layouts.getMainInputChannelSet();
    return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void AvelTimeProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    work.setSize (2, juce::jmax (1, samplesPerBlock));
    engine.setSettings (readSettings());
    engine.prepare (sampleRate, samplesPerBlock);
}

std::pair<float, float> AvelTimeProcessor::resolvedTimesMs() const
{
    const bool link = c.link->load() > 0.5f;
    const bool sync = c.sync->load() > 0.5f;
    float l, r;
    if (sync)
    {
        const double qms = 60000.0 / juce::jlimit (20.0, 999.0, bpm.load());
        const auto& d = divisions();
        const int il = juce::jlimit (0, (int) d.size() - 1, (int) c.divL->load());
        const int ir = link ? il : juce::jlimit (0, (int) d.size() - 1, (int) c.divR->load());
        l = (float) (d[(size_t) il].quarters * qms);
        r = (float) (d[(size_t) ir].quarters * qms);
    }
    else
    {
        l = c.timeL->load();
        r = link ? l : c.timeR->load();
    }
    return { juce::jlimit (1.0f, maxDelayMs, l), juce::jlimit (1.0f, maxDelayMs, r) };
}

dsp::Settings AvelTimeProcessor::readSettings()
{
    dsp::Settings s;
    auto [l, r] = resolvedTimesMs();
    s.timeMsL = l; s.timeMsR = r;
    s.feedback = c.feedback->load() * 0.01f;
    s.cross = c.cross->load() * 0.01f;
    s.routing = (int) c.routing->load();
    s.timeMode = (int) c.timeMode->load();
    s.width = c.width->load() * 0.01f;
    s.pan = c.pan->load() * 0.01f;
    s.drive = c.drive->load() * 0.01f;
    s.driveStyle = (int) c.driveStyle->load();
    s.diffusion = c.diffusion->load() * 0.01f;
    s.diffSize = c.diffSize->load() * 0.01f;
    s.pitch = c.pitch->load();
    s.duck = c.duck->load() * 0.01f;
    s.duckReleaseMs = c.duckRelease->load();
    s.freeze = c.freeze->load() > 0.5f;
    s.inputGain = juce::Decibels::decibelsToGain (c.input->load());
    s.mix = c.mix->load() * 0.01f;
    s.outputGain = juce::Decibels::decibelsToGain (c.output->load());
    for (int i = 0; i < numFilters; ++i)
    {
        auto& f = s.filters[(size_t) i];
        f.on = c.fOn[i]->load() > 0.5f;
        f.type = (FilterType) (int) c.fType[i]->load();
        f.freq = c.fFreq[i]->load();
        f.gainDb = c.fGain[i]->load();
        f.q = c.fQ[i]->load();
    }
    for (int i = 0; i < numLfos; ++i)
    {
        auto& lf = s.lfos[(size_t) i];
        lf.rateHz = c.lRate[i]->load();
        lf.sync = c.lSync[i]->load() > 0.5f;
        lf.quarters = divisions()[(size_t) juce::jlimit (0, (int) divisions().size() - 1, (int) c.lDiv[i]->load())].quarters;
        lf.shape = (LfoShape) (int) c.lShape[i]->load();
        lf.stereoPhase = c.lPhase[i]->load();
    }
    s.envAttackMs = c.envAttack->load();
    s.envReleaseMs = c.envRelease->load();
    s.envSensDb = c.envSens->load();
    for (int i = 0; i < numSlots; ++i)
    {
        auto& sl = s.slots[(size_t) i];
        sl.src = (int) c.mSrc[i]->load();
        sl.dst = (ModTarget) (int) c.mDst[i]->load();
        sl.amount = c.mAmt[i]->load() * 0.01f;
    }
    s.bpm = bpm.load();
    return s;
}

void AvelTimeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nIn = getTotalNumInputChannels(), nOut = getTotalNumOutputChannels();
    if (n == 0) return;

    double ppq = 0.0; bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm.store (*b);
            if (auto q = pos->getPpqPosition()) ppq = *q;
            playing = pos->getIsPlaying();
        }

    auto s = readSettings();
    s.ppq = ppq; s.playing = playing;
    engine.setSettings (s);

    if (work.getNumSamples() < n) work.setSize (2, n, false, false, true);
    float* L = work.getWritePointer (0);
    float* R = work.getWritePointer (1);
    const float* inL = buffer.getReadPointer (0);
    const float* inR = nIn > 1 ? buffer.getReadPointer (1) : inL;
    juce::FloatVectorOperations::copy (L, inL, n);
    juce::FloatVectorOperations::copy (R, inR, n);

    float pi[2] { 0, 0 };
    for (int i = 0; i < n; ++i) { pi[0] = std::max (pi[0], std::abs (L[i])); pi[1] = std::max (pi[1], std::abs (R[i])); }
    const float inGain = s.inputGain;
    for (int ch = 0; ch < 2; ++ch) inPeak[ch].store (std::max (pi[ch] * inGain, inPeak[ch].load()));

    // process in small pieces so the scope gets the wet signal
    int pos = 0;
    while (pos < n)
    {
        const int len = std::min (64, n - pos);
        engine.process (L + pos, R + pos, len);
        for (int i = 0; i < len; ++i) scopeIn.push (0.5f * (inL[pos + i] + inR[pos + i]) * inGain);
        // the engine keeps only its latest wet sample; approximate wet by output minus dry for the scope
        const float dryG = std::min (1.0f, 2.0f * (1.0f - s.mix)) * s.outputGain;
        for (int i = 0; i < len; ++i)
        {
            const float dry = 0.5f * (inL[pos + i] + inR[pos + i]) * inGain * dryG;
            scopeWet.push (0.5f * (L[pos + i] + R[pos + i]) - dry);
        }
        pos += len;
    }

    float po[2] { 0, 0 };
    for (int i = 0; i < n; ++i) { po[0] = std::max (po[0], std::abs (L[i])); po[1] = std::max (po[1], std::abs (R[i])); }
    for (int ch = 0; ch < 2; ++ch) outPeak[ch].store (std::max (po[ch], outPeak[ch].load()));

    if (nOut == 1)
    {
        auto* o = buffer.getWritePointer (0);
        for (int i = 0; i < n; ++i) o[i] = 0.5f * (L[i] + R[i]);
    }
    else
    {
        buffer.copyFrom (0, 0, L, n);
        buffer.copyFrom (1, 0, R, n);
        for (int ch = 2; ch < nOut; ++ch) buffer.clear (ch, 0, n);
    }
}

void AvelTimeProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", presets.currentName, nullptr);
    state.removeChild (state.getChildWithName ("UI"), nullptr);
    state.appendChild (uiState.createCopy(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void AvelTimeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto tree = juce::ValueTree::fromXml (*xml);
    if (! tree.hasType (apvts.state.getType())) return;
    auto ui = tree.getChildWithName ("UI");
    if (ui.isValid())
    {
        uiState.copyPropertiesFrom (ui, nullptr);
        tree.removeChild (ui, nullptr);
    }
    presets.currentName = tree.getProperty ("preset", "Default").toString();
    apvts.replaceState (tree);
}

void AvelTimeProcessor::switchAB (int slot)
{
    slot = juce::jlimit (0, 1, slot);
    if (slot == activeAB) return;
    abState[activeAB] = apvts.copyState();
    if (abState[slot].isValid())
    {
        auto t = abState[slot].createCopy();
        for (auto* prm : getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
            {
                const auto child = t.getChildWithProperty ("id", rp->getParameterID());
                if (child.isValid()) rp->setValueNotifyingHost (rp->convertTo0to1 ((float) child.getProperty ("value")));
            }
    }
    activeAB = slot;
}

void AvelTimeProcessor::copyABToOther()
{
    abState[1 - activeAB] = apvts.copyState();
}

juce::AudioProcessorEditor* AvelTimeProcessor::createEditor() { return new AvelTimeEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AvelTimeProcessor(); }
