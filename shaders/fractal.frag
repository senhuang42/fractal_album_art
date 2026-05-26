#version 330 core
// Escape-time fractal renderer with stripe-average coloring, smooth iteration,
// derivative normal-map shading, and distance estimation.
//
// Fidelity techniques (see README for sources):
//   * Smooth iteration count        -> bands with no stair-stepping.
//   * Stripe Average Coloring (SAC) -> averages sin(s*arg z) along the orbit
//     and interpolates by the fractional escape time. Fills smooth areas with
//     flowing detail and, crucially, has NO level-set banding (Haerkoenen 2007).
//   * Normal-map "fake 3D" shading  -> feathery embossed look from derivative.
//   * Distance estimation (iq)      -> exterior fade-to-void and filament glow.
//   * Gamma-correct supersampling   -> handled in downsample.frag.
//
// Mirrors the CPU reference in src/fractal_math.h.

out vec4 FragColor;

uniform vec2  uResolution;   // render-target size in pixels (post-SSAA)
uniform vec2  uCenter;       // view center in the complex plane
uniform float uScale;        // half vertical extent in complex units
uniform vec2  uJuliaC;       // Julia constant
uniform int   uType;         // 0 = Mandelbrot, 1 = Julia
uniform int   uMaxIter;
uniform float uBailout;      // escape radius
uniform float uExponent;     // z^exponent + c

uniform sampler2D uPalette;  // 1xN gradient (sampled along x)
uniform float uColorDensity; // palette cycles per iteration unit
uniform float uColorOffset;  // palette phase shift
uniform float uAngleColor;   // weight of escape-angle in the palette coord
uniform float uTrapColor;    // weight of orbit-trap distance in palette coord
uniform vec2  uTrapPoint;    // orbit-trap location in the complex plane
uniform float uStripeColor;   // gradient cycles the stripe value spans
uniform float uStripeFreq;    // stripe density s (integer 4/6/8 looks best)
uniform float uStripeContrast;// stretch stripe value around mid (the -mod knob)
uniform vec3  uInsideColor;  // color for points in the set

uniform float uShading;      // 0 = flat, 1 = full normal-map emboss
uniform float uLightAngle;   // light direction, degrees
uniform float uLightHeight;  // light elevation
uniform float uGlow;         // distance-estimate filament glow strength
uniform float uFalloff;      // exterior fade-to-void by distance (0 = off)

const float PI = 3.14159265358979;

vec2 cmul(vec2 a, vec2 b) { return vec2(a.x*b.x - a.y*b.y, a.x*b.y + a.y*b.x); }
vec2 cdiv(vec2 a, vec2 b) { float d = dot(b, b); return vec2(a.x*b.x + a.y*b.y, a.y*b.x - a.x*b.y) / d; }

// Complex power via polar form (used only when exponent != 2).
vec2 cpow(vec2 z, float p) {
    float r = length(z);
    if (r == 0.0) return vec2(0.0);
    float th = atan(z.y, z.x);
    return pow(r, p) * vec2(cos(p * th), sin(p * th));
}

void main() {
    // Map pixel -> complex plane, preserving aspect via the y axis.
    vec2 uv = (gl_FragCoord.xy - 0.5 * uResolution) / uResolution.y;
    vec2 p  = uCenter + uv * (2.0 * uScale);

    bool mandel = (uType == 0);
    vec2 z, c, dz;
    if (mandel) { c = p;       z = vec2(0.0); dz = vec2(0.0);      } // dz/dc
    else        { z = p;       c = uJuliaC;   dz = vec2(1.0, 0.0); } // dz/dz0

    float bail2 = uBailout * uBailout;
    bool  quad  = abs(uExponent - 2.0) < 1e-6;
    vec2  one   = mandel ? vec2(1.0, 0.0) : vec2(0.0);

    bool  useStripe = uStripeColor > 0.0;
    bool  useTrap   = uTrapColor   > 0.0;
    const int kStripeSkip = 1; // skip first iterations (transient orbit points)

    int   i;
    float m2        = dot(z, z);
    float trap      = 1e20;  // closest the orbit comes to uTrapPoint
    float stripeSum = 0.0;   // running sum of sin-stripe terms
    float lastTerm  = 0.0;   // most recent stripe term (for de-banding)
    int   stripeN   = 0;     // number of stripe terms summed
    for (i = 0; i < uMaxIter; i++) {
        // Update derivative using the current z, then advance z.
        if (quad) {
            dz = 2.0 * cmul(z, dz) + one;
            z  = cmul(z, z) + c;
        } else {
            dz = uExponent * cmul(cpow(z, uExponent - 1.0), dz) + one;
            z  = cpow(z, uExponent) + c;
        }
        if (useTrap) trap = min(trap, distance(z, uTrapPoint));
        if (useStripe && i >= kStripeSkip) {
            lastTerm   = 0.5 + 0.5 * sin(uStripeFreq * atan(z.y, z.x));
            stripeSum += lastTerm;
            stripeN++;
        }
        m2 = dot(z, z);
        if (m2 > bail2) break;
    }

    if (i >= uMaxIter) {
        FragColor = vec4(uInsideColor, 1.0);
        return;
    }

    // Continuous escape time -> smooth bands; frac(mu) drives SAC de-banding.
    float log_zn = 0.5 * log(m2);
    float nu     = log(log_zn / log(uBailout)) / log(uExponent);
    float mu     = float(i) + 1.0 - nu;

    // Stripe Average Coloring: average of sin-stripes along the orbit,
    // interpolated between including/excluding the last (overshot) orbit point
    // by the fractional escape time. This removes the level-set seams that a
    // raw orbit-trap min produces, and textures otherwise-flat regions.
    float sac = 0.0;
    if (useStripe && stripeN > 0) {
        float avgIncl = stripeSum / float(stripeN);
        float avgExcl = (stripeN > 1) ? (stripeSum - lastTerm) / float(stripeN - 1)
                                      : avgIncl;
        float d = fract(mu);
        sac = mix(avgExcl, avgIncl, d); // d*incl + (1-d)*excl
        // Stretch contrast around the midpoint so the value uses the full
        // gradient (analogous to the reference's large -mod parameter).
        sac = (sac - 0.5) * uStripeContrast + 0.5;
    }

    // Palette coordinate combines cues so color is rich and smooth everywhere.
    float angle = atan(z.y, z.x) / (2.0 * PI) + 0.5; // [0,1)
    float t = fract(mu * uColorDensity
                  + uStripeColor * sac
                  + uAngleColor  * angle
                  + uTrapColor   * trap
                  + uColorOffset);
    vec3  col = texture(uPalette, vec2(t, 0.5)).rgb;

    // Normal-map "fake 3D" shading from the escape derivative.
    if (uShading > 0.0) {
        vec2  u    = normalize(cdiv(z, dz));
        float ang  = radians(uLightAngle);
        vec2  lite = vec2(cos(ang), sin(ang));
        float h    = uLightHeight;
        float lam  = clamp((dot(u, lite) + h) / (1.0 + h), 0.0, 1.0);
        col = mix(col, col * lam, uShading);
    }

    // Distance estimate: plane-space distance to the set boundary, in pixels.
    float d       = sqrt(m2 / max(dot(dz, dz), 1e-20)) * 0.5 * log(m2);
    float pixsize = 2.0 * uScale / uResolution.y;
    float dpix    = d / pixsize;

    // Fade the exterior toward the void by distance -> color hugs the filigree.
    if (uFalloff > 0.0) {
        float vis = exp(-dpix * uFalloff);
        col = mix(uInsideColor, col, clamp(vis, 0.0, 1.0));
    }

    // Glow lights the very finest filaments right at the boundary.
    if (uGlow > 0.0) {
        float g = exp(-dpix * dpix * 0.5);
        col += uGlow * g * texture(uPalette, vec2(fract(uColorOffset + 0.5), 0.5)).rgb;
    }

    FragColor = vec4(col, 1.0);
}
