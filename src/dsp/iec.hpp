#pragma once
// IEC 60268-18 style meter scale and peak ballistics (friture/iec.py, ballistic_peak.py).

namespace fr {

inline double dBtoIEC(double dB) {
    if (dB < -70.0) return 0.0;
    if (dB < -60.0) return (dB + 70.0) * 0.0025;
    if (dB < -50.0) return (dB + 60.0) * 0.005 + 0.025;
    if (dB < -40.0) return (dB + 50.0) * 0.0075 + 0.075;
    if (dB < -30.0) return (dB + 40.0) * 0.015 + 0.15;
    if (dB < -20.0) return (dB + 30.0) * 0.02 + 0.3;
    return (dB + 20.0) * 0.025 + 0.5;
}

// Peak meter with hold + accelerating fall-off. Step once per 25 ms.
class BallisticPeak {
public:
    static constexpr double kDecayRate = 1.0 - 3e-6 / 500.0;
    static constexpr int kFalloff = 32;

    double value() const { return peak_; }

    void update(double iec) {
        double next;
        if (iec > peak_) {
            next = iec;
            hold_ = 0;
            decay_ = kDecayRate;
        } else if (hold_ + 1 <= kFalloff) {
            next = peak_;
            ++hold_;
        } else {
            next = decay_ * peak_;
            if (next < iec) {
                next = iec;
                hold_ = 0;
                decay_ = kDecayRate;
            } else {
                decay_ *= decay_;
            }
        }
        peak_ = next;
    }

private:
    double peak_ = 0.0;
    int hold_ = 0;
    double decay_ = kDecayRate;
};

}  // namespace fr
