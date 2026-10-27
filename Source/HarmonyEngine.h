#pragma once
#include "DspUtils.h"
#include <array>

// ---------------------------------------------------------------------------
// Pitch shifter dua-tap (delay line + crossfade Hann). ratio 1.0 = tanpa perubahan.
// ---------------------------------------------------------------------------
struct HarmonyShifter
{
    DelayLine line;
    float win = 1764.f, phase = 0.f;

    void prepare (double sr)
    {
        win = (float) (0.025 * sr);
        line.prepare ((int) win + 8);
        phase = 0.f;
    }

    void clear() { line.clear(); phase = 0.f; }

    inline float process (float x, float ratio)
    {
        float p2 = phase + 0.5f;
        if (p2 >= 1.f) p2 -= 1.f;

        const float d1 = 1.f + phase * win;
        const float d2 = 1.f + p2 * win;
        const float g1 = 0.5f - 0.5f * std::cos (6.28318530718f * phase);

        const float y = line.read (d1) * g1 + line.read (d2) * (1.f - g1);
        line.write (x);

        phase += (1.f - ratio) / win;
        phase -= std::floor (phase);
        return y;
    }
};

// ---------------------------------------------------------------------------
// HARMONY: mendeteksi nada suara (YIN), memetakannya ke tangga nada (Key + Major/Minor),
// lalu membuat 1-2 suara harmoni diatonis (oktaf / kuint / terts, bawah / atas).
// process() mengembalikan sinyal harmoni saja (wet).
// ---------------------------------------------------------------------------
class HarmonyEngine
{
public:
    static constexpr int numModes = 11;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        decim = std::max (1, (int) std::lround (sr / 11025.0));
        fsDec = sr / (double) decim;
        tauMax = std::min (190, (int) (fsDec / 70.0));
        tauMin = std::max (2, (int) (fsDec / 900.0));

        for (auto& s : shifter) s.prepare (sr);

        coefRatio = 1.f - std::exp (-1.f / (0.015f * (float) sr));
        coefGain  = 1.f - std::exp (-1.f / (0.015f * (float) sr));
        coefOut   = 1.f - std::exp (-1.f / (0.020f * (float) sr));

        lastKey = lastMode = lastScale = lastTone = -1;
        clear();
    }

    void clear()
    {
        for (auto& s : shifter) s.clear();
        for (auto& b : lowShelf)  b.reset();
        for (auto& b : highShelf) b.reset();
        ring.fill (0.f);
        wp = 0; decAcc = 0.f; decCount = 0; hopCount = 0;
        haveNote = false; voiced = false; unvoicedHops = 1000;
        midiSm = 0.f; curMidi = 0;
        for (int v = 0; v < 2; ++v)
        {
            ratioSm[v] = ratioT[v] = 1.f;
            gLSm[v] = gRSm[v] = tgtL[v] = tgtR[v] = 0.f;
        }
        outSm = outT;
    }

    void setParams (int keyIdx, int modeIdx, int scaleIdx, int toneIdx, float outDb)
    {
        keyIdx   = std::min (std::max (keyIdx, 0), 6);
        modeIdx  = std::min (std::max (modeIdx, 0), numModes - 1);
        scaleIdx = scaleIdx > 0 ? 1 : 0;
        toneIdx  = std::min (std::max (toneIdx, 0), 2);

        if (keyIdx != lastKey || modeIdx != lastMode || scaleIdx != lastScale)
        {
            static const int keySemis[7] = { 0, 2, 4, 5, 7, 9, 11 };
            static const int majorSteps[7] = { 0, 2, 4, 5, 7, 9, 11 };
            static const int minorSteps[7] = { 0, 2, 3, 5, 7, 8, 10 };

            // offset dalam langkah tangga nada: 3rd = 2, 5th = 4, oktaf = 7
            static const int modeTable[numModes][2] = {
                { -7, 0 }, { -4, 0 }, { -2, 0 }, { 2, 0 }, { 4, 0 }, { 7, 0 },
                { -7, 7 }, { -4, 2 }, { -4, 4 }, { -2, 2 }, { 2, 4 } };
            static const int modeVoices[numModes] = { 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2 };

            key = keySemis[keyIdx];
            for (int i = 0; i < 7; ++i) steps[i] = scaleIdx == 0 ? majorSteps[i] : minorSteps[i];
            numVoices = modeVoices[modeIdx];
            offs[0] = modeTable[modeIdx][0];
            offs[1] = modeTable[modeIdx][1];
            lastKey = keyIdx; lastMode = modeIdx; lastScale = scaleIdx;
            refreshTargets();
        }

        if (toneIdx != lastTone)
        {
            lastTone = toneIdx;
            for (int c = 0; c < 2; ++c)
            {
                if (toneIdx == 0)      { lowShelf[c].lowShelf (sr, 250.0, 0.0);  highShelf[c].highShelf (sr, 3500.0, 0.0); }
                else if (toneIdx == 1) { lowShelf[c].lowShelf (sr, 250.0, 2.5);  highShelf[c].highShelf (sr, 3000.0, -6.0); }
                else                   { lowShelf[c].lowShelf (sr, 250.0, -2.0); highShelf[c].highShelf (sr, 3500.0, 5.0); }
            }
        }

        outT = std::pow (10.f, outDb / 20.f);
    }

    inline void process (float inL, float inR, float& outL, float& outR)
    {
        const float mono = 0.5f * (inL + inR);

        // --- analisis nada (sinyal didesimasi ~11 kHz) ---
        decAcc += mono;
        if (++decCount >= decim)
        {
            ring[(size_t) (wp & ringMask)] = decAcc / (float) decim;
            ++wp;
            decAcc = 0.f; decCount = 0;
            if (++hopCount >= hop) { hopCount = 0; analyse(); }
        }

        // --- suara harmoni ---
        float wl = 0.f, wr = 0.f;
        for (int v = 0; v < 2; ++v)
        {
            ratioSm[v] += (ratioT[v] - ratioSm[v]) * coefRatio;
            gLSm[v] += (tgtL[v] - gLSm[v]) * coefGain;
            gRSm[v] += (tgtR[v] - gRSm[v]) * coefGain;
            if (v == 1 && numVoices < 2 && gLSm[1] < 1.0e-5f && gRSm[1] < 1.0e-5f) continue;

            const float y = shifter[v].process (mono, ratioSm[v]);
            wl += y * gLSm[v];
            wr += y * gRSm[v];
        }

        outSm += (outT - outSm) * coefOut;
        outL = highShelf[0].process (lowShelf[0].process (wl)) * outSm;
        outR = highShelf[1].process (lowShelf[1].process (wr)) * outSm;
    }

private:
    static constexpr int ringSize = 2048, ringMask = ringSize - 1;
    static constexpr int hop = 128, winLen = 384;

    static int floorDiv (int a, int b) { return (int) std::floor ((double) a / (double) b); }

    int scaleNote (int n) const
    {
        const int oct = floorDiv (n, 7);
        return key + 12 * oct + steps[n - 7 * oct];
    }

    int nearestDegree (int midi) const
    {
        const int rel = midi - key;
        const int oct = floorDiv (rel, 12);
        const int pc = rel - 12 * oct;
        int bestDeg = 0, bestDist = std::abs (pc - steps[0]);
        for (int d = 1; d < 7; ++d)
        {
            const int dist = std::abs (pc - steps[d]);
            if (dist < bestDist) { bestDist = dist; bestDeg = d; }
        }
        if (std::abs (pc - 12) < bestDist) return (oct + 1) * 7;
        return oct * 7 + bestDeg;
    }

    void refreshTargets()
    {
        const bool active = haveNote && unvoicedHops < holdHops;
        const int deg = haveNote ? nearestDegree (curMidi) : 0;

        for (int v = 0; v < 2; ++v)
        {
            const bool on = active && v < numVoices;
            if (haveNote && v < numVoices)
            {
                const int shift = scaleNote (deg + offs[v]) - scaleNote (deg);
                ratioT[v] = std::pow (2.f, (float) shift / 12.f);
            }
            else
                ratioT[v] = 1.f;

            // level & panning: 1 suara di tengah, 2 suara kiri / kanan
            const float level = numVoices == 1 ? 1.f : 0.75f;
            const float pan = numVoices == 1 ? 0.f : (v == 0 ? -0.6f : 0.6f);
            const float gl = pan > 0.f ? 1.f - pan : 1.f;
            const float gr = pan < 0.f ? 1.f + pan : 1.f;
            tgtL[v] = on ? level * gl : 0.f;
            tgtR[v] = on ? level * gr : 0.f;
        }
    }

    void analyse()
    {
        const int N = winLen + tauMax;
        for (int i = 0; i < N; ++i)
            frame[(size_t) i] = ring[(size_t) ((wp - N + i) & ringMask)];

        float e = 0.f;
        for (int i = 0; i < N; ++i) e += frame[(size_t) i] * frame[(size_t) i];
        const float rms = std::sqrt (e / (float) N);

        bool found = false;
        float f0 = 0.f;

        if (rms > 0.004f)
        {
            float sum = 0.f;
            cm[0] = 1.f;
            for (int tau = 1; tau <= tauMax; ++tau)
            {
                float acc = 0.f;
                for (int j = 0; j < winLen; ++j)
                {
                    const float d = frame[(size_t) j] - frame[(size_t) (j + tau)];
                    acc += d * d;
                }
                sum += acc;
                cm[(size_t) tau] = sum > 0.f ? acc * (float) tau / sum : 1.f;
            }

            int best = -1;
            for (int tau = tauMin; tau < tauMax; ++tau)
            {
                if (cm[(size_t) tau] < 0.15f)
                {
                    while (tau + 1 < tauMax && cm[(size_t) (tau + 1)] < cm[(size_t) tau]) ++tau;
                    best = tau;
                    break;
                }
            }
            if (best < 0)
            {
                int mn = tauMin;
                for (int tau = tauMin; tau < tauMax; ++tau)
                    if (cm[(size_t) tau] < cm[(size_t) mn]) mn = tau;
                if (cm[(size_t) mn] < 0.3f) best = mn;
            }

            if (best > 1 && best < tauMax)
            {
                float t = (float) best;
                const float s0 = cm[(size_t) (best - 1)], s1 = cm[(size_t) best], s2 = cm[(size_t) (best + 1)];
                const float den = s0 + s2 - 2.f * s1;
                if (std::abs (den) > 1.0e-9f) t += 0.5f * (s0 - s2) / den;
                f0 = (float) (fsDec / (double) t);
                found = f0 >= 70.f && f0 <= 900.f;
            }
        }

        if (found)
        {
            const float midiF = 69.f + 12.f * std::log2 (f0 / 440.f);
            if (! haveNote || std::abs (midiF - midiSm) > 1.5f) midiSm = midiF;
            else                                                midiSm += 0.4f * (midiF - midiSm);

            if (! haveNote)
            {
                curMidi = (int) std::lround (midiSm);
                haveNote = true;
            }
            else if (std::abs (midiSm - (float) curMidi) > 0.7f)
                curMidi = (int) std::lround (midiSm);

            unvoicedHops = 0;
        }
        else if (unvoicedHops < 1000)
            ++unvoicedHops;

        refreshTargets();
    }

    double sr = 44100.0, fsDec = 11025.0;
    int decim = 4, tauMax = 157, tauMin = 12;

    std::array<HarmonyShifter, 2> shifter;
    Biquad lowShelf[2], highShelf[2];

    std::array<float, ringSize> ring {};
    std::array<float, winLen + 200> frame {};
    std::array<float, 200> cm {};
    int wp = 0, decCount = 0, hopCount = 0;
    float decAcc = 0.f;

    static constexpr int holdHops = 6;
    bool haveNote = false, voiced = false;
    int unvoicedHops = 1000, curMidi = 0;
    float midiSm = 0.f;

    int key = 0, steps[7] = { 0, 2, 4, 5, 7, 9, 11 };
    int numVoices = 1, offs[2] = { 2, 0 };
    int lastKey = -1, lastMode = -1, lastScale = -1, lastTone = -1;

    float ratioT[2] = { 1.f, 1.f }, ratioSm[2] = { 1.f, 1.f };
    float tgtL[2] = { 0.f, 0.f }, tgtR[2] = { 0.f, 0.f };
    float gLSm[2] = { 0.f, 0.f }, gRSm[2] = { 0.f, 0.f };
    float outT = 1.f, outSm = 1.f;
    float coefRatio = 0.001f, coefGain = 0.001f, coefOut = 0.001f;
};
