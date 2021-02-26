#extension GL_GOOGLE_include_directive : enable
#include "random.glsl"

struct Vertex
{
  vec4 normal;
  vec4 color;
  vec4 uv;
};

struct Light{
  vec4 pos;
  vec4 color;
};

struct Material{
	vec4 diffuse;
    vec4 textures;
    vec4 shadingMetallicRoughness;
};

const float PI = 3.14159265;

// Polynomial approximation by Christophe Schlick
float Schlick(const float cosine, const float refractionIndex)
{
	float r0 = (1 - refractionIndex) / (1 + refractionIndex);
	r0 *= r0;
	return r0 + (1 - r0) * pow(1 - cosine, 5);
}

vec3 computeDiffuse(Material m, vec3 normal, vec3 lightDir)
{
    float NdotL = clamp(dot(normal, lightDir), 0.0f, 1.0f);
    return NdotL * m.diffuse.xyz;
};

vec3 computeSpecular(Material m, vec3 normal, vec3 lightDir, vec3 viewDir)
{
    // Specular
    vec3 V          = normalize(viewDir);
    vec3 R          = reflect(-normalize(lightDir), normalize(normal));
    float specular  = pow(clamp(dot(normalize(R), V), 0.0, 1.0), m.shadingMetallicRoughness.z);
    return vec3(1) * specular;
};

mat3 rotMat(const vec3 axis, const float angle)
{
    float c = cos(angle);
    float s = sin(angle);

    float t = 1 - c;
    float x = axis.x;
    float y = axis.y;
    float z = axis.z;

    return mat3(
        t * x * x + c,      t * x * y - s * z,  t * x * z + s * y,
        t * x * y + s * z,  t * y * y + c,      t * y * z - s * x,
        t * x * z - s * y,  t * y * z + s * x,  t * z * z + c
    );
}

vec3 sampleCone(inout uint seed, const vec3 direction, const float angle)
{
    float cosAngle = cos(angle);

    float z = rnd(seed) * (1.0 - cosAngle) + cosAngle;
    float phi = rnd(seed) * 2.0 * PI;

    float x = sqrt(1.0 - z * z) * cos(phi);
    float y = sqrt(1.0 - z * z) * sin(phi);
    vec3 north = vec3(0.0, 0.0, 1.0);

    vec3 axis = normalize(cross(north, normalize(direction)));
    float rotAngle = acos(dot(normalize(direction), north));

    mat3 rot = rotMat(axis, rotAngle);

    return rot * vec3(x, y, z);
}