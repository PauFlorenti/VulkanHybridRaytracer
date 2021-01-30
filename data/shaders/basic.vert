#version 460

#extension GL_GOOGLE_include_directive : enable

struct Material{
	vec4	  diffuse;
	vec4	  specular; // w is the Glossines factor
	float	  ior;	    // index of refraction
	float	  glossiness;
	int		  illum;
};

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inUV;

layout(location = 0) out vec3 outPosition;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 outColor;
layout(location = 3) out vec2 outUV;
layout(location = 4) out float outMat;
layout(location = 5) out mat4 outMatrix;

struct ObjectData{
	mat4 model;
};

// Set 0 - Camera information
layout(set = 0, binding = 0) uniform CameraBuffer
{
	mat4 view;
	mat4 projection;
} cameraData;

layout(push_constant) uniform constants
{
	mat4 matrix;
}pushC;

void main()
{
	mat4 transformationMatrix 	= cameraData.projection * cameraData.view * pushC.matrix;
	gl_Position 				=  transformationMatrix * vec4(inPosition, 1.0);

	outPosition = vec3(pushC.matrix * vec4(inPosition, 1.0)).xyz;
    outColor  	= inColor;
	outNormal 	= vec3(vec4(inNormal, 1.0) * pushC.matrix).xyz;
    outUV 		= inUV;
	outMat 		= 0; //float(pushC.matIdx) / 10;
	outMatrix = pushC.matrix;
}