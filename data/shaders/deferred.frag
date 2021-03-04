#version 460

layout (location = 0) out vec4 outFragColor;

layout (location = 0) in vec2 inUV;

layout (set = 0, binding = 0) uniform sampler2D positionTexture;
layout (set = 0, binding = 1) uniform sampler2D normalTexture;
layout (set = 0, binding = 2) uniform sampler2D albedoTexture;
layout (set = 0, binding = 3) uniform sampler2D motionTexture;

struct Light{
	vec4 pos;	// w used for max distance
	vec4 color;	// w used for intensity
	float radius;
};

layout (std140, set = 0, binding = 4) buffer LightBuffer
{
	Light lights[];
}lightBuffer;

layout(set = 0, binding = 5) uniform debugInfo{int target;} debug;

const vec3 ambient_light = vec3(0.0);

void main() 
{
	vec3 position 	= texture(positionTexture, inUV).xyz;
	vec3 normal 	= texture(normalTexture, inUV).xyz * 2.0 - vec3(1);
	vec3 albedo 	= texture(albedoTexture, inUV).xyz;
	vec3 motion		= texture(motionTexture, inUV).xyz;
	bool background = texture(positionTexture, inUV).w == 0 && texture(normalTexture, inUV).w == 0;

	if(debug.target > 0)
	{
		switch(debug.target){
			case 1:
				outFragColor = vec4(position, 1);
				break;
			case 2:
				outFragColor = vec4(normal, 0);
				break;
			case 3:
				outFragColor = vec4(albedo, 1);
				break;
		}
		return;
	}

	vec3 color = vec3(0), light_color = vec3(0);
	float attenuation = 1.0, light_intensity = 1.0;

	color = albedo;
	light_color += ambient_light;

	vec3 N = normalize(normal);
	
	for(int i = 0; i < lightBuffer.lights.length(); i++)
	{
		Light light 		= lightBuffer.lights[i];
		bool isDirectional 	= light.pos.w < 0;
		vec3 L 				= isDirectional ? light.pos.xyz : (light.pos.xyz - position.xyz);
		float NdotL 		= clamp(dot(N, normalize(L)), 0.0, 1.0);

		// Calculate the directional light
		if(isDirectional)
		{
			light_color += (NdotL * light.color.xyz);
		}
		else	// Calculate point lights
		{
			float light_max_distance 	= light.pos.w;
			float light_distance 		= length(L);
			light_intensity 			= light.color.w / (light_distance * light_distance);

			attenuation = light_max_distance - light_distance;
			attenuation /= light_max_distance;
			attenuation = max(attenuation, 0.0);
			attenuation = attenuation * attenuation;

			light_color += NdotL * light.color.xyz * light_intensity * attenuation;
		}
	}
	
	if(!background)
		color *= light_color;

	color = motion;

	outFragColor = vec4( color, 1.0f );
}