#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <math.h>

// the X and Z values are for drawing an icosahedron
#define IX 0.525731112119133606 
#define IZ 0.850650808352039932

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
using glm::vec2;
using glm::vec3;
using glm::vec4;
using glm::mat4;

struct vertex {
    vec3 position;
    vec3 normal;
};

struct uniform {
    GLint blockSize;
    GLubyte* blockBuffer;
    std::vector<GLint> offsets;
    GLuint ubod;
};

GLuint phongProgram, cloudProgram;
GLuint FBOd;
GLuint renderTexd;

int width = 1000, height = 1000;

int lastTime;
int nFrames;

vec3 cameraPos = vec3(-10.0f, 10.0f, -10.0f);
vec3 cameraDir;
float theta = glm::pi<float>()/4.0f, phi = glm::pi<float>()/1.5f;

vec2 mousePos = vec2(-10000, -10000);
bool shifting = false, pressLMB = false;

void readShader(const char* fname, char *source)
{
	FILE *fp;
	fp = fopen(fname,"r");
	if (fp==NULL)
	{
		fprintf(stderr,"The shader file %s cannot be opened!\n",fname);
		glfwTerminate();
		exit(1);
	}
	char tmp[300];
	while (fgets(tmp,300,fp)!=NULL)
	{
		strcat(source,tmp);
		strcat(source,"\n");
	}
}

unsigned int loadShader(const char *source, unsigned int mode)
{
	GLuint id;
	id = glCreateShader(mode);

	glShaderSource(id,1,&source,NULL);

	glCompileShader(id);

	char error[1000];

	glGetShaderInfoLog(id,1000,NULL,error);
	printf("Compile status: \n %s\n",error);

	return id;
}

void initShaders()
{
	char *vsSource, *fsSource;
	GLuint vs,fs;

	vsSource = (char *)malloc(sizeof(char)*20000);
	fsSource = (char *)malloc(sizeof(char)*20000);

    vsSource[0]='\0';
    fsSource[0]='\0';

    phongProgram = glCreateProgram();

    readShader("sceneVert.vs",vsSource);
    readShader("sceneFrag.fs",fsSource);

    vs = loadShader(vsSource,GL_VERTEX_SHADER);
    fs = loadShader(fsSource,GL_FRAGMENT_SHADER);

    glAttachShader(phongProgram, vs);
    glAttachShader(phongProgram, fs);

    glLinkProgram(phongProgram);

    vsSource = (char *)malloc(sizeof(char)*20000);
    fsSource = (char *)malloc(sizeof(char)*20000);

    vsSource[0]='\0';
    fsSource[0]='\0';

    cloudProgram = glCreateProgram();

    readShader("cloudVert.vs",vsSource);
    readShader("cloudFrag.fs",fsSource);

    vs = loadShader(vsSource,GL_VERTEX_SHADER);
    fs = loadShader(fsSource,GL_FRAGMENT_SHADER);

    glAttachShader(cloudProgram, vs);
    glAttachShader(cloudProgram, fs);

    glLinkProgram(cloudProgram);
}

static void error_callback(int error, const char* description)
{
    fprintf(stderr, "Error: %s\n", description);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);

    if (key == GLFW_KEY_LEFT_SHIFT) {
        shifting = ((action == GLFW_PRESS) || (action == GLFW_REPEAT));
    }
}

static void mouse_callback(GLFWwindow *window, int button, int action, int mods) {
    pressLMB = button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS;
}

static void cursor_callback(GLFWwindow *window, double x, double y) {
    if (mousePos.x == -10000 || mousePos.y == -10000) {
        mousePos = vec2(x, y);
    }

    if (shifting) {
        if (pressLMB) {
            if (x - mousePos.x > 0) {
                cameraPos += .1f*cameraDir;
            } else {
                cameraPos -= .1f*cameraDir;
            }
        }
    } else {
        if (pressLMB) {
            theta += (mousePos.x - x) * .005f;
            phi += (y - mousePos.y) * .005f;
        }
    }

    mousePos = vec2(x, y);
}

void showFPS(GLFWwindow* window) {
        double currentTime = glfwGetTime();
        double delta = currentTime - lastTime;
	    char ss[500] = {};
		std::string wTitle = "Icosahedron";
        nFrames++;
        if ( delta >= 1.0 ){ // If last update was more than 1 sec ago
            double fps = ((double)(nFrames)) / delta;

            std::string program = "Phong", spec = "Blinn-Phong";
            int prog;
            glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
            sprintf(ss,"%s running at %lf FPS. We are using %s shading and %s specular illumination.",
                    wTitle.c_str(),fps, program.c_str(), spec.c_str());
            glfwSetWindowTitle(window, ss);
            nFrames = 0;
            lastTime = currentTime;
        }
}

uniform initUBO(int &index, int size, std::string name, GLchar** blockComponents, GLuint program) {
    uniform current;

    // Get uniform location
    GLint blockIndex = glGetUniformBlockIndex(program, name.c_str());

    // Query the size of the buffer
    GLint blockSize;
    glGetActiveUniformBlockiv(program, blockIndex, GL_UNIFORM_BLOCK_DATA_SIZE, &blockSize);
    current.blockSize = blockSize;

    // Create the buffer
    current.blockBuffer = (GLubyte*)malloc(blockSize);

    // Query the indices of the block components
    GLuint indices[size];
    glGetUniformIndices(program, size, blockComponents, indices);

    // Get the offsets for each index
    GLint offsets[size];
    glGetActiveUniformsiv(program, size, indices, GL_UNIFORM_OFFSET, offsets);
    current.offsets.assign(offsets, offsets + size);

    // Generate descriptor
    glGenBuffers(1, &current.ubod);

    // Connect buffer to block
    glUniformBlockBinding(program, blockIndex, index);
    glBindBufferBase(GL_UNIFORM_BUFFER, index, current.ubod);

    index++;

    return current;
}

void initFBO() {
    // Generate and bind the framebuffer
    glGenFramebuffers(1, &FBOd);
    glBindFramebuffer(GL_FRAMEBUFFER, FBOd);

    // Create the texture object
    glGenTextures(1, &renderTexd);
    glBindTexture(GL_TEXTURE_2D, renderTexd);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    // Bind the texture to the FBO
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderTexd, 0);

    // Create the depth buffer
    GLuint depthBuf;
    glGenRenderbuffers(1, &depthBuf);
    glBindRenderbuffer(GL_RENDERBUFFER, depthBuf);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);

    // Bind the depth buffer to the FBO
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
    GL_RENDERBUFFER, depthBuf);

    // Set the targets for the fragment output variables
    GLenum drawBuffers[] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, drawBuffers);

    // Unbind the framebuffer, and revert to default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
    
int main(void) {
    GLFWwindow* window;

    GLuint vertpos_buffer;
    GLuint vertnormal_buffer;
    GLuint index_buffer;
    GLuint icoVAO;

    GLuint groundPosBuf;
    GLuint groundNormalBuf;
    GLuint groundIndexBuf;
    GLuint groundVAO;

    GLuint screenPosBuf;
    GLuint screenTexBuf;
    GLuint screenVAO;

	lastTime = 0;
	nFrames = 0;

    glfwSetErrorCallback(error_callback);
    if (!glfwInit())
        exit(EXIT_FAILURE);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

    window = glfwCreateWindow(width, height, "Icosahedron", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_callback);
    glfwSetCursorPosCallback(window, cursor_callback);
    glfwMakeContextCurrent(window);

    glfwSwapInterval(1);

	gladLoadGL();

	const GLubyte *renderer = glGetString( GL_RENDERER );
	const GLubyte *vendor = glGetString( GL_VENDOR );
	const GLubyte *version = glGetString( GL_VERSION );
	const GLubyte *glslVersion = glGetString( GL_SHADING_LANGUAGE_VERSION );

	GLint major, minor;
	glGetIntegerv(GL_MAJOR_VERSION, &major);
	glGetIntegerv(GL_MINOR_VERSION, &minor);

	printf("GL Vendor            : %s\n", vendor);
	printf("GL Renderer          : %s\n", renderer);
	printf("GL Version (string)  : %s\n", version);
	printf("GL Version (integer) : %d.%d\n", major, minor);
	printf("GLSL Version         : %s\n", glslVersion);

    // Set bounding box
    static float box[8][3] = {
            {5, 0, 5}, {-5, 0, 5}, {5, 0, -5}, {-5, 0, -5},
            {5, 10, 5}, {-5, 10, 5}, {5, 10, -5}, {-5, 10, -5},
    };

	// These are the 12 vertices for the icosahedron
	static GLfloat vpos[12][3] = {
   	{-IX, 0.0, IZ}, {IX, 0.0, IZ}, {-IX, 0.0, -IZ}, {IX, 0.0, -IZ},    
   	{0.0, IZ, IX}, {0.0, IZ, -IX}, {0.0, -IZ, IX}, {0.0, -IZ, -IX},    
   	{IZ, IX, 0.0}, {-IZ, IX, 0.0}, {IZ, -IX, 0.0}, {-IZ, -IX, 0.0} 
	};

    vertex vdata[12];

    // Fill in vertex data
    // Since this is an icosahedron we can take the normals as the vector from the center to the vertex
    for (int i = 0; i < 12; i++) {
        vdata[i].position = vec3(vpos[i][0], vpos[i][1], vpos[i][2]);
        vdata[i].normal = glm::normalize(vdata[i].position - vec3(0, 0, 0));
    }

	// These are the 20 faces.  Each of the three entries for each 
	// vertex gives the 3 vertices that make the face.
	static GLint tindices[20][3] = { 
   	{0,4,1}, {0,9,4}, {9,5,4}, {4,5,8}, {4,8,1},    
   	{8,10,1}, {8,3,10}, {5,3,8}, {5,2,3}, {2,7,3},    
   	{7,10,3}, {7,6,10}, {7,11,6}, {11,0,6}, {0,1,6}, 
   	{6,1,10}, {9,0,11}, {9,11,2}, {9,2,5}, {7,2,11} };

    glGenBuffers(1, &vertpos_buffer);
    glGenBuffers(1, &vertnormal_buffer);
    glGenBuffers(1, &index_buffer);

	glGenVertexArrays(1, &icoVAO);
	glBindVertexArray(icoVAO);
	
    glBindBuffer(GL_ARRAY_BUFFER, vertpos_buffer);
    glBufferData(GL_ARRAY_BUFFER, 12*sizeof(vertex), vdata, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, vertnormal_buffer);
    glBufferData(GL_ARRAY_BUFFER, 12*sizeof(vertex), vdata, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 60*sizeof(int), (void *)&(tindices[0][0]), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, vertpos_buffer);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)0);

    glBindBuffer(GL_ARRAY_BUFFER, vertnormal_buffer);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)(sizeof(float) * 3));

	glBindVertexArray(0);

    vertex groundVert[4];

    groundVert[0].position = vec3(10,0,10);
    groundVert[1].position = vec3(-10,0,10);
    groundVert[2].position = vec3(10,0,-10);
    groundVert[3].position = vec3(-10,0,-10);

    groundVert[0].normal = vec3(0, 1, 0);
    groundVert[1].normal = vec3(0, 1, 0);
    groundVert[2].normal = vec3(0, 1, 0);
    groundVert[3].normal = vec3(0, 1, 0);

    vec3 groundColor = vec3(0/255.0f, 64/255.0f, 0/255.0f);

    static GLuint groundIndex[4] = {0, 1, 2, 3};

    glGenBuffers(1, &groundPosBuf);
    glGenBuffers(1, &groundNormalBuf);
    glGenBuffers(1, &groundIndexBuf);

    glGenVertexArrays(1, &groundVAO);
    glBindVertexArray(groundVAO);

    glBindBuffer(GL_ARRAY_BUFFER, groundPosBuf);
    glBufferData(GL_ARRAY_BUFFER, sizeof(groundVert), groundVert, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, groundNormalBuf);
    glBufferData(GL_ARRAY_BUFFER, sizeof(groundVert), groundVert, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, groundIndexBuf);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(groundIndex), groundIndex, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, groundPosBuf);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)0);

    glBindBuffer(GL_ARRAY_BUFFER, groundNormalBuf);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)(3*sizeof(float)));

    glBindVertexArray(0);

    // Quad for multipass rendering
    GLfloat screenVert[] = {
            -1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f,
            -1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f, -1.0f, 1.0f, 0.0f
    };
    GLfloat screenTexCoords[] = {
            0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f,
            0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f
    };

    glGenBuffers(1, &screenPosBuf);
    glGenBuffers(1, &screenTexBuf);

    glGenVertexArrays(1, &screenVAO);
    glBindVertexArray(screenVAO);

    glBindBuffer(GL_ARRAY_BUFFER, screenPosBuf);
    glBufferData(GL_ARRAY_BUFFER, sizeof(screenVert), screenVert, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, screenTexBuf);
    glBufferData(GL_ARRAY_BUFFER, sizeof(screenTexCoords), screenTexCoords, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, screenPosBuf);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

    glBindBuffer(GL_ARRAY_BUFFER, screenTexBuf);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);

    glBindVertexArray(0);

    initFBO();

    initShaders();

    // Initialize UBOs
    uniform matrixUBO, lightUBO, materialUBO, camUBO, cloudUBO;
    int index = 0;

    GLchar* matrixComponents[4] = {"modelMat", "viewMat", "projectionMat", "normalMat"};
    GLchar* materialComponents[4] = {"reflectionAmbient", "reflectionDiffuse", "reflectionSpecular", "shine"};
    GLchar* lightComponents[4] = {"lightPosition", "ambient", "diffuse", "specular"};
    GLchar* camComponents[4] = {"camPosition", "camDirection"};
    GLchar* cloudComponents[4] = {"coverage", "stepCount", "color", "boundingBox"};

    matrixUBO = initUBO(index, 4, "matrixData", matrixComponents, phongProgram);
    lightUBO = initUBO(index, 4, "lightData", lightComponents, phongProgram);
    materialUBO = initUBO(index, 4, "materialData", materialComponents, phongProgram);
    camUBO = initUBO(index, 2, "camData", camComponents, cloudProgram);
    cloudUBO = initUBO(index, 4, "cloudData", cloudComponents, cloudProgram);

    // Copy static data into GPU
    vec4 lightPos = vec4(10.0f, 10.0f, 10.0f, 1.0f);
    vec3 lightAmb = vec3(1.0f, 1.0f, 1.0f), lightDif = vec3(1.0f, 1.0f, 1.0f), lightSpec = vec3(1.0f, 1.0f, 1.0f);

    memcpy(lightUBO.blockBuffer + lightUBO.offsets[0], &lightPos, sizeof(vec4));
    memcpy(lightUBO.blockBuffer + lightUBO.offsets[1], &lightAmb, sizeof(vec3));
    memcpy(lightUBO.blockBuffer + lightUBO.offsets[2], &lightDif, sizeof(vec3));
    memcpy(lightUBO.blockBuffer + lightUBO.offsets[3], &lightSpec, sizeof(vec3));

    vec3 isoColor = vec3(253/255.0f, 238/255.0f, 75/255.0f), specColor = vec3(.8f, .8f, .8f);
    float shine = 1.0f;

    memcpy(materialUBO.blockBuffer + materialUBO.offsets[0], &isoColor, sizeof(vec3));
    memcpy(materialUBO.blockBuffer + materialUBO.offsets[1], &isoColor, sizeof(vec3));
    memcpy(materialUBO.blockBuffer + materialUBO.offsets[2], &specColor, sizeof(vec3));
    memcpy(materialUBO.blockBuffer + materialUBO.offsets[3], &shine, sizeof(float));

    float coverage = .5f;
    int stepCount = 5;
    vec3 cloudColor = vec3(1.0f, 1.0f, 1.0f);

    memcpy(cloudUBO.blockBuffer + cloudUBO.offsets[0], &coverage, sizeof(float));
    memcpy(cloudUBO.blockBuffer + cloudUBO.offsets[1], &stepCount, sizeof(int));
    memcpy(cloudUBO.blockBuffer + cloudUBO.offsets[2], &cloudColor, sizeof(vec3));
    memcpy(cloudUBO.blockBuffer + cloudUBO.offsets[3], &box, sizeof(box));

    glBindBuffer(GL_UNIFORM_BUFFER, lightUBO.ubod);
    glBufferData(GL_UNIFORM_BUFFER, lightUBO.blockSize, lightUBO.blockBuffer, GL_STATIC_DRAW);

    glBindBuffer(GL_UNIFORM_BUFFER, materialUBO.ubod);
    glBufferData(GL_UNIFORM_BUFFER, materialUBO.blockSize, materialUBO.blockBuffer, GL_STATIC_DRAW);

    glBindBuffer(GL_UNIFORM_BUFFER, cloudUBO.ubod);
    glBufferData(GL_UNIFORM_BUFFER, cloudUBO.blockSize, cloudUBO.blockBuffer, GL_STATIC_DRAW);

    mat4 model = mat4(1.0f), icoModel = glm::translate(mat4(1.0f), vec3(lightPos)), view, projection, normal, icoNormal;
    mat4 identity = mat4(1.0f);

	glClearColor(0/255.0f, 191/255.0f, 254/255.0f, 0.0f);
	
    while (!glfwWindowShouldClose(window))
    {
        glViewport(0, 0, width, height);
        glBindFramebuffer(GL_FRAMEBUFFER, FBOd);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        glUseProgram(phongProgram);

        cameraDir = vec3(sin(theta) * sin(phi), cos(phi), sin(phi) * cos(theta));
        cameraDir = glm::normalize(cameraDir);

        view = glm::lookAt(cameraPos, cameraPos + cameraDir, vec3(0.0f,1.0f,0.0f));

    	projection = glm::perspective(glm::radians(45.0f), (float)width/height, 0.3f, 100.0f);

        normal = glm::mat3(glm::transpose(glm::inverse(view * model)));
        icoNormal = glm::mat3(glm::transpose(glm::inverse(view * model)));

        // Copy matrix data to the buffer
        memcpy(matrixUBO.blockBuffer + matrixUBO.offsets[0], &icoModel, sizeof(mat4));
        memcpy(matrixUBO.blockBuffer + matrixUBO.offsets[1], &view, sizeof(mat4));
        memcpy(matrixUBO.blockBuffer + matrixUBO.offsets[2], &projection, sizeof(mat4));
        memcpy(matrixUBO.blockBuffer + matrixUBO.offsets[3], &icoNormal, sizeof(mat4));
        glBindBuffer(GL_UNIFORM_BUFFER, matrixUBO.ubod);
        glBufferData(GL_UNIFORM_BUFFER, matrixUBO.blockSize, matrixUBO.blockBuffer, GL_DYNAMIC_DRAW);

        // Copy material data for icosahedron
        memcpy(materialUBO.blockBuffer + materialUBO.offsets[0], &isoColor, sizeof(vec3));
        memcpy(materialUBO.blockBuffer + materialUBO.offsets[1], &isoColor, sizeof(vec3));
        glBindBuffer(GL_UNIFORM_BUFFER, materialUBO.ubod);
        glBufferData(GL_UNIFORM_BUFFER, materialUBO.blockSize, materialUBO.blockBuffer, GL_DYNAMIC_DRAW);

        // Draw icosahedron
	    glBindVertexArray(icoVAO);
		glDrawElements(GL_TRIANGLES, 60, GL_UNSIGNED_INT,0);
    	glBindVertexArray(0);

        // Copy matrix data for ground plane
        memcpy(matrixUBO.blockBuffer + matrixUBO.offsets[0], &model, sizeof(mat4));
        memcpy(matrixUBO.blockBuffer + matrixUBO.offsets[3], &normal, sizeof(mat4));
        glBindBuffer(GL_UNIFORM_BUFFER, matrixUBO.ubod);
        glBufferData(GL_UNIFORM_BUFFER, matrixUBO.blockSize, matrixUBO.blockBuffer, GL_DYNAMIC_DRAW);

        // Copy material data for ground plane
        memcpy(materialUBO.blockBuffer + materialUBO.offsets[0], &groundColor, sizeof(vec3));
        memcpy(materialUBO.blockBuffer + materialUBO.offsets[1], &groundColor, sizeof(vec3));
        glBindBuffer(GL_UNIFORM_BUFFER, materialUBO.ubod);
        glBufferData(GL_UNIFORM_BUFFER, materialUBO.blockSize, materialUBO.blockBuffer, GL_DYNAMIC_DRAW);

        // Draw ground plane
        glBindVertexArray(groundVAO);
        glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        glUseProgram(cloudProgram);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, renderTexd);

        glDisable(GL_DEPTH_TEST);
        glClear(GL_COLOR_BUFFER_BIT);

        // Set Matrix Data
        memcpy(matrixUBO.blockBuffer + matrixUBO.offsets[0], &identity, sizeof(mat4));
        memcpy(matrixUBO.blockBuffer + matrixUBO.offsets[1], &identity, sizeof(mat4));
        memcpy(matrixUBO.blockBuffer + matrixUBO.offsets[2], &identity, sizeof(mat4));
        memcpy(matrixUBO.blockBuffer + matrixUBO.offsets[3], &identity, sizeof(mat4));
        glBindBuffer(GL_UNIFORM_BUFFER, matrixUBO.ubod);
        glBufferData(GL_UNIFORM_BUFFER, matrixUBO.blockSize, matrixUBO.blockBuffer, GL_DYNAMIC_DRAW);

        // Render the full-screen quad
        glBindVertexArray(screenVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

		showFPS(window);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
	
    glfwDestroyWindow(window);
    glfwTerminate();
    exit(EXIT_SUCCESS);
}