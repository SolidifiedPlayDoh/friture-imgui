#include "dsp/filters.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "config.hpp"
#include "dsp/generated_filters.hpp"

namespace fr {

// ---------------------------------------------------------------- LFilter

LFilter::LFilter(const double* b, const double* a, size_t len) {
    b_.assign(b, b + len);
    a_.assign(a, a + len);
    double a0 = a_[0];
    if (a0 != 1.0) {
        for (auto& v : b_) v /= a0;
        for (auto& v : a_) v /= a0;
    }
    z_.assign(len > 0 ? len - 1 : 0, 0.0);
}

void LFilter::reset() { std::fill(z_.begin(), z_.end(), 0.0); }

void LFilter::process(const double* x, size_t n, double* y) {
    const size_t order = z_.size();
    if (order == 0) {
        for (size_t i = 0; i < n; ++i) y[i] = b_[0] * x[i];
        return;
    }
    for (size_t m = 0; m < n; ++m) {
        double xm = x[m];
        double ym = b_[0] * xm + z_[0];
        for (size_t i = 1; i < order; ++i) z_[i - 1] = b_[i] * xm + z_[i] - a_[i] * ym;
        z_[order - 1] = b_[order] * xm - a_[order] * ym;
        y[m] = ym;
    }
}

// ---------------------------------------------------------------- Decimator

Decimator::Decimator() : lp_(tables::kDecB, tables::kDecA, tables::kDecLen) {}

void Decimator::process(const double* x, size_t n, std::vector<double>& out) {
    tmp_.resize(n);
    lp_.process(x, n, tmp_.data());
    out.resize((n + 1) / 2);
    for (size_t i = 0; i < out.size(); ++i) out[i] = tmp_[2 * i];
}

// ---------------------------------------------------------------- Octave bank

namespace {

std::vector<double> renard(int bpo) {
    std::vector<double> r = {1.0, 1.25, 1.6, 2.0, 2.5, 3.15, 4.0, 5.0, 6.3, 8.0};  // R10
    auto add = [&r](std::initializer_list<double> v) { r.insert(r.end(), v); };
    if (bpo >= 6)  // R20
        add({1.12, 1.4, 1.8, 2.24, 2.8, 3.55, 4.5, 5.6, 7.1, 9.0});
    if (bpo >= 12)  // R40
        add({1.06, 1.32, 1.70, 2.12, 2.65, 3.35, 4.25, 5.30, 6.70, 8.50,
             1.18, 1.50, 1.90, 2.36, 3.00, 3.75, 4.75, 6.00, 7.50, 9.50});
    if (bpo >= 24)  // R80
        add({1.03, 1.28, 1.65, 2.06, 2.58, 3.25, 4.12, 5.15, 6.50, 8.25,
             1.09, 1.36, 1.75, 2.18, 2.72, 3.45, 4.37, 5.45, 6.90, 8.75,
             1.15, 1.45, 1.85, 2.30, 2.90, 3.65, 4.62, 5.80, 7.30, 9.25,
             1.22, 1.55, 1.95, 2.43, 3.07, 3.87, 4.87, 6.15, 7.75, 9.75});
    std::sort(r.begin(), r.end());
    return r;
}

struct TopOctave {
    const double *b, *a;
    int bands, order;
};

TopOctave topOctave(int bpo) {
    switch (bpo) {
        case 1: return {tables::kB_1, tables::kA_1, tables::kBands_1, tables::kOrder_1};
        case 3: return {tables::kB_3, tables::kA_3, tables::kBands_3, tables::kOrder_3};
        case 6: return {tables::kB_6, tables::kA_6, tables::kBands_6, tables::kOrder_6};
        case 12: return {tables::kB_12, tables::kA_12, tables::kBands_12, tables::kOrder_12};
        default: return {tables::kB_24, tables::kA_24, tables::kBands_24, tables::kOrder_24};
    }
}

}  // namespace

int OctaveFilterBank::bandCount() const { return kNOctave * bpo_; }

int OctaveFilterBank::decimationOf(int k) const {
    // band k belongs to octave j counted from the top: j = (count-1-k)/bpo
    int j = (bandCount() - 1 - k) / bpo_;
    return 1 << j;
}

void OctaveFilterBank::setBandsPerOctave(int bpo) {
    if (bpo != 1 && bpo != 3 && bpo != 6 && bpo != 12 && bpo != 24) bpo = 3;
    bpo_ = bpo;

    TopOctave t = topOctave(bpo);
    bands_.clear();
    decimators_.clear();
    for (int j = 0; j < kNOctave; ++j) {
        for (int i = 0; i < t.bands; ++i)
            bands_.emplace_back(t.b + i * t.order, t.a + i * t.order, (size_t)t.order);
        decimators_.emplace_back();
    }

    // Centre / edge frequencies (octave_frequencies in friture/filter.py).
    int total = bandCount();
    double b = 1.0 / bpo;
    int imax = total / 2;
    int i0 = (total % 2 == 0) ? -imax : -imax;
    int i1 = (total % 2 == 0) ? imax : imax + 1;
    fi_.clear(); flow_.clear(); fhigh_.clear();
    for (int i = i0; i < i1; ++i) {
        double f = 1000.0 * std::pow(2.0, i * b);
        fi_.push_back(f);
        flow_.push_back(f * std::sqrt(std::pow(2.0, -b)));
        fhigh_.push_back(f * std::sqrt(std::pow(2.0, b)));
    }

    A_.clear(); B_.clear(); C_.clear();
    for (double f : fi_) {
        double f2 = f * f;
        double Rc = 12200.0 * 12200.0 * f2 / ((f2 + 20.6 * 20.6) * (f2 + 12200.0 * 12200.0));
        double Rb = 12200.0 * 12200.0 * f * f2 /
                    ((f2 + 20.6 * 20.6) * (f2 + 12200.0 * 12200.0) * std::sqrt(f2 + 158.5 * 158.5));
        double Ra = 12200.0 * 12200.0 * f2 * f2 /
                    ((f2 + 20.6 * 20.6) * (f2 + 12200.0 * 12200.0) *
                     std::sqrt(f2 + 107.7 * 107.7) * std::sqrt(f2 + 737.9 * 737.9));
        C_.push_back(0.06 + 20.0 * std::log10(Rc));
        B_.push_back(0.17 + 20.0 * std::log10(Rb));
        A_.push_back(2.0 + 20.0 * std::log10(Ra));
    }

    // Nominal labels.
    nominal_.clear();
    char buf[32];
    if (bpo == 1) {
        for (double f : fi_) {
            if (f >= 10000) std::snprintf(buf, sizeof buf, "%.1fk", f / 1000);
            else if (f >= 1000) std::snprintf(buf, sizeof buf, "%.2fk", f / 1000);
            else std::snprintf(buf, sizeof buf, "%d", (int)f);
            nominal_.push_back(buf);
        }
    } else {
        std::vector<double> basis = renard(bpo);
        int idx = 0;
        for (size_t i = 0; i < fi_.size(); ++i)
            if (fi_[i] == 1000.0) { idx = (int)i; break; }
        std::vector<std::string> upper;  // from 1 kHz upward
        size_t needUpper = fi_.size() - idx;
        for (int k = 0; upper.size() < needUpper; ++k)
            for (double f : basis) {
                std::snprintf(buf, sizeof buf, "%.*fk", 2 - k < 0 ? 0 : 2 - k, std::pow(10.0, k) * f);
                upper.push_back(buf);
            }
        upper.resize(needUpper);
        std::vector<std::string> lower;  // below 1 kHz, assembled from the bottom up
        for (int k = 0; lower.size() < (size_t)idx; ++k) {
            std::vector<std::string> block;
            for (double f : basis) {
                std::snprintf(buf, sizeof buf, "%d", (int)(std::pow(10.0, 2 - k) * f));
                block.push_back(buf);
            }
            lower.insert(lower.begin(), block.begin(), block.end());
        }
        std::vector<std::string> all = lower;
        all.insert(all.end(), upper.begin(), upper.end());
        // keep the last fi_.size() entries (python slices [-len:])
        nominal_.assign(all.end() - fi_.size(), all.end());
    }
}

void OctaveFilterBank::process(const double* x, size_t n, std::vector<std::vector<double>>& y) {
    const int count = bandCount();
    y.resize(count);
    if (n == 0) return;
    TopOctave t = topOctave(bpo_);

    stage_.assign(x, x + n);
    int k = count - 1;
    for (int j = 0; j < kNOctave; ++j) {
        for (int i = t.bands - 1; i >= 0; --i) {
            LFilter& f = bands_[(size_t)j * t.bands + i];
            y[k].resize(stage_.size());
            f.process(stage_.data(), stage_.size(), y[k].data());
            --k;
        }
        decimators_[j].process(stage_.data(), stage_.size(), next_);
        stage_.swap(next_);
    }
}

}  // namespace fr
