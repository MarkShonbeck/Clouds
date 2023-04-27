#version 410 core

layout (location=0) in vec3 Position;
layout (location=1) in vec3 Normal;

layout (location=0) out vec4 FragColor;
layout (location=1) out vec4 DepthData;

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

uniform materialData {
    vec3 reflectionAmbient;
    vec3 reflectionDiffuse;
    vec3 reflectionSpecular;
    float shine;
};

vec3 blinnPhong () {
    // Calculate ambient
    vec3 intensityAmbient = ambient * reflectionAmbient;

    // Get the vector to the light source
    vec3 source = normalize((viewMat * lightPosition).xyz - Position);

    // Strength of the lights reflection
    float sourceDotNormal = max(dot(source, normalize(Normal)), 0.0);

    // Calculate diffuse
    vec3 intensityDiffuse = diffuse * reflectionDiffuse * sourceDotNormal;

    // Calculate specular
    vec3 intensitySpec = vec3(0.0);
    if (sourceDotNormal > 0.0) {
        vec3 camera = normalize(-Position.xyz);
        vec3 halfway = normalize(camera + source);
        intensitySpec = specular * reflectionSpecular * pow(max(dot(halfway, normalize(Normal)), 0.0), 7*shine);
    }

    return intensityAmbient + intensityDiffuse + intensitySpec;
}

void main() {
    float Depth = -1.0*Position.z;
    DepthData = vec4(vec3(1/Depth), 1.0);

    FragColor = vec4(blinnPhong(), 1.0);
}
