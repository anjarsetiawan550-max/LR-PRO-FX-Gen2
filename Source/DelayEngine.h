#pragma once
#include "DspUtils.h"

// Stereo delay dengan filter HPF / LPF di JALUR MASUK delay line.
//
// Sebelumnya filter hanya ada di jalur feedback, sehingga echo pertama selalu
// lolos tanpa filter dan baru echo ke-2 dst yang terfilter ("cuma ekornya").
// Sekarang semua yang masuk ke delay line (sinyal input + feedback) melewati
// HPF dan LPF, jadi echo pertama sudah terfilter penuh, dan tiap pengulangan
// terfilter lagi (makin gelap / makin tipis, seperti delay analog/tape).
//
// Tiap filter = 2 biquad Butterworth berurutan (24 dB/oct) supaya efeknya
// jelas terdengar. process() mengembalikan sinyal wet (echo) saja.
class DelayEngine
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        const int maxS = (int) (2.1 * sr) + 8;
        line[0].prepare (maxS);
        line[1].prepare (maxS);
        clear();
        initialised = false;
        lastHp = lastLp = -1.f;
    }

    void clear()
    {
        line[0].clear(); line[1].clear();
        for (auto& ch : hp) for (auto& b : ch) b.reset();
        for (auto& ch : lp) for (auto& b : ch) b.reset();
    }

    void setParams (float timeMs, float feedback01, float hpHz, float lpHz)
    {
        timeT = std::min (std::max (timeMs, 1.0f), 2000.f) * 0.001f * (float) sr;
        if (! initialised) { timeSm = timeT; initialised = true; }
        fb = std::min (std::max (feedback01, 0.f), 0.95f);

        if (hpHz != lastHp)
        {
            for (auto& ch : hp) for (auto& b : ch) b.highPass (sr, hpHz);
            lastHp = hpHz;
        }
        if (lpHz != lastLp)
        {
            for (auto& ch : lp) for (auto& b : ch) b.lowPass (sr, lpHz);
            lastLp = lpHz;
        }
    }

    inline void process (float inL, float inR, float& outL, float& outR)
    {
        timeSm += (timeT - timeSm) * 0.0008f;
        const float d = std::max (1.0f, timeSm);

        const float yl = line[0].read (d);
        const float yr = line[1].read (d);

        // input + feedback -> HPF -> LPF -> masuk delay line
        line[0].write (filter (0, inL + yl * fb));
        line[1].write (filter (1, inR + yr * fb));

        outL = yl;
        outR = yr;
    }

private:
    inline float filter (int ch, float x)
    {
        x = hp[ch][1].process (hp[ch][0].process (x));
        return lp[ch][1].process (lp[ch][0].process (x));
    }

    double sr = 44100.0;
    DelayLine line[2];
    Biquad hp[2][2], lp[2][2];
    float timeT = 22050.f, timeSm = 22050.f, fb = 0.3f;
    float lastHp = -1.f, lastLp = -1.f;
    bool initialised = false;
};
