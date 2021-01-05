#extension GL_GOOGLE_include_directive : enable
#include "random.glsl"

struct Vertex
{
  vec4 normal;
  vec4 color;
};

struct Light{
  vec4 pos;
  vec4 color;
};

struct Material{
	vec4	  diffuse;
	vec4	  specular; // w is the Glossines factor
	float	  ior;	    // index of refraction
	float	  glossiness;
	int 	  illum;
};

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
    vec3 V          = normalize(-viewDir);
    vec3 R          = reflect(-lightDir, normal);
    float specular  = pow(max(dot(V, R), 0.0), m.glossiness);
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

    const vec3 diffuse      = computeDiffuse( m, normal, L);
    const vec4 colorAndDist = isScattered ? vec4(diffuse, t) : vec4(1, 1, 1, -1);

    return hitPayload( colorAndDist, vec4( reflected, isScattered ? 1 : 0), normal, 1);
};

hitPayload Dieletric( const Material m, const vec3 direction, const vec3 normal, const vec3 L, const float t, uint seed)
{
    const float NdotD       = dot( normal, direction );
    const vec3 refrNormal   = NdotD > 0.0 ? -normal : normal;
    const float refrEta     = NdotD > 0.0 ? 1 / m.ior : m.ior;
    const float cosine      = NdotD > 0 ? m.ior * NdotD : -NdotD;

    vec3 refracted          = refract( direction, refrNormal, refrEta );
    const float reflectProb = refracted != vec3( 0 ) ? Schlick( cosine, m.ior ) : 1;

    return rnd(seed) < reflectProb 
        ? hitPayload(vec4(1, 1, 1, t), vec4( reflect(direction, normal), 1), normal, 1)
        : hitPayload(vec4(1, 1, 1, t), vec4( refracted, 1), normal, 1);
};

hitPayload Scatter(const Material m, const vec3 direction, const vec3 normal, const vec3 L, const float t, uint seed)
{
    const vec3 normDirection = normalize(direction);

    switch(m.illum)
    {
        case 0:
            return Diffuse( m, normDirection, normal, L, t);
        case 3:
            return Metallic(m, normDirection, normal, L, t);
        case 4:
            return Dieletric( m, normDirection, normal, L, t, seed);
    }
};