
#include "vk_mesh.h"
#include "vk_initializers.h"
#include "vk_engine.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"

extern std::vector<std::string> searchPaths;
std::unordered_map<std::string, Mesh*> Mesh::_loadedMeshes;

VertexInputDescription Vertex::get_vertex_description()
{
	VertexInputDescription description;

	VkVertexInputBindingDescription mainBinding = {};
	mainBinding.binding		= 0;
	mainBinding.stride		= sizeof(Vertex);
	mainBinding.inputRate	= VK_VERTEX_INPUT_RATE_VERTEX;

	description.bindings.push_back(mainBinding);

	// Position will be stored at locaiton 0
	VkVertexInputAttributeDescription positionAttribute = {};
	positionAttribute.binding	= 0;
	positionAttribute.location	= 0;
	positionAttribute.format	= VK_FORMAT_R32G32B32_SFLOAT;
	positionAttribute.offset	= offsetof(Vertex, position);

	// Normal will be stores at location 1
	VkVertexInputAttributeDescription normalAttribute{};
	normalAttribute.binding		= 0;
	normalAttribute.location	= 1;
	normalAttribute.format		= VK_FORMAT_R32G32B32_SFLOAT;
	normalAttribute.offset		= offsetof(Vertex, normal);

	// Color will be stored at location 2
	VkVertexInputAttributeDescription colorAttribute = {};
	colorAttribute.binding	= 0;
	colorAttribute.location	= 2;
	colorAttribute.format	= VK_FORMAT_R32G32B32_SFLOAT;
	colorAttribute.offset	= offsetof(Vertex, color);

	// UV will be stored at location 3
	VkVertexInputAttributeDescription uvAttribute = {};
	uvAttribute.binding		= 0;
	uvAttribute.location	= 3;
	uvAttribute.format		= VK_FORMAT_R32G32_SFLOAT;
	uvAttribute.offset		= offsetof(Vertex, uv);

	description.attributes.push_back(positionAttribute);
	description.attributes.push_back(normalAttribute);
	description.attributes.push_back(colorAttribute);
	description.attributes.push_back(uvAttribute);

	return description;
}

namespace std {
	template<> struct hash<Vertex> {
		size_t operator()(Vertex const& vertex) const {
			return ((((hash<glm::vec3>()(vertex.position) ^
				(hash<glm::vec3>()(vertex.normal) << 1)) >> 1) ^
				(hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
				(hash<glm::vec2>()(vertex.uv) << 1);
		}
	};
}

Mesh* Mesh::GET(const char* filename)
{
	Mesh* mesh = new Mesh();
	std::string s = filename;
	std::string name = VulkanEngine::engine->findFile(s, searchPaths, true);
	
	if(!_loadedMeshes[name])
	{
		mesh->load_from_obj(name.c_str());
		_loadedMeshes[name] = mesh;
	}
	else
	{
		mesh = _loadedMeshes[name];
	}
	return mesh;
}

bool Mesh::load_from_obj(const char* filename)
{
	tinyobj::attrib_t attrib;

	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;

	std::string warn, err;

	tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename, nullptr);

	if (!warn.empty())
		std::cout << "WARN: " << warn << std::endl;

	if (!err.empty()) {
		std::cout << "ERR: " << err << std::endl;
		return false;
	}

	std::unordered_map<Vertex, uint32_t> uniqueVertices{};

	for (const auto& shape : shapes)
	{
		for (const auto& index : shape.mesh.indices)
		{
			Vertex vertex{};

			vertex.position = {
				attrib.vertices[3 * index.vertex_index + 0],
				attrib.vertices[3 * index.vertex_index + 1],
				attrib.vertices[3 * index.vertex_index + 2]
			};

			vertex.normal = {
				attrib.normals[3 * index.normal_index + 0],
				attrib.normals[3 * index.normal_index + 1],
				attrib.normals[3 * index.normal_index + 2]
			};

			vertex.color = { 1.0f, 1.0f, 1.0f };

			if (attrib.texcoords.size() > 0)
			{
				vertex.uv = {
					attrib.texcoords[2 * index.texcoord_index + 0],
					1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
				};
			}


			if (uniqueVertices.count(vertex) == 0) {
				uniqueVertices[vertex] = static_cast<uint32_t>(_vertices.size());
				_vertices.push_back(vertex);
			}

			_indices.push_back(uniqueVertices[vertex]);
		}
	}

	// upload mesh
	upload();

	return true;
}

Mesh* Mesh::get_quad()
{

	if (!_loadedMeshes["quad"])
	{
		Mesh* mesh = new Mesh();

		mesh->_vertices.push_back({ {  1.0f,  1.0f, 0.0f }, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f} });
		mesh->_vertices.push_back({ { -1.0f,  1.0f, 0.0f }, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f} });
		mesh->_vertices.push_back({ { -1.0f, -1.0f, 0.0f }, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} });
		mesh->_vertices.push_back({ {  1.0f, -1.0f, 0.0f }, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f} });

		mesh->_indices = {0, 1, 2, 2, 3, 0};

		mesh->upload();

		_loadedMeshes["quad"] = mesh;

		return mesh;
	}

	return _loadedMeshes["quad"];
}

Mesh* Mesh::get_triangle()
{
	if (!_loadedMeshes["triangle"])
	{
		Mesh* mesh = new Mesh();
		
		mesh->_vertices.clear();

		mesh->_vertices.push_back({ {  1.f,  1.f, 0.f }, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f} });
		mesh->_vertices.push_back({ { -1.f,  1.f, 0.f }, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f} });
		mesh->_vertices.push_back({ {  0.f, -1.f, 0.f }, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f} });

		mesh->_indices = { 0, 1, 2 };

		mesh->upload();
		_loadedMeshes["triangle"] = mesh;

		return mesh;
	}
	
	return _loadedMeshes["triangle"];
}

Mesh* Mesh::get_cube()
{
	if (!_loadedMeshes["cube"])
	{
		Mesh* mesh = new Mesh();

		mesh->_vertices.clear();

		mesh->_vertices.push_back({ { 1.0,  1.0, -1.0}, {0.0, 0.0, -1.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
		mesh->_vertices.push_back({ {-1.0,  1.0, -1.0}, {0.0, 0.0, -1.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
		mesh->_vertices.push_back({ {-1.0, -1.0, -1.0}, {0.0, 0.0, -1.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
		mesh->_vertices.push_back({ { 1.0, -1.0, -1.0}, {0.0, 0.0, -1.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });

		mesh->_vertices.push_back({ { 1.0,  1.0,  1.0}, {0.0, 0.0, 1.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
		mesh->_vertices.push_back({ {-1.0,  1.0,  1.0}, {0.0, 0.0, 1.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
		mesh->_vertices.push_back({ {-1.0, -1.0,  1.0}, {0.0, 0.0, 1.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
		mesh->_vertices.push_back({ { 1.0, -1.0,  1.0}, {0.0, 0.0, 1.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
	
		mesh->_vertices.push_back({ { 1.0,  1.0,  1.0}, {0.0, 1.0, 0.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
		mesh->_vertices.push_back({ {-1.0,  1.0,  1.0}, {0.0, 1.0, 0.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
		mesh->_vertices.push_back({ {-1.0,  1.0, -1.0}, {0.0, 1.0, 0.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
		mesh->_vertices.push_back({ { 1.0,  1.0, -1.0}, {0.0, 1.0, 0.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
	
		mesh->_vertices.push_back({ { 1.0, -1.0,  1.0}, {0.0, -1.0, 0.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
		mesh->_vertices.push_back({ {-1.0, -1.0,  1.0}, {0.0, -1.0, 0.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
		mesh->_vertices.push_back({ {-1.0, -1.0, -1.0}, {0.0, -1.0, 0.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
		mesh->_vertices.push_back({ { 1.0, -1.0, -1.0}, {0.0, -1.0, 0.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
	
		mesh->_vertices.push_back({ {-1.0,  1.0,  1.0}, {-1.0, 0.0, 0.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
		mesh->_vertices.push_back({ {-1.0, -1.0,  1.0}, {-1.0, 0.0, 0.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
		mesh->_vertices.push_back({ {-1.0, -1.0, -1.0}, {-1.0, 0.0, 0.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
		mesh->_vertices.push_back({ {-1.0,  1.0, -1.0}, {-1.0, 0.0, 0.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
	
		mesh->_vertices.push_back({ {1.0,  1.0,  1.0}, {1.0, 0.0, 0.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
		mesh->_vertices.push_back({ {1.0, -1.0,  1.0}, {1.0, 0.0, 0.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
		mesh->_vertices.push_back({ {1.0, -1.0, -1.0}, {1.0, 0.0, 0.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
		mesh->_vertices.push_back({ {1.0,  1.0, -1.0}, {1.0, 0.0, 0.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });

		mesh->_indices = {
			0, 1, 2, 2, 3, 0,
			4, 5, 6, 6, 7, 4,
			8, 9, 10, 10, 11, 8,
			12, 13, 14, 14, 15, 12,
			16, 17, 18, 18, 19, 16,
			20, 21, 22, 22, 23, 20 
		};

		// upload mesh
		mesh->upload();
		_loadedMeshes["cube"] = mesh;

		return mesh;
	}
	
	return _loadedMeshes["cube"];
}

void Mesh::create_vertex_buffer()
{
	const size_t bufferSize = _vertices.size() * sizeof(Vertex);

	VkBufferCreateInfo stagingBufferInfo = vkinit::buffer_create_info(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

	VmaAllocationCreateInfo vmaAllocInfo = {};
	vmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

	AllocatedBuffer stagingBuffer;

	VK_CHECK(vmaCreateBuffer(VulkanEngine::engine->_allocator, 
		&stagingBufferInfo, &vmaAllocInfo, 
		&stagingBuffer._buffer, 
		&stagingBuffer._allocation, 
		nullptr));

	// Copy Vertex data
	void* data;
	vmaMapMemory(VulkanEngine::engine->_allocator, stagingBuffer._allocation, &data);
	memcpy(data, _vertices.data(), _vertices.size() * sizeof(Vertex));
	vmaUnmapMemory(VulkanEngine::engine->_allocator, stagingBuffer._allocation);

	VkBufferCreateInfo vertexBufferInfo = vkinit::buffer_create_info(bufferSize,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

	vmaAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	VK_CHECK(vmaCreateBuffer(VulkanEngine::engine->_allocator, &vertexBufferInfo, &vmaAllocInfo,
		&_vertexBuffer._buffer,
		&_vertexBuffer._allocation,
		nullptr));

	// Copy vertex data
	VulkanEngine::engine->immediate_submit([=](VkCommandBuffer cmd) {
		VkBufferCopy copy;
		copy.dstOffset = 0;
		copy.srcOffset = 0;
		copy.size = bufferSize;
		vkCmdCopyBuffer(cmd, stagingBuffer._buffer, _vertexBuffer._buffer, 1, &copy);
		});

	VulkanEngine::engine->_mainDeletionQueue.push_function([=]() {
		vmaDestroyBuffer(VulkanEngine::engine->_allocator, this->_vertexBuffer._buffer, this->_vertexBuffer._allocation);
		});

	vmaDestroyBuffer(VulkanEngine::engine->_allocator, stagingBuffer._buffer, stagingBuffer._allocation);

}

void Mesh::create_index_buffer()
{
	const size_t bufferSize = _indices.size() * sizeof(uint32_t);
	VkBufferCreateInfo stagingBufferInfo = vkinit::buffer_create_info(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

	VmaAllocationCreateInfo vmaAllocInfo = {};
	vmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

	AllocatedBuffer stagingBuffer;

	VK_CHECK(vmaCreateBuffer(VulkanEngine::engine->_allocator, &stagingBufferInfo, &vmaAllocInfo,
		&stagingBuffer._buffer,
		&stagingBuffer._allocation,
		nullptr));

	void* data;
	vmaMapMemory(VulkanEngine::engine->_allocator, stagingBuffer._allocation, &data);
	memcpy(data, _indices.data(), _indices.size() * sizeof(uint32_t));
	vmaUnmapMemory(VulkanEngine::engine->_allocator, stagingBuffer._allocation);

	vmaAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	VkBufferCreateInfo indexBufferInfo = vkinit::buffer_create_info(bufferSize,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

	VK_CHECK(vmaCreateBuffer(VulkanEngine::engine->_allocator, &indexBufferInfo, &vmaAllocInfo,
		&_indexBuffer._buffer,
		&_indexBuffer._allocation,
		nullptr));

	// Copy index data
	VulkanEngine::engine->immediate_submit([=](VkCommandBuffer cmd) {
		VkBufferCopy copy;
		copy.dstOffset = 0;
		copy.srcOffset = 0;
		copy.size = bufferSize;
		vkCmdCopyBuffer(cmd, stagingBuffer._buffer, _indexBuffer._buffer, 1, &copy);
		});

	VulkanEngine::engine->_mainDeletionQueue.push_function([=]() {
		vmaDestroyBuffer(VulkanEngine::engine->_allocator, this->_indexBuffer._buffer, this->_indexBuffer._allocation);
		});
	
	vmaDestroyBuffer(VulkanEngine::engine->_allocator, stagingBuffer._buffer, stagingBuffer._allocation);
}

void Mesh::upload()
{
	VulkanEngine::engine->create_vertex_buffer(*this);
	VulkanEngine::engine->create_index_buffer(*this);
}

BlasInput Mesh::mesh_to_geometry()
{
	VkDeviceOrHostAddressConstKHR vertexBufferDeviceAddress{};
	VkDeviceOrHostAddressConstKHR indexBufferDeviceAddress{};

	vertexBufferDeviceAddress.deviceAddress = VulkanEngine::engine->getBufferDeviceAddress(_vertexBuffer._buffer);
	indexBufferDeviceAddress.deviceAddress	= VulkanEngine::engine->getBufferDeviceAddress(_indexBuffer._buffer);

	const uint32_t nTriangles = _indices.size() / 3;

	// Set the triangles geometry
	VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
	triangles.sType			= VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
	triangles.pNext			= nullptr;
	triangles.vertexFormat	= VK_FORMAT_R32G32B32_SFLOAT;
	triangles.vertexData	= vertexBufferDeviceAddress;
	triangles.vertexStride	= sizeof(Vertex);
	triangles.maxVertex		= static_cast<uint32_t>(_vertices.size());
	triangles.indexData		= indexBufferDeviceAddress;
	triangles.indexType		= VK_INDEX_TYPE_UINT32;

	VkAccelerationStructureGeometryKHR asGeometry = vkinit::acceleration_structure_geometry_khr();
	asGeometry.flags				= VK_GEOMETRY_OPAQUE_BIT_KHR;
	asGeometry.geometryType			= VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	asGeometry.geometry.triangles	= triangles;

	VkAccelerationStructureBuildRangeInfoKHR asBuildRangeInfo{};
	asBuildRangeInfo.primitiveCount		= nTriangles;
	asBuildRangeInfo.primitiveOffset	= 0;
	asBuildRangeInfo.firstVertex		= 0;
	asBuildRangeInfo.transformOffset	= 0;

	// Store all info in the BlasInput structure to be returned
	BlasInput input;
	input.asBuildRangeInfo	= asBuildRangeInfo;
	input.asGeometry		= asGeometry;
	input.nTriangles		= nTriangles;

	return input;
}

void Node::addChild(Node* child)
{
	assert(child->_parent == NULL);
	_children.push_back(child);
	child->_parent = this;
}

glm::mat4 Node::getGlobalMatrix(bool fast)
{
	if (_parent)
		_global_matrix = (fast ? _parent->_matrix : _parent->getGlobalMatrix()) * _matrix;
	else
		_global_matrix = _matrix;
	return _global_matrix;
}

std::vector<BlasInput> Node::node_to_geometry(
	const VkDeviceOrHostAddressConstKHR vertexBufferDeviceAddress,
	const VkDeviceOrHostAddressConstKHR indexBufferDeviceAddress)
{
	if (!_primitives.empty())
	{
		std::vector<BlasInput> blasVector;
		blasVector.reserve(_primitives.size());
		for (Primitive& p : _primitives)
		{
			const uint32_t nTriangles = p.indexCount / 3;

			// Set the triangles geometry
			VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
			triangles.sType						= VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
			triangles.pNext						= nullptr;
			triangles.vertexFormat				= VK_FORMAT_R32G32B32_SFLOAT;
			triangles.vertexData				= vertexBufferDeviceAddress;
			triangles.vertexStride				= sizeof(uint32_t);
			triangles.maxVertex					= static_cast<uint32_t>(p.vertexCount);
			triangles.indexData					= indexBufferDeviceAddress;
			triangles.indexType					= VK_INDEX_TYPE_UINT32;

			VkAccelerationStructureGeometryKHR asGeometry = vkinit::acceleration_structure_geometry_khr();
			asGeometry.flags					= VK_GEOMETRY_OPAQUE_BIT_KHR;
			asGeometry.geometryType				= VK_GEOMETRY_TYPE_TRIANGLES_KHR;
			asGeometry.geometry.triangles		= triangles;

			VkAccelerationStructureBuildRangeInfoKHR asBuildRangeInfo{};
			asBuildRangeInfo.firstVertex		= p.vertexOffset;
			asBuildRangeInfo.primitiveCount		= nTriangles;
			asBuildRangeInfo.primitiveOffset	= p.firstIndex * sizeof(uint32_t);
			asBuildRangeInfo.transformOffset	= 0;

			// Store all info in the BlasInput structure to be returned
			BlasInput input;
			input.asBuildRangeInfo	= asBuildRangeInfo;
			input.asGeometry		= asGeometry;
			input.nTriangles		= nTriangles;

			blasVector.emplace_back(input);
		}
		return blasVector;
	}
	return {};
}

std::vector<TlasInstance> Node::node_to_instance(int& instanceId, const glm::mat4 matrix)
{
	std::vector<TlasInstance> instances;
	instances.reserve(_primitives.size());
	for (Primitive& p : _primitives)
	{
		if (p.indexCount > 0)
		{
			TlasInstance instance{};
			instance.transform	= matrix * getGlobalMatrix(false);
			instance.blasId		= instanceId;
			instance.instanceId = instanceId;
			instances.emplace_back(instance);
			instanceId++;
		}
	}
	return instances;
}


void Prefab::loadNode(	const tinygltf::Node& inputNode,
						const tinygltf::Model& input,
						Node* parent,
						std::vector<uint32_t>& indexBuffer,
						std::vector<Vertex>& vertexBuffer)
{
	Node* node = new Node();
	node->_matrix = glm::mat4(1.0f);

	if (inputNode.translation.size() == 3)
		node->_matrix =	glm::translate(node->_matrix, glm::vec3(glm::make_vec3(inputNode.translation.data())));
	if (inputNode.rotation.size() == 4)
	{
		glm::quat q = glm::make_quat(inputNode.rotation.data());
		node->_matrix *= glm::mat4(q);
	}
	if (inputNode.scale.size() == 3) 
	{
		node->_matrix = glm::scale(node->_matrix, glm::vec3(glm::make_vec3(inputNode.scale.data())));
	}
	if (inputNode.matrix.size() == 16) 
	{
		node->_matrix = glm::make_mat4x4(inputNode.matrix.data());
	}

	if (inputNode.children.size() > 0)
	{
		node->_children.reserve(inputNode.children.size());
		for (size_t i = 0; i < inputNode.children.size(); i++)
			loadNode(input.nodes[inputNode.children[i]], input, node, indexBuffer, vertexBuffer);
	}

	if (inputNode.mesh > -1)
	{
		const tinygltf::Mesh mesh = input.meshes[inputNode.mesh];
		for (size_t i = 0; i < mesh.primitives.size(); i++)
		{
			const tinygltf::Primitive& glTFPrimitive	= mesh.primitives[i];
			uint32_t                   firstIndex		= static_cast<uint32_t>(indexBuffer.size());
			uint32_t                   vertexStart		= static_cast<uint32_t>(vertexBuffer.size());
			uint32_t                   indexCount		= 0;
			uint32_t				   vCount = 0;
			// Vertices
			{
				const float* positionBuffer = nullptr;
				const float* normalsBuffer = nullptr;
				const float* texCoordsBuffer = nullptr;
				size_t       vertexCount = 0;

				// Get buffer data for vertex normals
				if (glTFPrimitive.attributes.find("POSITION") != glTFPrimitive.attributes.end())
				{
					const tinygltf::Accessor& accessor =
						input.accessors[glTFPrimitive.attributes.find("POSITION")->second];
					const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
					positionBuffer = reinterpret_cast<const float*>(
						&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
					vertexCount = accessor.count;
					vCount = vertexCount;
				}
				// Get buffer data for vertex normals
				if (glTFPrimitive.attributes.find("NORMAL") != glTFPrimitive.attributes.end())
				{
					const tinygltf::Accessor& accessor =
						input.accessors[glTFPrimitive.attributes.find("NORMAL")->second];
					const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
					normalsBuffer = reinterpret_cast<const float*>(
						&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
				}
				// Get buffer data for vertex texture coordinates
				// glTF supports multiple sets, we only load the first one
				if (glTFPrimitive.attributes.find("TEXCOORD_0") != glTFPrimitive.attributes.end())
				{
					const tinygltf::Accessor& accessor =
						input.accessors[glTFPrimitive.attributes.find("TEXCOORD_0")->second];
					const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
					texCoordsBuffer = reinterpret_cast<const float*>(
						&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
				}

				// Append data to model's vertex buffer
				for (size_t v = 0; v < vertexCount; v++)
				{
					Vertex vert{};
					vert.position	= glm::vec4(glm::make_vec3(&positionBuffer[v * 3]), 1.0f);
					vert.normal		= glm::normalize(glm::vec3(normalsBuffer ? glm::make_vec3(&normalsBuffer[v * 3]) : glm::vec3(0.0f)));
					vert.uv			= texCoordsBuffer ? glm::make_vec2(&texCoordsBuffer[v * 2]) : glm::vec3(0.0f);
					vert.color		= glm::vec3(1.0f);
					vertexBuffer.push_back(vert);
				}
			}
			// Indices
			{
				const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.indices];
				const tinygltf::BufferView& bufferView = input.bufferViews[accessor.bufferView];
				const tinygltf::Buffer& buffer = input.buffers[bufferView.buffer];

				indexCount = static_cast<uint32_t>(accessor.count);

				// glTF supports different component types of indices
				switch (accessor.componentType)
				{
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
					uint32_t* buf = new uint32_t[accessor.count];
					memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset],
						accessor.count * sizeof(uint32_t));
					for (size_t index = 0; index < accessor.count; index++)
					{
						indexBuffer.push_back(buf[index] + vertexStart);
					}
					break;
				}
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
					uint16_t* buf = new uint16_t[accessor.count];
					memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset],
						accessor.count * sizeof(uint16_t));
					for (size_t index = 0; index < accessor.count; index++)
					{
						indexBuffer.push_back(buf[index] + vertexStart);
					}
					break;
				}
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
					uint8_t* buf = new uint8_t[accessor.count];
					memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset],
						accessor.count * sizeof(uint8_t));
					for (size_t index = 0; index < accessor.count; index++)
					{
						indexBuffer.push_back(buf[index] + vertexStart);
					}
					break;
				}
				default:
					std::cerr << "Index component type " << accessor.componentType << " not supported!"
						<< std::endl;
					return;
				}
			}
			Primitive primitive{};
			primitive.firstIndex	= firstIndex;
			primitive.indexCount	= indexCount;
			primitive.vertexOffset	= vertexStart;
			primitive.vertexCount	= vCount;
			primitive.materialIndex = std::max(0, glTFPrimitive.material);
			node->_primitives.push_back(primitive);
		}
	}
	if (parent)
	{
		parent->addChild(node);
	}
	else
		_nodes.push_back(*node);
}

std::vector<BlasInput> Prefab::gltf_to_geometry()
{
	VkDeviceOrHostAddressConstKHR vertexBufferDeviceAddress{};
	VkDeviceOrHostAddressConstKHR indexBufferDeviceAddress{};

	vertexBufferDeviceAddress.deviceAddress = VulkanEngine::engine->getBufferDeviceAddress(_mesh->_vertexBuffer._buffer);
	indexBufferDeviceAddress.deviceAddress = VulkanEngine::engine->getBufferDeviceAddress(_mesh->_indexBuffer._buffer);

	std::vector<BlasInput> input_list;

	for (Node& n : _nodes)
	{
		std::vector<BlasInput> node_result = n.node_to_geometry(vertexBufferDeviceAddress, indexBufferDeviceAddress);
		input_list.insert(input_list.end(), node_result.begin(), node_result.end());
		if (!n._children.empty()) {
			for(Node* child : n._children)
			{
				std::vector<BlasInput> child_result = child->node_to_geometry(vertexBufferDeviceAddress, indexBufferDeviceAddress);
				input_list.insert(input_list.end(), child_result.begin(), child_result.end());
			}
		}

	}
	return input_list;
}

void Prefab::drawNode(VkCommandBuffer& cmd, VkPipelineLayout pipelineLayout, Node node, glm::mat4& model)
{
	if (node._primitives.size() > 0)
	{
		material_matrix push_constant{
			node.getGlobalMatrix(true) * model,
			node._materialId
		};

		glm::mat4 node_matrix = model * node.getGlobalMatrix(false);
		
		for (Primitive& primitive : node._primitives)
		{
			if (primitive.materialIndex < 0)
				primitive.materialIndex = 0;
			if (primitive.indexCount > 0) {
				vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &node_matrix);
				vkCmdDrawIndexed(cmd, primitive.indexCount, 1, primitive.firstIndex, 0, 0);
			}
		}
	}

	for(auto& child : node._children)
		drawNode(cmd, pipelineLayout, *child, model);
}

void Prefab::draw(VkCommandBuffer& cmd, VkPipelineLayout pipelineLayout, glm::mat4& model)
{
	VkDeviceSize offset{ 0 };
	vkCmdBindVertexBuffers(cmd, 0, 1, &_mesh->_vertexBuffer._buffer, &offset);
	vkCmdBindIndexBuffer(cmd, _mesh->_indexBuffer._buffer, offset, VK_INDEX_TYPE_UINT32);

	for (Node& node : _nodes)
	{
		drawNode(cmd, pipelineLayout, node, model);
	}
}

Prefab* Prefab::GET(const std::string filename)
{
	std::cout << "Loading gltf... " << filename << std::endl;

	tinygltf::Model		gltfModel;
	tinygltf::TinyGLTF	gltfContext;
	std::string			warn, err;
	bool				fileLoaded{ false };

	fileLoaded = gltfContext.LoadASCIIFromFile(&gltfModel, &err, &warn, VulkanEngine::engine->findFile(filename, searchPaths, true));

	if (!err.empty())
		throw std::runtime_error(err.c_str());

	if (fileLoaded)
	{
		Prefab* model = new Prefab();
		// TODO: import materials
		const tinygltf::Scene& scene = gltfModel.scenes[0];
		for (size_t i = 0; i < scene.nodes.size(); i++)
		{
			const tinygltf::Node node = gltfModel.nodes[scene.nodes[i]];
			model->loadNode(node, gltfModel, nullptr, model->_mesh->_indices, model->_mesh->_vertices);
		}

		model->_mesh->upload();

		return model;
	}
}

