#pragma once
// Frequency axis scales and tick generation (port of friture/plotting/frequency_scales.py).

#include <string>
#include <vector>

namespace fr {

enum class FreqScale { Linear = 0, Logarithmic, Mel, Erb, Octave, OctaveC };

constexpr int kUserScaleCount = 5;  // those offered in the UI (Linear..Octave)
const char* scaleName(FreqScale s);

double scaleTransform(FreqScale s, double frequency);
double scaleInverse(FreqScale s, double value);

// Major / minor tick positions (in Hz, not transformed) for the visible range.
void scaleTicks(FreqScale s, double lo, double hi,
                std::vector<double>& major, std::vector<double>& minor);

// "440 Hz", "1.5k", etc., for axis labels.
std::string formatAxisFreq(double f);

// "A4", "C♯3", ... or "" if freq is invalid.
std::string frequencyToNote(double freq);
// "440 Hz (A4)" / "1.2 kHz (D6)"
std::string formatFrequency(double freq);

}  // namespace fr
