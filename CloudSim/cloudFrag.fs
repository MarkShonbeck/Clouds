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
    float cloudScale;
    vec3 cloudOffset;
};

subroutine void RenderLevelType();
subroutine uniform RenderLevelType RenderLevel;

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

float getDepth()  {
    float near = .3, far = 100;
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    float depth = texelFetch(DepthData, pixel, 0).x;
    float ndc = depth * 2.0 - 1.0;
    float linearDepth = (2.0 * near * far) / (far + near - ndc * (far - near));
    return linearDepth;
}

vec2 intersectBox(vec3 rayo, vec3 rayd) {
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
    float max = 0;
    for (int i = 0; i < 6; i++) {
        if (t[i] < 0) {
            t[i] = 0;
        }
        vec3 point = rayo + t[i]*rayd;
        if (point.x >= boundingBox[0] - .00001 && point.x <= boundingBox[3] + .00001 &&
            point.y >= boundingBox[1] - .00001 && point.y <= boundingBox[4] + .00001 &&
            point.z >= boundingBox[2] - .00001 && point.z <= boundingBox[5] + .00001) {
            if (t[i] < min) {
                min = t[i];
            }
            if (t[i] > max) {
                max = t[i];
            }
        }
    }

    return vec2(min, max);
}

float rand1(float n) {
 	return fract(cos(n*89.42)*343.42);
}
vec3 rand3(vec3 n) {
 	return vec3(rand1(n.x*23.62 + n.y*34.35 + n.z*29.39 - 300.0),
 	            rand1(n.x*45.13 + n.y*38.89 + n.z*41.05 + 256.0),
 	            rand1(n.x*25.97 + n.y*35.48 + n.z*38.20 - 134.6));
}
float worley_noise(vec3 n, float s) {
    float dis = 64.0;
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            for (int z = -1; z <= 1; z++) {
                vec3 p = floor(n/s) + vec3(x, y, z);
                float d = length(rand3(p) + vec3(x, y, z) - fract(n/s));
                if (dis > d) {
                    dis = d;
                }
            }
        }
    }
    return 1.0 - dis;
}

vec3 hash33(vec3 p3) {
	p3 = fract(p3 * vec3(0.1031, 0.11369, 0.13787));
    p3 += dot(p3, p3.yxz + 19.19);
    return -1.0 + 2.0 * fract(vec3((p3.x + p3.y)*p3.z, (p3.x + p3.z)*p3.y, (p3.y + p3.z)*p3.x));
}

float perlin_noise(vec3 p, float scale) {
    p *= scale;

    vec3 pi = floor(p);
    vec3 pf = p - pi;

    vec3 w = pf * pf * (3.0 - 2.0 * pf);

    return 	mix(
        		mix(
                	mix(dot(pf - vec3(0, 0, 0), hash33(pi + vec3(0, 0, 0))),
                        dot(pf - vec3(1, 0, 0), hash33(pi + vec3(1, 0, 0))),
                       	w.x),
                	mix(dot(pf - vec3(0, 0, 1), hash33(pi + vec3(0, 0, 1))),
                        dot(pf - vec3(1, 0, 1), hash33(pi + vec3(1, 0, 1))),
                       	w.x),
                	w.z),
        		mix(
                    mix(dot(pf - vec3(0, 1, 0), hash33(pi + vec3(0, 1, 0))),
                        dot(pf - vec3(1, 1, 0), hash33(pi + vec3(1, 1, 0))),
                       	w.x),
                   	mix(dot(pf - vec3(0, 1, 1), hash33(pi + vec3(0, 1, 1))),
                        dot(pf - vec3(1, 1, 1), hash33(pi + vec3(1, 1, 1))),
                       	w.x),
                	w.z),
    			w.y);
}

float sampleDensity(vec3 position) {
    position = (position + cloudOffset)/cloudScale;
    float density = 0;

    // 2 octaves of worley
    for (int i = 0; i < 2; i++) {
        density += worley_noise(position, pow(2, -i)) * pow(2, -i);
    }

    // 5 octaves of perlin
    for (int i = 0; i < 5; i++) {
            density += perlin_noise(position, pow(2, i)) * pow(2, -i);
    }

    density -= coverage;

    return max(density, 0);
}

float simpleRayMarch(vec3 startPoint, vec3 endPoint) {
    float stepDistance = (length(endPoint - startPoint))/(stepCount-1);
    vec3 stepDirection = normalize(endPoint - startPoint);
    float totalDensity = 0;

    for (int i = 0; i < stepCount; i++) {
        vec3 point = startPoint + i*stepDistance*stepDirection;

        totalDensity += sampleDensity(point) * stepDistance;
    }

    return exp(-totalDensity);
}

subroutine (RenderLevelType)
void simpleScene() {
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    vec4 color = texelFetch(ColorData, pixel, 0);

    FragColor = color;
}

subroutine (RenderLevelType)
void showBoundingBox() {
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    vec4 color = texelFetch(ColorData, pixel, 0);
    float depth = getDepth();

    generateRay();

    float t = intersectBox(rayo, rayd).x;

    FragColor = color;

    if (t > -.00001 && t < 10000 && t < depth) {
        FragColor = vec4(cloudColor, 1.0);
    }
}

subroutine (RenderLevelType)
void showNoise() {
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    vec4 color = texelFetch(ColorData, pixel, 0);
    float depth = getDepth();

    generateRay();

    vec2 t = intersectBox(rayo, rayd);

    FragColor = color;

    if (t.x < depth) {
        if (t.x > -.00001 && t.x < 10000) {
            t.y = min(t.y, depth);

            vec3 startPoint = rayo + t.x*rayd;
            vec3 endPoint = rayo + t.y*rayd;

            float light = simpleRayMarch(startPoint, endPoint);
            if (light < coverage) {
                FragColor = vec4(cloudColor, 1.0);
            } else {
                FragColor = color;
            }
        }
    }
}

subroutine (RenderLevelType)
void simpleRayMarchNoise() {
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    vec4 color = texelFetch(ColorData, pixel, 0);
    float depth = getDepth();

    generateRay();

    vec2 t = intersectBox(rayo, rayd);

    FragColor = color;

    if (t.x < depth) {
        if (t.x > -.00001 && t.x < 10000) {
            t.y = min(t.y, depth);

            vec3 startPoint = rayo + t.x*rayd;
            vec3 endPoint = rayo + t.y*rayd;

            float light = simpleRayMarch(startPoint, endPoint);
            FragColor = mix(vec4(cloudColor, 1.0), color, light);
        }
    }
}

void main() {
    RenderLevel();
}