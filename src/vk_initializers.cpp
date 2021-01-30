
#include "vk_initializers.h"

VkCommandPoolCreateInfo vkinit::command_pool_create_info(
	uint32_t queueFamilyIndex, 
	VkCommandPoolCreateFlags flags /*= 0*/)
{
	VkCommandPoolCreateInfo info = {};
	info.sType				= VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	info.pNext				= nullptr;
	info.queueFamilyIndex	= queueFamilyIndex;
	info.flags				= flags;
	
	return info;
}

VkCommandBufferAllocateInfo vkinit::command_buffer_allocate_info(
	VkCommandPool cmdPool,
	uint32_t count /*= 1*/ ,
	VkCommandBufferLevel level /* VK_COMMAND_BUFFER_LEVEL_PRIMARY*/ )
{
	VkCommandBufferAllocateInfo info = {};
	info.sType				= VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	info.pNext				= nullptr;
	info.commandPool		= cmdPool;
	info.commandBufferCount = count;
	info.level				= level;

	return info;
}

VkPipelineShaderStageCreateInfo vkinit::pipeline_shader_stage_create_info(
	VkShaderStageFlagBits stage,
	VkShaderModule shaderModule
)
{
	VkPipelineShaderStageCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	info.pNext = nullptr;
	info.stage = stage;
	info.module = shaderModule;
	info.pName = "main";
	
	return info;
}

VkPipelineVertexInputStateCreateInfo vkinit::vertex_input_state_create_info()
{
	VkPipelineVertexInputStateCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	info.pNext = nullptr;

	info.vertexBindingDescriptionCount		= 0;
	info.vertexAttributeDescriptionCount	= 0;
	
	return info;
}

VkPipelineInputAssemblyStateCreateInfo vkinit::input_assembly_create_info(VkPrimitiveTopology topology)
{
	VkPipelineInputAssemblyStateCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	info.pNext = nullptr;
	info.topology = topology;
	info.primitiveRestartEnable = VK_FALSE;

	return info;
}

VkPipelineRasterizationStateCreateInfo vkinit::rasterization_state_create_info(VkPolygonMode polygonMode)
{
	VkPipelineRasterizationStateCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	info.pNext = nullptr;

	info.depthClampEnable			= VK_FALSE;
	info.rasterizerDiscardEnable	= VK_FALSE;

	info.polygonMode = polygonMode;
	info.lineWidth	= 1.0f;
	info.cullMode	= VK_CULL_MODE_NONE;
	info.frontFace	= VK_FRONT_FACE_CLOCKWISE;

	info.depthBiasEnable			= VK_FALSE;
	info.depthBiasConstantFactor	= 0.0f;
	info.depthBiasClamp				= 0.0f;
	info.depthBiasSlopeFactor		= 0.0f;

	return info;
}

VkPipelineMultisampleStateCreateInfo vkinit::multisample_state_create_info()
{
	VkPipelineMultisampleStateCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	info.pNext = nullptr;

	info.sampleShadingEnable	= VK_FALSE;
	info.rasterizationSamples	= VK_SAMPLE_COUNT_1_BIT;
	info.minSampleShading		= 1.0f;
	info.pSampleMask			= nullptr;
	info.alphaToCoverageEnable	= VK_FALSE;
	info.alphaToOneEnable		= VK_FALSE;

	return info;
}

VkPipelineColorBlendAttachmentState vkinit::color_blend_attachment_state(
	VkColorComponentFlags colorWriteMask, VkBool32 blendEnable)
{
	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = colorWriteMask;
	colorBlendAttachment.blendEnable = blendEnable;

	return colorBlendAttachment;
}

VkPipelineLayoutCreateInfo vkinit::pipeline_layout_create_info()
{
	VkPipelineLayoutCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	info.pNext = nullptr;

	info.flags					= 0;
	info.setLayoutCount			= 0;
	info.pSetLayouts			= nullptr;
	info.pushConstantRangeCount = 0;
	info.pPushConstantRanges	= nullptr;

	return info;
}

VkImageCreateInfo vkinit::image_create_info(
	VkFormat format,
	VkImageUsageFlags usageFlags,
	VkExtent3D extent,
	uint32_t arrayLayers /*= 1*/,
	VkImageCreateFlags flags /*= 0*/)
{
	VkImageCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	info.pNext = nullptr;

	info.imageType = VK_IMAGE_TYPE_2D;

	info.format			= format;
	info.extent			= extent;
	info.usage			= usageFlags;
	info.mipLevels		= 1;
	info.arrayLayers	= arrayLayers;
	info.samples		= VK_SAMPLE_COUNT_1_BIT;
	info.tiling			= VK_IMAGE_TILING_OPTIMAL;
	info.flags			= flags;
	
	return info;
}

VkImageViewCreateInfo vkinit::image_view_create_info(
	VkFormat format, 
	VkImage image, 
	VkImageAspectFlags aspectFlags)
{
	VkImageViewCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	info.pNext = nullptr;

	info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	info.image							 = image;
	info.format							 = format;
	info.subresourceRange.baseMipLevel	 = 0;
	info.subresourceRange.levelCount	 = 1;
	info.subresourceRange.baseArrayLayer = 0;
	info.subresourceRange.layerCount	 = 1;
	info.subresourceRange.aspectMask	 = aspectFlags;

	return info;
}

VkFenceCreateInfo vkinit::fence_create_info(VkFenceCreateFlags flags /*=0*/)
{
	VkFenceCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	info.pNext = nullptr;
	info.flags = flags;

	return info;
}

VkSemaphoreCreateInfo vkinit::semaphore_create_info(VkSemaphoreCreateFlags flags /*=0*/)
{
	VkSemaphoreCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	info.pNext = nullptr;
	info.flags = flags;

	return info;
}

VkFramebufferCreateInfo vkinit::framebuffer_create_info(VkRenderPass renderPass, VkExtent2D extent)
{
	VkFramebufferCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	info.pNext = nullptr;

	info.renderPass			= renderPass;
	info.attachmentCount	= 1;
	info.width				= extent.width;
	info.height				= extent.height;
	info.layers				= 1;

	return info;
}

VkDescriptorSetLayoutBinding vkinit::descriptorset_layout_binding(
	VkDescriptorType type,
	VkShaderStageFlags stageFlags,
	uint32_t binding,
	uint32_t count /*= 1*/) 
{
	VkDescriptorSetLayoutBinding setBinding = {};
	setBinding.stageFlags			= stageFlags;
	setBinding.binding				= binding;
	setBinding.descriptorType		= type;
	setBinding.descriptorCount		= count;	// 1 by default
	setBinding.pImmutableSamplers   = nullptr;

	return setBinding;
}

VkDescriptorSetLayoutCreateInfo vkinit::descriptor_set_layout_create_info(
	uint32_t bindingCount, // = 0
	const std::vector<VkDescriptorSetLayoutBinding>& bindings, // = empty vector
	VkDescriptorSetLayoutCreateFlags flags)	// = 0
{
	VkDescriptorSetLayoutCreateInfo info = {};
	info.sType			= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	info.pNext			= nullptr;
	info.bindingCount	= bindingCount;
	info.pBindings		= bindings.data();
	info.flags			= flags;
	return info;
}

VkDescriptorBufferInfo vkinit::descriptor_buffer_info(
	const VkBuffer& buffer,		// = nullptr
	const VkDeviceSize range,	// = 0
	const VkDeviceSize offset)	// = 0
{
	VkDescriptorBufferInfo info = {};
	info.buffer = buffer;
	info.range	= range;
	info.offset = offset;
	return info;
}

VkWriteDescriptorSet vkinit::write_descriptor_buffer(
	VkDescriptorType type,
	VkDescriptorSet dstSet,
	VkDescriptorBufferInfo* bufferInfo,
	uint32_t binding,
	uint32_t count /*=1*/)
{
	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.pNext = nullptr;

	write.dstBinding		= binding;
	write.dstSet			= dstSet;
	write.descriptorCount	= count;
	write.descriptorType	= type;
	write.pBufferInfo		= bufferInfo;

	return write;
}

VkCommandBufferBeginInfo vkinit::command_buffer_begin_info(VkCommandBufferUsageFlags usageFlags)
{
	VkCommandBufferBeginInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	info.pNext = nullptr;
	info.flags = usageFlags;
	info.pInheritanceInfo = nullptr;

	return info;
}

VkSubmitInfo vkinit::submit_info(VkCommandBuffer* cmd)
{
	VkSubmitInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	info.pNext = nullptr;

	info.waitSemaphoreCount = 0;
	info.pWaitSemaphores = nullptr;
	info.pWaitDstStageMask = nullptr;
	info.commandBufferCount = 1;
	info.pCommandBuffers = cmd;
	info.signalSemaphoreCount = 0;
	info.pSignalSemaphores = nullptr;

	return info;
}

VkSamplerCreateInfo vkinit::sampler_create_info(VkFilter filters, VkSamplerAddressMode samplerAddressMode)
{
	VkSamplerCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	info.pNext = nullptr;
	
	info.magFilter = filters;
	info.minFilter = filters;
	info.addressModeU = samplerAddressMode;
	info.addressModeV = samplerAddressMode;
	info.addressModeW = samplerAddressMode;

	return info;
}

VkWriteDescriptorSet vkinit::write_descriptor_image(VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorImageInfo* imageInfo, uint32_t binding, uint32_t count)
{
	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.pNext = nullptr;

	write.dstBinding		= binding;
	write.dstSet			= dstSet;
	write.descriptorCount	= count;
	write.descriptorType	= type;
	write.pImageInfo		= imageInfo;

	return write;
}

VkBufferCreateInfo vkinit::buffer_create_info(size_t bufferSize, VkBufferUsageFlags usage)
{
	VkBufferCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	info.pNext = nullptr;
	info.size  = bufferSize;
	info.usage = usage;

	return info;
}

VkDescriptorPoolCreateInfo vkinit::descriptor_pool_create_info(const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets, VkDescriptorPoolCreateFlags flags)
{
	VkDescriptorPoolCreateInfo info = {};
	info.sType			= VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	info.pNext			= nullptr;
	info.flags = flags;
	info.poolSizeCount	= static_cast<uint32_t>(poolSizes.size());
	info.pPoolSizes		= poolSizes.data();
	info.maxSets		= maxSets;

	return info;
}

VkDescriptorSetAllocateInfo vkinit::descriptor_set_allocate_info(VkDescriptorPool descriptorPool, const VkDescriptorSetLayout* descriptorSetLayout, uint32_t descriptorSetCount)
{
	VkDescriptorSetAllocateInfo info{};
	info.sType				= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	info.pNext				= nullptr;
	info.descriptorPool		= descriptorPool;
	info.pSetLayouts		= descriptorSetLayout;
	info.descriptorSetCount = descriptorSetCount;

	return info;
}

VkDescriptorImageInfo vkinit::descriptor_image_create_info(VkSampler sampler, VkImageView view, VkImageLayout layout)
{
	VkDescriptorImageInfo info = {};
	info.sampler		= sampler;
	info.imageView		= view;
	info.imageLayout	= layout;

	return info;
}

VkPipelineColorBlendStateCreateInfo vkinit::color_blend_state_create_info(uint32_t attachmentCount, VkPipelineColorBlendAttachmentState* pAttachments)
{
	VkPipelineColorBlendStateCreateInfo info = {};
	info.sType				= VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	info.pNext				= nullptr;
	info.attachmentCount	= attachmentCount;
	info.pAttachments		= pAttachments;

	return info;
}

// VKRay
VkAccelerationStructureGeometryKHR vkinit::acceleration_structure_geometry_khr()
{
	VkAccelerationStructureGeometryKHR asgeometry{};
	asgeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;

	return asgeometry;
}

VkAccelerationStructureBuildGeometryInfoKHR vkinit::acceleration_structure_build_geometry_info()
{
	VkAccelerationStructureBuildGeometryInfoKHR info = {};
	info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;

	return info;
}

VkAccelerationStructureBuildSizesInfoKHR vkinit::acceleration_structure_build_sizes_info()
{
	VkAccelerationStructureBuildSizesInfoKHR info{};
	info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

	return info;
}