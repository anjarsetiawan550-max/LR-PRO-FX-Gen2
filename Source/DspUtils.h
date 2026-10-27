#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

// ---------------------------------------------------------------------------
// Biquad (RBJ cookbook), Direct Form II transposed
// ---------------------------------------------------------------------------
struct Biquad
{
    float b0 = 1.f, b1 = 0.f, b2 = 0.f, a1 = 0.f, a2 = 0.f, z1 = 0.f, z2 = 0.f;

    void reset() { z1 = z2 = 0.f; }

    inline float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void setCoeffs (double B0, double B1, double B2, double A0, double A1, double A2)
    {
        const double i = 1.0 / A0;
        b0 = (float) (B0 * i);  b1 = (float) (B1 * i);  b2 = (float) (B2 * i);
        a1 = (float) (A1 * i);  a2 = (float) (A2 * i);
    }

    static double clampFreq (double sr, double f) { return std::min (std::max (f, 5.0), sr * 0.45); }

    void lowPass (double sr, double f, double q = 0.70710678)
    {
        const double w0 = 2.0 * 3.14159265358979323846 * clampFreq (sr, f) / sr;
        const double c = std::cos (w0), al = std::sin (w0) / (2.0 * q);
        setCoeffs ((1 - c) * 0.5, 1 - c, (1 - c) * 0.5, 1 + al, -2 * c, 1 - al);
    }

    void highPass (double sr, double f, double q = 0.70710678)
    {
        const double w0 = 2.0 * 3.14159265358979323846 * clampFreq (sr, f) / sr;
        const double c = std::cos (w0), al = std::sin (w0) / (2.0 * q);
        setCoeffs ((1 + c) * 0.5, -(1 + c), (1 + c) * 0.5, 1 + al, -2 * c, 1 - al);
    }

    void lowShelf (double sr, double f, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = 2.0 * 3.14159265358979323846 * clampFreq (sr, f) / sr;
        const double c = std::cos (w0), s = std::sin (w0);
        const double al = s * 0.5 * std::sqrt (2.0);              // S = 1
        const double t = 2.0 * std::sqrt (A) * al;
        setCoeffs (A * ((A + 1) - (A - 1) * c + t),
                   2 * A * ((A - 1) - (A + 1) * c),
                   A * ((A + 1) - (A - 1) * c - t),
                   (A + 1) + (A - 1) * c + t,
                   -2 * ((A - 1) + (A + 1) * c),
                   (A + 1) + (A - 1) * c - t);
    }

    void highShelf (double sr, double f, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = 2.0 * 3.14159265358979323846 * clampFreq (sr, f) / sr;
        const double c = std::cos (w0), s = std::sin (w0);
        const double al = s * 0.5 * std::sqrt (2.0);              // S = 1
        const double t = 2.0 * std::sqrt (A) * al;
        setCoeffs (A * ((A + 1) + (A - 1) * c + t),
                   -2 * A * ((A - 1) + (A + 1) * c),
                   A * ((A + 1) + (A - 1) * c - t),
                   (A + 1) - (A - 1) * c + t,
                   2 * ((A - 1) - (A + 1) * c),
                   (A + 1) - (A - 1) * c - t);
    }
};

// ---------------------------------------------------------------------------
// Delay line with fractional (linear interpolated) read.
// Convention: call read() BEFORE write() in the same sample step; read (d)
// then returns the input that was written exactly d steps ago.
// ---------------------------------------------------------------------------
struct DelayLine
{
    std::vector<float> buf;
    int w = 0, mask = 0;

    void prepare (int maxDelaySamples)
    {
        int n = 1;
        while (n < maxDelaySamples + 4) n <<= 1;
        buf.assign ((size_t) n, 0.f);
        mask = n - 1;
        w = 0;
    }

    void clear() { std::fill (buf.begin(), buf.end(), 0.f); w = 0; }

    inline void write (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }

    inline float read (float d) const
    {
        const float pos = (float) w - d;
        const int i0 = (int) std::floor (pos);
        const float f = pos - (float) i0;
        const float a = buf[(size_t) (i0 & mask)];
        const float b = buf[(size_t) ((i0 + 1) & mask)];
        return a + f * (b - a);
    }
};

// ---------------------------------------------------------------------------
// Schroeder all-pass diffuser
// ---------------------------------------------------------------------------
struct Allpass
{
    std::vector<float> buf;
    int idx = 0;
    float g = 0.5f;

    void prepare (int len) { buf.assign ((size_t) std::max (1, len), 0.f); idx = 0; }
    void clear() { std::fill (buf.begin(), buf.end(), 0.f); idx = 0; }

    inline float process (float x)
    {
        const float d = buf[(size_t) idx];
        const float v = x + g * d;
        buf[(size_t) idx] = v;
        if (++idx >= (int) buf.size()) idx = 0;
        return d - g * v;
    }
};
