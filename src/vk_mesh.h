#pragma once

#include <vk_types.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm/gtx/hash.hpp>
#include <glm/glm/vec3.hpp>
#include <glm/glm/mat4x4.hpp>

struct VertexInputDescription{
	std::vector<VkVertexInputBindingDescription> bindings;
	std::vector<VkVertexInputAttributeDescription> attributes;

	VkPipelineVertexInputStateCreateFlags flags = 0;
};

struct rtVertexAttribute
{
	glm::vec4 normal;
	glm::vec4 color;
	glm::vec4 uv;
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

struct BlasInput {
	VkAccelerationStructureGeometryKHR			asGeometry;
	VkAccelerationStructureBuildRangeInfoKHR	asBuildRangeInfo;
	uint32_t									nTriangles;
};

struct TlasInstance {
	uint32_t					blasId{ 0 };		// Index of the BLAS
	uint32_t					instanceId{ 0 };	// Instance index
	uint32_t					hitGroupId{ 0 };	// Hit group index in the SBT
	uint32_t					mask{ 0xFF };		// Visibility mask
	VkGeometryInstanceFlagsKHR	flags{ VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR };
	glm::mat4					transform{ glm::mat4(1) };	// Identity model matrix
};

struct material_matrix {
	glm::mat4 matrix;
	int material;
};

struct Primitive
{
	uint32_t firstIndex{ 0 };
	uint32_t indexCount{ 0 };
	uint32_t vertexOffset{ 0 };
	uint32_t vertexCount{ 0 };
	int32_t materialIndex;
};

struct Mesh
{
	static std::unordered_map<std::string, Mesh*> _loadedMeshes;
	std::vector<Vertex>		_vertices;
	std::vector<uint32_t>	_indices;
	std::vector<Primitive>	_primitives;
	
	AllocatedBuffer			_vertexBuffer;
	AllocatedBuffer			_indexBuffer;

	static Mesh* GET(const char* filename);

	static Mesh* get_quad();
	static Mesh* get_triangle();
	static Mesh* get_cube();

	void upload();
	BlasInput mesh_to_geometry();

private:

	bool load_from_obj(const char* filename);
	void create_vertex_buffer();
	void create_index_buffer();

};

class Node
{
public:
	std::string				_name;
	Node*					_parent;
	std::vector<Node*>		_children;

	std::vector<Primitive>	_primitives;
	glm::mat4				_matrix;
	glm::mat4				_global_matrix;
	int						_materialId;

	Node() {};

	void addChild(Node* child);
	glm::mat4 getGlobalMatrix(bool fast = false);
	std::vector<BlasInput> node_to_geometry(
		const VkDeviceOrHostAddressConstKHR vertexBufferDeviceAddress, 
		const VkDeviceOrHostAddressConstKHR indexBufferDeviceAddress);
	std::vector<TlasInstance> node_to_instance(int& instanceId, const glm::mat4 matrix);

};

namespace tinygltf {
	class Node;
	class Model;
};

class Prefab
{
public:
	std::string				_name;
	std::vector<Node>		_nodes;

	Mesh* _mesh;

	Prefab() { _mesh = new Mesh(); }

	static Prefab* GET(std::string filename);
	void draw(VkCommandBuffer& cmd, VkPipelineLayout pipelineLayout, glm::mat4& model);
	std::vector<BlasInput> gltf_to_geometry();

private:

	void loadNode(	const tinygltf::Node& inputNode,
					const tinygltf::Model& input,
					Node* parent,
					std::vector<uint32_t>& indexBuffer,
					std::vector<Vertex>& vertexBuffer);
	void loadMaterials(const tinygltf::Model& tmodel);
	void drawNode(VkCommandBuffer& cmd, VkPipelineLayout pipelineLayout, Node node, glm::mat4& model);
};