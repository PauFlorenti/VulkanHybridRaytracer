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

hitPayload Diffuse( const Material m, const vec3 direction, const vec3 normal, const vec3 L, const float t)
{
    const vec3 diffuse = computeDiffuse(m, normal, L);
    return hitPayload( vec4(diffuse, t), vec4(0), normal, 1);
};

hitPayload Metallic( const Material m, const vec3 direction, const vec3 normal, const vec3 L, const float t)
{
    const vec3 reflected    = reflect(direction, normal);
    const bool isScattered  = dot( reflected, normal ) > 0;
    const vec4 diffuse      = isScattered ? vec4(computeDiffuse( m, normal, L), t) : vec4(1, 1, 1, -1);

    return hitPayload( diffuse, vec4( reflected, isScattered ? 1 : 0), normal, 1);
};

hitPayload Dieletric( const Material m, const vec3 direction, const vec3 normal, const vec3 L, const float t, uint seed)
{
    float ior = m.diffuse.w;
    const float NdotD       = dot( normal, direction );
    const vec3 refrNormal   = NdotD > 0.0 ? -normal : normal;
    const float refrEta     = NdotD > 0.0 ? 1 / ior : ior;
    const float cosine      = NdotD > 0.0 ? ior * NdotD : -NdotD;

    vec3 refracted          = refract( direction, refrNormal, refrEta );
    const float reflectProb = refracted != vec3( 0 ) ? Schlick( cosine, ior ) : 1;

    //if( refracted == vec3( 0.0 ))
	//    refracted = reflect( direction, normal );
	//return hitPayload( vec4( 1, 1, 1, t ) , vec4( refracted, 1 ), normal, seed);

    return rnd(seed) < reflectProb 
        ? hitPayload(vec4(1, 1, 1, t), vec4( reflect(direction, normal), 1), normal, 1)
        : hitPayload(vec4(1, 1, 1, t), vec4( refracted, 1), normal, 1);
};

hitPayload Scatter(const Material m, const vec3 direction, const vec3 normal, const vec3 L, const float t, uint seed)
{
    const vec3 normDirection = normalize(direction);

    switch(int(m.shadingMetallicRoughness.x))
    {
        case 0:
            return Diffuse( m, normDirection, normal, L, t);
        case 3:
            return Metallic(m, normDirection, normal, L, t);
        case 4:
            return Dieletric( m, normDirection, normal, L, t, seed);
    }
};

vec3 uniformSampleCone(float r1, float r2, float cosThetaMax)
{
    float cosTheta = (1.0 - r1) + r1 * cosThetaMax;
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    float phi = r2 * 2 * PI;
    return vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
};

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