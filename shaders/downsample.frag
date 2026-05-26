#version 330 core
// Box-filter supersampling resolve: averages an NxN block of the high-res
// render into one output pixel. Averaging is done in linear light for correct
// anti-aliasing, then a final color grade (saturation, gamma) is applied.

out vec4 FragColor;

uniform sampler2D uTex;     // high-res render
uniform int   uSSAA;        // block size per axis
uniform float uSaturation;  // 1 = unchanged
uniform float uGamma;       // 1 = unchanged

vec3 toLinear(vec3 c) { return pow(c, vec3(2.2)); }
vec3 toSRGB(vec3 c)   { return pow(c, vec3(1.0 / 2.2)); }

void main() {
    ivec2 base = ivec2(gl_FragCoord.xy) * uSSAA;
    vec3 acc = vec3(0.0);
    for (int y = 0; y < uSSAA; y++)
        for (int x = 0; x < uSSAA; x++)
            acc += toLinear(texelFetch(uTex, base + ivec2(x, y), 0).rgb);
    acc /= float(uSSAA * uSSAA);

    vec3 col = toSRGB(acc);

    // Saturation around luminance.
    float luma = dot(col, vec3(0.2126, 0.7152, 0.0722));
    col = mix(vec3(luma), col, uSaturation);

    // Gamma grade.
    if (uGamma != 1.0) col = pow(max(col, 0.0), vec3(1.0 / uGamma));

    FragColor = vec4(clamp(col, 0.0, 1.0), 1.0);
}
