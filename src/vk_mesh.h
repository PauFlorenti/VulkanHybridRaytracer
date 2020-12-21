#pragma once

#include <vk_types.h>
#include <vector>
#include <glm/glm/vec3.hpp>
#include <glm/glm/mat4x4.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm/gtx/hash.hpp>

struct VertexInputDescription{
	std::vector<VkVertexInputBindingDescription> bindings;
	std::vector<VkVertexInputAttributeDescription> attributes;

	VkPipelineVertexInputStateCreateFlags flags = 0;
};

struct Vertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec3 color;
	glm::vec2 uv;

	static VertexInputDescription get_vertex_description();

	bool operator==(const Vertex& other) const {
		return position == other.position && color == other.color && uv == other.uv;
	}
};

struct Mesh
{
	std::vector<Vertex> _vertices;
	std::vector<uint32_t> _indices;
	
	AllocatedBuffer _vertexBuffer;
	AllocatedBuffer _indexBuffer;

	bool load_from_obj(const char* filename);

	static Mesh* get_quad();
	void get_triangle();
	void get_cube();

	void create_vertex_buffer();
	void create_index_buffer();

	void upload();
};


