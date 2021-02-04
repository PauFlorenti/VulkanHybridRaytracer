#pragma once

#include <vulkan/vulkan.h>

#include <functional>
#include <deque>
#include <array>
#include <vector>
#include <unordered_map>
#include <map>
#include <iostream>
#include <fstream>

#include <vma/vk_mem_alloc.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm/glm.hpp>
#include <glm/glm/gtx/hash.hpp>
#include <glm/glm/vec3.hpp>
#include <glm/glm/mat4x4.hpp>
#include <glm/glm/gtc/type_ptr.hpp>
#include <glm/glm/gtc/matrix_transform.hpp>

struct AllocatedBuffer {
	VkBuffer		_buffer = VK_NULL_HANDLE;
	VmaAllocation	_allocation;
};

struct AllocatedImage {
	VkImage			_image;
	VmaAllocation	_allocation;
};