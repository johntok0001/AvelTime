#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "Presets.h"
#include "DSP/DelayEngine.h"

/** Lock-free mono sample FIFO for the analyzer (audio thread writes, UI reads the latest block). */
struct ScopeFifo
{
    static constexpr int size = 8192;
    std::array<float, size> data {};
    std::atomic<int> write { 0 };
    void push (float x) noexcept
    {
        const int w = write.load (std::memory_order_relaxed);
        data[(size_t) w] = x;
        write.store ((w + 1) & (size - 1), std::memory_order_release);
    }
    void copyLatest (float* dest, int n) const noexcept
    {
        const int w = write.load (std::memory_order_acquire);
        for (int i = 0; i < n; ++i) dest[i] = data[(size_t) ((w - n + i) & (size - 1))];
    }
};

class AvelTimeProcessor : public juce::AudioProcessor
{
public:
    AvelTimeProcessor();
    ~AvelTimeProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Avel Time"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 30.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    /** Delay times (ms) after tempo sync and link, as the engine uses them. */
    std::pair<float, float> resolvedTimesMs() const;
    double getBpm() const { return bpm.load(); }
    double getSampleRateSafe() const { return sr > 0 ? sr : 48000.0; }

    // A/B
    void switchAB (int slot);
    void copyABToOther();
    int activeAB = 0;

    juce::UndoManager undo;
    juce::AudioProcessorValueTreeState apvts;
    avel::time::PresetManager presets { apvts };
    avel::time::dsp::Engine engine;

    // UI state saved with the session
    juce::ValueTree uiState { "UI" };

    // metering / scope
    std::atomic<float> inPeak[2] {}, outPeak[2] {};
    ScopeFifo scopeIn, scopeWet;

private:
    avel::time::dsp::Settings readSettings();
    juce::RangedAudioParameter* p (const juce::String& id) const { return apvts.getParameter (id); }
    float raw (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }

    struct Cache
    {
        std::atomic<float>* timeL, *timeR, *link, *sync, *divL, *divR, *feedback, *cross, *routing, *timeMode, *width, *pan,
            *drive, *driveStyle, *diffusion, *diffSize, *pitch, *duck, *duckRelease, *freeze, *input, *mix, *output,
            *envAttack, *envRelease, *envSens;
        std::atomic<float>* fOn[avel::time::numFilters], *fType[avel::time::numFilters], *fFreq[avel::time::numFilters],
            *fGain[avel::time::numFilters], *fQ[avel::time::numFilters];
        std::atomic<float>* lRate[avel::time::numLfos], *lSync[avel::time::numLfos], *lDiv[avel::time::numLfos],
            *lShape[avel::time::numLfos], *lPhase[avel::time::numLfos];
        std::atomic<float>* mSrc[avel::time::numSlots], *mDst[avel::time::numSlots], *mAmt[avel::time::numSlots];
    } c;

    juce::ValueTree abState[2];
    std::atomic<double> bpm { 120.0 };
    double sr = 0.0;
    juce::AudioBuffer<float> work;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AvelTimeProcessor)
};
