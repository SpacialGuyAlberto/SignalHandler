#pragma once

#include <complex>
#include <cstddef>
#include <span>
#include <vector>

namespace gpr3d::dsp {

/// Smallest power of two greater than or equal to n (minimum 1).
[[nodiscard]] std::size_t next_power_of_two(std::size_t n) noexcept;

/// 1D Discrete Fourier Transform (accelerated with FFTW3 when compiled with GPR3D_USE_FFTW3).
[[nodiscard]] std::vector<std::complex<double>> fft_forward(
    std::span<const std::complex<double>> input);

/// Normalized 1D Inverse Discrete Fourier Transform (1/N scaling included).
[[nodiscard]] std::vector<std::complex<double>> fft_inverse(
    std::span<const std::complex<double>> spectrum);

/// Compute the discrete-time analytic signal z[n] = x[n] + j * Hilbert(x[n]).
/// Its magnitude std::abs(z[n]) yields the instantaneous radar wavelet envelope.
[[nodiscard]] std::vector<std::complex<double>> analytic_signal(
    std::span<const double> signal);

/// Convenience helper returning instantaneous Hilbert envelope |z[n]| for a real trace.
[[nodiscard]] std::vector<double> hilbert_envelope(std::span<const double> signal);

}  // namespace gpr3d::dsp

