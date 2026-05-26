#version 330 core
// Separable Gaussian blur with an optional bright-pass extract, used for bloom.
// Run twice (horizontal then vertical); the first pass extracts bright areas.

out vec4 FragColor;

uniform sampler2D uTex;
uniform vec2  uTexSize;    // size of uTex in pixels
uniform vec2  uDir;        // blur direction in pixels: (1,0) then (0,1)
uniform int   uExtract;    // 1 = subtract threshold first (bright pass)
uniform float uThreshold;  // bloom threshold

vec3 sample(vec2 px) {
    vec3 c = texture(uTex, px / uTexSize).rgb;
    if (uExtract == 1) {
        float lum = dot(c, vec3(0.299, 0.587, 0.114));
        c *= max(lum - uThreshold, 0.0) / max(lum, 1e-4); // keep hue, drop dim
    }
    return c;
}

void main() {
    // 9-tap Gaussian (sigma ~ 2.5), weights normalized.
    float w[5] = float[](0.20236, 0.17891, 0.12384, 0.06738, 0.02853);
    vec2  base = gl_FragCoord.xy;
    vec3  sum  = sample(base) * w[0];
    for (int i = 1; i < 5; i++) {
        vec3 ofs = vec3(0.0);
        sum += sample(base + uDir * float(i)) * w[i];
        sum += sample(base - uDir * float(i)) * w[i];
    }
    FragColor = vec4(sum, 1.0);
}
