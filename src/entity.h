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

class Entity {
public:
	glm::mat4 m_matrix;
	bool m_visible;
	bool m_selected;

	virtual void update() = 0;
};

// TODO: Object class
class Object : public Entity
{
public:
	Mesh* mesh;
	Material* material;
	int id{ 0 };

	Object(glm::vec3 position = glm::vec3(0), Mesh* mesh = NULL, Material* material = NULL);

	void update() {};
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
};