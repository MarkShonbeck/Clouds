#version 420 core

layout (location=0) in vec3 VertexPosition;
layout (location=1) in vec2 inTexCoord;

layout (location=0) out vec2 texCoord;

uniform matrixData {
    mat4 modelMat;
    mat4 viewMat;
    mat4 projectionMat;
    mat3 normalMat;
};

void main() {
    texCoord = inTexCoord;

    gl_Position = projectionMat * viewMat * modelMat * vec4(VertexPosition, 1.0);
}