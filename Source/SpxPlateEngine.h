#pragma once
#include "DspUtils.h"

// ---------------------------------------------------------------------------
// SPX PLATE: reverb plate bergaya "digital 90-an" (karakter mengacu ke Yamaha SPX).
//
// Struktur (Dattorro plate, input mono seperti plate fisik):
//   input (L+R)/2 -> pre-delay -> bandwidth LPF -> 4 allpass diffuser
//   -> tank 2 cabang bersilang (modAP -> delay -> damping LPF -> AP -> delay)
//   -> output diambil dari banyak titik tap, lalu lebar stereo, Low Cut, Hi Cut.
//
// Parameter sama persis dengan Plate / Hall (Decay, PreDelay, Room, Diffusion,
// Damping, Low Cut, Hi Cut) supaya menu di LCD tetap sama.
//
// Karakter yang dituju (hasil ukur dari rekaman referensi):
//   - build-up rapat tanpa early reflection yang menonjol (diffusion tinggi)
//   - ekor gelap-hangat: HF meluruh lebih cepat dari mid (Damping)
//   - lebar stereo sedang/fokus (tidak selebar hall)
// ---------------------------------------------------------------------------
class SpxPlateEngine
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        s  = (float) (sr / 29761.0);                       // semua panjang Dattorro dalam sampel @29761 Hz

        // panjang maksimum tank = panjang asli x room max (2.0) + margin modulasi
        const int d1L = (int) std::ceil (4453.0 * 2.0 * s) + 32, d2L = (int) std::ceil (3720.0 * 2.0 * s) + 32;
        const int d1R = (int) std::ceil (4217.0 * 2.0 * s) + 32, d2R = (int) std::ceil (3163.0 * 2.0 * s) + 32;
        L.d1.prepare (d1L);  L.d2.prepare (d2L);
        R.d1.prepare (d1R);  R.d2.prepare (d2R);

        static const int inLen[4] = { 142, 107, 379, 277 };
        for (int k = 0; k < 4; ++k) inAp[k].prepare ((int) std::lround (inLen[k] * s));

        L.mod.prepare ((int) std::ceil (672.0 * s) + 40);
        R.mod.prepare ((int) std::ceil (908.0 * s) + 40);
        L.ap2.prepare ((int) std::ceil (1800.0 * s) + 8);
        R.ap2.prepare ((int) std::ceil (2656.0 * s) + 8);

        pre.prepare ((int) (0.25 * sr) + 8);

        modPhaseL = 0.f;  modPhaseR = 1.7f;
        modInc = (float) (6.28318530717958647692 * 0.70 / sr);
        modIncR = (float) (6.28318530717958647692 * 0.93 / sr);

        clear();
        decaySm = decayT; scaleSm = scaleT; preSm = preT;
        lastLow = lastHi = -1.f;
    }

    void clear()
    {
        L.clear(); R.clear();
        for (auto& a : inAp) a.clear();
        pre.clear();
        bw = 0.f;
        prevL = prevR = 0.f;
        hpStL = hpStR = lpStL = lpStR = 0.f;
    }

    void setParams (float decaySec, float preDelayMs, float room01,
                    float diffusion01, float damping01, float lowCutHz, float hiCutHz)
    {
        decayT = std::max (0.2f, decaySec);
        scaleT = std::pow (2.0f, room01 * 2.0f - 1.0f);                // 0.5 .. 2.0
        preT   = std::min (preDelayMs, 240.f) * 0.001f * (float) sr;

        // diffusion: 0..100 % -> koefisien allpass (nilai Dattorro pada 100 %)
        const float d = std::min (std::max (diffusion01, 0.f), 1.f);
        inAp[0].g = inAp[1].g = 0.75f  * d;
        inAp[2].g = inAp[3].g = 0.625f * d;
        L.mod.g = R.mod.g = -0.70f * d;
        L.ap2.g = R.ap2.g =  0.50f * d;

        // damping: 0 % = 18 kHz (terang) ... 100 % = 1.2 kHz (gelap), kurva eksponensial
        const float dampHz = 18000.f * std::pow (1200.f / 18000.f, std::min (std::max (damping01, 0.f), 1.f));
        dampA = 1.0f - std::exp (-6.28318530717958647692f * dampHz / (float) sr);

        // bandwidth input (karakter agak gelap, tetap, tidak tergantung parameter)
        bwA = 1.0f - std::exp (-6.28318530717958647692f * 11000.f / (float) sr);

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

        // --- input: mono -> pre-delay -> bandwidth -> diffuser ---
        const float pd = std::max (1.0f, preSm);
        float x = pre.read (pd);
        pre.write (0.5f * (inL + inR));
        bw += bwA * (x - bw);
        x = bw;
        for (int k = 0; k < 4; ++k) x = inAp[k].process (x);

        // --- panjang tank (ikut Room) & penguatan agar Decay = RT60 ---
        const float lenL1 = 4453.f * s * scaleSm, lenL2 = 3720.f * s * scaleSm;
        const float lenR1 = 4217.f * s * scaleSm, lenR2 = 3163.f * s * scaleSm;

        // waktu satu putaran setengah-tank (detik): delay + allpass di cabang itu
        const float tHalfL = (lenL1 + lenL2 + 672.f * s + 1800.f * s) / (float) sr;
        const float tHalfR = (lenR1 + lenR2 + 908.f * s + 2656.f * s) / (float) sr;
        // dua penguatan per cabang, masing-masing setengah dari redaman cabang itu
        const float gL = std::pow (10.0f, -1.5f * tHalfL / decaySm);
        const float gR = std::pow (10.0f, -1.5f * tHalfR / decaySm);

        // --- modulasi allpass (LFO sinus kecil, mencegah ringing) ---
        const float depth = 5.0f * s;
        const float mL = depth * std::sin (modPhaseL);
        const float mR = depth * std::sin (modPhaseR);
        modPhaseL += modInc;  if (modPhaseL > 6.28318530717958647692f) modPhaseL -= 6.28318530717958647692f;
        modPhaseR += modIncR; if (modPhaseR > 6.28318530717958647692f) modPhaseR -= 6.28318530717958647692f;

        // --- tank kiri (disuapi output cabang kanan sampel lalu) ---
        {
            float a = L.mod.processMod (x + prevR, 672.f * s + mL);
            const float o1 = L.d1.read (std::max (2.0f, lenL1));  L.d1.write (a);
            L.damp += dampA * (o1 - L.damp);
            float b = L.damp * gL;
            b = L.ap2.process (b);
            const float o2 = L.d2.read (std::max (2.0f, lenL2));  L.d2.write (b);
            nextL = o2 * gL;
        }
        // --- tank kanan (disuapi output cabang kiri sampel lalu) ---
        {
            float a = R.mod.processMod (x + prevL, 908.f * s + mR);
            const float o1 = R.d1.read (std::max (2.0f, lenR1));  R.d1.write (a);
            R.damp += dampA * (o1 - R.damp);
            float b = R.damp * gR;
            b = R.ap2.process (b);
            const float o2 = R.d2.read (std::max (2.0f, lenR2));  R.d2.write (b);
            nextR = o2 * gR;
        }
        prevL = nextL;
        prevR = nextR;

        // --- output tap (posisi tap Dattorro; tap pada delay ikut skala Room) ---
        const float sc = s * scaleSm;
        float oL =  R.d1.read (266.f  * sc) + R.d1.read (2974.f * sc) - R.ap2.read (1913.f * s)
                  + R.d2.read (1996.f * sc) - L.d1.read (1990.f * sc) - L.ap2.read (187.f  * s)
                  - L.d2.read (1066.f * sc);
        float oR =  L.d1.read (353.f  * sc) + L.d1.read (3627.f * sc) - L.ap2.read (1228.f * s)
                  + L.d2.read (2673.f * sc) - R.d1.read (2111.f * sc) - R.ap2.read (335.f  * s)
                  - R.d2.read (121.f  * sc);

        // lebar stereo ~70 % (fokus, tidak selebar hall)
        const float mid = 0.5f * (oL + oR), side = 0.5f * (oL - oR) * width;
        oL = mid + side;
        oR = mid - side;

        // Low Cut (HP) lalu Hi Cut (LP), one-pole seperti tipe Plate/Hall
        const float xl2 = oL * outGain, xr2 = oR * outGain;
        hpStL += hpA * (xl2 - hpStL);  const float hL = xl2 - hpStL;
        hpStR += hpA * (xr2 - hpStR);  const float hR = xr2 - hpStR;
        lpStL += lpA * (hL - lpStL);
        lpStR += lpA * (hR - lpStR);
        outL = lpStL;
        outR = lpStR;
    }

private:
    // allpass dengan baca fraksional (untuk modulasi) + akses tap
    struct ModAllpass
    {
        DelayLine line;
        float g = 0.5f;

        void prepare (int maxLen) { line.prepare (maxLen); }
        void clear() { line.clear(); }

        inline float processMod (float x, float delaySamples)
        {
            const float d = line.read (std::max (2.0f, delaySamples));
            const float v = x + g * d;
            line.write (v);
            return d - g * v;
        }
    };

    // allpass biasa dengan akses tap (panjang tetap)
    struct TapAllpass
    {
        DelayLine line;
        float g = 0.5f;
        int len = 1;

        void prepare (int maxLen) { line.prepare (maxLen); len = std::max (2, maxLen - 8); }
        void clear() { line.clear(); }

        inline float process (float x)
        {
            const float d = line.read ((float) len);
            const float v = x + g * d;
            line.write (v);
            return d - g * v;
        }
        inline float read (float tap) const { return line.read (std::min (std::max (tap, 1.0f), (float) len)); }
    };

    struct Branch
    {
        ModAllpass mod;
        DelayLine d1, d2;
        TapAllpass ap2;
        float damp = 0.f;

        void clear() { mod.clear(); d1.clear(); d2.clear(); ap2.clear(); damp = 0.f; }
    };

    double sr = 44100.0;
    float s = 1.f;

    Allpass inAp[4];
    DelayLine pre;
    Branch L, R;

    float bw = 0.f, bwA = 1.f;
    float dampA = 0.3f;
    float prevL = 0.f, prevR = 0.f, nextL = 0.f, nextR = 0.f;
    float modPhaseL = 0.f, modPhaseR = 0.f, modInc = 0.f, modIncR = 0.f;

    float hpA = 0.f, lpA = 1.f;
    float hpStL = 0.f, hpStR = 0.f, lpStL = 0.f, lpStR = 0.f;
    float lastLow = -1.f, lastHi = -1.f;

    float decayT = 1.8f, decaySm = 1.8f;
    float scaleT = 1.f,  scaleSm = 1.f;
    float preT = 0.f,    preSm = 0.f;

    static constexpr float width   = 0.70f;
    static constexpr float outGain = 0.55f;
};
