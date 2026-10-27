#pragma once

#include <cmath>
#include <complex>
#include <algorithm>
#include "../Params.h"

namespace avel::time::dsp
{
    /** Trapezoidal state-variable filter (Simper). Stable under fast modulation.
        Every response is written as m0*input + m1*band + m2*low, so the UI can draw
        the exact curve from the same coefficients. */
    struct SvfCoeffs
    {
        float g = 0.0f, k = 1.0f, a1 = 1.0f, a2 = 0.0f, a3 = 0.0f;
        float m0 = 1.0f, m1 = 0.0f, m2 = 0.0f;
    };

    inline SvfCoeffs designSvf (FilterType type, float freq, float gainDb, float q, double sampleRate)
    {
        SvfCoeffs c;
        const double nyq = sampleRate * 0.5;
        freq = (float) std::clamp ((double) freq, 10.0, nyq * 0.95);
        q = std::max (0.05f, q);
        const float A = std::pow (10.0f, gainDb / 40.0f);
        float g = (float) std::tan (juce::MathConstants<double>::pi * freq / sampleRate);
        float k = 1.0f / q;

        switch (type)
        {
            case FilterType::lowCut:    c.m0 = 1.0f; c.m1 = -k; c.m2 = -1.0f; break;
            case FilterType::highCut:   c.m0 = 0.0f; c.m1 = 0.0f; c.m2 = 1.0f; break;
            case FilterType::bandPass:  c.m0 = 0.0f; c.m1 = k; c.m2 = 0.0f; break;
            case FilterType::notch:     c.m0 = 1.0f; c.m1 = -k; c.m2 = 0.0f; break;
            case FilterType::bell:      k = 1.0f / (q * A); c.m0 = 1.0f; c.m1 = k * (A * A - 1.0f); c.m2 = 0.0f; break;
            case FilterType::lowShelf:  g /= std::sqrt (A); c.m0 = 1.0f; c.m1 = k * (A - 1.0f); c.m2 = A * A - 1.0f; break;
            case FilterType::highShelf: g *= std::sqrt (A); c.m0 = A * A; c.m1 = k * (1.0f - A) * A; c.m2 = 1.0f - A * A; break;
        }
        c.g = g; c.k = k;
        c.a1 = 1.0f / (1.0f + g * (g + k));
        c.a2 = g * c.a1;
        c.a3 = g * c.a2;
        return c;
    }

    /** Exact magnitude of the digital filter at frequency f. */
    inline float svfMagnitude (const SvfCoeffs& c, double f, double sampleRate)
    {
        const double w = std::tan (juce::MathConstants<double>::pi * std::min (f, sampleRate * 0.4999) / sampleRate) / c.g;
        const std::complex<double> s (0.0, w);
        const auto den = s * s + (double) c.k * s + 1.0;
        const auto h = (double) c.m0 + (double) c.m1 * s / den + (double) c.m2 / den;
        return (float) std::abs (h);
    }

    struct Svf
    {
        float ic1 = 0.0f, ic2 = 0.0f;
        void reset() { ic1 = ic2 = 0.0f; }
        inline float process (float v0, const SvfCoeffs& c)
        {
            const float v3 = v0 - ic2;
            const float v1 = c.a1 * ic1 + c.a2 * v3;
            const float v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            return c.m0 * v0 + c.m1 * v1 + c.m2 * v2;
        }
    };
}
