#pragma once

#include "vk_types.h"

namespace vkutil
{
	template <typename T>
	int getIndex(std::vector<T> v, T k);

	template <typename T>
	bool existsInVector(std::vector<T> v, T k);

	std::string findFile(const std::string& filename, const std::vector<std::string>& directories, bool warn);
	//void create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, AllocatedBuffer& buffer, bool destroy = true);
	//void create_attachment(VkFormat format, VkImageUsageFlagBits usage, Texture* texture);
}