#pragma once

#include <vk_types.h>

class VulkanEngine;

namespace vkutil {

	bool load_image_from_file(VulkanEngine& engine, const char* filename, AllocatedImage& outImage);

	//bool load_cubemap(VulkanEngine& engine, const char* filename, VkFormat format, AllocatedImage& outImage);

}