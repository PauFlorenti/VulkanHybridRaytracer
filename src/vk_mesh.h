#pragma once

#include <vk_types.h>
#include <vector>
#include <glm/glm/vec3.hpp>
#include <glm/glm/mat4x4.hpp>
#include <glm/glm/gtc/matrix_transform.hpp>

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

	void get_quad();
	void get_triangle();
	void get_cube();
};

enum Camera_Movement {
	FORWARD,
	BACKWARD,
	LEFT,
	RIGHT,
	UP,
	DOWN
};

const float YAW		= -90.0f;
const float PITCH	= 0.0f;
const float SPEED	= 0.01;
const float SENSITIVITY = 0.1f;

class Camera
{
public:

	Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3 up = glm::vec3(0.0, 1.0f, 0.0f), float yaw = YAW, float pitch = PITCH);

	glm::vec3 _position;
	glm::vec3 _direction;
	glm::vec3 _up;
	glm::vec3 _right;

	float _yaw;
	float _pitch;
	float _speed;
	float _sensitivity;

	void processKeyboard(Camera_Movement direction, const float dt);
	void rotate(float xoffset, float yoffset, bool constrainPitch = true);

	glm::mat4 getView();
	glm::mat4 getProjection();

private:
	void updateCameraVectors();
};
