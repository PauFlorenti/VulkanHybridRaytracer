#pragma once

#include <vk_mesh.h>

// Define the type of light available
enum lightType{
	DIRECTIONAL_LIGHT,
	POINT_LIGHT,
	SPOT_LIGHT
};

struct MTLMaterial {
	glm::vec4		diffuse{ 1,1,1,1 };
	glm::vec4		specular{ 1,1,1,1 }; // w is the Glossines factor
	float			ior{ 1 };	// index of refraction
	float			glossiness{ 1 };
	alignas(8) int	illum{ 0 };
};

struct EntityIndices {
	int matIdx;
	int albedoIdx;
};

class Entity 
{
public:
	glm::mat4	m_matrix;
	bool		m_visible;
	bool		m_selected;

	virtual void update() = 0;
	virtual void setColor(glm::vec3 color) = 0;
	virtual void draw(VkCommandBuffer& cmd, VkPipelineLayout pipelineLayout, glm::mat4 model) = 0;
};

// TODO: Object class
class Object : public Entity
{
public:
	Prefab*			prefab;
	Material*		material;
	int				materialIdx{ 0 };
	int				id{ 0 };

	Object(glm::vec3 position = glm::vec3(0), Mesh* mesh = NULL, Material* material = NULL);

	void update() {};
	void setColor(glm::vec3 color) {};
	void draw(VkCommandBuffer& cmd, VkPipelineLayout pipelineLayout, glm::mat4 model);

};

class Light : public Entity
{
public:
	lightType type;
	glm::vec3 color;
	glm::vec3 position;
	float intensity;
	float maxDistance;

	Light(lightType type = POINT_LIGHT, glm::vec3 color = glm::vec3(1), glm::vec3 position = glm::vec3(0), float intensity = 1000.0f, float maxDistance = 500.0f);

	void update();
	void setColor(glm::vec3 color);
	void draw(VkCommandBuffer& cmd, VkPipelineLayout pipelineLayout, glm::mat4 model) {};
};
