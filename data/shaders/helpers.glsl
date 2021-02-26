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

vec3 uniformSampleCone(float r1, float r2, float cosThetaMax)
{
    float cosTheta = (1.0 - r1) + r1 * cosThetaMax;
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    float phi = r2 * 2 * PI;
    return vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
};


mat3 AngleAxis3x3(float angle, vec3 axis)
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

// return a random direction vector inside a cone
vec3 getConeSample(inout uint seed, vec3 direction, float angle)
{
    const float cosAngle = cos(angle);
    const float z = rnd(seed) * (1.0 - cosAngle) + cosAngle;
    const float phi = rnd(seed) * 2.0 * PI;

    const float x = sqrt(1.0 - z * z) * cos(phi);
    const float y = sqrt(1.0 - z * z) * sin(phi);
    const vec3 north = vec3(0, 0, 1); 

    // Find rotation axis and rotation angle
    const vec3 axis = normalize(cross(north, normalize(direction)));
    const float rotAngle = acos(dot(normalize(direction), north));

    mat3 rot = AngleAxis3x3(rotAngle, axis);
    return rot * vec3(x, y, z);
}

mat3 makeDirectionMatrix(vec3 dir)
{
    vec3 xAxis = normalize(cross(vec3(0, 1, 0), dir));
    vec3 yAxis = normalize(cross(dir, xAxis));

    mat3 matrix;
    matrix[0] = vec3(xAxis.x, yAxis.x, dir.x);
    matrix[1] = vec3(xAxis.y, yAxis.y, dir.y);
    matrix[2] = vec3(xAxis.z, yAxis.z, dir.z);

    return matrix;
};

