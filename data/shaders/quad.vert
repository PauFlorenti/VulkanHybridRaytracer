#version 450

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec3 inColor;
layout (location = 3) in vec2 inUV;

layout (location = 0) out vec2 outUV;
layout (location = 1) out vec3 outCameraPosition;

struct Camera{
	mat4 view;
	mat4 projection;
	mat4 pView;
	mat4 pProj;
};

layout (set = 0, binding = 7) uniform PositionBuffer {vec3 position;} camera;

layout(push_constant) uniform constants
{
	vec4 data;
	mat4 matrix;
}pushC;

void main()
{
	outUV = inUV;
	outCameraPosition = camera.position;
	gl_Position = vec4( inPosition, 1.0 );
}