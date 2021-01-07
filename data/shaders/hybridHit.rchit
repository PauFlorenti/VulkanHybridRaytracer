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
layout (set = 0, std140, binding = 7) uniform Lights { Light lights[3]; } lightsBuffer;
layout (set = 0, binding = 8, scalar) buffer Vertices { Vertex v[]; } vertices[];
layout (set = 0, binding = 9) buffer Indices { int i[]; } indices[];
layout (set = 0, binding = 10, scalar) buffer Matrices { mat4 m; } matrices[];
layout (set = 0, binding = 11) uniform MaterialBuffer { Material mat[10]; } materials;
layout (set = 0, binding = 12) buffer sceneBuffer { int matIdx; } matIndices[];

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
  vec3 N        = normalize(vec4(vec4(normal, 1) * matrices[gl_InstanceCustomIndexEXT].m).xyz);
  vec3 worldPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;

  // Init values used for lightning
	vec3 color = vec3(0), specular = vec3(0);
	float attenuation = 1.0, light_intensity = 1.0;

  int matIdx = matIndices[gl_InstanceCustomIndexEXT].matIdx;

  Material mat = materials.mat[matIdx];

  for(int i = 0; i < lightsBuffer.lights.length(); i++)
  {
    // Init basic light information
		Light light 				      = lightsBuffer.lights[i];
		bool isDirectional        = light.pos.w < 0;
		vec3 L 						        = !isDirectional ? (light.pos.xyz - worldPos) : light.pos.xyz;
		float NdotL 				      = clamp(dot(N, normalize(L)), 0.0, 1.0);
		float light_max_distance 	= light.pos.w;
		float light_distance 		  = length(L);
		L = normalize(L);
		float light_intensity 		= isDirectional ? 1.0f : (light.color.w / (light_distance * light_distance));

    // init as shadowed
    shadowed = true;
    // Check if light has impact
    if(NdotL > 0)
    {
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
		attenuation = isDirectional ? 0.3 : attenuation * attenuation;

    if(shadowed)
    {
      attenuation = 0;
    }
    else
    {
      specular += computeSpecular(mat, N, L, gl_WorldRayDirectionEXT);
    }

    vec3 difColor;
    difColor = computeDiffuse(mat, N, L);
    if(mat.illum == 1)
    {
      difColor = computeDiffuse(mat, N, L);
    }
    else if(mat.illum == 3)
    {
      prd.origin        = worldPos;
      vec3 reflected    = reflect(gl_WorldRayDirectionEXT, N);
      bool isScattered  = dot(reflected, N) > 0;
      difColor          = isScattered ? computeDiffuse(mat, N, L) : vec3(1); //computeDiffuse(mat, N, L);
      prd.direction     = vec4(reflected, isScattered ? 1 : 0);
    }
    color += light_intensity * light.color.xyz * (difColor) /*attenuation*/;
  }

  // Return final color in the payload
  prd.colorAndDist = vec4(color, 1);
}