#version 420 core

layout (location=0) in vec2 texCoord;
layout (binding=0) uniform sampler2D ColorData;
layout (binding=1) uniform sampler2D DepthData;

layout (location=0) out vec4 FragColor;

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
    vec3 cloudColor;
    float boundingBox[6];
};

vec3 rayo, rayd;

void generateRay() {
    float d = 1; // distance of the image plane from the camera
    vec3 m, q; // m is the middle of the image, q is the bottom left point of the image
    vec3 camRight;
    vec3 camUp;
    vec3 pixelCenter;

    m = camPosition+camDirection*d;
    camRight = normalize(cross(camDirection, vec3(0.0, 1.0, 0.0))); // the direction of the camera's +x axis
    camUp = normalize(cross(camRight, camDirection));
    q = m+(-.5*camRight)+(-.5*camUp); // we assume image plane left boundary is at x=-0.5 and right boundary at x=0.5

    pixelCenter = q + (texCoord.x)*camRight + (texCoord.y)*camUp;
    rayo = camPosition; // ray origin
    rayd = normalize(pixelCenter-camPosition); // ray direction
}

float intersectBox(vec3 rayo, vec3 rayd) {
    float t[6] = {10001, 10001, 10001, 10001, 10001, 10001};

    if (rayd.x != 0) {
        t[0] = (boundingBox[0] - rayo.x)/rayd.x;
        t[3] = (boundingBox[3] - rayo.x)/rayd.x;
    }
    if (rayd.y != 0) {
        t[1] = (boundingBox[1] - rayo.y)/rayd.y;
        t[4] = (boundingBox[4] - rayo.y)/rayd.y;
    }
    if (rayd.z != 0) {
        t[2] = (boundingBox[2] - rayo.z)/rayd.z;
        t[5] = (boundingBox[5] - rayo.z)/rayd.z;
    }

    float min = 10001;
    for (int i = 0; i < 6; i++) {
        if (t[i] < 0) {
            t[i] = 0;
        }
        if (t[i] < min) {
            vec3 point = rayo + t[i]*rayd;
            if (point.x >= boundingBox[0] - .00001 && point.x <= boundingBox[3] + .00001 &&
                point.y >= boundingBox[1] - .00001 && point.y <= boundingBox[4] + .00001 &&
                point.z >= boundingBox[2] - .00001 && point.z <= boundingBox[5] + .00001) {
                min = t[i];
            }
        }
    }

    return min;
}

void main() {
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    vec4 color = texelFetch(ColorData, pixel, 0);
    vec4 depthVec = texelFetch(DepthData, pixel, 0);

    float depth = 1/depthVec.x;

    generateRay();

    float t = intersectBox(rayo, rayd);

    FragColor = color;

    if (t > -.00001 && t < 10000 && (depthVec.a < .00001 || t < depth)) {
        FragColor = vec4(cloudColor, 1.0);
    }
}