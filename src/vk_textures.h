#pragma once

#include <vk_types.h>
//#include "vk_utils.h"

class VulkanEngine;

struct Texture {
	AllocatedImage  image;
	VkImageView		imageView;

	static std::vector<std::pair<std::string, Texture*>> _textures;
	static Texture* GET(const char* filename, const bool cubemap = false);
	static int get_id(const char* filename);
};

namespace vkutil {

	bool load_image_from_file(VulkanEngine& engine, const char* filename, AllocatedImage& outImage);

	bool load_cubemap(VulkanEngine& engine, const char* filename, VkFormat format, AllocatedImage& outImage);
}