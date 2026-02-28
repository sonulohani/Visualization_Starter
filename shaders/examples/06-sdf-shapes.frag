// Example 06: SDF Shapes (Circle + Box with Smooth Union)
// Shows: signed distance functions, combining shapes

#ifdef GL_ES
precision mediump float;
#endif

uniform vec2  u_resolution;
uniform float u_time;

float sdCircle(vec2 p, float r) {
    return length(p) - r;
}

float sdBox(vec2 p, vec2 size) {
    vec2 d = abs(p) - size;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}

float smoothUnion(float a, float b, float k) {
    float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
    return mix(b, a, h) - k * h * (1.0 - h);
}

void main() {
    vec2 uv = gl_FragCoord.xy / u_resolution.xy - 0.5;
    uv.x *= u_resolution.x / u_resolution.y;

    // Animate the circle position
    vec2 circlePos = vec2(0.2 * sin(u_time), 0.2 * cos(u_time));

    float circle = sdCircle(uv - circlePos, 0.15);
    float box    = sdBox(uv, vec2(0.15, 0.15));

    // Smooth blend between shapes
    float d = smoothUnion(circle, box, 0.1);

    // Color: orange inside, dark outside
    vec3 inside  = vec3(1.0, 0.5, 0.1);
    vec3 outside = vec3(0.05);
    float mask   = smoothstep(0.01, 0.0, d);
    vec3 color   = mix(outside, inside, mask);

    // Add outline
    float outline = smoothstep(0.02, 0.01, abs(d));
    color = mix(color, vec3(1.0), outline * 0.5);

    gl_FragColor = vec4(color, 1.0);
}
