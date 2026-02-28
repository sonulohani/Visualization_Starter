// Example 07: Simple Glow Effect
// Shows: inverse distance glow, multiple light sources

#ifdef GL_ES
precision mediump float;
#endif

uniform vec2  u_resolution;
uniform float u_time;

void main() {
    vec2 uv = gl_FragCoord.xy / u_resolution.xy - 0.5;
    uv.x *= u_resolution.x / u_resolution.y;

    vec3 color = vec3(0.0);

    // Three orbiting glow points
    for (int i = 0; i < 3; i++) {
        float angle = u_time * 1.5 + float(i) * 2.094; // 2π/3 = evenly spaced
        vec2 pos = 0.2 * vec2(cos(angle), sin(angle));

        // Glow = inverse of distance
        float d = length(uv - pos);
        float glow = 0.01 / d;

        // Each light gets a different color
        vec3 lightColor;
        if (i == 0) lightColor = vec3(1.0, 0.3, 0.2);
        else if (i == 1) lightColor = vec3(0.2, 1.0, 0.3);
        else lightColor = vec3(0.3, 0.2, 1.0);

        color += lightColor * glow;
    }

    // Clamp to prevent over-bright
    color = clamp(color, 0.0, 1.0);

    gl_FragColor = vec4(color, 1.0);
}
