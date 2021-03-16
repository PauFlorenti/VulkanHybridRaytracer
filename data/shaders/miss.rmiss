#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_GOOGLE_include_directive : enable

#include "raycommon.glsl"

layout(location = 0) rayPayloadInEXT hitPayload prd;

layout (set = 0, binding = 10) uniform sampler2D[] skybox;

#define PI 3.141592

void main()
{
    vec3 dir = normalize(gl_WorldRayDirectionEXT);
    //mat3 mat; 
    //mat[0] = vec3(cos(-90), 0, sin(-90));
    //mat[1] = vec3(0, 1, 0);
    //mat[2] = vec3(-sin(-90), 0, cos(-90));
    //dir = mat * dir;
    vec2 uv = vec2(0.5 + atan(dir.x, dir.z) / (2 * PI), 0.5 - asin(dir.y) / PI);
    vec3 color = texture(skybox[0], uv).xyz;
    prd.colorAndDist = vec4(color, -1);
}