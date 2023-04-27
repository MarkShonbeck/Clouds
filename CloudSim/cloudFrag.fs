#version 420 core

// layout(location=0) in vec3 Position;
// layout(location=1) in vec3 Normal;
layout(binding=0) uniform sampler2D Texture0;

layout(location=0) out vec4 FragColor;

uniform matrixData {
    mat4 modelMat;
    mat4 viewMat;
    mat4 projectionMat;
    mat3 normalMat;
};

uniform lightData {
    vec4 lightPosition;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

uniform camData {
    vec3 camPosition;
    vec3 camDirection;
};

uniform cloudData {
    float coverage;
    int stepCount;
    vec3 color;
    float boundingBox[24];
};

void main() {
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    vec4 color = texelFetch(Texture0, pixel, 0);

    FragColor = color;
}