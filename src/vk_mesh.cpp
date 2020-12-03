
#include "vk_mesh.h"
#include <iostream>
#include <unordered_map>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

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

	return true;
}

Mesh* Mesh::get_quad()
{
	Mesh* mesh = new Mesh();
	
	//_vertices.clear();

	//_vertices.push_back({ {  1.0f,  1.0f, 0.0f }, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f} });
	//_vertices.push_back({ { -1.0f,  1.0f, 0.0f }, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f} });
	//_vertices.push_back({ { -1.0f, -1.0f, 0.0f }, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} });
	//_vertices.push_back({ {  1.0f, -1.0f, 0.0f }, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f} });

	//_indices = {0, 1, 2, 2, 3, 0};

	mesh->_vertices.push_back({ {  1.0f,  1.0f, 0.0f }, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f} });
	mesh->_vertices.push_back({ { -1.0f,  1.0f, 0.0f }, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f} });
	mesh->_vertices.push_back({ { -1.0f, -1.0f, 0.0f }, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} });
	mesh->_vertices.push_back({ {  1.0f, -1.0f, 0.0f }, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f} });

	mesh->_indices = {0, 1, 2, 2, 3, 0};

	return mesh;
}

void Mesh::get_triangle()
{
	_vertices.clear();

	_vertices.push_back({ {  1.f,  1.f, 0.f }, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f} });
	_vertices.push_back({ { -1.f,  1.f, 0.f }, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f} });
	_vertices.push_back({ {  0.f, -1.f, 0.f }, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f} });

	_indices = { 0, 1, 2 };
}

void Mesh::get_cube()
{
	_vertices.clear();

	_vertices.push_back({ { 1.0,  1.0, -1.0}, {0.0, 0.0, -1.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
	_vertices.push_back({ {-1.0,  1.0, -1.0}, {0.0, 0.0, -1.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
	_vertices.push_back({ {-1.0, -1.0, -1.0}, {0.0, 0.0, -1.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
	_vertices.push_back({ { 1.0, -1.0, -1.0}, {0.0, 0.0, -1.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });

	_vertices.push_back({ { 1.0,  1.0,  1.0}, {0.0, 0.0, 1.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
	_vertices.push_back({ {-1.0,  1.0,  1.0}, {0.0, 0.0, 1.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
	_vertices.push_back({ {-1.0, -1.0,  1.0}, {0.0, 0.0, 1.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
	_vertices.push_back({ { 1.0, -1.0,  1.0}, {0.0, 0.0, 1.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
	
	_vertices.push_back({ { 1.0,  1.0,  1.0}, {0.0, 1.0, 0.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
	_vertices.push_back({ {-1.0,  1.0,  1.0}, {0.0, 1.0, 0.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
	_vertices.push_back({ {-1.0,  1.0, -1.0}, {0.0, 1.0, 0.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
	_vertices.push_back({ { 1.0,  1.0, -1.0}, {0.0, 1.0, 0.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
	
	_vertices.push_back({ { 1.0, -1.0,  1.0}, {0.0, -1.0, 0.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
	_vertices.push_back({ {-1.0, -1.0,  1.0}, {0.0, -1.0, 0.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
	_vertices.push_back({ {-1.0, -1.0, -1.0}, {0.0, -1.0, 0.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
	_vertices.push_back({ { 1.0, -1.0, -1.0}, {0.0, -1.0, 0.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
	
	_vertices.push_back({ {-1.0,  1.0,  1.0}, {-1.0, 0.0, 0.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
	_vertices.push_back({ {-1.0, -1.0,  1.0}, {-1.0, 0.0, 0.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
	_vertices.push_back({ {-1.0, -1.0, -1.0}, {-1.0, 0.0, 0.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
	_vertices.push_back({ {-1.0,  1.0, -1.0}, {-1.0, 0.0, 0.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
	
	_vertices.push_back({ {1.0,  1.0,  1.0}, {1.0, 0.0, 0.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
	_vertices.push_back({ {1.0, -1.0,  1.0}, {1.0, 0.0, 0.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
	_vertices.push_back({ {1.0, -1.0, -1.0}, {1.0, 0.0, 0.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });
	_vertices.push_back({ {1.0,  1.0, -1.0}, {1.0, 0.0, 0.0}, {1.0, 1.0, 1.0}, {1.0, 1.0} });

	_indices = {
		0, 1, 2, 2, 3, 0,
		4, 5, 6, 6, 7, 4,
		8, 9, 10, 10, 11, 8,
		12, 13, 14, 14, 15, 12,
		16, 17, 18, 18, 19, 16,
		20, 21, 22, 22, 23, 20 
	};
}

Camera::Camera(glm::vec3 position, glm::vec3 up, float yaw, float pitch) : _direction(glm::vec3(0, 0, -1)), _speed(SPEED), _sensitivity(SENSITIVITY)
{
	_position	= position;
	_up			= up;
	_yaw		= yaw;
	_pitch		= pitch;
	updateCameraVectors();
}

void Camera::processKeyboard(Camera_Movement direction, const float dt)
{
	float movementSpeed = _speed * dt;
	if (direction == FORWARD)
		_position += _direction * movementSpeed;
	if (direction == BACKWARD)
		_position -= _direction * movementSpeed;
	if (direction == RIGHT)
		_position += _right * movementSpeed;
	if (direction == LEFT)
		_position -= _right * movementSpeed;
	if (direction == UP)
		_position += _up * movementSpeed;
	if (direction == DOWN)
		_position -= _up * movementSpeed;
}

void Camera::rotate(float xoffset, float yoffset, bool constrainPitch)
{
	xoffset *= _sensitivity;
	yoffset *= _sensitivity;

	_yaw -= xoffset;
	_pitch += yoffset;

	if (constrainPitch) {
		if (_pitch > 89.0f)
			_pitch = 89.0f;
		if (_pitch < -89.0f)
			_pitch = -89.9f;
	}

	updateCameraVectors();
}

glm::mat4 Camera::getView()
{
	return glm::lookAt(_position, _position + _direction, glm::vec3(0, 1, 0));
}

glm::mat4 Camera::getProjection()
{
	// TODO
	return glm::mat4(1);
}

void Camera::updateCameraVectors()
{
	glm::vec3 front;
	front.x = cos(glm::radians(_yaw)) * cos(glm::radians(_pitch));
	front.y = sin(glm::radians(_pitch));
	front.z = sin(glm::radians(_yaw)) * cos(glm::radians(_pitch));

	_direction = glm::normalize(front);
	_right = glm::normalize(glm::cross(_direction, glm::vec3(0, 1, 0)));
	_up = glm::normalize(glm::cross(_right, _direction));
}
