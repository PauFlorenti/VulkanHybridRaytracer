#pragma once

#include <vk_types.h>

namespace vkinit {

	VkCommandPoolCreateInfo command_pool_create_info(	
		uint32_t queueFamilyIndex, 
		VkCommandPoolCreateFlags flags = 0);

	VkCommandBufferAllocateInfo command_buffer_allocate_info(
		VkCommandPool cmdPool, 
		uint32_t count = 1, 
		VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_info(
		VkShaderStageFlagBits stage,
		VkShaderModule shaderModule);

	VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info();

	VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info(
		VkPrimitiveTopology topology);

	VkPipelineRasterizationStateCreateInfo rasterization_state_create_info(
		VkPolygonMode polygonMode);

	VkPipelineMultisampleStateCreateInfo multisample_state_create_info();

	VkPipelineColorBlendAttachmentState color_blend_attachment_state(VkColorComponentFlags colorWriteMask, VkBool32 blendEnable);

	VkPipelineLayoutCreateInfo pipeline_layout_create_info();

	VkImageCreateInfo image_create_info(
		VkFormat format, 
		VkImageUsageFlags usageFlags, 
		VkExtent3D extent);

	VkImageViewCreateInfo image_view_create_info(
		VkFormat format,
		VkImage image,
		VkImageAspectFlags aspectFlags);

	VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info(
		bool bDepthTest,
		bool bDepthWrite,
		VkCompareOp compareOp);

	VkFenceCreateInfo fence_create_info(VkFenceCreateFlags flags = 0);

	VkSemaphoreCreateInfo semaphore_create_info(VkSemaphoreCreateFlags flags = 0);

	VkFramebufferCreateInfo framebuffer_create_info(VkRenderPass renderPass, VkExtent2D extent);

	VkDescriptorSetLayoutBinding descriptorset_layout_binding(
		VkDescriptorType type, 
		VkShaderStageFlags stageFlags,
		uint32_t binding,
		uint32_t count = 1);

	VkWriteDescriptorSet write_descriptor_buffer(
		VkDescriptorType type, 
		VkDescriptorSet dstSet, 
		VkDescriptorBufferInfo* bufferInfo, 
		uint32_t binding);

	VkCommandBufferBeginInfo command_buffer_begin_info(VkCommandBufferUsageFlags usageFlags);

	VkSubmitInfo submit_info(VkCommandBuffer* cmd);

	VkSamplerCreateInfo sampler_create_info(VkFilter filters, VkSamplerAddressMode samplerAddressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT);

	VkWriteDescriptorSet write_descriptor_image(VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorImageInfo* imageInfo, uint32_t binding, uint32_t count = 1);

	VkBufferCreateInfo buffer_create_info(size_t bufferSize, VkBufferUsageFlags usage);

	VkDescriptorPoolCreateInfo descriptor_pool_create_info(uint32_t count, VkDescriptorPoolSize* pPoolSizes, uint32_t maxSets);

	VkDescriptorImageInfo descriptor_image_create_info(VkSampler sampler, VkImageView imageview, VkImageLayout layout);

	VkPipelineColorBlendStateCreateInfo color_blend_state_create_info(uint32_t attachmentCount, VkPipelineColorBlendAttachmentState* pAttachments);

}