#version 460
#extension GL_EXT_nonuniform_qualifier : enable

layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec3 inColor;
layout (location = 3) in vec2 inUV;
layout (location = 4) in float inMat;
layout (location = 5) in mat4 inMatrix;

layout (location = 0) out vec4 outPosition;
layout (location = 1) out vec4 outNormal;
layout (location = 2) out vec4 outAlbedo;

// Set 1: texture array
layout(set = 0, binding = 1) uniform sampler2D[] textures;

layout(push_constant) uniform constants
{
	layout (offset = 64)vec4 color;
    vec4 textures;
    vec4 shadingMetallicRoughness;
}pushC;

void main()
{

    vec3 N = normalize( inNormal );
    vec3 color      = pushC.textures.x > -1 ? texture(textures[int(pushC.textures.x)], inUV).xyz * inColor : pushC.color.xyz;
    vec3 emissive   = pushC.textures.z > -1 ? texture(textures[int(pushC.textures.z)], inUV).xyz : vec3(0);
    color += emissive;

    float materialIdx = float(inMat);

    outPosition = vec4( inWorldPos, materialIdx );
    outNormal   = vec4( N * 0.5 + vec3(0.5), 1 );
    outAlbedo   = vec4( color, 1.0 );
}