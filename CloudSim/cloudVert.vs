#version 420 core

layout (location=0) in vec3 VertexPosition;

uniform matrixData {
    mat4 modelMat;
    mat4 viewMat;
    mat4 projectionMat;
    mat3 normalMat;
};

void main() {
    gl_Position = projectionMat * viewMat * modelMat * vec4(VertexPosition, 1.0);
}