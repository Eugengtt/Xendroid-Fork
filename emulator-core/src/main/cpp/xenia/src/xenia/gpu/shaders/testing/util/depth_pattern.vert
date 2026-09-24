#version 450

layout(location = 0) out float outDepth;

void main() {
    // Fullscreen triangle
    vec2 positions[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2(3.0, -1.0),
        vec2(-1.0, 3.0)
    );

    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);

    // Varying depth based on position
    // Maps from [-1,3] range to [0,0.5] for X coordinate
    outDepth = (positions[gl_VertexIndex].x + 1.0) * 0.125;
}
