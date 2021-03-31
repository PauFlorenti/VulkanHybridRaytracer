#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable

#include "helpers.glsl"

struct shadowPayload{
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
layout(set = 0, binding = 8) uniform sampler2D motionTexture;
layout(set = 0, binding = 9) uniform SampleBuffer {int samples;} samplesBuffer;

const vec2 BlueNoiseInDisk[64] = vec2[64](
    vec2(0.478712,0.875764),
    vec2(-0.337956,-0.793959),
    vec2(-0.955259,-0.028164),
    vec2(0.864527,0.325689),
    vec2(0.209342,-0.395657),
    vec2(-0.106779,0.672585),
    vec2(0.156213,0.235113),
    vec2(-0.413644,-0.082856),
    vec2(-0.415667,0.323909),
    vec2(0.141896,-0.939980),
    vec2(0.954932,-0.182516),
    vec2(-0.766184,0.410799),
    vec2(-0.434912,-0.458845),
    vec2(0.415242,-0.078724),
    vec2(0.728335,-0.491777),
    vec2(-0.058086,-0.066401),
    vec2(0.202990,0.686837),
    vec2(-0.808362,-0.556402),
    vec2(0.507386,-0.640839),
    vec2(-0.723494,-0.229240),
    vec2(0.489740,0.317826),
    vec2(-0.622663,0.765301),
    vec2(-0.010640,0.929347),
    vec2(0.663146,0.647618),
    vec2(-0.096674,-0.413835),
    vec2(0.525945,-0.321063),
    vec2(-0.122533,0.366019),
    vec2(0.195235,-0.687983),
    vec2(-0.563203,0.098748),
    vec2(0.418563,0.561335),
    vec2(-0.378595,0.800367),
    vec2(0.826922,0.001024),
    vec2(-0.085372,-0.766651),
    vec2(-0.921920,0.183673),
    vec2(-0.590008,-0.721799),
    vec2(0.167751,-0.164393),
    vec2(0.032961,-0.562530),
    vec2(0.632900,-0.107059),
    vec2(-0.464080,0.569669),
    vec2(-0.173676,-0.958758),
    vec2(-0.242648,-0.234303),
    vec2(-0.275362,0.157163),
    vec2(0.382295,-0.795131),
    vec2(0.562955,0.115562),
    vec2(0.190586,0.470121),
    vec2(0.770764,-0.297576),
    vec2(0.237281,0.931050),
    vec2(-0.666642,-0.455871),
    vec2(-0.905649,-0.298379),
    vec2(0.339520,0.157829),
    vec2(0.701438,-0.704100),
    vec2(-0.062758,0.160346),
    vec2(-0.220674,0.957141),
    vec2(0.642692,0.432706),
    vec2(-0.773390,-0.015272),
    vec2(-0.671467,0.246880),
    vec2(0.158051,0.062859),
    vec2(0.806009,0.527232),
    vec2(-0.057620,-0.247071),
    vec2(0.333436,-0.516710),
    vec2(-0.550658,-0.315773),
    vec2(-0.652078,0.589846),
    vec2(0.008818,0.530556),
    vec2(-0.210004,0.519896) 
);

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

  const vec2 pixelCenter	= vec2(gl_LaunchIDEXT.xy) + vec2(0.5);	// gl_LaunchIDEXT represents the floating-point pixel coordinates normalized between 0 and 1
	const vec2 inUV 		= pixelCenter/vec2(gl_LaunchSizeEXT.xy);	//gl_LaunchSizeExt is the image size provided in the traceRayEXT function

  // Use above results to calculate normal vector
  // Calculate worldPos by using ray information
  const vec3 normal   = v0.normal.xyz * barycentricCoords.x + v1.normal.xyz * barycentricCoords.y + v2.normal.xyz * barycentricCoords.z;
  const vec3 N        = normalize(model * vec4(normal, 0)).xyz;
  const vec3 worldPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
  vec2 motion   = texture(motionTexture, inUV).xy * 2.0 - vec2(1.0);

  // Init values used for lightning

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
    float shadowFactor              = 0.0;
    int shadowSamples               = samplesBuffer.samples;

    bool flag = false;

    // Check if light has impact, then calculate shadow
    if( NdotL > 0 )
    {
      for(int a = 0; a < shadowSamples; a++)
      {
        if(a == 4 && shadowFactor == 4){
          flag = true;
          break;
        }
        //int index = int(prd.frame) % shadowSamples;
        //if(index >= 64)
        //  index = index - 64;
        int index = a;
        vec2 samplePos = BlueNoiseInDisk[index];
        // Init as shadowed
        shadowed          = true;
        float angle = rnd(prd.seed) * 2.0f * PI;
        float pointRadius = light.radius * sqrt(rnd(prd.seed));
        vec2 diskPoint = vec2(pointRadius * cos(angle), pointRadius * sin(angle));
        //diskPoint.x = samplePos.x * cos(angle) - samplePos.y * sin(angle);
        //diskPoint.y = samplePos.x * sin(angle) + samplePos.y * cos(angle);
        //diskPoint *= light.radius;
        vec3 lTangent = normalize(cross(L, vec3(0, 1, 0)));
        vec3 lBitangent = normalize(cross(lTangent, L));
        vec3 target = worldPos + L + diskPoint.x * lTangent + diskPoint.y * lBitangent;
        const vec3 dir = normalize(target - worldPos);
        const uint flags  = gl_RayFlagsOpaqueEXT | gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT;
        float tmin = 0.001, tmax = light_distance + 1;
        
        // Shadow ray cast
        traceRayEXT(topLevelAS, flags, 0xFF, 1, 0, 0, 
          worldPos + dir * 1e-2, tmin, dir, tmax, 1);

        if(!shadowed){
          shadowFactor++;
        }
      }
      if(flag){
        shadowFactor /= 4;
      }
      else{
        shadowFactor /= shadowSamples;
      }
    }

    motion.y = 1.0 - motion.y;
    vec2 reprojectedUV = gl_LaunchIDEXT.xy - motion;
    /*
    float a     = 0.2;
    vec3 cLast  = imageLoad(shadowImage[i], ivec2(gl_LaunchIDEXT.xy)).rgb;
    //vec3 cLast = imageLoad(shadowImage[i], ivec2(reprojectedUV)).rgb;
    //vec3 c      = mix(cLast, vec3(shadowFactor), a);
    vec3 c = vec3(shadowFactor);
    imageStore(shadowImage[i], ivec2(gl_LaunchIDEXT.xy), vec4(c, 1)); 
    */
    //vec3 c = vec3(shadowFactor);
    //imageStore(shadowImage[i], ivec2(gl_LaunchIDEXT.xy), vec4(c, 1));
    
    
    if(prd.frame > 0)
    {
      float a = 1.0f / float(prd.frame + 1);
      if(prd.frame > 64)
        return;
      vec3 cLast = imageLoad(shadowImage[i], ivec2(gl_LaunchIDEXT.xy)).rgb;
      //a = 0.2;
      //vec3 cLast = imageLoad(shadowImage[i], ivec2(reprojectedUV)).rgb;
      vec3 c = mix(cLast, vec3(shadowFactor), a);
      imageStore(shadowImage[i], ivec2(gl_LaunchIDEXT.xy), vec4(c, 1)); 
    }
    else{
      vec3 cLast = imageLoad(shadowImage[i], ivec2(reprojectedUV)).rgb;
      vec3 c = mix(cLast, vec3(shadowFactor), 0.3);
      imageStore(shadowImage[i], ivec2(gl_LaunchIDEXT.xy), vec4(c, 1)); 
    }
  }
}
