#version 330 core
// Composite the blurred bloom back over the base image with a screen blend,
// which adds a soft luminous glow to bright areas without hard clipping.

out vec4 FragColor;

uniform sampler2D uBase;
uniform sampler2D uBloom;
uniform vec2  uTexSize;
uniform float uStrength;

void main() {
    vec2 uv    = gl_FragCoord.xy / uTexSize;
    vec3 base  = texture(uBase, uv).rgb;
    vec3 bloom = texture(uBloom, uv).rgb;
    vec3 c = 1.0 - (1.0 - base) * (1.0 - uStrength * bloom); // screen
    FragColor = vec4(clamp(c, 0.0, 1.0), 1.0);
}
