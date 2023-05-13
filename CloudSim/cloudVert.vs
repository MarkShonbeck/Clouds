#version 420 core

layout (location=0) in vec3 VertexPosition;
layout (location=1) in vec2 inTexCoord;

layout (location=0) out vec2 texCoord;

void main() {
    texCoord = inTexCoord;

    gl_Position = vec4(VertexPosition, 1.0);
}