#version 410 core

layout (location=0) in vec3 VertexPosition;
layout (location=1) in vec3 VertexNormal;

layout (location=0) out vec3 Position;
layout (location=1) out vec3 Normal;

uniform matrixData {
    mat4 modelMat;
    mat4 viewMat;
    mat4 projectionMat;
    mat3 normalMat;
};

void main() {
    // Convert to eye space
    Normal = normalize(normalMat * VertexNormal);
    Position = (viewMat * modelMat * vec4(VertexPosition, 1.0)).xyz;

    gl_Position = projectionMat * viewMat * modelMat * vec4(VertexPosition, 1.0);
}