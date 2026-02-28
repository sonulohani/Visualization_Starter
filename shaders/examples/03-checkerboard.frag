// Example 03: Checkerboard Pattern
// Shows: floor(), mod(), fract(), tiling

#ifdef GL_ES
precision mediump float;
#endif

uniform vec2 u_resolution;

void main() {
    vec2 uv = gl_FragCoord.xy / u_resolution.xy;

    // Number of tiles
    float tiles = 8.0;

    // Which cell are we in?
    vec2 cell = floor(uv * tiles);

    // Alternate black/white
    float checker = mod(cell.x + cell.y, 2.0);

    vec3 color = vec3(checker);

    gl_FragColor = vec4(color, 1.0);
}
