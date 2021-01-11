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
    //gl_Position = cameraData.projection * cameraData.view * mat4(1) * vec4(inPosition, 1);
    gl_Position = cameraData.projection * cameraData.view * matrixBuf.matrix * vec4(inPosition, 1);
    outUV = inUV;
}