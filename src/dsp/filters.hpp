#pragma once
// Direct-form-II-transposed IIR filter with persistent state (scipy.signal.lfilter
// semantics), a 2x decimator, and Friture's decimating octave filter bank.

#include <cstddef>
#include <string>
#include <vector>

namespace fr {

class LFilter {
public:
    LFilter() = default;
    LFilter(const double* b, const double* a, size_t len);
    void process(const double* x, size_t n, double* y);
    void reset();

private:
    std::vector<double> b_, a_, z_;
};

// Low-pass with the shared 13-tap "dec" filter, then keep every 2nd sample.
class Decimator {
public:
    Decimator();
    // Output has ceil(n/2) samples.
    void process(const double* x, size_t n, std::vector<double>& out);

private:
    LFilter lp_;
    std::vector<double> tmp_;
};

class OctaveFilterBank {
public:
    explicit OctaveFilterBank(int bandsPerOctave = 3) { setBandsPerOctave(bandsPerOctave); }
    void setBandsPerOctave(int bpo);
    int bandsPerOctave() const { return bpo_; }
    int bandCount() const;

    // y[k] is band k's output (lowest band first), possibly decimated.
    void process(const double* x, size_t n, std::vector<std::vector<double>>& y);

    // Decimation factor of band k.
    int decimationOf(int k) const;

    // Band centre / edges in Hz and A/B/C weighting offsets, label strings.
    const std::vector<double>& fi() const { return fi_; }
    const std::vector<double>& flow() const { return flow_; }
    const std::vector<double>& fhigh() const { return fhigh_; }
    const std::vector<double>& weightA() const { return A_; }
    const std::vector<double>& weightB() const { return B_; }
    const std::vector<double>& weightC() const { return C_; }
    const std::vector<std::string>& nominal() const { return nominal_; }

private:
    int bpo_ = 0;
    std::vector<LFilter> bands_;         // [octave][band]
    std::vector<Decimator> decimators_;  // [octave]
    std::vector<double> fi_, flow_, fhigh_, A_, B_, C_;
    std::vector<std::string> nominal_;
    std::vector<double> stage_, next_;
};

}  // namespace fr
