// Example 02: Smooth Circle
// Shows: centering UVs, aspect ratio correction, length(), smoothstep()

#ifdef GL_ES
precision mediump float;
#endif

uniform vec2  u_resolution;
uniform float u_time;

void main() {
    // Center coordinates: -0.5 → 0.5
    vec2 uv = gl_FragCoord.xy / u_resolution.xy - 0.5;

    // Fix aspect ratio so circle isn't squished
    uv.x *= u_resolution.x / u_resolution.y;

    // Distance from center
    float d = length(uv);

    // Pulsing radius
    float radius = 0.3 + 0.05 * sin(u_time * 3.0);

    // Smooth circle edge
    float circle = smoothstep(radius, radius - 0.01, d);

    // Blue circle on dark background
    vec3 bg    = vec3(0.05, 0.05, 0.1);
    vec3 fg    = vec3(0.2, 0.5, 1.0);
    vec3 color = mix(bg, fg, circle);

    gl_FragColor = vec4(color, 1.0);
}
