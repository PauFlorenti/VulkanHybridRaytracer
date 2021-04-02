#version 450

layout(location = 0) in vec2 inUV;

layout(set = 0, binding = 1) uniform sampler2D sky;

layout(location = 0) out vec4 outPosition;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outFragColor;
layout(location = 3) out vec4 outMotion;
layout(location = 4) out vec4 outMaterial;
layout(location = 5) out vec4 outEmissive;

void main()
{
    outPosition     = vec4(0);
    outNormal       = vec4(0);
    outFragColor    = texture(sky, inUV);
    outMotion       = vec4(0);
    outMaterial     = vec4(0);
    outEmissive     = vec4(0);
}