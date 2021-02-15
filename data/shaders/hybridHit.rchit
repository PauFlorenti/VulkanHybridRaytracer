#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable

#include "raycommon.glsl"
#include "helpers.glsl"

layout (location = 0) rayPayloadInEXT hitPayload prd;
layout (location = 1) rayPayloadEXT bool shadowed;
hitAttributeEXT vec3 attribs;

layout (set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;
layout (set = 0, std140, binding = 6) buffer Lights { Light lights[]; } lightsBuffer;
layout (set = 0, binding = 7, scalar) buffer Vertices { Vertex v[]; } vertices[];
layout (set = 0, binding = 8) buffer Indices { int i[]; } indices[];
layout (set = 0, binding = 9) uniform sampler2D[] textures;
layout (set = 0, binding = 11) buffer MaterialBuffer { Material mat[]; } materials;
layout (set = 0, binding = 12) buffer sceneBuffer { vec4 idx[]; } objIndices;
layout (set = 0, binding = 13, scalar) buffer Matrices { mat4 m[]; } matrices;

void main()
{
  // Do all vertices, indices and barycentrics calculations
  
  const vec3 barycentricCoords = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
  
  vec4 objIdx = objIndices.idx[gl_InstanceCustomIndexEXT];

  int instanceID        = int(objIdx.x);
  int materialID        = int(objIdx.y);
  int transformationID  = int(objIdx.z);
  int firstIndex        = int(objIdx.w);

  ivec3 ind     = ivec3(indices[instanceID].i[3 * gl_PrimitiveID + firstIndex + 0], 
                        indices[instanceID].i[3 * gl_PrimitiveID + firstIndex + 1], 
                        indices[instanceID].i[3 * gl_PrimitiveID + firstIndex + 2]);

  Vertex v0     = vertices[instanceID].v[ind.x];
  Vertex v1     = vertices[instanceID].v[ind.y];
  Vertex v2     = vertices[instanceID].v[ind.z];

  mat4 model = matrices.m[transformationID];

  // Use above results to calculate normal vector
  // Calculate worldPos by using ray information
  vec3 normal   = v0.normal.xyz * barycentricCoords.x + v1.normal.xyz * barycentricCoords.y + v2.normal.xyz * barycentricCoords.z;
  vec2 uv       = v0.uv.xy * barycentricCoords.x + v1.uv.xy * barycentricCoords.y + v2.uv.xy * barycentricCoords.z;
  vec3 N        = normalize(model * vec4(normal, 0)).xyz;
  vec3 worldPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;

  Material mat = materials.mat[materialID];
  vec3 albedo = texture(textures[int(mat.textures.x)], uv).xyz;

  // Init values used for lightning
	vec3 color = vec3(0);
	float attenuation = 1.0, light_intensity = 1.0;
  //vec3 rayDir = normalize(gl_WorldRayDirectionEXT);
  vec3 rayDir = gl_WorldRayDirectionEXT;

  for(int i = 0; i < lightsBuffer.lights.length(); i++)
  {
    // Init basic light information
		Light light 				      = lightsBuffer.lights[i];
		bool isDirectional        = light.pos.w < 0;
		vec3 L 						        = !isDirectional ? (light.pos.xyz - worldPos) : light.pos.xyz;
		float NdotL 				      = clamp(dot(N, normalize(L)), 0.0, 1.0);
		float light_max_distance 	= light.pos.w;
		float light_distance 		  = length(L);
		float light_intensity 		= isDirectional ? 1.0f : (light.color.w / (light_distance * light_distance));

    // init as shadowed
    shadowed = true;
    // Check if light has impact
    if(NdotL > 0)
    {
      // Shadow ray cast
      float tmin = 0.001, tmax = light_distance;
      traceRayEXT(topLevelAS, 
        gl_RayFlagsOpaqueEXT | gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT, 
        0xFF, 
        1, 0, 1, 
        worldPos + N * 1e-4, tmin, 
        L, tmax, 
        1);
    }

    // Calculate attenuation factor
		attenuation = light_max_distance - light_distance;
		attenuation /= light_max_distance;
		attenuation = max(attenuation, 0.0);
		attenuation = isDirectional ? 0.3 : attenuation * attenuation;

    if(shadowed || light_intensity == 0)
    {
      attenuation = 0.0;
    }

    vec3 difColor = vec3(0);
    vec3 specular = vec3(0);

    int shadingMode = int(mat.shadingMetallicRoughness.x);

    if(shadingMode == 0)  // DIFUS
    {
      difColor  = computeDiffuse(mat, N, L) * albedo;
      color    += (difColor + specular) * light_intensity * light.color.xyz * attenuation;
      prd       = hitPayload(vec4(color, gl_HitTEXT), vec4(1, 1, 1, 0), worldPos, prd.seed);
    }
    else if(shadingMode == 3) // MIRALL
    {
      const vec3 reflected    = reflect(normalize(gl_WorldRayDirectionEXT), N);
      const bool isScattered  = dot(reflected, N) > 0;

      difColor = isScattered ? computeDiffuse(mat, N, L) : vec3(1);
      //specular = computeSpecular(mat, N, L, -gl_WorldRayDirectionEXT);
      color += light_intensity * light.color.xyz * (difColor + specular) * attenuation;

      prd = hitPayload(vec4(color, gl_HitTEXT), vec4(reflected, isScattered ? 1 : 0), worldPos, prd.seed);
    }
    else if(shadingMode == 4) // VIDRE
    {
      float ior = mat.diffuse.w;
      float NdotD     = dot( N, rayDir );
			vec3 refrNormal = NdotD > 0.0 ? -N : N;
			float refrEta   = NdotD > 0.0 ? 1 / ior : ior;
			float cosine    = NdotD > 0.0 ? ior * NdotD : -NdotD;

			vec3 refracted = refract( rayDir, refrNormal, refrEta );
			const float reflectProb = refracted != vec3(0) ? Schlick( cosine, ior ) : 1;
			
			vec4 direction = rnd(prd.seed) < reflectProb ? vec4(reflect(rayDir, N), 1) : vec4(refracted, 1);
      color += mat.diffuse.xyz;
      prd = hitPayload(vec4(color, gl_HitTEXT), direction, worldPos, prd.seed);
    }
  }
}