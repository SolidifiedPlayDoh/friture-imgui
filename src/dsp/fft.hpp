#pragma once
#include <complex>
#include <cstddef>

#include "pocketfft_hdronly.h"

namespace fr {

// numpy.fft.rfft equivalent: n real samples -> n/2+1 complex bins.
inline void rfft(const double* in, size_t n, std::complex<double>* out) {
    pocketfft::shape_t shape{n};
    pocketfft::stride_t si{(ptrdiff_t)sizeof(double)};
    pocketfft::stride_t so{(ptrdiff_t)sizeof(std::complex<double>)};
    pocketfft::r2c(shape, si, so, 0, true, in, out, 1.0);
}

// numpy.fft.irfft equivalent (includes the 1/n scaling).
inline void irfft(const std::complex<double>* in, size_t n, double* out) {
    pocketfft::shape_t shape{n};
    pocketfft::stride_t si{(ptrdiff_t)sizeof(std::complex<double>)};
    pocketfft::stride_t so{(ptrdiff_t)sizeof(double)};
    pocketfft::c2r(shape, si, so, 0, false, in, out, 1.0 / (double)n);
}

}  // namespace fr
