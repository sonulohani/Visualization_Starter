// Example 05: Rainbow Color Cycling
// Shows: cos() trick for palette generation, vec3 offsets

#ifdef GL_ES
precision mediump float;
#endif

uniform vec2  u_resolution;
uniform float u_time;

void main() {
    vec2 uv = gl_FragCoord.xy / u_resolution.xy;

    // The classic one-liner palette:
    // Remap cos (-1→1) to color space (0→1)
    // Phase offsets (0, 2, 4) shift R/G/B channels apart
    vec3 color = 0.5 + 0.5 * cos(u_time + uv.xyx * 6.0 + vec3(0.0, 2.0, 4.0));

    gl_FragColor = vec4(color, 1.0);
}
