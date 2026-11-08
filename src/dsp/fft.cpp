#include "gpr3d/dsp/fft.hpp"

#include <cmath>
#include <mutex>
#include <numbers>

#ifdef GPR3D_USE_FFTW3
#include <fftw3.h>
#endif

namespace gpr3d::dsp {

namespace {

#ifndef GPR3D_USE_FFTW3
void radix2_fft_inplace(std::vector<std::complex<double>>& a, bool inverse) {
    const std::size_t n = a.size();
    if (n <= 1) {
        return;
    }

    // Bit-reversal permutation
    for (std::size_t i = 1, j = 0; i < n; ++i) {
        std::size_t bit = n >> 1U;
        for (; (j & bit) != 0U; bit >>= 1U) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            std::swap(a[i], a[j]);
        }
    }

    const double sign = inverse ? 1.0 : -1.0;
    for (std::size_t len = 2; len <= n; len <<= 1U) {
        const double ang = sign * 2.0 * std::numbers::pi / static_cast<double>(len);
        const std::complex<double> wlen(std::cos(ang), std::sin(ang));
        for (std::size_t i = 0; i < n; i += len) {
            std::complex<double> w(1.0, 0.0);
            for (std::size_t j = 0; j < len / 2; ++j) {
                const std::complex<double> u = a[i + j];
                const std::complex<double> v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }

    if (inverse) {
        const double inv_n = 1.0 / static_cast<double>(n);
        for (auto& x : a) {
            x *= inv_n;
        }
    }
}
#else
std::mutex& fftw_planner_mutex() {
    static std::mutex m;
    return m;
}
#endif

}  // namespace

std::size_t next_power_of_two(std::size_t n) noexcept {
    std::size_t p = 1;
    while (p < n) {
        p <<= 1U;
    }
    return p;
}

std::vector<std::complex<double>> fft_forward(std::span<const std::complex<double>> input) {
    if (input.empty()) {
        return {};
    }

#ifdef GPR3D_USE_FFTW3
    const int n = static_cast<int>(input.size());
    std::vector<std::complex<double>> in_copy(input.begin(), input.end());
    std::vector<std::complex<double>> out(input.size());
    fftw_plan plan{};
    {
        std::lock_guard<std::mutex> lock(fftw_planner_mutex());
        plan = fftw_plan_dft_1d(
            n,
            reinterpret_cast<fftw_complex*>(in_copy.data()),
            reinterpret_cast<fftw_complex*>(out.data()),
            FFTW_FORWARD,
            FFTW_ESTIMATE);
    }
    fftw_execute(plan);
    {
        std::lock_guard<std::mutex> lock(fftw_planner_mutex());
        fftw_destroy_plan(plan);
    }
    return out;
#else
    const std::size_t n = next_power_of_two(input.size());
    std::vector<std::complex<double>> buf(n, {0.0, 0.0});
    std::copy(input.begin(), input.end(), buf.begin());
    radix2_fft_inplace(buf, false);
    return buf;
#endif
}

std::vector<std::complex<double>> fft_inverse(std::span<const std::complex<double>> spectrum) {
    if (spectrum.empty()) {
        return {};
    }

#ifdef GPR3D_USE_FFTW3
    const int n = static_cast<int>(spectrum.size());
    std::vector<std::complex<double>> in_copy(spectrum.begin(), spectrum.end());
    std::vector<std::complex<double>> out(spectrum.size());
    fftw_plan plan{};
    {
        std::lock_guard<std::mutex> lock(fftw_planner_mutex());
        plan = fftw_plan_dft_1d(
            n,
            reinterpret_cast<fftw_complex*>(in_copy.data()),
            reinterpret_cast<fftw_complex*>(out.data()),
            FFTW_BACKWARD,
            FFTW_ESTIMATE);
    }
    fftw_execute(plan);
    {
        std::lock_guard<std::mutex> lock(fftw_planner_mutex());
        fftw_destroy_plan(plan);
    }
    const double inv_n = 1.0 / static_cast<double>(spectrum.size());
    for (auto& v : out) {
        v *= inv_n;
    }
    return out;
#else
    const std::size_t n = next_power_of_two(spectrum.size());
    std::vector<std::complex<double>> buf(n, {0.0, 0.0});
    std::copy(spectrum.begin(), spectrum.end(), buf.begin());
    radix2_fft_inplace(buf, true);
    return buf;
#endif
}

std::vector<std::complex<double>> analytic_signal(std::span<const double> signal) {
    if (signal.empty()) {
        return {};
    }
    const std::size_t orig_n = signal.size();
    const std::size_t n_fft = next_power_of_two(orig_n);

    std::vector<std::complex<double>> x(n_fft, {0.0, 0.0});
    for (std::size_t i = 0; i < orig_n; ++i) {
        x[i] = {signal[i], 0.0};
    }

    auto spec = fft_forward(x);
    const std::size_t N = spec.size();

    // Marple (1999) discrete-time analytic signal filter H[k]:
    // H[0] = 1, H[1 .. N/2-1] = 2, H[N/2] = 1 (for even N), H[N/2+1 .. N-1] = 0
    if (N > 1) {
        const std::size_t half = N / 2;
        for (std::size_t k = 1; k < half; ++k) {
            spec[k] *= 2.0;
        }
        const std::size_t neg_start = (N % 2 == 0) ? (half + 1) : (half + 1);
        for (std::size_t k = neg_start; k < N; ++k) {
            spec[k] = {0.0, 0.0};
        }
    }

    auto z = fft_inverse(spec);
    z.resize(orig_n);
    return z;
}

std::vector<double> hilbert_envelope(std::span<const double> signal) {
    const auto z = analytic_signal(signal);
    std::vector<double> env(z.size());
    for (std::size_t i = 0; i < z.size(); ++i) {
        env[i] = std::abs(z[i]);
    }
    return env;
}

}  // namespace gpr3d::dsp

