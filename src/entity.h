#pragma once

#include <glm/glm/glm.hpp>
#include <glm/glm/gtc/type_ptr.hpp>
#include <glm/glm/gtc/matrix_transform.hpp>

#include <vk_mesh.h>

struct Material {
	VkDescriptorSet		textureSet;
	VkPipeline			pipeline;
	VkPipelineLayout	pipelineLayout;
};

struct MTLMaterial {
	glm::vec4	diffuse{ 1,1,1,1 };
	glm::vec4	specular{ 1,1,1,1 }; // w is the Glossines factor
	float		ior{ 1 };	// index of refraction
	float		glossiness{ 1 };
	alignas(8) int			illum{ 0 };
	//int id;
};

class Entity {
public:
	glm::mat4 m_matrix;
	bool m_visible;
	bool m_selected;

	virtual void update() = 0;
	virtual void setColor(glm::vec3 color) = 0;
};

// TODO: Object class
class Object : public Entity
{
public:
	Mesh*			mesh;
	Material*		material;
	int				materialIdx{ 0 };
	int				id{ 0 };

	Object(glm::vec3 position = glm::vec3(0), Mesh* mesh = NULL, Material* material = NULL);

	void update() {};
	void setColor(glm::vec3 color);
};

enum lightType{
	DIRECTIONAL,
	POINT,
	SPOT
};

class Light : public Entity
{
public:
	lightType type;
	glm::vec3 color;
	glm::vec3 position;
	float intensity;
	float maxDistance;

	Light(lightType type = POINT, glm::vec3 color = glm::vec3(1), glm::vec3 position = glm::vec3(0), float intensity = 1000.0f, float maxDistance = 500.0f);

	void update();
	void setColor(glm::vec3 color);
};