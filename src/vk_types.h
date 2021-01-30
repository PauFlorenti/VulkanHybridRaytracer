#pragma once

//#define VK_ENABLE_BETA_EXTENSIONS
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

struct AllocatedBuffer {
	VkBuffer		_buffer = VK_NULL_HANDLE;
	VmaAllocation	_allocation;
};

struct AllocatedImage {
	VkImage			_image;
	VmaAllocation	_allocation;
};