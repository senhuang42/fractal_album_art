// config.h — parameters describing a single fractal render or an animation.
//
// Everything here is plain data with no OpenGL dependency so it can be
// constructed and inspected in unit tests.
#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace fractal {

// Which family to iterate.
enum class FractalType {
    Mandelbrot, // c = pixel,        z0 = 0
    Julia,      // c = fixed const,  z0 = pixel
};

// How an animation evolves over its duration. All modes are designed so that
// frame 0 and the final frame line up, giving a seamless loop.
enum class AnimMode {
    Rotate, // Julia constant orbits the origin: c = r * e^(i*theta), theta 0..2pi
    Zoom,   // smooth zoom from start scale to end scale toward a target point
    Cycle,  // static fractal, palette phase sweeps a full cycle
};

// An RGB color in linear-ish [0,1] sRGB space.
struct Color {
    float r = 0.0f, g = 0.0f, b = 0.0f;
    bool operator==(const Color& o) const {
        return r == o.r && g == o.g && b == o.b;
    }
};

// A full description of one still image. The animation layer mutates a copy of
// this per frame (center, scale, julia_c, color_offset).
struct RenderConfig {
    FractalType type = FractalType::Julia;

    // View into the complex plane. `scale` is half the vertical extent in
    // complex units, so smaller scale == deeper zoom.
    double center_x = 0.0;
    double center_y = 0.0;
    double scale    = 1.35;

    // Julia constant (ignored for Mandelbrot). This default gives bold
    // double spirals reminiscent of the reference art.
    double julia_cre = -0.512511;
    double julia_cim = 0.521295;

    // Iteration controls.
    int    max_iter   = 400;
    double exponent   = 2.0;   // z^exponent + c
    double bailout    = 256.0; // escape radius (|z| > bailout => escaped)

    // Output image size in pixels (pre-supersampling).
    int width  = 1600;
    int height = 1600;
    int ssaa   = 4; // supersampling factor per axis (1 = off) -> 16 samples/px

    // Coloring.
    std::vector<Color> palette;        // gradient stops; filled from name/spec
    bool   cyclic        = true;       // close the gradient loop for seamless cycling
    double color_density = 0.18;       // palette cycles per iteration unit
    double color_offset  = 0.0;        // palette phase shift [0,1)
    // Blends the escape-angle (direction of final z) into the palette
    // coordinate. Kept small by default: it adds hue grain at the boundary but
    // becomes chaotic speckle deep in the body, so iteration + trap dominate.
    double angle_color   = 0.1;        // 0 = pure iteration bands
    // Orbit-trap coloring: weight and location of the trap point. The minimum
    // distance the orbit comes to this point varies smoothly across the dense
    // body, giving lush color where iteration banding alone goes flat. This is
    // the main source of the references' rich body color.
    double trap_color    = 1.6;        // 0 = no orbit-trap contribution
    double trap_x        = 0.0;
    double trap_y        = 0.0;
    Color  inside_color  = {0,0,0};    // color for points in the set
    double saturation    = 1.3;        // final grade
    double gamma         = 1.05;       // final grade (1 = none)

    // Derivative-based fidelity controls (see shaders/fractal.frag).
    // `shading` blends in normal-map "fake 3D" lighting computed from the
    // escape derivative — this is what gives the feathery, embossed look.
    double shading      = 0.6;   // 0 = flat smooth color, 1 = full emboss
    double light_angle  = 45.0;  // light direction, degrees
    double light_height = 1.2;   // light elevation (higher = flatter)
    // `glow` lights the thin filaments using a distance estimate.
    double glow         = 0.0;   // 0 = off
    // `falloff` fades the exterior toward inside_color by distance to the set,
    // producing the signature black void with color hugging the filigree.
    // 0 = exterior fully colored; larger = tighter color shell.
    double falloff      = 0.009;

    std::string output = "fractal.png";
};

// Extends RenderConfig with timeline controls for `video`.
struct VideoConfig : RenderConfig {
    AnimMode mode     = AnimMode::Rotate;
    double   duration = 20.0; // seconds
    int      fps      = 30;
    int      crf      = 18;   // x264 quality (lower = better)

    // Rotate mode: |c| stays fixed at this radius.
    double rotate_radius = 0.7885;
    // Zoom mode: end scale and target point.
    double zoom_end      = 0.005;
    double zoom_target_x = 0.0;
    double zoom_target_y = 0.0;

    int total_frames() const { return static_cast<int>(duration * fps + 0.5); }
};

} // namespace fractal
