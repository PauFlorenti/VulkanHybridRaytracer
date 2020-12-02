
#include "entity.h"

Object::Object(glm::vec3 position, Mesh* mesh, Material* material) :
	mesh(mesh),
	material(material)
{
	m_matrix = glm::translate(glm::mat4(1), position);
}

Light::Light(lightType type, glm::vec3 color /*white*/, glm::vec3 position/*0,0,0*/, float intensity/*1000*/, float maxDistance/*500*/) :
	type{ type },
	color{ color },
	position{ position },
	intensity{ intensity },
	maxDistance{ maxDistance }
{
	this->m_matrix		= glm::translate(glm::mat4(1), position);
	this->m_selected	= false;
	this->m_visible		= true;
}

void Light::update()
{
	this->position = (glm::vec3)m_matrix[3];
}