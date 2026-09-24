#version 460
// Clockwise fullscreen triangle from gl_VertexIndex, no vertex inputs; the
// in-pass resolve draw bounds coverage with the viewport and scissor.
void main() {
  gl_Position = vec4(float((gl_VertexIndex & 1) << 2) - 1.0,
                     float((gl_VertexIndex & 2) << 1) - 1.0, 0.0, 1.0);
}
