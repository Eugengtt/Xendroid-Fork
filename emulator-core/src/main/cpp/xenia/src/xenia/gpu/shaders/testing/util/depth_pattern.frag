#version 450

layout(location = 0) in float inDepth;

void main() {
    gl_FragDepth = inDepth;
}
