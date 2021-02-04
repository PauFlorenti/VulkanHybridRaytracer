#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable

#include "raycommon.glsl"
#include "helpers.glsl"

layout(location = 0) rayPayloadInEXT hitPayload prd;
layout(location = 1) rayPayloadEXT bool shadowed;
hitAttributeEXT vec3 attribs;

struct entityIndices{
  int matIdx;
};

layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;
layout(set = 0, binding = 3, scalar) buffer Vertices { Vertex v[]; } vertices[];
layout(set = 0, binding = 4) buffer Indices { int i[]; } indices[];
layout(set = 0, binding = 5, scalar) buffer Matrices { mat4 m; } matrices[];
layout(set = 0, std140, binding = 6) buffer Lights { Light lights[]; } lightsBuffer;
layout(set = 0, binding = 7) buffer MaterialBuffer { Material mat[]; } materials;
layout(set = 0, binding = 8) buffer sceneBuffer { entityIndices idx; } matIndices[];
layout(set = 0, binding = 9) uniform sampler2D[] textures;

void main()
{
  // Do all vertices, indices and barycentrics calculations
  const vec3 barycentricCoords = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);

  ivec3 ind     = ivec3(indices[gl_InstanceCustomIndexEXT].i[3 * gl_PrimitiveID + 0], 
                        indices[gl_InstanceCustomIndexEXT].i[3 * gl_PrimitiveID + 1], 
                        indices[gl_InstanceCustomIndexEXT].i[3 * gl_PrimitiveID + 2]);

  Vertex v0     = vertices[gl_InstanceCustomIndexEXT].v[ind.x];
  Vertex v1     = vertices[gl_InstanceCustomIndexEXT].v[ind.y];
  Vertex v2     = vertices[gl_InstanceCustomIndexEXT].v[ind.z];

  // Use above results to calculate normal vector
  // Calculate worldPos by using ray information
  vec3 normal   = v0.normal.xyz * barycentricCoords.x + v1.normal.xyz * barycentricCoords.y + v2.normal.xyz * barycentricCoords.z;
  vec2 uv       = v0.uv.xy * barycentricCoords.x + v1.uv.xy * barycentricCoords.y + v2.uv.xy * barycentricCoords.z;
  vec3 N        = normalize(vec4(vec4(normal, 1) * matrices[gl_InstanceCustomIndexEXT].m).xyz);
  vec3 worldPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;

  // Init values used for lightning
	vec3 color = vec3(0), light_color = vec3(0), specular = vec3(0);
	float attenuation = 1.0, light_intensity = 1.0;

  int matIdx  = matIndices[gl_InstanceCustomIndexEXT].idx.matIdx;
  //int textIdx = matIndices[gl_InstanceCustomIndexEXT].idx.albedoIdx;

  Material mat  = materials.mat[matIdx];
  vec3 albedo   = mat.textures.x > -1 ? texture(textures[int(mat.textures.x)], uv).xyz : vec3(1);

  // Calculate light influence for each light
  for(int i = 0; i < lightsBuffer.lights.length(); i++)
  {
    // Init basic light information
    Light light = lightsBuffer.lights[i];
    bool isDirectional = light.pos.w < 0;

		vec3 L                    = isDirectional ? light.pos.xyz : light.pos.xyz - worldPos;
		float light_max_distance  = light.pos.w;
		float light_distance      = length(L);
    L                         = normalize(L);
		float NdotL               = clamp(dot(N, L), 0.0, 1.0);
		light_intensity           = isDirectional ? 1.0 : light.color.w / (light_distance * light_distance);
    light_color               = light.color.xyz;

    // Check if light has impact
    if( NdotL > 0 )
    {
      // init as shadowed
      shadowed = true;
      mat3 dirMatrix = makeDirectionMatrix(L);

      float r1 = rnd(prd.seed);
      float r2 = rnd(prd.seed);
      vec3 dir = normalize(uniformSampleCone(r1, r2, cos(10.0)));

      // Shadow ray cast
      float tmin = 0.001, tmax = light_distance + 1;
      traceRayEXT(topLevelAS, 
        gl_RayFlagsOpaqueEXT | gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT, 
        0xFF, 
        1, 0, 1, 
        worldPos, tmin, 
        L, tmax, 
        1);
    }

    // Calculate attenuation factor
		attenuation = light_max_distance - light_distance;
		attenuation /= light_max_distance;
		attenuation = max(attenuation, 0.0);
		attenuation = attenuation * attenuation;

    if(shadowed){
      attenuation = 0;
    }

    vec3 difColor = vec3(0);
    int shadingMode = int(mat.shadingMetallicRoughness.x);
    if(shadingMode == 0)  // DIFUS
    {
      difColor  = computeDiffuse(mat, N, L) * albedo;
      specular  = computeSpecular(mat, N, L, -gl_WorldRayDirectionEXT);
      color    += (difColor + specular) * light_intensity * light.color.xyz * attenuation;
      prd       = hitPayload(vec4(color, gl_HitTEXT), vec4(1, 1, 1, 0), worldPos, prd.seed);
    }
    else if(shadingMode == 3) // MIRALL
    {
      
      vec3 reflected    = reflect(normalize(gl_WorldRayDirectionEXT), N);
      bool isScattered  = dot(reflected, N) > 0;
      difColor          = isScattered ? computeDiffuse(mat, N, L) : vec3(1);
      specular = computeSpecular(mat, N, L, -gl_WorldRayDirectionEXT);

      color += light_intensity * light.color.xyz * (difColor + specular) * attenuation;
      prd = hitPayload(vec4(color, gl_HitTEXT), vec4(reflected, isScattered ? 1 : 0), worldPos, prd.seed);
    }
    else if(shadingMode == 4) // VIDRE
    {
      float ior = mat.diffuse.w;
      float NdotD     = dot( N, normalize(gl_WorldRayDirectionEXT) );
			vec3 refrNormal = NdotD > 0.0 ? -N : N;
			float refrEta   = NdotD > 0.0 ? 1 / ior : ior;
			float cosine    = NdotD > 0.0 ? ior * NdotD : -NdotD;

			vec3 refracted = refract( gl_WorldRayDirectionEXT, refrNormal, refrEta );
			const float reflectProb = refracted != vec3(0) ? Schlick( cosine, ior ) : 1;
			
			vec4 direction = rnd(prd.seed) < reflectProb ? vec4(reflect(gl_WorldRayDirectionEXT, N), 1) : vec4(refracted, 1);
      color += computeDiffuse(mat, N, L);
      prd = hitPayload(vec4(color, gl_HitTEXT), direction, worldPos, prd.seed);
    }
  }
}
