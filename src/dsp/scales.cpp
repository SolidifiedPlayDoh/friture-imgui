#include "dsp/scales.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace fr {

namespace {

int adjusted(double x) { return (int)std::floor(std::log10(std::fabs(x))); }

double floorWithPrecision(double x, int prec) {
    return std::floor(x * std::pow(10.0, -prec)) * std::pow(10.0, prec);
}

double ceilWithinInterval(double x, double interval) {
    int prec = adjusted(interval);
    double candidate = floorWithPrecision(x, prec + 1);
    while (candidate < x) candidate += interval;
    return candidate;
}

double roundWithPrecision(double x, int prec) {
    const double p = std::pow(10.0, prec);
    const double c[3] = {1.0 * p, 2.0 * p, 5.0 * p};
    int best = 0;
    for (int i = 1; i < 3; ++i)
        if (std::fabs(x - c[i]) < std::fabs(x - c[best])) best = i;
    return c[best];
}

void linearTicks(double lo, double hi, std::vector<double>& major, std::vector<double>& minor) {
    double tmin = std::min(lo, hi), tmax = std::max(lo, hi);
    double range = tmax - tmin;
    if (!(range > 0)) return;
    double base = range / 6.0;
    int prec = adjusted(base);
    double interval = roundWithPrecision(base, prec);
    if (interval == 0) return;
    double rmin = ceilWithinInterval(tmin, interval);
    int n = (int)std::floor((tmax - rmin) / interval);
    for (int i = 0; i <= n; ++i) major.push_back(rmin + interval * i);
    if (major.size() < 2) return;

    int digit = (int)std::lround(interval / std::pow(10.0, adjusted(interval)));
    int div = (digit == 2) ? 4 : 5;
    double minorInterval = std::fabs(interval / div);
    double x = major[0];
    while (x >= tmin) x -= minorInterval;
    x += minorInterval;
    while (x <= tmax) {
        minor.push_back(x);
        x += minorInterval;
    }
}

void logTicks(double lo, double hi, std::vector<double>& major, std::vector<double>& minor) {
    double tmin = std::min(lo, hi), tmax = std::max(lo, hi);
    if (tmin <= 0.0) {
        tmin = 1e-20;
        tmax = std::max(tmax, tmin);
    }
    int lmin = (int)std::ceil(std::log10(tmin));
    int lmax = (int)std::floor(std::log10(tmax));
    for (int i = lmin; i <= lmax; ++i) major.push_back(std::pow(10.0, i));
    if (major.size() < 2) return;
    for (int a = 2; a <= 9; ++a) {
        double m = a * major.front() / 10.0;
        if (m >= tmin && m <= tmax) minor.push_back(m);
    }
    for (int a = 2; a <= 9; ++a)
        for (double x : major)
            if (a * x <= tmax) minor.push_back(a * x);
    for (int a = 2; a <= 9; ++a) {
        double m = a * major.back();
        if (m <= tmax) minor.push_back(m);
    }
}

void octaveTicks(double lo, double hi, double ref, bool thirds,
                 std::vector<double>& major, std::vector<double>& minor) {
    if (lo > hi) std::swap(lo, hi);
    lo = std::max(1e-20, lo);
    hi = std::max(1e-20, hi);
    int minA = (int)std::ceil(std::log2(lo / ref));
    int maxA = (int)std::floor(std::log2(hi / ref));
    for (int i = minA; i <= maxA; ++i) major.push_back(ref * std::pow(2.0, i));
    std::vector<double> ratios;
    if (thirds) {
        ratios = {std::pow(2.0, 3.0 / 12), std::pow(2.0, 6.0 / 12), std::pow(2.0, 9.0 / 12)};
    } else {
        for (int k = 0; k < 12; ++k) ratios.push_back(std::pow(2.0, k / 12.0));
    }
    for (int a = minA - 1; a <= maxA; ++a)
        for (double t : ratios) {
            double f = ref * std::pow(2.0, a) * t;
            if (f >= lo && f <= hi) minor.push_back(f);
        }
}

constexpr double kErbA = 21.33228113095401739888262;

}  // namespace

const char* scaleName(FreqScale s) {
    switch (s) {
        case FreqScale::Linear: return "Linear";
        case FreqScale::Logarithmic: return "Logarithmic";
        case FreqScale::Mel: return "Mel";
        case FreqScale::Erb: return "ERB";
        case FreqScale::Octave: return "Octave";
        case FreqScale::OctaveC: return "Octave (C)";
    }
    return "";
}

double scaleTransform(FreqScale s, double f) {
    switch (s) {
        case FreqScale::Linear: return f;
        case FreqScale::Logarithmic: return std::log10(std::max(f, 1e-20));
        case FreqScale::Mel: return 2595.0 * std::log10(1.0 + f / 700.0);
        case FreqScale::Erb: return kErbA * std::log10(1.0 + 0.00437 * f);
        case FreqScale::Octave:
        case FreqScale::OctaveC: return std::log2(std::max(f, 1e-20));
    }
    return f;
}

double scaleInverse(FreqScale s, double v) {
    switch (s) {
        case FreqScale::Linear: return v;
        case FreqScale::Logarithmic: return std::pow(10.0, v);
        case FreqScale::Mel: return 700.0 * (std::pow(10.0, v / 2595.0) - 1.0);
        case FreqScale::Erb: return (std::pow(10.0, v / kErbA) - 1.0) / 0.00437;
        case FreqScale::Octave:
        case FreqScale::OctaveC: return std::pow(2.0, v);
    }
    return v;
}

void scaleTicks(FreqScale s, double lo, double hi,
                std::vector<double>& major, std::vector<double>& minor) {
    major.clear();
    minor.clear();
    switch (s) {
        case FreqScale::Linear: linearTicks(lo, hi, major, minor); break;
        case FreqScale::Logarithmic:
        case FreqScale::Mel:
        case FreqScale::Erb: logTicks(lo, hi, major, minor); break;
        case FreqScale::Octave: octaveTicks(lo, hi, 440.0, true, major, minor); break;
        case FreqScale::OctaveC:
            octaveTicks(lo, hi, 220.0 * std::pow(2.0, 3.0 / 12), false, major, minor);
            break;
    }
}

std::string formatAxisFreq(double f) {
    char b[32];
    if (f >= 1000.0) {
        double k = f / 1000.0;
        if (std::fabs(k - std::round(k)) < 1e-9) std::snprintf(b, sizeof b, "%.0fk", k);
        else std::snprintf(b, sizeof b, "%.3gk", k);
    } else if (std::fabs(f - std::round(f)) < 1e-9) {
        std::snprintf(b, sizeof b, "%.0f", f);
    } else {
        std::snprintf(b, sizeof b, "%.4g", f);
    }
    return b;
}

std::string frequencyToNote(double freq) {
    if (std::isnan(freq) || freq <= 0) return "";
    static const char* notes[12] = {"C", "C#", "D", "D#", "E", "F",
                                    "F#", "G", "G#", "A", "A#", "B"};
    long semitone = std::lround(std::log2(freq / 440.0) * 12.0) + 9;
    long octave = (long)std::floor(semitone / 12.0) + 4;
    long idx = ((semitone % 12) + 12) % 12;
    return std::string(notes[idx]) + std::to_string(octave);
}

std::string formatFrequency(double freq) {
    char b[64];
    if (freq < 1000.0)
        std::snprintf(b, sizeof b, "%.0f Hz (%s)", freq, frequencyToNote(freq).c_str());
    else
        std::snprintf(b, sizeof b, "%.1f kHz (%s)", freq / 1000.0, frequencyToNote(freq).c_str());
    return b;
}

}  // namespace fr
