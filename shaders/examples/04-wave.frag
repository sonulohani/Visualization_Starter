// Example 04: Animated Wave
// Shows: sin(), animation with u_time, drawing lines

#ifdef GL_ES
precision mediump float;
#endif

uniform vec2  u_resolution;
uniform float u_time;

void main() {
    vec2 uv = gl_FragCoord.xy / u_resolution.xy;

    // Wave function: y offset based on x and time
    float wave = 0.5 + 0.2 * sin(uv.x * 10.0 + u_time * 3.0);

    // Draw a soft line at the wave position
    float thickness = 0.015;
    float line = smoothstep(thickness, 0.0, abs(uv.y - wave));

    // Green line on dark background
    vec3 bg    = vec3(0.02, 0.02, 0.05);
    vec3 fg    = vec3(0.1, 1.0, 0.4);
    vec3 color = bg + fg * line;

    gl_FragColor = vec4(color, 1.0);
}
