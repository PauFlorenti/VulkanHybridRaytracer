#include <vk_textures.h>

#include <vk_initializers.h>
#include "vk_engine.h"

#include "stb_image.h"

bool vkutil::load_image_from_file(VulkanEngine& engine, const char* filename, AllocatedImage& outImage)
{
	int texWidth, textHeight, texChannels;

	stbi_uc* pixels = stbi_load(filename, &texWidth, &textHeight, &texChannels, STBI_rgb_alpha);

	if (!pixels) {
		std::cout << "Failed to load texture file: " << filename << std::endl;
		return false;
	}

	void* pixels_ptr = pixels;
	VkDeviceSize imageSize = texWidth * textHeight * 4;

	// The format R8G8B8A8 match exactly with the pixels loaded from stbi_load lib
	VkFormat image_format = VK_FORMAT_R8G8B8A8_UNORM;

	// Allocate temporary buffer for holding texture data to upload
	AllocatedBuffer stagingBuffer;
	engine.create_buffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, stagingBuffer);

	void* data;
	vmaMapMemory(engine._allocator, stagingBuffer._allocation, &data);
	memcpy(data, pixels_ptr, static_cast<size_t>(imageSize));
	vmaUnmapMemory(engine._allocator, stagingBuffer._allocation);

	stbi_image_free(pixels);

	VkExtent3D imageExtent;
	imageExtent.width = static_cast<uint32_t>(texWidth);
	imageExtent.height = static_cast<uint32_t>(textHeight);
	imageExtent.depth = 1;

	VkImageCreateInfo dimb_info = vkinit::image_create_info(
		image_format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, imageExtent);

	AllocatedImage newImage;

	VmaAllocationCreateInfo dimg_allocinfo = {};
	dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	vmaCreateImage(engine._allocator, &dimb_info, &dimg_allocinfo, &newImage._image, &newImage._allocation, nullptr);

	engine.immediate_submit([&](VkCommandBuffer cmd) {
		VkImageSubresourceRange range;
		range.aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT;
		range.baseMipLevel		= 0;
		range.levelCount		= 1;
		range.baseArrayLayer	= 0;
		range.layerCount		= 1;

		VkImageMemoryBarrier imageBarrier_toTransfer = {};
		imageBarrier_toTransfer.sType				= VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageBarrier_toTransfer.pNext				= nullptr;
		imageBarrier_toTransfer.oldLayout			= VK_IMAGE_LAYOUT_UNDEFINED;
		imageBarrier_toTransfer.newLayout			= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imageBarrier_toTransfer.image				= newImage._image;
		imageBarrier_toTransfer.subresourceRange	= range;
		imageBarrier_toTransfer.srcAccessMask		= 0;
		imageBarrier_toTransfer.dstAccessMask		= VK_ACCESS_TRANSFER_WRITE_BIT;

		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			0, 0, nullptr, 0, nullptr, 1, &imageBarrier_toTransfer);

		VkBufferImageCopy copyRegion = {};
		copyRegion.bufferOffset						= 0;
		copyRegion.bufferRowLength					= 0;
		copyRegion.bufferImageHeight				= 0;
		copyRegion.imageSubresource.aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.imageSubresource.mipLevel		= 0;
		copyRegion.imageSubresource.baseArrayLayer	= 0;
		copyRegion.imageSubresource.layerCount		= 1;
		copyRegion.imageExtent						= imageExtent;

		vkCmdCopyBufferToImage(cmd, stagingBuffer._buffer, newImage._image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

		VkImageMemoryBarrier imageBarrier_toReadable = imageBarrier_toTransfer;
		imageBarrier_toReadable.oldLayout		= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imageBarrier_toReadable.newLayout		= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageBarrier_toReadable.srcAccessMask	= VK_ACCESS_TRANSFER_WRITE_BIT;
		imageBarrier_toReadable.dstAccessMask	= VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier_toReadable);

	});

	engine._mainDeletionQueue.push_function([=]() {
		vmaDestroyImage(engine._allocator, newImage._image, newImage._allocation);
	});

	//vmaDestroyBuffer(engine._allocator, stagingBuffer._buffer, stagingBuffer._allocation);

	std::cout << "Texture loaded successfully " << filename << std::endl;

	outImage = newImage;
	return true;
}

/*
bool vkutil::load_cubemap(VulkanEngine& engine, const char* filename, const VkFormat format, AllocatedImage& outImage)
{
	int texWidth, textHeight, texChannels;
	stbi_uc* pixels = stbi_load(filename, &texWidth, &textHeight, &texChannels, STBI_rgb_alpha);

	if (!pixels) {
		std::cout << "Failed to load texture file: " << filename << std::endl;
		return false;
	}

	void* pixels_ptr = pixels;
	VkDeviceSize imageSize = texWidth * textHeight * 4;

	AllocatedBuffer stagingBuffer;
	engine.create_buffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, stagingBuffer);

	void* data;
	vmaMapMemory(engine._allocator, stagingBuffer._allocation, &data);
	memcpy(data, pixels_ptr, static_cast<size_t>(imageSize));
	vmaUnmapMemory(engine._allocator, stagingBuffer._allocation);

	stbi_image_free(pixels);

	VkExtent3D imageExtent;
	imageExtent.width = static_cast<uint32_t>(texWidth);
	imageExtent.height = static_cast<uint32_t>(textHeight);
	imageExtent.depth = 1;

	VkImageCreateInfo dimb_info = vkinit::image_create_info(
		format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, imageExtent, 6, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);

	AllocatedImage newImage;

	VmaAllocationCreateInfo dimg_allocinfo = {};
	dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	vmaCreateImage(engine._allocator, &dimb_info, &dimg_allocinfo, &newImage._image, &newImage._allocation, nullptr);

	std::vector<VkBufferImageCopy> bufferCopyRegions;
	uint32_t offset = 0;

	for (uint32_t face = 0; face < 6; face++)
	{

	}

	engine.immediate_submit([&](VkCommandBuffer cmd) {
		VkImageSubresourceRange range;
		range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		range.baseMipLevel = 0;
		range.levelCount = 1;
		range.baseArrayLayer = 0;
		range.layerCount = 1;

		VkImageMemoryBarrier imageBarrier_toTransfer = {};
		imageBarrier_toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageBarrier_toTransfer.pNext = nullptr;
		imageBarrier_toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageBarrier_toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imageBarrier_toTransfer.image = newImage._image;
		imageBarrier_toTransfer.subresourceRange = range;
		imageBarrier_toTransfer.srcAccessMask = 0;
		imageBarrier_toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			0, 0, nullptr, 0, nullptr, 1, &imageBarrier_toTransfer);

		VkBufferImageCopy copyRegion = {};
		copyRegion.bufferOffset = 0;
		copyRegion.bufferRowLength = 0;
		copyRegion.bufferImageHeight = 0;
		copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.imageSubresource.mipLevel = 0;
		copyRegion.imageSubresource.baseArrayLayer = 0;
		copyRegion.imageSubresource.layerCount = 1;
		copyRegion.imageExtent = imageExtent;

		vkCmdCopyBufferToImage(cmd, stagingBuffer._buffer, newImage._image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

		VkImageMemoryBarrier imageBarrier_toReadable = imageBarrier_toTransfer;
		imageBarrier_toReadable.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imageBarrier_toReadable.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageBarrier_toReadable.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		imageBarrier_toReadable.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier_toReadable);

		});

	engine._mainDeletionQueue.push_function([=]() {
		vmaDestroyImage(engine._allocator, newImage._image, newImage._allocation);
		});


}
*/