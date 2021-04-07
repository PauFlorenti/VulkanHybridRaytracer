#version 460
#extension GL_EXT_nonuniform_qualifier : enable

layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec3 inColor;
layout (location = 3) in vec2 inUV;
layout (location = 4) in vec4 inNdc;
layout (location = 5) in vec4 inNdcPrev;

layout (location = 0) out vec4 outPosition;
layout (location = 1) out vec4 outNormal;
layout (location = 2) out vec3 outAlbedo;
layout (location = 3) out vec2 outMotion;
layout (location = 4) out vec3 outMaterial;
layout (location = 5) out vec3 outEmissive;

// Set 1: texture array
layout(set = 0, binding = 1) uniform sampler2D[] textures;

layout(push_constant) uniform constants
{
	layout (offset = 64)vec4 color;
    vec4 textures;
    vec4 shadingMetallicRoughness;
}pushC;

mat3 cotangent_frame(vec3 N, vec3 p, vec2 uv)
{
    vec3 dp1 = dFdx( p );
    vec3 dp2 = dFdy( p );
    vec2 duv1 = dFdx( p ).xy;
    vec2 duv2 = dFdy( p ).xy;

    vec3 dp2perp = cross( dp2, N );
    vec3 dp1perp = cross( N, dp1 );
    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;

    float invmax = inversesqrt( max(dot(T, T), dot(B, B)));
    return mat3(T * invmax, B * invmax, N);
}

vec3 perturbNormal(vec3 N, vec3 WP, vec2 uv, vec3 normal_pixel)
{
    normal_pixel = normal_pixel * 255./127. - 128./127.;
    mat3 TBN = cotangent_frame(N, WP, uv);
    return normalize(TBN * normal_pixel);
}

void main()
{

    vec3 color      = pushC.textures.x > -1 ? texture(textures[int(pushC.textures.x)], inUV).xyz * inColor : pushC.color.xyz;
    vec3 N          = pushC.textures.y > -1 ? texture(textures[int(pushC.textures.y)], inUV).xyz : normalize( inNormal );
    vec3 emissive   = pushC.textures.z > -1 ? texture(textures[int(pushC.textures.z)], inUV).xyz : vec3(0);
    vec3 material   = pushC.textures.w > -1 ? texture(textures[int(pushC.textures.w)], inUV).xyz : vec3(0, pushC.shadingMetallicRoughness.z, pushC.shadingMetallicRoughness.y);

    float materialIdx = pushC.shadingMetallicRoughness.w;

    if(pushC.textures.y > -1)
    {
        N = perturbNormal(inNormal, inWorldPos, inUV, N);
    }

    outPosition = vec4( inWorldPos, materialIdx );
    outNormal   = vec4( N * 0.5 + vec3(0.5), 1 );
    outAlbedo   = vec3( color );
    outMaterial = vec3( material );
    outEmissive = vec3( emissive );

    // convert to clip space
    vec3 ndc            = inNdc.xyz / inNdc.w;
    vec3 ndcPrev        = inNdcPrev.xyz / inNdcPrev.w;
    vec2 motion         = ndc.xy - ndcPrev.xy;
    outMotion           = vec2(motion * 0.5 + vec2(0.5));
}