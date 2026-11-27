#pragma once
// Windowed power spectrum + A/B/C weighting curves (port of friture/audioproc.py).

#include <complex>
#include <vector>

namespace fr {

class AudioProc {
public:
    AudioProc() { setFftSize(1024); }

    void setFftSize(int n);
    int fftSize() const { return n_; }

    const std::vector<double>& freq() const { return freq_; }
    const std::vector<double>& weightA() const { return A_; }
    const std::vector<double>& weightB() const { return B_; }
    const std::vector<double>& weightC() const { return C_; }
    const std::vector<double>& window() const { return window_; }

    // |FFT(samples * hann)|^2 / N^2, n/2+1 bins. `samples` has fftSize() entries.
    void analyze(const double* samples, std::vector<double>& power);

    // weighting: 0 none, 1 A, 2 B, 3 C. Returns an array (size n/2+1) of dB offsets.
    std::vector<double> weighting(int kind) const;

private:
    int n_ = 0;
    std::vector<double> window_, freq_, A_, B_, C_;
    std::vector<double> scratchIn_;
    std::vector<std::complex<double>> scratchOut_;
};

}  // namespace fr
