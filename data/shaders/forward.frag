#version 460
#extension GL_EXT_nonuniform_qualifier : enable

layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec3 inColor;
layout (location = 3) in vec2 inUV;
layout (location = 4) in float materialIdx;
layout (location = 5) in mat4 inMatrix;

layout (location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform sampler2D[] textures;

void main()
{
    vec3 N 		= normalize( inNormal );
    //vec3 color 	= texture(textures[pushC.id], inUV).xyz;
	vec3 color = vec3(1);

    outColor = vec4( color, 1.0 );
}