#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

struct Vertex
{
  vec4 normal;
  vec4 color;
};

struct Light{
  vec4 pos;
  vec4 color;
};

layout(location = 0) rayPayloadInEXT vec3 hitValue;
layout(location = 1) rayPayloadEXT bool shadowed;
hitAttributeEXT vec3 attribs;

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 3, set = 0, scalar) buffer Vertices { Vertex v[]; } vertices[];
layout(binding = 4, set = 0) buffer Indices { int i[]; } indices[];
layout(binding = 5, set = 0, scalar) buffer Matrices { mat4 m; } matrices[];
layout(std140, binding = 6, set = 0) uniform Lights { Light lights[3]; } lightsBuffer;

void main()
{
  // Do all vertices, indices and barycentrics calculations
  const vec3 barycentricCoords = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);

  ivec3 ind = ivec3(indices[gl_InstanceCustomIndexEXT].i[3 * gl_PrimitiveID + 0], 
                    indices[gl_InstanceCustomIndexEXT].i[3 * gl_PrimitiveID + 1], 
                    indices[gl_InstanceCustomIndexEXT].i[3 * gl_PrimitiveID + 2]);

  Vertex v0 = vertices[gl_InstanceCustomIndexEXT].v[ind.x];
  Vertex v1 = vertices[gl_InstanceCustomIndexEXT].v[ind.y];
  Vertex v2 = vertices[gl_InstanceCustomIndexEXT].v[ind.z];

  // Use above results to calculate normal vector
  // Calculate worldPos by using ray information
  vec3 normal = v0.normal.xyz * barycentricCoords.x + v1.normal.xyz * barycentricCoords.y + v2.normal.xyz * barycentricCoords.z;
  vec3 N = normalize(vec4(vec4(normal, 1) * matrices[gl_InstanceCustomIndexEXT].m).xyz);
  vec3 worldPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;

  // Init values used for lightning
	vec3 color = vec3(0), light_color = vec3(0);
	float attenuation = 1.0, light_intensity = 1.0;
  color = v0.color.xyz;

  // Calculate light influence for each light
  for(int i = 0; i < lightsBuffer.lights.length(); i++){

    shadowed = false;
    // Init basic light information
    Light light = lightsBuffer.lights[i];

		vec3 L = normalize(light.pos.xyz - worldPos);
		float NdotL = clamp(dot(N, L), 0.0, 1.0);
		float light_max_distance = light.pos.w;
		float light_distance = length(light.pos.xyz - worldPos);
		light_intensity = light.color.w / (light_distance * light_distance);

    // Check if light has impact
    if(dot(N, L) > 0 && light_distance <= light_max_distance && light_intensity > 0)
    {
      // init as shadowed
      shadowed = true;
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

		attenuation = light_max_distance - light_distance;
		attenuation /= light_max_distance;
		attenuation = max(attenuation, 0.0);
		attenuation = attenuation * attenuation;

		light_color += (NdotL * light.color.xyz) * attenuation * light_intensity;

    if(shadowed)
      light_color *= 0.2;
  }

  // Return final color in the payload
  hitValue = color * light_color;

}
