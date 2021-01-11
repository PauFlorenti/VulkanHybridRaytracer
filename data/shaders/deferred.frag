#version 460

layout (location = 0) out vec4 outFragColor;

layout (location = 0) in vec2 inUV;

layout (set = 0, binding = 0) uniform sampler2D positionTexture;
layout (set = 0, binding = 1) uniform sampler2D normalTexture;
layout (set = 0, binding = 2) uniform sampler2D albedoTexture;

struct Light{
	vec4 pos;	// w used for max distance
	vec4 color;	// w used for intensity
};

layout (std140, set = 0, binding = 4) uniform LightBuffer
{
	Light lights[3];
}lightBuffer;

const vec3 ambient_light = vec3(0.2);

void main() 
{
	vec3 position 	= texture(positionTexture, inUV).xyz;
	vec3 normal 	= texture(normalTexture, inUV).xyz * 2.0 - vec3(1);
	vec3 albedo 	= texture(albedoTexture, inUV).xyz;
	bool background = texture(positionTexture, inUV).w == 0 && texture(normalTexture, inUV).w == 0;

	vec3 color = vec3(0), light_color = vec3(0);
	float attenuation = 1.0, light_intensity = 1.0;

	color = albedo;
	light_color += ambient_light;

	vec3 N = normalize(normal);
	
	for(int i = 0; i < lightBuffer.lights.length(); i++)
	{
		Light light = lightBuffer.lights[i];
		vec3 L;

		// Calculate the directional light
		bool isDirectional = light.pos.w < 0;
		if(isDirectional)
		{
			L = light.pos.xyz;
			float NdotL = clamp(dot(N, normalize(L)), 0.0, 1.0);
			light_color += (NdotL * light.color.xyz) * light_intensity * attenuation;
		}
		else	// Calculate point lights
		{
			L = normalize(light.pos.xyz - position.xyz);

			float light_max_distance = light.pos.w;
			float light_distance = length(L);
			light_intensity = light.color.w / (light_distance * light_distance);

			attenuation = light_max_distance - light_distance;
			attenuation /= light_max_distance;
			attenuation = max(attenuation, 0.0);
			attenuation = attenuation * attenuation;

			float NdotL = clamp(dot(N, normalize(L)), 0.0, 1.0);
			light_color += (NdotL * light.color.xyz) * attenuation * light_intensity;
		}
	}
	
	if(!background)
		color *= light_color;
	else
		color = albedo;

	outFragColor = vec4( color, 1.0f );
}