#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable

#include "helpers.glsl"

struct shadowPayload{
	//vec3 color;
  uint seed;
  float frame;
};

layout(location = 0) rayPayloadInEXT shadowPayload prd;
layout(location = 1) rayPayloadEXT bool shadowed;
hitAttributeEXT vec3 attribs;

layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 1, set = 0, rgba8) uniform image2D[] shadowImage;
layout(set = 0, binding = 3, scalar) buffer Vertices { Vertex v[]; } vertices[];
layout(set = 0, binding = 4) buffer Indices { int i[]; } indices[];
layout(set = 0, binding = 5, scalar) buffer Matrices { mat4 m[]; } matrices;
layout(set = 0, std140, binding = 6) buffer Lights { Light lights[]; } lightsBuffer;
layout(set = 0, binding = 7) buffer sceneBuffer { vec4 idx[]; } objIndices;

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

  const mat4 model = matrices.m[transformationID];

  // Use above results to calculate normal vector
  // Calculate worldPos by using ray information
  const vec3 normal   = v0.normal.xyz * barycentricCoords.x + v1.normal.xyz * barycentricCoords.y + v2.normal.xyz * barycentricCoords.z;
  const vec3 N        = normalize(model * vec4(normal, 0)).xyz;
  const vec3 worldPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;

  // Init values used for lightning
  float shadowFactor    = 0.0;

  // Calculate light influence for each light
  for(int i = 0; i < lightsBuffer.lights.length(); i++)
  {
    // Init basic light information
    Light light                     = lightsBuffer.lights[i];
    const bool isDirectional        = light.pos.w < 0;
	  vec3 L                          = isDirectional ? light.pos.xyz : (light.pos.xyz - worldPos);
	  const float light_max_distance  = light.pos.w;
	  const float light_distance      = length(L);
    L                               = normalize(L);
	  const float NdotL               = clamp(dot(N, L), 0.0, 1.0);
	  const float light_intensity     = isDirectional ? 1.0 : (light.color.w / (light_distance * light_distance));
    float shadowFactor    = 0.0;

    // Check if light has impact, then calculate shadow
    if( NdotL > 0 )
    {
      for(int a = 0; a < SHADOWSAMPLES; a++)
      {
        // Init as shadowed
        shadowed          = true;
        const vec3 dir    = normalize(sampleSphere(prd.seed, light.pos.xyz, light.radius) - worldPos);
        const uint flags  = gl_RayFlagsOpaqueEXT | gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT;
        float tmin = 0.001, tmax = light_distance + 1;
        
        // Shadow ray cast
        traceRayEXT(topLevelAS, flags, 0xFF, 1, 0, 0, 
          worldPos + dir * 1e-2, tmin, dir, tmax, 1);

        if(!shadowed){
          shadowFactor++;
        }
      }
      shadowFactor /= SHADOWSAMPLES;
    }

    if(prd.frame > 0)
    {
      float a = 1.0f / float(prd.frame + 1);
      vec3 old_color = imageLoad(shadowImage[i], ivec2(gl_LaunchIDEXT.xy)).rgb;
      vec3 c = mix(old_color, vec3(shadowFactor), a);
      imageStore(shadowImage[i], ivec2(gl_LaunchIDEXT.xy), vec4(c, 1));
    }
    else{
      imageStore(shadowImage[i], ivec2(gl_LaunchIDEXT.xy), vec4(vec3(shadowFactor), 1.0));
    }
  }
}
