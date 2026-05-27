// test_fractal_math.cpp — escape-time iteration and distance estimation.
#include "test_util.h"
#include "fractal_math.h"

using namespace fractal;

void test_fractal_math() {
    const int    iters = 256;
    const double bail   = 256.0;

    // ---- Mandelbrot membership ----
    // Origin and (-1, 0) are interior (never escape).
    CHECK(!mandelbrot(0.0, 0.0, iters, bail).escaped);
    CHECK(!mandelbrot(-1.0, 0.0, iters, bail).escaped);
    CHECK(mandelbrot(0.0, 0.0, iters, bail).iter == iters);

    // Points well outside the set escape, and quickly.
    auto out = mandelbrot(2.0, 2.0, iters, bail);
    CHECK(out.escaped);
    CHECK(out.iter < 10);

    // Farther out escapes sooner than nearer the boundary (monotone-ish).
    auto far  = mandelbrot(5.0, 5.0, iters, bail);
    auto near = mandelbrot(0.30, 0.0, iters, bail); // just outside the cardioid
    CHECK(far.iter <= near.iter);

    // ---- Smooth iteration count is well-behaved ----
    CHECK(out.smooth > 0.0);
    CHECK(std::isfinite(out.smooth));
    // Smooth value sits within ~1 of the integer escape iteration.
    CHECK(out.smooth > out.iter - 2.0 && out.smooth <= out.iter + 1.0);

    // ---- Julia set (the default spiral constant) ----
    const double cre = -0.7269, cim = 0.1889;
    CHECK(julia(2.0, 2.0, cre, cim, iters, bail).escaped); // far point escapes
    auto jin = julia(0.0, 0.0, cre, cim, iters, bail);
    CHECK(std::isfinite(jin.smooth));

    // Exponent != 2 path runs and still classifies far points as escaped.
    auto cube = mandelbrot(2.0, 2.0, iters, bail, /*exponent=*/3.0);
    CHECK(cube.escaped);

    // ---- Formula variants (Burning Ship, Tricorn) ----
    // Burning Ship folds components positive, so a point that is interior for
    // the plain Mandelbrot can behave differently. Check it runs and that the
    // abs-fold actually changes the orbit vs. quadratic at an asymmetric point.
    auto qd = escapeTime({0,0}, {-0.5, 0.55}, iters, bail, 2.0, Formula::Quadratic);
    auto bs = escapeTime({0,0}, {-0.5, 0.55}, iters, bail, 2.0, Formula::BurningShip);
    auto tc = escapeTime({0,0}, {-0.5, 0.55}, iters, bail, 2.0, Formula::Tricorn);
    CHECK(std::isfinite(bs.smooth) || !bs.escaped);
    CHECK(bs.iter != qd.iter || tc.iter != qd.iter); // at least one diverges from quadratic
    // Far points still escape fast under every formula.
    CHECK(escapeTime({0,0}, {3.0, 3.0}, iters, bail, 2.0, Formula::BurningShip).escaped);
    CHECK(escapeTime({0,0}, {3.0, 3.0}, iters, bail, 2.0, Formula::Tricorn).escaped);

    // ---- Distance estimation ----
    // Interior -> negative sentinel.
    CHECK(distanceEstimate({0,0}, {0,0}, /*mandel=*/true, iters, bail) < 0.0);
    // Exterior -> positive finite distance, shrinking toward the boundary.
    double d_far  = distanceEstimate({0,0}, {3.0, 3.0}, true, iters, bail);
    double d_near = distanceEstimate({0,0}, {0.30, 0.0}, true, iters, bail);
    CHECK(d_far > 0.0 && std::isfinite(d_far));
    CHECK(d_near > 0.0 && std::isfinite(d_near));
    CHECK(d_near < d_far); // closer to the set => smaller estimate

    // Julia distance estimate (z0 = point, dz0 = 1) is positive outside.
    double dj = distanceEstimate({2.0, 2.0}, {cre, cim}, /*mandel=*/false, iters, bail);
    CHECK(dj > 0.0 && std::isfinite(dj));
}
