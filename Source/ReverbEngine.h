#pragma once
#include "DspUtils.h"

// 8-line feedback delay network (Householder matrix) with input diffusion,
// pre-delay, per-line damping, modulation and wet low-cut / hi-cut.
// Type 0 = Plate (dense, bright), type 1 = Hall (long, modulated).
// NOTE: line lengths must be long enough for modal overlap at the chosen decay,
// otherwise individual modes ring audibly ("nguuung"). Do not shorten them.
class ReverbEngine
{
public:
    static constexpr int N = 8;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        const double r = sr / 44100.0;
        rr = (float) r;

        const int maxLen = (int) std::ceil (5300.0 * r * 2.0) + (int) std::ceil (16.0 * r) + 16;
        for (int i = 0; i < N; ++i)
        {
            lines[i].prepare (maxLen);
            phaseInc[i] = (float) (2.0 * 3.14159265358979323846 * (0.11 + 0.063 * i) / sr);
            phase[i] = 0.9f * (float) i;
        }

        static const int apL[4] = { 142, 107, 379, 277 };
        static const int apR[4] = { 157, 113, 389, 293 };
        for (int k = 0; k < 4; ++k)
        {
            apLeft[k].prepare ((int) (apL[k] * r));
            apRight[k].prepare ((int) (apR[k] * r));
        }

        preL.prepare ((int) (0.25 * sr) + 8);
        preR.prepare ((int) (0.25 * sr) + 8);

        clear();
        decaySm = decayT; scaleSm = scaleT; preSm = preT;
        lastLow = lastHi = -1.f;
    }

    void clear()
    {
        for (auto& l : lines) l.clear();
        for (auto& a : apLeft) a.clear();
        for (auto& a : apRight) a.clear();
        preL.clear(); preR.clear();
        for (int i = 0; i < N; ++i) damp[i] = 0.f;
        hpStL = hpStR = lpStL = lpStR = 0.f;
    }

    void setParams (int type, float decaySec, float preDelayMs, float room01,
                    float diffusion01, float damping01, float lowCutHz, float hiCutHz)
    {
        if (type != curType) { curType = type; clear(); }

        decayT = std::max (0.1f, decaySec);
        scaleT = std::pow (2.0f, room01 * 2.0f - 1.0f);                 // 0.5 .. 2.0
        preT   = std::min (preDelayMs, 240.f) * 0.001f * (float) sr;

        const float g = diffusion01 * 0.75f;
        for (auto& a : apLeft)  a.g = g;
        for (auto& a : apRight) a.g = g;

        dampA = damping01 * 0.88f;
        modDepth = (type == 0) ? 3.0f : 6.0f;
        injGain  = (type == 0) ? 0.7f : 0.5f;

        // Low Cut / Hi Cut: one-pole filters (same as LR PRO FX).
        if (lowCutHz != lastLow)
        {
            hpA = 1.0f - std::exp (-6.28318530717958647692f * lowCutHz / (float) sr);
            lastLow = lowCutHz;
        }
        if (hiCutHz != lastHi)
        {
            lpA = 1.0f - std::exp (-6.28318530717958647692f * hiCutHz / (float) sr);
            lastHi = hiCutHz;
        }
    }

    inline void process (float inL, float inR, float& outL, float& outR)
    {
        decaySm += (decayT - decaySm) * 0.001f;
        scaleSm += (scaleT - scaleSm) * 0.0005f;
        preSm   += (preT   - preSm)   * 0.002f;

        const float pd = std::max (1.0f, preSm);
        float xl = preL.read (pd); preL.write (inL);
        float xr = preR.read (pd); preR.write (inR);

        for (int k = 0; k < 4; ++k) { xl = apLeft[k].process (xl); xr = apRight[k].process (xr); }

        const float* base = (curType == 0) ? plateLen : hallLen;
        const float depth = modDepth * rr;
        const float twoPi = 6.28318530717958647692f;

        float y[N];
        for (int i = 0; i < N; ++i)
        {
            const float len = base[i] * rr * scaleSm;
            const float m = depth * std::sin (phase[i]);
            phase[i] += phaseInc[i];
            if (phase[i] > twoPi) phase[i] -= twoPi;

            const float v = lines[i].read (std::max (2.0f, len + m));
            damp[i] = v + dampA * (damp[i] - v);
            const float g = std::exp (-6.907755f * len / (decaySm * (float) sr));
            y[i] = damp[i] * g;
        }

        float sum = 0.f;
        for (int i = 0; i < N; ++i) sum += y[i];
        sum *= (2.0f / (float) N);

        for (int i = 0; i < N; ++i)
            lines[i].write (y[i] - sum + ((i & 1) ? xr : xl) * injGain);

        static const float sL[N] = { 1, -1, 1, -1, 1, -1, 1, -1 };
        static const float sR[N] = { 1, 1, -1, -1, 1, 1, -1, -1 };
        float oL = 0.f, oR = 0.f;
        for (int i = 0; i < N; ++i) { oL += sL[i] * y[i]; oR += sR[i] * y[i]; }

        // high-pass (Low Cut) then low-pass (Hi Cut), one-pole each
        const float xl2 = oL * 0.3f, xr2 = oR * 0.3f;
        hpStL += hpA * (xl2 - hpStL);  const float hL = xl2 - hpStL;
        hpStR += hpA * (xr2 - hpStR);  const float hR = xr2 - hpStR;
        lpStL += lpA * (hL - lpStL);
        lpStR += lpA * (hR - lpStR);
        outL = lpStL;
        outR = lpStR;
    }

private:
    double sr = 44100.0;
    float rr = 1.f;
    int curType = 0;

    DelayLine lines[N];
    Allpass apLeft[4], apRight[4];
    DelayLine preL, preR;
    float damp[N] = {};
    float phase[N] = {}, phaseInc[N] = {};
    float hpA = 0.f, lpA = 1.f;
    float hpStL = 0.f, hpStR = 0.f, lpStL = 0.f, lpStR = 0.f;

    float decayT = 2.5f, decaySm = 2.5f;
    float scaleT = 1.f,  scaleSm = 1.f;
    float preT = 0.f,    preSm = 0.f;
    float dampA = 0.3f, modDepth = 1.5f, injGain = 0.7f;
    float lastLow = -1.f, lastHi = -1.f;

    static constexpr float plateLen[N] = { 1811, 2111, 2393, 2663, 2953, 3299, 3581, 3877 };
    static constexpr float hallLen[N]  = { 2371, 2749, 3163, 3581, 3989, 4421, 4831, 5237 };
};
