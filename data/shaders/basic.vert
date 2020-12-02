
#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inUV;

layout(location = 0) out vec3 outPosition;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 outColor;
layout(location = 3) out vec2 outUV;

struct ObjectData{
	mat4 model;
};

// Set 0 - Camera information
layout(set = 0, binding = 0) uniform CameraBuffer
{
	mat4 viewprojection;
} cameraData;

// Set 1 - Object info, matrices at the moment
layout(std140, set = 1, binding = 0) readonly buffer ObjectBuffer{
	ObjectData objects[];
}objectBuffer;

void main()
{
	mat4 modelMatrix = objectBuffer.objects[gl_BaseInstance].model;
	mat4 transformationMatrix = cameraData.viewprojection * modelMatrix;
	gl_Position =  transformationMatrix * vec4(inPosition, 1.0);

	outPosition = vec3(modelMatrix * vec4(inPosition, 1.0)).xyz;
    outColor  	= inColor;
	outNormal 	= inNormal;
    outUV 		= inUV;
}