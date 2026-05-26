// fractal_math.h — CPU reference implementation of the escape-time iteration.
//
// This mirrors the math in shaders/fractal.frag exactly. The GPU is the
// production path; this CPU version exists so the core algorithm can be unit
// tested deterministically without a GL context, and so render output can be
// spot-checked against a known-good reference.
#pragma once

#include <cmath>
#include <complex>

namespace fractal {

struct EscapeResult {
    int    iter;      // iterations taken (== max_iter if it never escaped)
    bool   escaped;   // true if |z| exceeded the bailout radius
    double smooth;    // continuous (fractional) iteration count; only meaningful when escaped
};

// Iterate z -> z^exponent + c starting from z0, until |z| > bailout or
// max_iter is reached. Returns the (smoothed) escape time.
//
// The smooth iteration count uses the standard normalized formula for the
// quadratic map; for non-quadratic exponents it uses the generalized
// log-of-log form. Both produce continuous bands suitable for gradient mapping.
inline EscapeResult escapeTime(std::complex<double> z,
                               std::complex<double> c,
                               int    max_iter,
                               double bailout,
                               double exponent = 2.0) {
    const double bail2 = bailout * bailout;
    int i = 0;
    double mag2 = std::norm(z); // |z|^2
    for (; i < max_iter; ++i) {
        if (exponent == 2.0) {
            // Fast path matching the shader's z*z.
            const double zr = z.real(), zi = z.imag();
            z = std::complex<double>(zr * zr - zi * zi, 2.0 * zr * zi) + c;
        } else {
            z = std::pow(z, exponent) + c;
        }
        mag2 = std::norm(z);
        if (mag2 > bail2) {
            // Continuous escape time. For power p:
            //   nu = log_p( log|z| / log(bailout) )
            //   smooth = i + 1 - nu
            const double log_zn = 0.5 * std::log(mag2); // = log|z|
            const double nu     = std::log(log_zn / std::log(bailout)) / std::log(exponent);
            return EscapeResult{i + 1, true, static_cast<double>(i + 1) - nu};
        }
    }
    return EscapeResult{max_iter, false, static_cast<double>(max_iter)};
}

// Convenience wrappers that pick z0/c per fractal family.
inline EscapeResult mandelbrot(double px, double py, int max_iter,
                               double bailout, double exponent = 2.0) {
    return escapeTime({0.0, 0.0}, {px, py}, max_iter, bailout, exponent);
}

inline EscapeResult julia(double px, double py, double cre, double cim,
                          int max_iter, double bailout, double exponent = 2.0) {
    return escapeTime({px, py}, {cre, cim}, max_iter, bailout, exponent);
}

// Distance estimate to the fractal boundary, mirroring shaders/fractal.frag.
// Tracks the derivative dz alongside z (z'_{n+1} = 2 z_n z'_n [+1 for the
// Mandelbrot dz/dc]). Returns a negative value for points that never escaped
// (treated as interior). Formula: d = sqrt(|z|^2/|dz|^2) * 0.5 * log(|z|^2).
// (iquilezles.org/articles/distancefractals)
inline double distanceEstimate(std::complex<double> z0,
                               std::complex<double> c,
                               bool   is_mandelbrot,
                               int    max_iter,
                               double bailout) {
    std::complex<double> z  = z0;
    std::complex<double> dz = is_mandelbrot ? std::complex<double>(0, 0)
                                            : std::complex<double>(1, 0);
    const std::complex<double> one = is_mandelbrot ? std::complex<double>(1, 0)
                                                   : std::complex<double>(0, 0);
    const double bail2 = bailout * bailout;
    double m2 = std::norm(z);
    for (int i = 0; i < max_iter; ++i) {
        dz = 2.0 * z * dz + one;
        z  = z * z + c;
        m2 = std::norm(z);
        if (m2 > bail2) {
            const double d2 = std::norm(dz);
            return std::sqrt(m2 / std::max(d2, 1e-20)) * 0.5 * std::log(m2);
        }
    }
    return -1.0; // interior
}

} // namespace fractal
