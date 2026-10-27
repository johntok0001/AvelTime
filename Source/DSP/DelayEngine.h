#pragma once

// Avel Time delay engine.
// Two delay lines (L/R) with a processing chain inside the feedback loop:
//   input + feedback (with cross-feed) -> 4 filters -> drive -> safety limiter -> DC block
//   -> diffusion -> pitch shifter -> delay line
// Delay-time changes follow one of three behaviours (tape glide, crossfade, quick jump).
// Two LFOs and an envelope follower drive six modulation slots.

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <array>
#include <atomic>
#include <cmath>
#include "FilterMath.h"

namespace avel::time::dsp
{
    //==============================================================================================
    class DelayLine
    {
    public:
        void prepare (int minSize)
        {
            int n = 1;
            while (n < minSize + 8) n <<= 1;
            buf.assign ((size_t) n, 0.0f);
            mask = n - 1;
            w = 0;
        }
        void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
        int size() const { return (int) buf.size(); }

        inline void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }

        /** Read d samples behind the newest written sample (d >= 1 means the last pushed sample at d = 1). */
        inline float read (float d) const
        {
            d = std::clamp (d, 1.0f, (float) (mask - 4));
            const float fi = std::floor (d);
            const float t = d - fi;
            const int i = (int) fi;
            const float xm1 = at (i - 1), x0 = at (i), x1 = at (i + 1), x2 = at (i + 2);
            // 4-point Hermite
            const float c1 = 0.5f * (x1 - xm1);
            const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
            const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
            return ((c3 * t + c2) * t + c1) * t + x0;
        }

    private:
        inline float at (int d) const { return buf[(size_t) ((w - d) & mask)]; }
        std::vector<float> buf;
        int mask = 0, w = 0;
    };

    //==============================================================================================
    struct FilterSettings { bool on = false; FilterType type = FilterType::bell; float freq = 1000.0f, gainDb = 0.0f, q = 0.707f; };
    struct LfoSettings    { float rateHz = 1.0f; bool sync = false; double quarters = 4.0; LfoShape shape = LfoShape::sine; float stereoPhase = 0.0f; };
    struct SlotSettings   { int src = 0; ModTarget dst = ModTarget::none; float amount = 0.0f; };

    struct Settings
    {
        float timeMsL = 375.0f, timeMsR = 375.0f;
        float feedback = 0.45f, cross = 0.0f;
        int routing = 0;      // 0 stereo, 1 ping-pong, 2 mono
        int timeMode = 0;     // 0 tape, 1 fade, 2 jump
        float width = 1.0f, pan = 0.0f;
        float drive = 0.0f; int driveStyle = 0;
        float diffusion = 0.0f, diffSize = 0.5f;
        float pitch = 0.0f;
        float duck = 0.0f, duckReleaseMs = 250.0f;
        bool freeze = false;
        float inputGain = 1.0f, mix = 0.3f, outputGain = 1.0f;
        std::array<FilterSettings, numFilters> filters;
        std::array<LfoSettings, numLfos> lfos;
        float envAttackMs = 5.0f, envReleaseMs = 150.0f, envSensDb = 0.0f;
        std::array<SlotSettings, numSlots> slots;
        double bpm = 120.0, ppq = 0.0; bool playing = false;
    };

    //==============================================================================================
    class Engine
    {
    public:
        static constexpr int controlBlock = 32;

        void prepare (double sr, int maxBlock)
        {
            sampleRate = sr;
            juce::ignoreUnused (maxBlock);
            const int maxDelay = (int) std::ceil ((maxDelayMs + 200.0f) * 0.001 * sr) + 64;
            for (auto& ch : chans)
            {
                ch.line.prepare (maxDelay);
                for (auto& ap : ch.ap) ap.line.prepare ((int) std::ceil (0.05 * sr));
                ch.pitchLine.prepare ((int) std::ceil (0.2 * sr));
            }
            reset();
        }

        void reset()
        {
            for (auto& ch : chans)
            {
                ch.line.clear();
                for (auto& f : ch.filt) f.reset();
                for (auto& ap : ch.ap) ap.line.clear();
                ch.pitchLine.clear();
                ch.dcX = ch.dcY = 0.0f;
                ch.adaaX1 = 0.0f;
                ch.dA = ch.dB = ch.dTape = -1.0f;
                ch.fade = 1.0f;
                ch.pitchPhase = 0.0f;
                ch.modTimePrev = ch.modTimeNext = 0.0f;
            }
            for (auto& p : lfoPhase) p = 0.0;
            env = duckEnv = 0.0f;
            duckGain = 1.0f;
            freezeAmt = 0.0f;
            pitchEngage = 0.0f;
            fastEnv = slowEnv = 0.0f;
            first = true;
        }

        const Settings& getSettings() const { return set; }
        void setSettings (const Settings& s) { set = s; }

        /** In-place stereo processing. */
        void process (float* L, float* R, int n)
        {
            if (first) initSmoothers();

            int pos = 0;
            while (pos < n)
            {
                const int len = std::min (controlBlock, n - pos);
                updateControl (len);
                processChunk (L + pos, R + pos, len);
                pos += len;
            }
            first = false;
        }

        // UI feedback
        std::atomic<float> lfoValue[numLfos] {};
        std::atomic<float> lfoPhaseOut[numLfos] {};
        std::atomic<float> envValue { 0.0f };
        std::atomic<float> duckValue { 1.0f };
        std::atomic<int64_t> sampleCounter { 0 }, lastOnset { -1000000 };
        std::atomic<float> effTimeMsL { 375.0f }, effTimeMsR { 375.0f };

        float getPitchLatencyMs() const { return pitchWindowMs * 0.5f; }

    private:
        //------------------------------------------------------------------------------------------
        struct Allpass
        {
            DelayLine line;
            inline float process (float x, float d, float g)
            {
                const float delayed = line.read (d);
                const float v = x + g * delayed;
                line.push (v);
                return delayed - g * v;
            }
        };

        struct Channel
        {
            DelayLine line;
            std::array<Svf, numFilters> filt;
            std::array<SvfCoeffs, numFilters> coeffs;
            std::array<bool, numFilters> filtOn {};
            std::array<Allpass, 4> ap;
            DelayLine pitchLine;
            float pitchPhase = 0.0f;
            float dcX = 0.0f, dcY = 0.0f;
            float adaaX1 = 0.0f;
            float dA = -1.0f, dB = -1.0f, dTape = -1.0f, fade = 1.0f;
            float targetD = 0.0f;
            float modTimePrev = 0.0f, modTimeNext = 0.0f;
            // per-channel control values for the chunk
            float fb = 0.0f, drive = 0.0f, diff = 0.0f, pitchRatio = 1.0f, pitchAmt = 0.0f;
        };

        //------------------------------------------------------------------------------------------
        static inline float hash01 (int64_t i, int seed)
        {
            uint32_t x = (uint32_t) ((uint64_t) i * 2654435761ull) ^ ((uint32_t) seed * 0x9E3779B9u);
            x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16;
            return (float) (x & 0xffffff) / (float) 0xffffff;
        }

        static float lfoShape (LfoShape s, double totalPhase, int seed)
        {
            const double fl = std::floor (totalPhase);
            const float p = (float) (totalPhase - fl);
            const auto cycle = (int64_t) fl;
            switch (s)
            {
                case LfoShape::sine:         return std::sin (juce::MathConstants<float>::twoPi * p);
                case LfoShape::triangle:     return p < 0.25f ? 4.0f * p : p < 0.75f ? 2.0f - 4.0f * p : 4.0f * p - 4.0f;
                case LfoShape::sawUp:        return 2.0f * p - 1.0f;
                case LfoShape::sawDown:      return 1.0f - 2.0f * p;
                case LfoShape::square:       return p < 0.5f ? 1.0f : -1.0f;
                case LfoShape::sampleHold:   return hash01 (cycle, seed) * 2.0f - 1.0f;
                case LfoShape::smoothRandom:
                {
                    const float a = hash01 (cycle, seed) * 2.0f - 1.0f, b = hash01 (cycle + 1, seed) * 2.0f - 1.0f;
                    const float t = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * p);
                    return a + (b - a) * t;
                }
            }
            return 0.0f;
        }

    public:
        /** Evaluate an LFO shape for display. */
        static float lfoDisplay (LfoShape s, double phase, int seed) { return lfoShape (s, phase, seed); }

    private:
        void initSmoothers()
        {
            const double r = 0.03;
            for (auto* sm : { &smFeedback, &smCross, &smWidth, &smPan, &smMix, &smIn, &smOut })
                sm->reset (sampleRate, r);
            smFeedback.setCurrentAndTargetValue (set.feedback);
            smCross.setCurrentAndTargetValue (set.cross);
            smWidth.setCurrentAndTargetValue (set.width);
            smPan.setCurrentAndTargetValue (set.pan);
            smMix.setCurrentAndTargetValue (set.mix);
            smIn.setCurrentAndTargetValue (set.inputGain);
            smOut.setCurrentAndTargetValue (set.outputGain);
            for (int i = 0; i < numFilters; ++i) filtFreqSm[(size_t) i] = set.filters[(size_t) i].freq;
            diffSizeSm = set.diffSize;
            freezeAmt = set.freeze ? 1.0f : 0.0f;
        }

        float msToSamples (float ms) const { return ms * 0.001f * (float) sampleRate; }

        void updateControl (int len)
        {
            const float dt = (float) len / (float) sampleRate;

            // ---- LFOs
            float lfoL[numLfos], lfoR[numLfos];
            for (int i = 0; i < numLfos; ++i)
            {
                const auto& l = set.lfos[(size_t) i];
                double rate = l.rateHz;
                if (l.sync)
                {
                    rate = set.bpm / 60.0 / std::max (1.0e-3, l.quarters);
                    if (set.playing && ! syncedLastBlock[i]) lfoPhase[(size_t) i] = set.ppq / std::max (1.0e-3, l.quarters);
                }
                syncedLastBlock[i] = l.sync && set.playing;
                lfoL[i] = lfoShape (l.shape, lfoPhase[(size_t) i], 17 + i * 31);
                lfoR[i] = lfoShape (l.shape, lfoPhase[(size_t) i] + l.stereoPhase / 360.0, 17 + i * 31);
                lfoPhase[(size_t) i] += rate * dt;
                if (lfoPhase[(size_t) i] > 1.0e6) lfoPhase[(size_t) i] -= std::floor (lfoPhase[(size_t) i]);
                lfoValue[i].store (lfoL[i], std::memory_order_relaxed);
                lfoPhaseOut[i].store ((float) (lfoPhase[(size_t) i] - std::floor (lfoPhase[(size_t) i])), std::memory_order_relaxed);
            }
            const float envV = std::clamp ((juce::Decibels::gainToDecibels (env, -100.0f) + 48.0f + set.envSensDb) / 48.0f, 0.0f, 1.0f);
            envValue.store (envV, std::memory_order_relaxed);

            // ---- modulation sums [target][channel]
            float mod[(int) ModTarget::count][2] {};
            for (auto& s : set.slots)
            {
                if (s.src == 0 || s.dst == ModTarget::none || std::abs (s.amount) < 1.0e-6f) continue;
                float vl = 0.0f, vr = 0.0f;
                if (s.src == 1)      { vl = lfoL[0]; vr = lfoR[0]; }
                else if (s.src == 2) { vl = lfoL[1]; vr = lfoR[1]; }
                else                 { vl = vr = envV; }
                mod[(int) s.dst][0] += vl * s.amount;
                mod[(int) s.dst][1] += vr * s.amount;
            }
            auto m = [&] (ModTarget t, int c) { return mod[(int) t][c]; };
            auto mAvg = [&] (ModTarget t) { return 0.5f * (mod[(int) t][0] + mod[(int) t][1]); };

            // ---- freeze, pitch engagement
            const float frzStep = dt / 0.04f;
            freezeAmt = set.freeze ? std::min (1.0f, freezeAmt + frzStep) : std::max (0.0f, freezeAmt - frzStep);

            bool pitchModded = false;
            for (auto& s : set.slots) if (s.src != 0 && s.dst == ModTarget::pitch && std::abs (s.amount) > 1.0e-6f) pitchModded = true;
            const bool wantPitch = std::abs (set.pitch) > 0.005f || pitchModded;
            const float pStep = dt / 0.05f;
            pitchEngage = wantPitch ? std::min (1.0f, pitchEngage + pStep) : std::max (0.0f, pitchEngage - pStep);

            // ---- smoothed globals
            smFeedback.setTargetValue (set.feedback);
            smCross.setTargetValue (std::clamp (set.cross + mAvg (ModTarget::cross), 0.0f, 1.0f));
            smWidth.setTargetValue (std::clamp (set.width + mAvg (ModTarget::width), 0.0f, 2.0f));
            smPan.setTargetValue (std::clamp (set.pan + mAvg (ModTarget::pan), -1.0f, 1.0f));
            smMix.setTargetValue (std::clamp (set.mix + mAvg (ModTarget::mix), 0.0f, 1.0f));
            smIn.setTargetValue (set.inputGain);
            smOut.setTargetValue (set.outputGain);

            const float smoothK = 1.0f - std::exp (-dt / 0.02f);
            diffSizeSm += (set.diffSize - diffSizeSm) * smoothK;
            for (int i = 0; i < numFilters; ++i)
                filtFreqSm[(size_t) i] += (set.filters[(size_t) i].freq - filtFreqSm[(size_t) i]) * smoothK;

            // ---- loop latency compensation (diffusion + pitch window)
            const float diffAmtBase = set.diffusion;
            const float apScale = 0.3f + 1.4f * diffSizeSm;

            // ---- per-channel controls
            for (int c = 0; c < 2; ++c)
            {
                auto& ch = chans[(size_t) c];
                ch.fb = std::clamp (m (ModTarget::feedback, c), -1.2f, 1.2f); // offset, added per sample to the smoothed base
                ch.drive = std::clamp (set.drive + m (ModTarget::drive, c), 0.0f, 1.0f);
                ch.diff = std::clamp (diffAmtBase + m (ModTarget::diffusion, c), 0.0f, 1.0f);
                const float semis = std::clamp (set.pitch + 12.0f * m (ModTarget::pitch, c), -24.0f, 24.0f);
                ch.pitchRatio = std::pow (2.0f, semis / 12.0f);
                ch.pitchAmt = std::clamp (std::abs (semis) / 0.1f, 0.0f, 1.0f);

                for (int i = 0; i < numFilters; ++i)
                {
                    const auto& f = set.filters[(size_t) i];
                    ch.filtOn[(size_t) i] = f.on;
                    if (! f.on) continue;
                    const float fm = m ((ModTarget) ((int) ModTarget::f1 + i), c);
                    const float freq = filtFreqSm[(size_t) i] * std::pow (2.0f, 4.0f * fm);
                    ch.coeffs[(size_t) i] = designSvf (f.type, freq, f.gainDb, f.q, sampleRate);
                }

                // target delay
                const float baseMs = c == 0 ? set.timeMsL : set.timeMsR;
                const float modMs = 10.0f * (m (ModTarget::time, c) + m (c == 0 ? ModTarget::timeL : ModTarget::timeR, c));
                float compMs = pitchEngage * pitchWindowMs * 0.5f;
                compMs += ch.diff * 0.5f * apScale * (4.1f + 6.7f + 10.9f + 15.3f) * 0.6f;
                ch.targetD = std::max (1.0f, msToSamples (baseMs - compMs));
                ch.modTimePrev = ch.modTimeNext;
                ch.modTimeNext = msToSamples (modMs);
                if (first) ch.modTimePrev = ch.modTimeNext;

                if (ch.dA < 0.0f) { ch.dA = ch.dB = ch.dTape = ch.targetD; ch.fade = 1.0f; }
                (c == 0 ? effTimeMsL : effTimeMsR).store (baseMs, std::memory_order_relaxed);
            }

            apLens[0] = msToSamples (4.1f * apScale);
            apLens[1] = msToSamples (6.7f * apScale);
            apLens[2] = msToSamples (10.9f * apScale);
            apLens[3] = msToSamples (15.3f * apScale);
            for (int k = 0; k < 4; ++k) apLensR[k] = apLens[k] * (1.0f + 0.07f * (float) (k + 1) / 4.0f);

            // ---- envelope coefficients
            envAtt = std::exp (-1.0f / (0.001f * set.envAttackMs * (float) sampleRate));
            envRel = std::exp (-1.0f / (0.001f * set.envReleaseMs * (float) sampleRate));
            duckRel = std::exp (-1.0f / (0.001f * set.duckReleaseMs * (float) sampleRate));
            duckAtt = std::exp (-1.0f / (0.002f * (float) sampleRate));
        }

        //------------------------------------------------------------------------------------------
        inline float delayFor (Channel& ch, float modT)
        {
            const int mode = set.timeMode;
            if (mode == 0)
            {
                // tape: glide toward the target with a limited rate (pitch bends like a tape machine)
                const float k = 1.0f / (0.18f * (float) sampleRate);
                float step = (ch.targetD - ch.dTape) * k * 1.5f;
                step = std::clamp (step, -0.85f, 0.85f);
                if (std::abs (ch.targetD - ch.dTape) < 0.0005f) ch.dTape = ch.targetD; else ch.dTape += step;
                ch.dA = ch.dB = ch.dTape; ch.fade = 1.0f;
                return -1.0f;   // single head
            }
            // fade / jump: two heads, crossfade when the target moves
            if (ch.fade >= 1.0f)
            {
                ch.dA = ch.dB;
                if (std::abs (ch.targetD - ch.dB) > 0.5f) { ch.dB = ch.targetD; ch.fade = 0.0f; }
            }
            juce::ignoreUnused (modT);
            ch.dTape = ch.dB;
            return 0.0f;
        }

        static inline float logCosh (float x)
        {
            const float ax = std::abs (x);
            return ax > 12.0f ? ax - 0.69314718f : std::log (std::cosh (x));
        }

        inline float sat (int style, float x)
        {
            switch (style)
            {
                case 0: return std::tanh (x);
                case 1: return std::tanh (x + 0.35f) - 0.33637554f;    // tanh(0.35)
                default: return std::clamp (x, -1.0f, 1.0f);
            }
        }
        inline float satF (int style, float x)
        {
            switch (style)
            {
                case 0: return logCosh (x);
                case 1: return logCosh (x + 0.35f) - 0.33637554f * x;
                default: { const float ax = std::abs (x); return ax < 1.0f ? 0.5f * x * x : ax - 0.5f; }
            }
        }

        inline float drive (Channel& ch, float x, float amount)
        {
            if (amount <= 0.0001f) { ch.adaaX1 = x; return x; }
            const float gain = juce::Decibels::decibelsToGain (amount * 30.0f);
            const float makeup = 1.0f / std::pow (gain, 0.72f);
            const float u = x * gain, u1 = ch.adaaX1 * gain;
            float y;
            const float du = u - u1;
            if (std::abs (du) > 1.0e-4f) y = (satF (set.driveStyle, u) - satF (set.driveStyle, u1)) / du;
            else y = sat (set.driveStyle, 0.5f * (u + u1));
            ch.adaaX1 = x;
            const float blend = std::min (1.0f, amount * 5.0f);
            return x + (y * makeup - x) * blend;
        }

        static inline float safety (float x)
        {
            const float ax = std::abs (x);
            if (ax <= 0.8f) return x;
            return std::copysign (0.8f + 0.25f * std::tanh ((ax - 0.8f) * 4.0f), x);
        }

        inline float pitchShift (Channel& ch, float x)
        {
            ch.pitchLine.push (x);
            const float W = msToSamples (pitchWindowMs);
            const float plain = ch.pitchLine.read (W * 0.5f);
            if (ch.pitchAmt <= 0.0f) return plain;
            ch.pitchPhase += (1.0f - ch.pitchRatio) / W;
            ch.pitchPhase -= std::floor (ch.pitchPhase);
            const float p1 = ch.pitchPhase;
            float p2 = p1 + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;
            const float w1 = std::sin (juce::MathConstants<float>::pi * p1);
            const float w2 = std::sin (juce::MathConstants<float>::pi * p2);
            const float shifted = w1 * w1 * ch.pitchLine.read (1.0f + p1 * W) + w2 * w2 * ch.pitchLine.read (1.0f + p2 * W);
            return plain + (shifted - plain) * ch.pitchAmt;
        }

        //------------------------------------------------------------------------------------------
        void processChunk (float* L, float* R, int n)
        {
            const float fadeStep = set.timeMode == 1 ? 1.0f / (0.09f * (float) sampleRate)
                                                     : 1.0f / (0.004f * (float) sampleRate);
            const int routing = set.routing;
            auto counter = sampleCounter.load (std::memory_order_relaxed);
            const float onsetRel = std::exp (-1.0f / (0.25f * (float) sampleRate));
            const float fastRel = std::exp (-1.0f / (0.01f * (float) sampleRate));

            for (int i = 0; i < n; ++i)
            {
                const float gIn = smIn.getNextValue();
                float xL = L[i] * gIn, xR = R[i] * gIn;
                const float dryL = xL, dryR = xR;

                // envelope (mod source + ducking + onset for the UI)
                const float peak = std::max (std::abs (xL), std::abs (xR));
                env = peak > env ? envAtt * env + (1.0f - envAtt) * peak : envRel * env + (1.0f - envRel) * peak;
                duckEnv = peak > duckEnv ? duckAtt * duckEnv + (1.0f - duckAtt) * peak : duckRel * duckEnv + (1.0f - duckRel) * peak;
                fastEnv = std::max (peak, fastEnv * fastRel);
                slowEnv = std::max (fastEnv * 0.999f, slowEnv * onsetRel);
                if (fastEnv > 0.02f && fastEnv > slowEnv * 1.9f) { lastOnset.store (counter + i, std::memory_order_relaxed); slowEnv = fastEnv; }

                const float t = (float) i / (float) n;
                const float fbBase = smFeedback.getNextValue();
                float cross = smCross.getNextValue();
                if (routing == 1) cross = 1.0f;
                const float frz = freezeAmt;

                float wet[2];
                for (int c = 0; c < 2; ++c)
                {
                    auto& ch = chans[(size_t) c];
                    const float modT = ch.modTimePrev + (ch.modTimeNext - ch.modTimePrev) * t;
                    const float r = delayFor (ch, modT);
                    if (r < 0.0f) wet[c] = ch.line.read (ch.dTape + modT);
                    else
                    {
                        if (ch.fade < 1.0f)
                        {
                            const float a = ch.line.read (ch.dA + modT), b = ch.line.read (ch.dB + modT);
                            const float g = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * ch.fade);
                            wet[c] = a + (b - a) * g;
                            ch.fade = std::min (1.0f, ch.fade + fadeStep);
                        }
                        else wet[c] = ch.line.read (ch.dB + modT);
                    }
                }

                // routing of the input into the loop
                float inL = xL, inR = xR;
                if (routing == 1)      { inL = 0.5f * (xL + xR); inR = 0.0f; }
                else if (routing == 2) { inL = inR = 0.5f * (xL + xR); }
                const float inGainLoop = 1.0f - frz;

                const float fbSig[2] { wet[0] * (1.0f - cross) + wet[1] * cross, wet[1] * (1.0f - cross) + wet[0] * cross };
                const float in[2] { inL, inR };

                for (int c = 0; c < 2; ++c)
                {
                    auto& ch = chans[(size_t) c];
                    const float fb = std::clamp (fbBase + ch.fb, 0.0f, 1.2f);
                    const float fbEff = fb + (1.0f - fb) * frz;
                    float s = in[c] * inGainLoop + fbSig[c] * fbEff;

                    // lossy part of the chain (bypassed by freeze)
                    float p = s;
                    for (int k = 0; k < numFilters; ++k)
                        if (ch.filtOn[(size_t) k]) p = ch.filt[(size_t) k].process (p, ch.coeffs[(size_t) k]);
                    p = drive (ch, p, ch.drive);
                    s = s + (p - s) * (1.0f - frz);

                    s = safety (s);
                    // DC blocker
                    const float y = s - ch.dcX + 0.9995f * ch.dcY;
                    ch.dcX = s; ch.dcY = y; s = y;

                    // diffusion
                    if (ch.diff > 0.0005f)
                    {
                        float d = s;
                        const float g = 0.62f;
                        for (int k = 0; k < 4; ++k) d = ch.ap[(size_t) k].process (d, c == 0 ? apLens[k] : apLensR[k], g);
                        s = s + (d - s) * ch.diff;
                    }
                    else
                        for (int k = 0; k < 4; ++k) ch.ap[(size_t) k].line.push (s);

                    // pitch shifter (latency is compensated in the delay time)
                    if (pitchEngage > 0.0f)
                    {
                        const float saved = ch.pitchAmt;
                        ch.pitchAmt *= (1.0f - frz);
                        const float ps = pitchShift (ch, s);
                        ch.pitchAmt = saved;
                        s = s + (ps - s) * pitchEngage;
                    }
                    else ch.pitchLine.push (s);

                    if (! std::isfinite (s)) s = 0.0f;
                    ch.line.push (s);
                }

                // ducking
                const float duckTarget = 1.0f / (1.0f + set.duck * 8.0f * duckEnv);
                duckGain += (duckTarget - duckGain) * 0.002f;
                float wL = wet[0] * duckGain, wR = wet[1] * duckGain;

                // width + pan
                const float wdt = smWidth.getNextValue();
                const float mid = 0.5f * (wL + wR), side = 0.5f * (wL - wR) * wdt;
                wL = mid + side; wR = mid - side;
                const float pan = smPan.getNextValue();
                wL *= pan > 0.0f ? 1.0f - pan : 1.0f;
                wR *= pan < 0.0f ? 1.0f + pan : 1.0f;

                const float mix = smMix.getNextValue();
                const float dryG = std::min (1.0f, 2.0f * (1.0f - mix));
                const float wetG = std::min (1.0f, 2.0f * mix);
                const float gOut = smOut.getNextValue();
                L[i] = (dryL * dryG + wL * wetG) * gOut;
                R[i] = (dryR * dryG + wR * wetG) * gOut;
                wetOutL = wL; wetOutR = wR;
            }
            duckValue.store (duckGain, std::memory_order_relaxed);
            sampleCounter.store (counter + n, std::memory_order_relaxed);
        }

    public:
        float wetOutL = 0.0f, wetOutR = 0.0f;

    private:
        double sampleRate = 48000.0;
        Settings set;
        std::array<Channel, 2> chans;
        std::array<double, numLfos> lfoPhase {};
        bool syncedLastBlock[numLfos] {};
        float env = 0.0f, duckEnv = 0.0f, duckGain = 1.0f;
        float envAtt = 0.0f, envRel = 0.0f, duckAtt = 0.0f, duckRel = 0.0f;
        float fastEnv = 0.0f, slowEnv = 0.0f;
        float freezeAmt = 0.0f, pitchEngage = 0.0f;
        float apLens[4] {}, apLensR[4] {};
        float diffSizeSm = 0.5f;
        std::array<float, numFilters> filtFreqSm {};
        const float pitchWindowMs = 46.0f;
        bool first = true;
        juce::SmoothedValue<float> smFeedback, smCross, smWidth, smPan, smMix, smIn, smOut;
    };
}
