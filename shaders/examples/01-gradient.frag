// Example 01: Simple Gradient
// Copy-paste into Shadertoy or any GLSL sandbox.
// Shows: normalized coordinates, vec3 color output

#ifdef GL_ES
precision mediump float;
#endif

uniform vec2 u_resolution;

void main() {
    // Normalize pixel coords to 0.0 → 1.0
    vec2 uv = gl_FragCoord.xy / u_resolution.xy;

    // Horizontal red gradient + vertical green gradient
    vec3 color = vec3(uv.x, uv.y, 0.5);

    gl_FragColor = vec4(color, 1.0);
}
