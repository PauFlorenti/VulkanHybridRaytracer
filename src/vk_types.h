#pragma once

#define VK_ENABLE_BETA_EXTENSIONS
#include <vulkan/vulkan.hpp>

#include <functional>
#include <deque>

#include <vma/vk_mem_alloc.h>

struct AllocatedBuffer {
	VkBuffer		_buffer;
	VmaAllocation	_allocation;
};

struct AllocatedImage {
	VkImage			_image;
	VmaAllocation	_allocation;
};