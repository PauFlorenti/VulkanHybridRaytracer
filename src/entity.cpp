
#include "entity.h"

Object::Object(glm::vec3 position, Mesh* mesh, Material* material) :
	mesh(mesh),
	material(material)
{
	m_matrix = glm::translate(glm::mat4(1), position);
}

void Object::setColor(glm::vec3 color) {
	for (Vertex& v : mesh->_vertices)
		v.color = color;
}

void Object::draw(VkCommandBuffer& cmd, VkPipelineLayout pipelineLayout, glm::mat4 model)
{
	if(mesh)
	{
		VkDeviceSize offset = { 0 };
		vkCmdBindVertexBuffers(cmd, 0, 1, &mesh->_vertexBuffer._buffer, &offset);
		vkCmdBindIndexBuffer(cmd, mesh->_indexBuffer._buffer, offset, VK_INDEX_TYPE_UINT32);

		material_matrix pushConstant = { m_matrix, materialIdx };

		vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &m_matrix);

		vkCmdDrawIndexed(cmd, static_cast<uint32_t>(mesh->_indices.size()), 1, 0, 0, 0);
	}
	else if(prefab) {
		prefab->draw(cmd, pipelineLayout, model);
	}
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

void Light::setColor(glm::vec3 color) {
	color = color;
}