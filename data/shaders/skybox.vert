#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inUV;

layout(location = 0) out vec2 outUV;

// Set 0 - Camera information
layout(set = 0, binding = 0) uniform CameraBuffer
{
	mat4 view;
    mat4 projection;
} cameraData;

layout(set = 0, binding = 2) uniform MatrixBuffer { mat4 matrix; } matrixBuf;

void main()
{
    float angle = radians(-90);
    mat4 rotMatrix = mat4(  vec4(cos(angle), 0, sin(angle), 0),
                            vec4(0, 1, 0, 0),
                            vec4(-sin(angle), 0, cos(angle), 0),
                            vec4(0, 0, 0, 1));

    mat4 model = matrixBuf.matrix * rotMatrix;
    gl_Position = cameraData.projection * cameraData.view * model * vec4(inPosition, 1);
    outUV = inUV;
}