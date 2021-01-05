#version 460
#extension GL_EXT_nonuniform_qualifier : enable

layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec3 inColor;
layout (location = 3) in vec2 inUV;
layout (location = 4) in float inMat;
//layout (location = 5) in float materialIdx;

layout (location = 0) out vec4 outPosition;
layout (location = 1) out vec4 outNormal;
layout (location = 2) out vec4 outAlbedo;
//layout (location = 3) out vec4 outMaterial;

layout(set = 0, binding = 1) uniform sceneData
{
    vec4 fogColor;
	vec4 fogDistances;
	vec4 ambientColor;
	vec4 sunlightDirection;
	vec4 sunlightColor;
}scene;

layout(push_constant) uniform constants
{
	int id;
}pushC;

layout(set = 2, binding = 0) uniform sampler2D[] textures;

void main()
{
    vec3 N      = normalize( inNormal );
    vec3 color  = texture(textures[pushC.id], inUV).xyz * inColor;

    float materialIdx = float(inMat);

    outPosition = vec4( inWorldPos, materialIdx );
    outNormal   = vec4( N * 0.5 + vec3(0.5), 1 );
    outAlbedo   = vec4( color, 1.0 );
    //outMaterial = inMat;
}