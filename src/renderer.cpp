
#include <renderer.h>
#include <vk_engine.h>
#include <vk_initializers.h>

Renderer::Renderer()
{
	device		= &VulkanEngine::engine->_device;
	swapchain	= &VulkanEngine::engine->_swapchain;
	frameNumber	= &VulkanEngine::engine->_frameNumber;
	gizmoEntity	= nullptr;

	init_commands();
	init_render_pass();
	init_forward_render_pass();
	init_offscreen_render_pass();
	init_framebuffers();
	init_offscreen_framebuffers();
	init_sync_structures();

	init_descriptors();
	init_deferred_descriptors();
	setup_descriptors();
	init_forward_pipeline();
	init_deferred_pipelines();
	build_previous_command_buffer();

	// Ray tracing
	vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(*device, "vkCreateAccelerationStructureKHR"));
	vkBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(*device, "vkBuildAccelerationStructuresKHR"));
	vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(vkGetDeviceProcAddr(*device, "vkGetAccelerationStructureBuildSizesKHR"));
	vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(vkGetDeviceProcAddr(*device, "vkGetAccelerationStructureDeviceAddressKHR"));
	vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(*device, "vkCmdBuildAccelerationStructuresKHR"));
	vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(vkGetDeviceProcAddr(*device, "vkGetRayTracingShaderGroupHandlesKHR"));
	vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(vkGetDeviceProcAddr(*device, "vkCreateRayTracingPipelinesKHR"));
	vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(vkGetDeviceProcAddr(*device, "vkCmdTraceRaysKHR"));
	vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(vkGetDeviceProcAddr(*device, "vkDestroyAccelerationStructureKHR"));

	create_bottom_acceleration_structure();
	create_top_acceleration_structure();
	create_storage_image();
	create_rt_descriptors();
	init_raytracing_pipeline();
	create_shader_binding_table();
	build_raytracing_command_buffers();
}

void Renderer::init_commands()
{
	// Create a command pool for commands to be submitted to the graphics queue
	VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(VulkanEngine::engine->_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
	VkCommandPoolCreateInfo uploadCommandPoolInfo = vkinit::command_pool_create_info(VulkanEngine::engine->_graphicsQueueFamily);

	for (int i = 0; i < FRAME_OVERLAP; i++)
	{
		VK_CHECK(vkCreateCommandPool(*device, &commandPoolInfo, nullptr, &_frames[i]._commandPool));

		// Allocate the default command buffer that will be used for rendering
		VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_frames[i]._commandPool, 1);

		VK_CHECK(vkAllocateCommandBuffers(*device, &cmdAllocInfo, &_frames[i]._mainCommandBuffer));

		VulkanEngine::engine->_mainDeletionQueue.push_function([=]() {
			vkDestroyCommandPool(*device, _frames[i]._commandPool, nullptr);
			});
	}

	VK_CHECK(vkCreateCommandPool(*device, &uploadCommandPoolInfo, nullptr, &_commandPool));

	VkCommandBufferAllocateInfo cmdDeferredAllocInfo = vkinit::command_buffer_allocate_info(_commandPool);
	VK_CHECK(vkAllocateCommandBuffers(*device, &cmdDeferredAllocInfo, &_offscreenComandBuffer));

	VulkanEngine::engine->_mainDeletionQueue.push_function([=]() {
		vkDestroyCommandPool(*device, _commandPool, nullptr);
		});
}

void Renderer::init_render_pass()
{

	VkAttachmentDescription color_attachment = {};
	color_attachment.format			= VulkanEngine::engine->_swapchainImageFormat;
	color_attachment.samples		= VK_SAMPLE_COUNT_1_BIT;
	color_attachment.loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR;
	color_attachment.storeOp		= VK_ATTACHMENT_STORE_OP_STORE;
	// Do not care about stencil at the moment
	color_attachment.stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	// We do not know or care about the starting layout of the attachment
	color_attachment.initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;
	// After the render pass ends, the image has to be on a layout ready for display
	color_attachment.finalLayout	= VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference color_attachment_ref = {};
	// Attachment number will index into the pAttachments array in the parent renderpass itself
	color_attachment_ref.attachment = 0;
	color_attachment_ref.layout		= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription depth_attachment = {};
	depth_attachment.format				= VulkanEngine::engine->_depthFormat;
	depth_attachment.samples			= VK_SAMPLE_COUNT_1_BIT;
	depth_attachment.flags				= 0;
	depth_attachment.loadOp				= VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_attachment.storeOp			= VK_ATTACHMENT_STORE_OP_STORE;
	depth_attachment.stencilLoadOp		= VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_attachment.stencilStoreOp		= VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_attachment.initialLayout		= VK_IMAGE_LAYOUT_UNDEFINED;
	depth_attachment.finalLayout		= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depth_attachment_ref = {};
	depth_attachment_ref.attachment		= 1;
	depth_attachment_ref.layout			= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	std::array<VkSubpassDependency, 2> dependencies{};
	dependencies[0].srcSubpass			= VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass			= 0;
	dependencies[0].srcStageMask		= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[0].dstStageMask		= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].srcAccessMask		= VK_ACCESS_MEMORY_READ_BIT;
	dependencies[0].dstAccessMask		= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags		= VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[1].srcSubpass			= 0;
	dependencies[1].dstSubpass			= VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask		= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask		= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[1].srcAccessMask		= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask		= VK_ACCESS_MEMORY_READ_BIT;
	dependencies[1].dependencyFlags		= VK_DEPENDENCY_BY_REGION_BIT;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint			= VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount		= 1;
	subpass.pColorAttachments			= &color_attachment_ref;
	subpass.pDepthStencilAttachment		= &depth_attachment_ref;

	VkAttachmentDescription attachments[2] = { color_attachment, depth_attachment };

	VkRenderPassCreateInfo render_pass_info = {};
	render_pass_info.sType				= VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_info.pNext				= nullptr;
	render_pass_info.attachmentCount	= 2;
	render_pass_info.pAttachments		= &attachments[0];
	render_pass_info.subpassCount		= 1;
	render_pass_info.pSubpasses			= &subpass;
	render_pass_info.dependencyCount	= 1;
	render_pass_info.pDependencies		= dependencies.data();

	VkDevice device = VulkanEngine::engine->_device;
	VK_CHECK(vkCreateRenderPass(device, &render_pass_info, nullptr, &_renderPass));

	VulkanEngine::engine->_mainDeletionQueue.push_function([=]() {
		vkDestroyRenderPass(device, _renderPass, nullptr);
		});
}

void Renderer::init_forward_render_pass()
{
	VkAttachmentDescription color_attachment = {};
	color_attachment.format			= VulkanEngine::engine->_swapchainImageFormat;
	color_attachment.samples		= VK_SAMPLE_COUNT_1_BIT;
	color_attachment.loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR;
	color_attachment.storeOp		= VK_ATTACHMENT_STORE_OP_STORE;
	color_attachment.stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color_attachment.initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;
	color_attachment.finalLayout	= VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference color_attachment_ref = {};
	color_attachment_ref.attachment = 0;
	color_attachment_ref.layout		= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription depth_attachment = {};
	// Depth attachment
	depth_attachment.flags			= 0;
	depth_attachment.format			= VulkanEngine::engine->_depthFormat;
	depth_attachment.samples		= VK_SAMPLE_COUNT_1_BIT;
	depth_attachment.loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_attachment.storeOp		= VK_ATTACHMENT_STORE_OP_STORE;
	depth_attachment.stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_attachment.initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;
	depth_attachment.finalLayout	= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depth_attachment_ref = {};
	depth_attachment_ref.attachment = 1;
	depth_attachment_ref.layout		= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	//we are going to create 1 subpass, which is the minimum you can do
	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint		= VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount	= 1;
	subpass.pColorAttachments		= &color_attachment_ref;
	subpass.pDepthStencilAttachment = &depth_attachment_ref;

	//1 dependency, which is from "outside" into the subpass. And we can read or write color
	VkSubpassDependency dependency = {};
	dependency.srcSubpass		= VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass		= 0;
	dependency.srcStageMask		= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask	= 0;
	dependency.dstStageMask		= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask	= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	//array of 2 attachments, one for the color, and other for depth
	VkAttachmentDescription attachments[2] = { color_attachment,depth_attachment };

	VkRenderPassCreateInfo render_pass_info = {};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_info.attachmentCount	= 2;
	render_pass_info.pAttachments		= &attachments[0];
	render_pass_info.subpassCount		= 1;
	render_pass_info.pSubpasses			= &subpass;
	render_pass_info.dependencyCount	= 1;
	render_pass_info.pDependencies		= &dependency;

	VK_CHECK(vkCreateRenderPass(*device, &render_pass_info, nullptr, &_forwardRenderPass));

	VulkanEngine::engine->_mainDeletionQueue.push_function([=]() {
		vkDestroyRenderPass(*device, _forwardRenderPass, nullptr);
		});
}

void Renderer::init_offscreen_render_pass()
{
	Texture position, normal, albedo, depth;
	VulkanEngine::engine->create_attachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &position);
	VulkanEngine::engine->create_attachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &normal);
	VulkanEngine::engine->create_attachment(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &albedo);
	VulkanEngine::engine->create_attachment(VulkanEngine::engine->_depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, &depth);

	_deferredTextures.push_back(position);
	_deferredTextures.push_back(normal);
	_deferredTextures.push_back(albedo);
	_deferredTextures.push_back(depth);

	std::array<VkAttachmentDescription, 4> attachmentDescs = {};

	// Init attachment properties
	for (uint32_t i = 0; i < 4; i++)
	{
		attachmentDescs[i].samples			= VK_SAMPLE_COUNT_1_BIT;
		attachmentDescs[i].loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachmentDescs[i].storeOp			= VK_ATTACHMENT_STORE_OP_STORE;
		attachmentDescs[i].stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachmentDescs[i].stencilStoreOp	= VK_ATTACHMENT_STORE_OP_DONT_CARE;
		if (i == 3)
		{
			attachmentDescs[i].initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;
			attachmentDescs[i].finalLayout		= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}
		else
		{
			attachmentDescs[i].initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;
			attachmentDescs[i].finalLayout		= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
	}

	// Formats
	attachmentDescs[0].format = VK_FORMAT_R16G16B16A16_SFLOAT;
	attachmentDescs[1].format = VK_FORMAT_R16G16B16A16_SFLOAT;
	attachmentDescs[2].format = VK_FORMAT_R8G8B8A8_UNORM;
	attachmentDescs[3].format = VulkanEngine::engine->_depthFormat;

	std::vector<VkAttachmentReference> colorReferences;
	colorReferences.push_back({ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
	colorReferences.push_back({ 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
	colorReferences.push_back({ 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

	VkAttachmentReference depthReference;
	depthReference.attachment = 3;
	depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.pColorAttachments = colorReferences.data();
	subpass.colorAttachmentCount = static_cast<uint32_t>(colorReferences.size());
	subpass.pDepthStencilAttachment = &depthReference;

	std::array<VkSubpassDependency, 2> dependencies;

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.pNext = nullptr;
	renderPassInfo.pAttachments = attachmentDescs.data();
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescs.size());
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 2;
	renderPassInfo.pDependencies = dependencies.data();

	VK_CHECK(vkCreateRenderPass(*device, &renderPassInfo, nullptr, &_offscreenRenderPass));

	VulkanEngine::engine->_mainDeletionQueue.push_function([=]() {
		vkDestroyRenderPass(*device, _offscreenRenderPass, nullptr);
		for (int i = 0; i < _deferredTextures.size(); i++) {
			vkDestroyImageView(*device, _deferredTextures[i].imageView, nullptr);
			vmaDestroyImage(VulkanEngine::engine->_allocator, _deferredTextures[i].image._image, _deferredTextures[i].image._allocation);
		}
		});
}

FrameData& Renderer::get_current_frame()
{
	return _frames[*frameNumber % FRAME_OVERLAP];
}

void Renderer::rasterize()
{
	ImGui::Render();

	VK_CHECK(vkWaitForFences(*device, 1, &get_current_frame()._renderFence, VK_TRUE, 1000000000));
	VK_CHECK(vkResetFences(*device, 1, &get_current_frame()._renderFence));

	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	vkAcquireNextImageKHR(*device, *swapchain, UINT64_MAX, get_current_frame()._presentSemaphore, VK_NULL_HANDLE, &VulkanEngine::engine->_indexSwapchainImage);

	build_forward_command_buffer();

	VkSubmitInfo submit = {};
	submit.sType				= VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.pNext				= nullptr;
	submit.pWaitDstStageMask	= waitStages;
	submit.waitSemaphoreCount	= 1;
	submit.pWaitSemaphores		= &get_current_frame()._presentSemaphore;
	submit.signalSemaphoreCount = 1;
	submit.pSignalSemaphores	= &get_current_frame()._renderSemaphore;
	submit.pCommandBuffers		= &get_current_frame()._mainCommandBuffer; 

	VK_CHECK(vkQueueSubmit(VulkanEngine::engine->_graphicsQueue, 1, &submit, get_current_frame()._renderFence));

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType				= VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext				= nullptr;
	presentInfo.swapchainCount		= 1;
	presentInfo.pSwapchains			= swapchain;
	presentInfo.waitSemaphoreCount	= 1;
	presentInfo.pWaitSemaphores		= &get_current_frame()._renderSemaphore;
	presentInfo.pImageIndices		= &VulkanEngine::engine->_indexSwapchainImage;

	VK_CHECK(vkQueuePresentKHR(VulkanEngine::engine->_graphicsQueue, &presentInfo));
}

void Renderer::render()
{
	ImGui::Render();

	// Wait until the gpu has finished rendering the last frame. Timeout 1 second
	VK_CHECK(vkWaitForFences(*device, 1, &get_current_frame()._renderFence, VK_TRUE, 1000000000));
	VK_CHECK(vkResetFences(*device, 1, &get_current_frame()._renderFence));

	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	VkResult result = vkAcquireNextImageKHR(*device, *swapchain, UINT64_MAX, get_current_frame()._presentSemaphore, VK_NULL_HANDLE, &VulkanEngine::engine->_indexSwapchainImage);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || VulkanEngine::engine->_resized) {
		VulkanEngine::engine->recreate_swapchain();
		return;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		throw std::runtime_error("Failed to acquire swap chain image");
	}

	// First pass
	VkSubmitInfo submit = {};
	submit.sType				= VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.pNext				= nullptr;
	submit.pWaitDstStageMask	= waitStages;
	submit.waitSemaphoreCount	= 1;
	submit.pWaitSemaphores		= &get_current_frame()._presentSemaphore;
	submit.signalSemaphoreCount = 1;
	submit.pSignalSemaphores	= &_offscreenSemaphore;
	submit.commandBufferCount	= 1;
	submit.pCommandBuffers		= &_offscreenComandBuffer;

	VK_CHECK(vkQueueSubmit(VulkanEngine::engine->_graphicsQueue, 1, &submit, VK_NULL_HANDLE));

	build_deferred_command_buffer();

	// Second pass
	submit.pWaitSemaphores		= &_offscreenSemaphore;
	submit.pSignalSemaphores	= &get_current_frame()._renderSemaphore;
	submit.pCommandBuffers		= &get_current_frame()._mainCommandBuffer;
	VK_CHECK(vkQueueSubmit(VulkanEngine::engine->_graphicsQueue, 1, &submit, get_current_frame()._renderFence));

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType				= VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext				= nullptr;
	presentInfo.swapchainCount		= 1;
	presentInfo.pSwapchains			= swapchain;
	presentInfo.waitSemaphoreCount	= 1;
	presentInfo.pWaitSemaphores		= &get_current_frame()._renderSemaphore;
	presentInfo.pImageIndices		= &VulkanEngine::engine->_indexSwapchainImage;

	result = vkQueuePresentKHR(VulkanEngine::engine->_graphicsQueue, &presentInfo);
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
		VulkanEngine::engine->recreate_swapchain();
	}
	else if (result != VK_SUCCESS)
		throw std::runtime_error("failed to present swap chain images!");
}

void Renderer::raytrace()
{
	ImGui::Render();

	VK_CHECK(vkWaitForFences(*device, 1, &get_current_frame()._renderFence, VK_TRUE, 1000000000));
	VK_CHECK(vkResetFences(*device, 1, &get_current_frame()._renderFence));

	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	VK_CHECK(vkResetCommandBuffer(get_current_frame()._mainCommandBuffer, 0));

	VkResult result = vkAcquireNextImageKHR(*device, *swapchain, UINT64_MAX, get_current_frame()._presentSemaphore, VK_NULL_HANDLE, &VulkanEngine::engine->_indexSwapchainImage);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || VulkanEngine::engine->_resized) {
		VulkanEngine::engine->recreate_swapchain();
		return;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		throw std::runtime_error("Failed to acquire swap chain image");
	}

	build_raytracing_command_buffers();

	VkSubmitInfo submit{};
	submit.sType				= VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.pNext				= nullptr;
	submit.pWaitDstStageMask	= waitStages;
	submit.waitSemaphoreCount	= 1;
	submit.pWaitSemaphores		= &get_current_frame()._presentSemaphore;
	submit.signalSemaphoreCount = 1;
	submit.pSignalSemaphores	= &get_current_frame()._renderSemaphore;
	submit.commandBufferCount	= 1;
	submit.pCommandBuffers		= &get_current_frame()._mainCommandBuffer;

	VK_CHECK(vkQueueSubmit(VulkanEngine::engine->_graphicsQueue, 1, &submit, get_current_frame()._renderFence));

	VkPresentInfoKHR present{};
	present.sType				= VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present.pNext				= nullptr;
	present.swapchainCount		= 1;
	present.pSwapchains			= swapchain;
	present.waitSemaphoreCount	= 1;
	present.pWaitSemaphores		= &get_current_frame()._renderSemaphore;
	present.pImageIndices		= &VulkanEngine::engine->_indexSwapchainImage;

	result = vkQueuePresentKHR(VulkanEngine::engine->_graphicsQueue, &present);
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
		VulkanEngine::engine->recreate_swapchain();
	}
	else if (result != VK_SUCCESS)
		throw std::runtime_error("failed to present swap chain images!");
}

void Renderer::render_gui()
{
	// Imgui new frame
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL2_NewFrame(VulkanEngine::engine->_window);

	ImGui::NewFrame();

	ImGui::Begin("Debug window");
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
	for (auto& light : VulkanEngine::engine->_lights)
	{
		if (ImGui::TreeNode(&light, "Light")) {
			if (ImGui::Button("Select"))
				gizmoEntity = light;
			ImGui::SliderFloat3("Position", &((glm::vec3)light->m_matrix[3])[0], -200, 200);
			ImGui::ColorEdit3("Color", &light->color.x);
			ImGui::SliderFloat("Intensity", &light->intensity, 0, 1000);
			ImGui::SliderFloat("Max Distance", &light->maxDistance, 0, 500);
			ImGui::TreePop();
		}
	}
	for (auto& entity : VulkanEngine::engine->_renderables)
	{
		if (ImGui::TreeNode(&entity, "Entity")) {
			if (ImGui::Button("Select"))
				gizmoEntity = entity;
			ImGui::SliderFloat3("Position", &((glm::vec3)entity->m_matrix[3])[0], -200, 200);
			ImGui::TreePop();
		}
	}
	ImGui::End();

	if (!gizmoEntity)
		return;

	glm::mat4& matrix = gizmoEntity->m_matrix;

	ImGuizmo::BeginFrame();

	static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::TRANSLATE);
	static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::WORLD);
	if (ImGui::IsKeyPressed(90))
		mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
	if (ImGui::IsKeyPressed(69))
		mCurrentGizmoOperation = ImGuizmo::ROTATE;
	if (ImGui::IsKeyPressed(82)) // r Key
		mCurrentGizmoOperation = ImGuizmo::SCALE;
	if (ImGui::RadioButton("Translate", mCurrentGizmoOperation == ImGuizmo::TRANSLATE))
		mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
	ImGui::SameLine();
	if (ImGui::RadioButton("Rotate", mCurrentGizmoOperation == ImGuizmo::ROTATE))
		mCurrentGizmoOperation = ImGuizmo::ROTATE;
	ImGui::SameLine();
	if (ImGui::RadioButton("Scale", mCurrentGizmoOperation == ImGuizmo::SCALE))
		mCurrentGizmoOperation = ImGuizmo::SCALE;
	float matrixTranslation[3], matrixRotation[3], matrixScale[3];
	ImGuizmo::DecomposeMatrixToComponents(&matrix[0][0], matrixTranslation, matrixRotation, matrixScale);
	ImGui::InputFloat3("Tr", matrixTranslation, 3);
	ImGui::InputFloat3("Rt", matrixRotation, 3);
	ImGui::InputFloat3("Sc", matrixScale, 3);
	ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation, matrixScale, &matrix[0][0]);

	if (mCurrentGizmoOperation != ImGuizmo::SCALE)
	{
		if (ImGui::RadioButton("Local", mCurrentGizmoMode == ImGuizmo::LOCAL))
			mCurrentGizmoMode = ImGuizmo::LOCAL;
		ImGui::SameLine();
		if (ImGui::RadioButton("World", mCurrentGizmoMode == ImGuizmo::WORLD))
			mCurrentGizmoMode = ImGuizmo::WORLD;
	}
	static bool useSnap(false);
	if (ImGui::IsKeyPressed(83))
		useSnap = !useSnap;
	ImGui::Checkbox("", &useSnap);
	ImGui::SameLine();
	glm::vec3 snap;
	switch (mCurrentGizmoOperation)
	{
	case ImGuizmo::TRANSLATE:
		//snap = config.mSnapTranslation;
		ImGui::InputFloat3("Snap", &snap.x);
		break;
	case ImGuizmo::ROTATE:
		//snap = config.mSnapRotation;
		ImGui::InputFloat("Angle Snap", &snap.x);
		break;
	case ImGuizmo::SCALE:
		//snap = config.mSnapScale;
		ImGui::InputFloat("Scale Snap", &snap.x);
		break;
	}
	ImGuiIO& io = ImGui::GetIO();
	ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
	glm::mat4 projection = glm::perspective(glm::radians(70.0f), 1700.f / 900.f, 0.1f, 200.0f);
	ImGuizmo::Manipulate(&VulkanEngine::engine->_camera->getView()[0][0], &projection[0][0], mCurrentGizmoOperation, mCurrentGizmoMode, &matrix[0][0], NULL, useSnap ? &snap.x : NULL);

	ImGui::EndFrame();
}

void Renderer::init_framebuffers()
{
	VkFramebufferCreateInfo framebufferInfo = vkinit::framebuffer_create_info(_renderPass, VulkanEngine::engine->_windowExtent);

	// Grab how many images we have in the swapchain
	const uint32_t swapchain_imagecount = VulkanEngine::engine->_swapchainImages.size();
	_framebuffers = std::vector<VkFramebuffer>(swapchain_imagecount);

	for (unsigned int i = 0; i < swapchain_imagecount; i++)
	{
		VkImageView attachments[2];
		attachments[0] = VulkanEngine::engine->_swapchainImageViews[i];
		attachments[1] = VulkanEngine::engine->_depthImageView;

		framebufferInfo.attachmentCount = 2;
		framebufferInfo.pAttachments	= attachments;
		VK_CHECK(vkCreateFramebuffer(*device, &framebufferInfo, nullptr, &_framebuffers[i]));

		VulkanEngine::engine->_mainDeletionQueue.push_function([=]() {
			vkDestroyFramebuffer(*device, _framebuffers[i], nullptr);
			vkDestroyImageView(*device, VulkanEngine::engine->_swapchainImageViews[i], nullptr);
			});
	}
}

void Renderer::init_offscreen_framebuffers()
{
	std::array<VkImageView, 4> attachments;
	attachments[0] = _deferredTextures.at(0).imageView;	// Position
	attachments[1] = _deferredTextures.at(1).imageView;	// Normal
	attachments[2] = _deferredTextures.at(2).imageView;	// Color	
	attachments[3] = _deferredTextures.at(3).imageView;	// Depth

	VkFramebufferCreateInfo framebufferInfo = vkinit::framebuffer_create_info(_offscreenRenderPass, VulkanEngine::engine->_windowExtent);
	framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	framebufferInfo.pAttachments = attachments.data();

	VK_CHECK(vkCreateFramebuffer(*device, &framebufferInfo, nullptr, &_offscreenFramebuffer));

	VkSamplerCreateInfo sampler = vkinit::sampler_create_info(VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
	sampler.mipmapMode		= VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler.mipLodBias		= 0.0f;
	sampler.maxAnisotropy	= 1.0f;
	sampler.minLod			= 0.0f;
	sampler.maxLod			= 1.0f;
	sampler.borderColor		= VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	VK_CHECK(vkCreateSampler(*device, &sampler, nullptr, &_offscreenSampler));

	VulkanEngine::engine->_mainDeletionQueue.push_function([=]() {
		vkDestroyFramebuffer(*device, _offscreenFramebuffer, nullptr);
		vkDestroySampler(*device, _offscreenSampler, nullptr);
		});
}

void Renderer::init_sync_structures()
{
	// Create syncronization structures

	VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
	VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

	VK_CHECK(vkCreateSemaphore(*device, &semaphoreCreateInfo, nullptr, &_offscreenSemaphore));

	for (int i = 0; i < FRAME_OVERLAP; i++)
	{
		VK_CHECK(vkCreateFence(*device, &fenceCreateInfo, nullptr, &_frames[i]._renderFence));

		VulkanEngine::engine->_mainDeletionQueue.push_function([=]() {
			vkDestroyFence(*device, _frames[i]._renderFence, nullptr);
			});

		// We do not need any flags for the sempahores

		VK_CHECK(vkCreateSemaphore(*device, &semaphoreCreateInfo, nullptr, &_frames[i]._presentSemaphore));
		VK_CHECK(vkCreateSemaphore(*device, &semaphoreCreateInfo, nullptr, &_frames[i]._renderSemaphore));

		VulkanEngine::engine->_mainDeletionQueue.push_function([=]() {
			vkDestroySemaphore(*device, _frames[i]._presentSemaphore, nullptr);
			vkDestroySemaphore(*device, _frames[i]._renderSemaphore, nullptr);
			});
	}

	VulkanEngine::engine->_mainDeletionQueue.push_function([=]() {
		vkDestroySemaphore(*device, _offscreenSemaphore, nullptr);
		});
}

void Renderer::init_descriptors()
{
	std::vector<VkDescriptorPoolSize> sizes = {
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 10},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10},
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10}
	};

	VkDescriptorPoolCreateInfo pool_info = vkinit::descriptor_pool_create_info(sizes, 10, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);

	vkCreateDescriptorPool(*device, &pool_info, nullptr, &_descriptorPool);

	// Set = 0
	// binding camera data at 0
	VkDescriptorSetLayoutBinding cameraBind = vkinit::descriptorset_layout_binding(
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
	// binding scene data at 1
	VkDescriptorSetLayoutBinding sceneBind = vkinit::descriptorset_layout_binding(
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1);

	VkDescriptorSetLayoutBinding bindings[] = { cameraBind, sceneBind };

	VkDescriptorSetLayoutCreateInfo setInfo = {};
	setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	setInfo.pNext = nullptr;

	setInfo.bindingCount = 2;
	setInfo.pBindings = bindings;
	setInfo.flags = 0;

	vkCreateDescriptorSetLayout(*device, &setInfo, nullptr, &_offscreenDescriptorSetLayout);

	// Set = 1
	// binding matrices at 0
	VkDescriptorSetLayoutBinding objectBind = vkinit::descriptorset_layout_binding(
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);

	VkDescriptorSetLayoutCreateInfo set2Info = {};
	set2Info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	set2Info.pNext = nullptr;

	set2Info.bindingCount = 1;
	set2Info.pBindings = &objectBind;
	set2Info.flags = 0;

	vkCreateDescriptorSetLayout(*device, &set2Info, nullptr, &_objectDescriptorSetLayout);

	// Set = 2
	// binding single texture at 0
	uint32_t nText = VulkanEngine::engine->_textures.size();
	VkDescriptorSetLayoutBinding textureBind = vkinit::descriptorset_layout_binding(
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0, nText);

	VkDescriptorSetLayoutCreateInfo set3Info = {};
	set3Info.sType			= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	set3Info.pNext			= nullptr;
	set3Info.bindingCount	= 1;
	set3Info.pBindings		= &textureBind;
	set3Info.flags			= 0;

	vkCreateDescriptorSetLayout(*device, &set3Info, nullptr, &_textureDescriptorSetLayout);

	VulkanEngine::engine->_cameraBuffer = VulkanEngine::engine->create_buffer(sizeof(glm::mat4), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	const size_t sceneBufferSize		= VulkanEngine::engine->pad_uniform_buffer_size(sizeof(GPUSceneData));
	VulkanEngine::engine->_sceneBuffer	= VulkanEngine::engine->create_buffer(sceneBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	const int MAX_OBJECTS = 10000;
	VulkanEngine::engine->_objectBuffer = VulkanEngine::engine->create_buffer(sizeof(GPUObjectData) * MAX_OBJECTS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.pNext = nullptr;

	allocInfo.descriptorPool		= _descriptorPool;
	allocInfo.descriptorSetCount	= 1;
	allocInfo.pSetLayouts			= &_offscreenDescriptorSetLayout;

	vkAllocateDescriptorSets(*device, &allocInfo, &_offscreenDescriptorSet);

	VkDescriptorSetAllocateInfo objectAllocInfo = {};
	objectAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	objectAllocInfo.pNext = nullptr;

	objectAllocInfo.descriptorPool		= _descriptorPool;
	objectAllocInfo.descriptorSetCount	= 1;
	objectAllocInfo.pSetLayouts			= &_objectDescriptorSetLayout;

	vkAllocateDescriptorSets(*device, &objectAllocInfo, &_objectDescriptorSet);

	VkDescriptorBufferInfo cameraInfo = {};
	cameraInfo.buffer = VulkanEngine::engine->_cameraBuffer._buffer;
	cameraInfo.offset = 0;
	cameraInfo.range  = sizeof(glm::mat4);

	VkDescriptorBufferInfo sceneInfo = {};
	sceneInfo.buffer = VulkanEngine::engine->_sceneBuffer._buffer;
	sceneInfo.offset = 0;
	sceneInfo.range  = VK_WHOLE_SIZE;

	VkDescriptorBufferInfo objectInfo = {};
	objectInfo.buffer = VulkanEngine::engine->_objectBuffer._buffer;
	objectInfo.offset = 0;
	objectInfo.range  = sizeof(GPUObjectData) * MAX_OBJECTS;

	VkWriteDescriptorSet cameraWrite = vkinit::write_descriptor_buffer(
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, _offscreenDescriptorSet, &cameraInfo, 0);
	VkWriteDescriptorSet sceneWrite = vkinit::write_descriptor_buffer(
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, _offscreenDescriptorSet, &sceneInfo, 1);
	VkWriteDescriptorSet objectWrite = vkinit::write_descriptor_buffer(
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, _objectDescriptorSet, &objectInfo, 0);

	VkWriteDescriptorSet writes[] = { cameraWrite, sceneWrite, objectWrite };

	vkUpdateDescriptorSets(*device, 3, writes, 0, nullptr);

	// Textures descriptor ---

	VkDescriptorSetAllocateInfo textureAllocInfo = {};
	textureAllocInfo.sType				= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	textureAllocInfo.pNext				= nullptr;
	textureAllocInfo.descriptorSetCount = 1;
	textureAllocInfo.pSetLayouts		= &_textureDescriptorSetLayout;
	textureAllocInfo.descriptorPool		= _descriptorPool;

	VK_CHECK(vkAllocateDescriptorSets(*device, &textureAllocInfo, &_textureDescriptorSet));

	VkSamplerCreateInfo samplerInfo = vkinit::sampler_create_info(VK_FILTER_NEAREST);
	VkSampler sampler;
	vkCreateSampler(*device, &samplerInfo, nullptr, &sampler);

	std::vector<VkDescriptorImageInfo> imageInfos;
	for (auto const& texture : VulkanEngine::engine->_textures)
	{
		VkDescriptorImageInfo imageBufferInfo = {};
		imageBufferInfo.sampler		= sampler;
		imageBufferInfo.imageView	= texture.second.imageView;
		imageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		imageInfos.push_back(imageBufferInfo);
	}

	VkWriteDescriptorSet write = vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, _textureDescriptorSet, imageInfos.data(), 0, VulkanEngine::engine->_textures.size());

	vkUpdateDescriptorSets(*device, 1, &write, 0, nullptr);

	VulkanEngine::engine->_mainDeletionQueue.push_function([=]() {
		vkDestroyDescriptorPool(*device, _descriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(*device, _offscreenDescriptorSetLayout, nullptr);
		vkDestroyDescriptorSetLayout(*device, _objectDescriptorSetLayout, nullptr);
		vkDestroyDescriptorSetLayout(*device, _textureDescriptorSetLayout, nullptr);
		vmaDestroyBuffer(VulkanEngine::engine->_allocator, VulkanEngine::engine->_cameraBuffer._buffer, VulkanEngine::engine->_cameraBuffer._allocation);
		vmaDestroyBuffer(VulkanEngine::engine->_allocator, VulkanEngine::engine->_sceneBuffer._buffer, VulkanEngine::engine->_sceneBuffer._allocation);
		vmaDestroyBuffer(VulkanEngine::engine->_allocator, VulkanEngine::engine->_objectBuffer._buffer, VulkanEngine::engine->_objectBuffer._allocation);
		vkDestroySampler(*device, sampler, nullptr);
		});
}

void Renderer::setup_descriptors()
{
	for (int i = 0; i < FRAME_OVERLAP; i++)
	{
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType					= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.pNext					= nullptr;
		allocInfo.descriptorPool		= _descriptorPool;
		allocInfo.descriptorSetCount	= 1;
		allocInfo.pSetLayouts			= &_deferredSetLayout;

		vkAllocateDescriptorSets(*device, &allocInfo, &_frames[i].deferredDescriptorSet);

		VkDescriptorImageInfo texDescriptorPosition = vkinit::descriptor_image_create_info(
			_offscreenSampler, _deferredTextures[0].imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);	// Position
		VkDescriptorImageInfo texDescriptorNormal = vkinit::descriptor_image_create_info(
			_offscreenSampler, _deferredTextures[1].imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);	// Normal
		VkDescriptorImageInfo texDescriptorAlbedo = vkinit::descriptor_image_create_info(
			_offscreenSampler, _deferredTextures[2].imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);	// Albedo

		int nLights = VulkanEngine::engine->_lights.size();
		_frames[i]._lightBuffer = VulkanEngine::engine->create_buffer(sizeof(uboLight) * nLights, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

		VkDescriptorBufferInfo lightBuffer;
		lightBuffer.buffer = _frames[i]._lightBuffer._buffer;
		lightBuffer.offset = 0;
		lightBuffer.range  = sizeof(uboLight) * nLights;

		std::vector<VkWriteDescriptorSet> writes = {
			vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, _frames[i].deferredDescriptorSet, &texDescriptorPosition, 0),
			vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, _frames[i].deferredDescriptorSet, &texDescriptorNormal, 1),
			vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, _frames[i].deferredDescriptorSet, &texDescriptorAlbedo, 2),
			vkinit::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, _frames[i].deferredDescriptorSet, &lightBuffer, 3)
		};

		vkUpdateDescriptorSets(*device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

		VulkanEngine::engine->_mainDeletionQueue.push_function([=]() {
			vmaDestroyBuffer(VulkanEngine::engine->_allocator, _frames[i]._lightBuffer._buffer, _frames[i]._lightBuffer._allocation);
		});
	}
}

void Renderer::init_deferred_descriptors()
{
	std::vector<VkDescriptorPoolSize> poolSizes = {
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10},
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10}
	};

	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings =
	{
		vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),	// Position
		vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),	// Normals
		vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),	// Albedo
		vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 3)
	};

	VkDescriptorSetLayoutCreateInfo setInfo = {};
	setInfo.sType			= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	setInfo.pNext			= nullptr;
	setInfo.bindingCount	= static_cast<uint32_t>(setLayoutBindings.size());
	setInfo.pBindings		= setLayoutBindings.data();

	VK_CHECK(vkCreateDescriptorSetLayout(*device, &setInfo, nullptr, &_deferredSetLayout));

	VulkanEngine::engine->_mainDeletionQueue.push_function([=]() {
		vkDestroyDescriptorSetLayout(*device, _deferredSetLayout, nullptr);
		});
}

void Renderer::init_forward_pipeline()
{
	VulkanEngine* engine = VulkanEngine::engine;
	VkShaderModule vertexShader;
	if (!VulkanEngine::engine->load_shader_module("data/shaders/basic.vert.spv", &vertexShader)) {
		std::cout << "Could not load forward vertex shader!" << std::endl;
	}
	VkShaderModule fragmentShader;
	if (!VulkanEngine::engine->load_shader_module("data/shaders/forward.frag.spv", &fragmentShader)) {
		std::cout << "Could not load fragment vertex shader!" << std::endl;
	}

	PipelineBuilder pipBuilder;
	pipBuilder._shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, vertexShader));
	pipBuilder._shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader));

	VkDescriptorSetLayout offscreenSetLayouts[] = { _offscreenDescriptorSetLayout, _objectDescriptorSetLayout, _textureDescriptorSetLayout };

	VkPushConstantRange push_constant;
	push_constant.offset		= 0;
	push_constant.size			= sizeof(int);
	push_constant.stageFlags	= VK_SHADER_STAGE_FRAGMENT_BIT;

	VkPipelineLayoutCreateInfo offscreenPipelineLayoutInfo = vkinit::pipeline_layout_create_info();
	offscreenPipelineLayoutInfo.setLayoutCount			= 3;
	offscreenPipelineLayoutInfo.pSetLayouts				= offscreenSetLayouts;
	offscreenPipelineLayoutInfo.pushConstantRangeCount	= 1;
	offscreenPipelineLayoutInfo.pPushConstantRanges		= &push_constant;

	VK_CHECK(vkCreatePipelineLayout(*device, &offscreenPipelineLayoutInfo, nullptr, &_forwardPipelineLayout));

	VertexInputDescription vertexDescription = Vertex::get_vertex_description();
	pipBuilder._vertexInputInfo = vkinit::vertex_input_state_create_info();
	pipBuilder._vertexInputInfo.vertexAttributeDescriptionCount = vertexDescription.attributes.size();
	pipBuilder._vertexInputInfo.pVertexAttributeDescriptions	= vertexDescription.attributes.data();
	pipBuilder._vertexInputInfo.vertexBindingDescriptionCount	= vertexDescription.bindings.size();
	pipBuilder._vertexInputInfo.pVertexBindingDescriptions		= vertexDescription.bindings.data();

	pipBuilder._pipelineLayout = _forwardPipelineLayout;

	pipBuilder._inputAssembly		= vkinit::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipBuilder._rasterizer			= vkinit::rasterization_state_create_info(VK_POLYGON_MODE_FILL);
	pipBuilder._depthStencil		= vkinit::depth_stencil_create_info(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);
	pipBuilder._viewport.x			= 0.0f;
	pipBuilder._viewport.y			= 0.0f;
	pipBuilder._viewport.maxDepth	= 1.0f;
	pipBuilder._viewport.minDepth	= 0.0f;
	pipBuilder._viewport.width		= (float)VulkanEngine::engine->_windowExtent.width;
	pipBuilder._viewport.height		= (float)VulkanEngine::engine->_windowExtent.height;
	pipBuilder._scissor.offset		= { 0, 0 };
	pipBuilder._scissor.extent		= VulkanEngine::engine->_windowExtent;

	pipBuilder._colorBlendStateInfo = vkinit::color_blend_state_create_info(1, &vkinit::color_blend_attachment_state(0xf, VK_FALSE));
	pipBuilder._multisampling = vkinit::multisample_state_create_info();

	_forwardPipeline = pipBuilder.build_pipeline(*device, _forwardRenderPass);

	VulkanEngine::engine->create_material(_forwardPipeline, _forwardPipelineLayout, "forward");

	vkDestroyShaderModule(*device, vertexShader, nullptr);
	vkDestroyShaderModule(*device, fragmentShader, nullptr);

	VulkanEngine::engine->_mainDeletionQueue.push_function([=]() {
		vkDestroyPipelineLayout(*device, _forwardPipelineLayout, nullptr);
		vkDestroyPipeline(*device, _forwardPipeline, nullptr);
		});
}

void Renderer::init_deferred_pipelines()
{
	VulkanEngine* engine = VulkanEngine::engine;

	VkShaderModule offscreenVertexShader;
	if (!VulkanEngine::engine->load_shader_module("data/shaders/basic.vert.spv", &offscreenVertexShader)) {
		std::cout << "Could not load geometry vertex shader!" << std::endl;
	}
	VkShaderModule offscreenFragmentShader;
	if (!VulkanEngine::engine->load_shader_module("data/shaders/geometry_shader.frag.spv", &offscreenFragmentShader)) {
		std::cout << "Could not load geometry fragment shader!" << std::endl;
	}
	VkShaderModule quadVertexShader;
	if (!VulkanEngine::engine->load_shader_module("data/shaders/quad.vert.spv", &quadVertexShader)) {
		std::cout << "Could not load deferred vertex shader!" << std::endl;
	}
	VkShaderModule deferredFragmentShader;
	if (!VulkanEngine::engine->load_shader_module("data/shaders/testQuad.frag.spv", &deferredFragmentShader)) {
		std::cout << "Could not load deferred fragment shader!" << std::endl;
	}

	PipelineBuilder pipBuilder;
	pipBuilder._shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, offscreenVertexShader));
	pipBuilder._shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, offscreenFragmentShader));

	VkDescriptorSetLayout offscreenSetLayouts[] = { _offscreenDescriptorSetLayout, _objectDescriptorSetLayout, _textureDescriptorSetLayout };

	VkPushConstantRange push_constant;
	push_constant.offset		= 0;
	push_constant.size			= sizeof(int);
	push_constant.stageFlags	= VK_SHADER_STAGE_FRAGMENT_BIT;

	VkPipelineLayoutCreateInfo offscreenPipelineLayoutInfo = vkinit::pipeline_layout_create_info();
	offscreenPipelineLayoutInfo.setLayoutCount			= 3;
	offscreenPipelineLayoutInfo.pSetLayouts				= offscreenSetLayouts;
	offscreenPipelineLayoutInfo.pushConstantRangeCount	= 1;
	offscreenPipelineLayoutInfo.pPushConstantRanges		= &push_constant;

	VK_CHECK(vkCreatePipelineLayout(*device, &offscreenPipelineLayoutInfo, nullptr, &_offscreenPipelineLayout));

	VertexInputDescription vertexDescription = Vertex::get_vertex_description();
	pipBuilder._vertexInputInfo = vkinit::vertex_input_state_create_info();
	pipBuilder._vertexInputInfo.vertexAttributeDescriptionCount = vertexDescription.attributes.size();
	pipBuilder._vertexInputInfo.pVertexAttributeDescriptions	= vertexDescription.attributes.data();
	pipBuilder._vertexInputInfo.vertexBindingDescriptionCount	= vertexDescription.bindings.size();
	pipBuilder._vertexInputInfo.pVertexBindingDescriptions		= vertexDescription.bindings.data();

	pipBuilder._pipelineLayout = _offscreenPipelineLayout;

	pipBuilder._inputAssembly		= vkinit::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipBuilder._rasterizer			= vkinit::rasterization_state_create_info(VK_POLYGON_MODE_FILL);
	pipBuilder._depthStencil		= vkinit::depth_stencil_create_info(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);
	pipBuilder._viewport.x			= 0.0f;
	pipBuilder._viewport.y			= 0.0f;
	pipBuilder._viewport.maxDepth	= 1.0f;
	pipBuilder._viewport.minDepth	= 0.0f;
	pipBuilder._viewport.width		= (float)VulkanEngine::engine->_windowExtent.width;
	pipBuilder._viewport.height		= (float)VulkanEngine::engine->_windowExtent.height;
	pipBuilder._scissor.offset		= { 0, 0 };
	pipBuilder._scissor.extent		= VulkanEngine::engine->_windowExtent;

	std::array<VkPipelineColorBlendAttachmentState, 3> blendAttachmentStates = {
		vkinit::color_blend_attachment_state(
			VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT, VK_FALSE),
		vkinit::color_blend_attachment_state(0xf, VK_FALSE),
		vkinit::color_blend_attachment_state(0xf, VK_FALSE)
	};

	VkPipelineColorBlendStateCreateInfo colorBlendInfo = vkinit::color_blend_state_create_info(static_cast<uint32_t>(blendAttachmentStates.size()), blendAttachmentStates.data());

	pipBuilder._colorBlendStateInfo = colorBlendInfo;
	pipBuilder._multisampling = vkinit::multisample_state_create_info();

	_offscreenPipeline = pipBuilder.build_pipeline(*device, _offscreenRenderPass);

	VulkanEngine::engine->create_material(_offscreenPipeline, _offscreenPipelineLayout, "offscreen");

	// Second pipeline -----------------------------------------------------------------------------

	VkPushConstantRange push_constant_final;
	push_constant_final.offset		= 0;
	push_constant_final.size		= sizeof(pushConstants);
	push_constant_final.stageFlags	= VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayout finalSetLayout[] = { _deferredSetLayout };

	VkPipelineLayoutCreateInfo deferredPipelineLayoutInfo = vkinit::pipeline_layout_create_info();
	deferredPipelineLayoutInfo.setLayoutCount			= 1;
	deferredPipelineLayoutInfo.pSetLayouts				= &_deferredSetLayout;
	deferredPipelineLayoutInfo.pushConstantRangeCount	= 1;
	deferredPipelineLayoutInfo.pPushConstantRanges		= &push_constant_final;

	VK_CHECK(vkCreatePipelineLayout(*device, &deferredPipelineLayoutInfo, nullptr, &_finalPipelineLayout));

	pipBuilder._colorBlendStateInfo = vkinit::color_blend_state_create_info(1, &vkinit::color_blend_attachment_state(0xf, VK_FALSE));

	pipBuilder._shaderStages.clear();
	pipBuilder._shaderStages.push_back(
		vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, quadVertexShader));
	pipBuilder._shaderStages.push_back(
		vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, deferredFragmentShader));
	pipBuilder._pipelineLayout = _finalPipelineLayout;

	_finalPipeline = pipBuilder.build_pipeline(*device, _renderPass);

	vkDestroyShaderModule(*device, offscreenVertexShader, nullptr);
	vkDestroyShaderModule(*device, offscreenFragmentShader, nullptr);
	vkDestroyShaderModule(*device, quadVertexShader, nullptr);
	vkDestroyShaderModule(*device, deferredFragmentShader, nullptr);

	VulkanEngine::engine->_mainDeletionQueue.push_function([=]() {
		vkDestroyPipelineLayout(*device, _offscreenPipelineLayout, nullptr);
		vkDestroyPipelineLayout(*device, _finalPipelineLayout, nullptr);
		vkDestroyPipeline(*device, _offscreenPipeline, nullptr);
		vkDestroyPipeline(*device, _finalPipeline, nullptr);
		});
}

void Renderer::build_forward_command_buffer()
{
	VkCommandBufferBeginInfo cmdBufInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VK_CHECK(vkResetCommandBuffer(get_current_frame()._mainCommandBuffer, 0));
	VkCommandBuffer *cmd = &get_current_frame()._mainCommandBuffer;

	std::array<VkClearValue, 2> clearValues;
	clearValues[0].color		= { 0.0f, 1.0f, 0.0f, 1.0f };
	clearValues[1].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType						= VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass					= _forwardRenderPass;
	renderPassBeginInfo.renderArea.extent.width		= VulkanEngine::engine->_windowExtent.width;
	renderPassBeginInfo.renderArea.extent.height	= VulkanEngine::engine->_windowExtent.height;
	renderPassBeginInfo.clearValueCount				= static_cast<uint32_t>(clearValues.size());
	renderPassBeginInfo.pClearValues				= clearValues.data();
	renderPassBeginInfo.framebuffer					= _framebuffers[VulkanEngine::engine->_indexSwapchainImage];


	VK_CHECK(vkBeginCommandBuffer(*cmd, &cmdBufInfo));

	vkCmdBeginRenderPass(*cmd, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	// Set = 0 Camera data descriptor
	uint32_t uniform_offset = VulkanEngine::engine->pad_uniform_buffer_size(sizeof(GPUSceneData));
	vkCmdBindDescriptorSets(*cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _forwardPipelineLayout, 0, 1, &_offscreenDescriptorSet, 1, &uniform_offset);
	// Set = 1 Object data descriptor
	vkCmdBindDescriptorSets(*cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _forwardPipelineLayout, 1, 1, &_objectDescriptorSet, 0, nullptr);
	// Set = 2 Texture data descriptor
	vkCmdBindDescriptorSets(*cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _forwardPipelineLayout, 2, 1, &_textureDescriptorSet, 0, nullptr);

	Mesh* lastMesh = nullptr;

	for (size_t i = 0; i < VulkanEngine::engine->_renderables.size(); i++)
	{
		Object* object = VulkanEngine::engine->_renderables[i];

		vkCmdBindPipeline(*cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _forwardPipeline);

		VkDeviceSize offset = { 0 };

		int constant = object->id;
		vkCmdPushConstants(*cmd, _forwardPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(int), &constant);

		if (lastMesh != object->mesh) {
			vkCmdBindVertexBuffers(*cmd, 0, 1, &object->mesh->_vertexBuffer._buffer, &offset);
			vkCmdBindIndexBuffer(*cmd, object->mesh->_indexBuffer._buffer, 0, VK_INDEX_TYPE_UINT32);
			lastMesh = object->mesh;
		}
		vkCmdDrawIndexed(*cmd, static_cast<uint32_t>(object->mesh->_indices.size()), VulkanEngine::engine->_renderables.size(), 0, 0, i);
	}

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *cmd);

	vkCmdEndRenderPass(*cmd);
	VK_CHECK(vkEndCommandBuffer(*cmd));
}

void Renderer::build_previous_command_buffer()
{
	if (_offscreenComandBuffer == VK_NULL_HANDLE)
	{
		VkCommandBufferAllocateInfo allocInfo = vkinit::command_buffer_allocate_info(_commandPool);
		VK_CHECK(vkAllocateCommandBuffers(*device, &allocInfo, &_offscreenComandBuffer));
	}

	VkCommandBufferBeginInfo cmdBufInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);

	std::array<VkClearValue, 4> clearValues;
	clearValues[0].color = { 0.2f, 0.2f, 0.2f, 1.0f };
	clearValues[1].color = { 0.0f, 0.5f, 0.0f, 1.0f };
	clearValues[2].color = { 0.0f, 0.0f, 0.0f, 1.0f };
	clearValues[3].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType						= VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass					= _offscreenRenderPass;
	renderPassBeginInfo.framebuffer					= _offscreenFramebuffer;
	renderPassBeginInfo.renderArea.extent.width		= VulkanEngine::engine->_windowExtent.width;
	renderPassBeginInfo.renderArea.extent.height	= VulkanEngine::engine->_windowExtent.height;
	renderPassBeginInfo.clearValueCount				= static_cast<uint32_t>(clearValues.size());;
	renderPassBeginInfo.pClearValues				= clearValues.data();

	VK_CHECK(vkBeginCommandBuffer(_offscreenComandBuffer, &cmdBufInfo));

	vkCmdBeginRenderPass(_offscreenComandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	// Set = 0 Camera data descriptor
	uint32_t uniform_offset = VulkanEngine::engine->pad_uniform_buffer_size(sizeof(GPUSceneData));
	vkCmdBindDescriptorSets(_offscreenComandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _offscreenPipelineLayout, 0, 1, &_offscreenDescriptorSet, 1, &uniform_offset);
	// Set = 1 Object data descriptor
	vkCmdBindDescriptorSets(_offscreenComandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _offscreenPipelineLayout, 1, 1, &_objectDescriptorSet, 0, nullptr);
	// Set = 2 Texture data descriptor
	vkCmdBindDescriptorSets(_offscreenComandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _offscreenPipelineLayout, 2, 1, &_textureDescriptorSet, 0, nullptr);

	Mesh* lastMesh = nullptr;

	for (size_t i = 0; i < VulkanEngine::engine->_renderables.size(); i++)
	{
		Object* object = VulkanEngine::engine->_renderables[i];

		vkCmdBindPipeline(_offscreenComandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _offscreenPipeline);

		VkDeviceSize offset = { 0 };

		int constant = object->id;
		vkCmdPushConstants(_offscreenComandBuffer, _offscreenPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(int), &constant);

		if (lastMesh != object->mesh) {
			vkCmdBindVertexBuffers(_offscreenComandBuffer, 0, 1, &object->mesh->_vertexBuffer._buffer, &offset);
			vkCmdBindIndexBuffer(_offscreenComandBuffer, object->mesh->_indexBuffer._buffer, 0, VK_INDEX_TYPE_UINT32);
			lastMesh = object->mesh;
		}
		vkCmdDrawIndexed(_offscreenComandBuffer, static_cast<uint32_t>(object->mesh->_indices.size()), VulkanEngine::engine->_renderables.size(), 0, 0, i);
	}

	vkCmdEndRenderPass(_offscreenComandBuffer);
	VK_CHECK(vkEndCommandBuffer(_offscreenComandBuffer));
}

void Renderer::build_deferred_command_buffer()
{
	VkCommandBufferBeginInfo cmdBufInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	std::array<VkClearValue, 2> clearValues;
	clearValues[0].color = { 0.0f, 1.0f, 0.0f, 1.0f };
	clearValues[1].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType						= VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass					= _renderPass;
	renderPassBeginInfo.renderArea.extent.width		= VulkanEngine::engine->_windowExtent.width;
	renderPassBeginInfo.renderArea.extent.height	= VulkanEngine::engine->_windowExtent.height;
	renderPassBeginInfo.clearValueCount				= static_cast<uint32_t>(clearValues.size());
	renderPassBeginInfo.pClearValues				= clearValues.data();
	renderPassBeginInfo.framebuffer					= _framebuffers[VulkanEngine::engine->_indexSwapchainImage];

	VK_CHECK(vkResetCommandBuffer(get_current_frame()._mainCommandBuffer, 0));

	vkBeginCommandBuffer(get_current_frame()._mainCommandBuffer, &cmdBufInfo);

	vkCmdBeginRenderPass(get_current_frame()._mainCommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(get_current_frame()._mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _finalPipeline);

	VkDeviceSize offset = { 0 };

	Mesh* quad = VulkanEngine::engine->get_mesh("quad");

	vkCmdPushConstants(get_current_frame()._mainCommandBuffer, _finalPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pushConstants), &_constants);

	vkCmdBindDescriptorSets(get_current_frame()._mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _finalPipelineLayout, 0, 1, &get_current_frame().deferredDescriptorSet, 0, nullptr);
	vkCmdBindVertexBuffers(get_current_frame()._mainCommandBuffer, 0, 1, &quad->_vertexBuffer._buffer, &offset);
	vkCmdBindIndexBuffer(get_current_frame()._mainCommandBuffer, quad->_indexBuffer._buffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(get_current_frame()._mainCommandBuffer, static_cast<uint32_t>(quad->_indices.size()), 1, 0, 0, 1);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), get_current_frame()._mainCommandBuffer);

	vkCmdEndRenderPass(get_current_frame()._mainCommandBuffer);
	VK_CHECK(vkEndCommandBuffer(get_current_frame()._mainCommandBuffer));
}

void Renderer::create_storage_image()
{
	VkExtent3D extent = { VulkanEngine::engine->_windowExtent.width, VulkanEngine::engine->_windowExtent.height, 1 };
	VkImageCreateInfo imageInfo = vkinit::image_create_info(VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT, extent);
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VmaAllocationCreateInfo allocInfo{};
	allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	allocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	vmaCreateImage(VulkanEngine::engine->_allocator, &imageInfo, &allocInfo, 
		&_rtImage.image._image, &_rtImage.image._allocation, nullptr);

	VkImageViewCreateInfo imageViewInfo = vkinit::image_view_create_info(VK_FORMAT_B8G8R8A8_UNORM, _rtImage.image._image, VK_IMAGE_ASPECT_COLOR_BIT);
	VK_CHECK(vkCreateImageView(*device, &imageViewInfo, nullptr, &_rtImage.imageView));

	VkBufferCreateInfo bufferInfo = vkinit::buffer_create_info(sizeof(RTCameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

	allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
	allocInfo.requiredFlags = 0;

	VK_CHECK(vmaCreateBuffer(VulkanEngine::engine->_allocator, &bufferInfo, &allocInfo, &VulkanEngine::engine->rtCameraBuffer._buffer, &VulkanEngine::engine->rtCameraBuffer._allocation, nullptr));

	glm::mat4 view = VulkanEngine::engine->_camera->getView();
	glm::mat4 projection = glm::perspective(glm::radians(70.0f), 1700.f / 900.f, 0.1f, 200.0f);
	projection[1][1] *= -1;

	RTCameraData camera;
	camera.invProj = glm::inverse(projection);
	camera.invView = glm::inverse(view);

	void* data;
	vmaMapMemory(VulkanEngine::engine->_allocator, VulkanEngine::engine->rtCameraBuffer._allocation, &data);
	memcpy(data, &camera, sizeof(RTCameraData));
	vmaUnmapMemory(VulkanEngine::engine->_allocator, VulkanEngine::engine->rtCameraBuffer._allocation);
	
	VulkanEngine::engine->immediate_submit([&](VkCommandBuffer cmd) {
		VkImageMemoryBarrier imageMemoryBarrier{};
		imageMemoryBarrier.sType			= VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageMemoryBarrier.image			= _rtImage.image._image;
		imageMemoryBarrier.oldLayout		= VK_IMAGE_LAYOUT_UNDEFINED;
		imageMemoryBarrier.newLayout		= VK_IMAGE_LAYOUT_GENERAL;
		//imageMemoryBarrier.dstQueueFamilyIndex = VulkanEngine::engine->_graphicsQueueFamily;
		imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
	});

	VulkanEngine::engine->_mainDeletionQueue.push_function([=]() {
		vmaDestroyImage(VulkanEngine::engine->_allocator, _rtImage.image._image, _rtImage.image._allocation);
		vkDestroyImageView(*device, _rtImage.imageView, nullptr);
		vmaDestroyBuffer(VulkanEngine::engine->_allocator, VulkanEngine::engine->rtCameraBuffer._buffer, VulkanEngine::engine->rtCameraBuffer._allocation);
		});
	
}

// VKRAY
// ---------------------------------------------------------------------------------------
// Create all the BLAS
// - Go through all meshes in the scene and convert them to BlasInput (holds geometry and rangeInfo)
// - Build as many BLAS as BlasInput (geometries defined in the scene)

void Renderer::create_bottom_acceleration_structure()
{
	std::vector<BlasInput> allBlas; 
	allBlas.reserve(VulkanEngine::engine->_renderables.size());
	for (Object* obj : VulkanEngine::engine->_renderables) {
		BlasInput input = object_to_geometry(*obj);
		allBlas.push_back(input);
	}

	buildBlas(allBlas);
}


void Renderer::create_top_acceleration_structure()
{
	uint32_t nInstances = static_cast<uint32_t>(VulkanEngine::engine->_renderables.size());

	std::vector<TlasInstance> tlas;
	tlas.reserve(nInstances);

	for (uint32_t i = 0; i < nInstances; i++)
	{
		Object *node = VulkanEngine::engine->_renderables[i];
		TlasInstance instance{};
		instance.transform	= node->m_matrix;
		instance.blasId		= i;
		instance.instanceId = i;	// gl_InstanceCustomIndexEXT
		tlas.emplace_back(instance);
	}

	buildTlas(tlas);
}

// ---------------------------------------------------------------------------------------
// This function will create as many BLAS as input objects.
// - Create a buildGeometryInfo for each input object and add the necessary information
// - Create the AS object where handle and device addres is stored
// - Find the max scratch size for all the inputs in order to only use one buffer
// - Finally submit the creation commands
void Renderer::buildBlas(const std::vector<BlasInput>& input, VkBuildAccelerationStructureFlagsKHR flags)
{
	// Make own copy of the information coming from input
	assert(_blas.empty());	// Make sure that we are only building blas once
	_blas = std::vector<BlasInput>(input.begin(), input.end());
	uint32_t blasSize = static_cast<uint32_t>(_blas.size());

	_bottomLevelAS.resize(blasSize);	// Prepare all necessary BLAS to create

	// We will prepare the building information for each of the blas
	std::vector<VkAccelerationStructureBuildGeometryInfoKHR> asBuildGeoInfos(blasSize);
	for (uint32_t i = 0; i < blasSize; i++)
	{
		asBuildGeoInfos[i] = vkinit::acceleration_structure_build_geometry_info();
		asBuildGeoInfos[i].type						= VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		asBuildGeoInfos[i].flags					= flags;
		asBuildGeoInfos[i].geometryCount			= 1;
		asBuildGeoInfos[i].pGeometries				= &_blas[i].asGeometry;
		asBuildGeoInfos[i].mode						= VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		asBuildGeoInfos[i].srcAccelerationStructure = VK_NULL_HANDLE;
	}

	// Used to search for the maximum scratch size to only use one scratch for the whole build
	VkDeviceSize maxScratch{ 0 };

	for (uint32_t i = 0; i < blasSize; i++)
	{
		VkAccelerationStructureBuildSizesInfoKHR asBuildSizesInfo{};
		asBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
		vkGetAccelerationStructureBuildSizesKHR(*device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &asBuildGeoInfos[i], &_blas[i].nTriangles, &asBuildSizesInfo);

		create_acceleration_structure(_bottomLevelAS[i], VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, asBuildSizesInfo);

		asBuildGeoInfos[i].dstAccelerationStructure = _bottomLevelAS[i].handle;

		maxScratch = std::max(maxScratch, asBuildSizesInfo.buildScratchSize);
	}

	ScratchBuffer scratchBuffer = VulkanEngine::engine->createScratchBuffer(maxScratch);

	// Once the scratch buffer is created we finally end inflating the asBuildGeosInfo struct and can proceed to submit the creation command
	for (uint32_t i = 0; i < blasSize; i++) {

		asBuildGeoInfos[i].scratchData.deviceAddress = scratchBuffer.deviceAddress;

		std::vector<VkAccelerationStructureBuildRangeInfoKHR*> asBuildStructureRangeInfo = { &_blas[i].asBuildRangeInfo };

		VulkanEngine::engine->immediate_submit([=](VkCommandBuffer cmd) {
			vkCmdBuildAccelerationStructuresKHR(cmd, 1, &asBuildGeoInfos[i], asBuildStructureRangeInfo.data());
		});
	}

	// Finally we can free the scratch buffer
	vkFreeMemory(*device, scratchBuffer.memory, nullptr);
	vkDestroyBuffer(*device, scratchBuffer.buffer, nullptr);
}

// ---------------------------------------------------------------------------------------
// This creates the TLAS from the input instances
void Renderer::buildTlas(const std::vector<TlasInstance>& instances, VkBuildAccelerationStructureFlagsKHR flags)
{
	// Cannot be built twice
	assert(_topLevelAS.handle == VK_NULL_HANDLE);

	std::vector<VkAccelerationStructureInstanceKHR> geometryInstances;
	geometryInstances.reserve(instances.size());

	for (const TlasInstance& instance : instances)
	{
		geometryInstances.push_back(object_to_instance(instance));
	}

	VkDeviceSize instancesSize = geometryInstances.size() * sizeof(VkAccelerationStructureInstanceKHR);

	AllocatedBuffer instanceBuffer = VulkanEngine::engine->create_buffer(instancesSize,
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
		VMA_MEMORY_USAGE_CPU_TO_GPU);

	void* instanceData;
	vmaMapMemory(VulkanEngine::engine->_allocator, instanceBuffer._allocation, &instanceData);
	memcpy(instanceData, geometryInstances.data(), instancesSize);
	vmaUnmapMemory(VulkanEngine::engine->_allocator, instanceBuffer._allocation);

	VkDeviceOrHostAddressConstKHR instanceDataDeviceAddress{};
	instanceDataDeviceAddress.deviceAddress = VulkanEngine::engine->getBufferDeviceAddress(instanceBuffer._buffer);

	// Create a stucture that holds a device pointer to the uploaded instances.
	VkAccelerationStructureGeometryInstancesDataKHR instancesData{};
	instancesData.sType					= VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	instancesData.arrayOfPointers		= VK_FALSE;
	instancesData.data					= instanceDataDeviceAddress;

	// Put the above structure to the structure geometry
	VkAccelerationStructureGeometryKHR asGeometry = vkinit::acceleration_structure_geometry_khr();
	asGeometry.geometryType				= VK_GEOMETRY_TYPE_INSTANCES_KHR;
	asGeometry.flags					= VK_GEOMETRY_OPAQUE_BIT_KHR;
	asGeometry.geometry.instances		= instancesData;

	// Find sizes
	VkAccelerationStructureBuildGeometryInfoKHR asBuildGeometryInfo = vkinit::acceleration_structure_build_geometry_info();
	asBuildGeometryInfo.type			= VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	asBuildGeometryInfo.mode			= VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	asBuildGeometryInfo.flags			= flags;
	asBuildGeometryInfo.geometryCount	= 1;
	asBuildGeometryInfo.pGeometries		= &asGeometry;
	asBuildGeometryInfo.srcAccelerationStructure = VK_NULL_HANDLE;

	uint32_t									primitiveCount = static_cast<uint32_t>(instances.size());
	VkAccelerationStructureBuildSizesInfoKHR	asBuildSizesInfo = vkinit::acceleration_structure_build_sizes_info();
	vkGetAccelerationStructureBuildSizesKHR(
		VulkanEngine::engine->_device,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&asBuildGeometryInfo,
		&primitiveCount,
		&asBuildSizesInfo
	);

	create_acceleration_structure(_topLevelAS, VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, asBuildSizesInfo);

	ScratchBuffer scratchBuffer = VulkanEngine::engine->createScratchBuffer(asBuildSizesInfo.buildScratchSize);

	asBuildGeometryInfo.dstAccelerationStructure = _topLevelAS.handle;
	asBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

	VkAccelerationStructureBuildRangeInfoKHR asBuildRangeInfo{};
	asBuildRangeInfo.primitiveCount		= static_cast<uint32_t>(instances.size());
	asBuildRangeInfo.primitiveOffset	= 0;
	asBuildRangeInfo.firstVertex		= 0;
	asBuildRangeInfo.transformOffset	= 0;

	const std::vector<VkAccelerationStructureBuildRangeInfoKHR*> asBuildStructureRangeInfos = { &asBuildRangeInfo };

	VulkanEngine::engine->immediate_submit([=](VkCommandBuffer cmd) {
		vkCmdBuildAccelerationStructuresKHR(cmd, 1, &asBuildGeometryInfo, asBuildStructureRangeInfos.data());
		});

	vmaDestroyBuffer(VulkanEngine::engine->_allocator, instanceBuffer._buffer, instanceBuffer._allocation);
	vkFreeMemory(*device, scratchBuffer.memory, nullptr);
	vkDestroyBuffer(*device, scratchBuffer.buffer, nullptr);
}

void Renderer::create_acceleration_structure(AccelerationStructure& accelerationStructure, VkAccelerationStructureTypeKHR type, VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo)
{
	//accelerationStructure.buffer = VulkanEngine::engine->create_buffer(buildSizeInfo.accelerationStructureSize,
	//	VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
	//	VMA_MEMORY_USAGE_CPU_TO_GPU);
	VkBufferCreateInfo bufferCreateInfo{};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = buildSizeInfo.accelerationStructureSize;
	bufferCreateInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	VK_CHECK(vkCreateBuffer(*device, &bufferCreateInfo, nullptr, &accelerationStructure.buffer));

	VkMemoryRequirements memRequirements{};
	vkGetBufferMemoryRequirements(*device, accelerationStructure.buffer, &memRequirements);
	VkMemoryAllocateFlagsInfo memAllocateFlagsInfo{};
	memAllocateFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
	memAllocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;

	VkMemoryAllocateInfo memAllocateInfo{};
	memAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAllocateInfo.pNext = &memAllocateFlagsInfo;
	memAllocateInfo.allocationSize = memRequirements.size;
	memAllocateInfo.memoryTypeIndex = VulkanEngine::engine->get_memory_type(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK(vkAllocateMemory(*device, &memAllocateInfo, nullptr, &accelerationStructure.memory));
	VK_CHECK(vkBindBufferMemory(*device, accelerationStructure.buffer, accelerationStructure.memory, 0));
	
	VkAccelerationStructureCreateInfoKHR asCreateInfo{};
	asCreateInfo.sType	= VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	asCreateInfo.buffer = accelerationStructure.buffer;
	asCreateInfo.size	= buildSizeInfo.accelerationStructureSize;
	asCreateInfo.type	= type;

	vkCreateAccelerationStructureKHR(*device,
		&asCreateInfo, nullptr, &accelerationStructure.handle);

	VkAccelerationStructureDeviceAddressInfoKHR asDeviceAddressInfo{};
	asDeviceAddressInfo.sType					= VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	asDeviceAddressInfo.accelerationStructure	= accelerationStructure.handle;

	accelerationStructure.deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(*device, &asDeviceAddressInfo);

	VulkanEngine::engine->_mainDeletionQueue.push_function([=]() {
		//vmaDestroyBuffer(VulkanEngine::engine->_allocator, accelerationStructure.buffer._buffer, accelerationStructure.buffer._allocation);
		vkFreeMemory(*device, accelerationStructure.memory, nullptr);
		vkDestroyBuffer(*device, accelerationStructure.buffer, nullptr);
		vkDestroyAccelerationStructureKHR(VulkanEngine::engine->_device, accelerationStructure.handle, nullptr);
		});
}

// Use information from the model mesh passed to fill in all geometry information
BlasInput Renderer::object_to_geometry(const Object& model)
{
	VkDeviceOrHostAddressConstKHR vertexBufferDeviceAddress{};
	VkDeviceOrHostAddressConstKHR indexBufferDeviceAddress{};

	vertexBufferDeviceAddress.deviceAddress = VulkanEngine::engine->getBufferDeviceAddress(model.mesh->_vertexBuffer._buffer);
	indexBufferDeviceAddress.deviceAddress = VulkanEngine::engine->getBufferDeviceAddress(model.mesh->_indexBuffer._buffer);

	const uint32_t nTriangles = model.mesh->_indices.size() / 3;

	// Set the triangles geometry
	VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
	triangles.sType						= VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
	triangles.pNext						= nullptr;
	triangles.vertexFormat				= VK_FORMAT_R32G32B32_SFLOAT;
	triangles.vertexData				= vertexBufferDeviceAddress;
	triangles.vertexStride				= sizeof(Vertex);
	triangles.maxVertex					= static_cast<uint32_t>(model.mesh->_vertices.size());
	triangles.indexData					= indexBufferDeviceAddress;
	triangles.indexType					= VK_INDEX_TYPE_UINT32;

	VkAccelerationStructureGeometryKHR asGeometry = vkinit::acceleration_structure_geometry_khr();
	asGeometry.flags					= VK_GEOMETRY_OPAQUE_BIT_KHR;
	asGeometry.geometryType				= VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	asGeometry.geometry.triangles		= triangles;

	VkAccelerationStructureBuildRangeInfoKHR asBuildRangeInfo{};
	asBuildRangeInfo.primitiveCount		= nTriangles;
	asBuildRangeInfo.primitiveOffset	= 0;
	asBuildRangeInfo.firstVertex		= 0;
	asBuildRangeInfo.transformOffset	= 0;

	// Store all info in the BlasInput structure to be returned
	BlasInput input;
	input.asBuildRangeInfo				= asBuildRangeInfo;
	input.asGeometry					= asGeometry;
	input.nTriangles					= nTriangles;

	return input;
}

// Pass the information from our instance to the vk instance to function in the TLAS
VkAccelerationStructureInstanceKHR Renderer::object_to_instance(const TlasInstance& instance)
{
	assert(size_t(instance.blasId) < _blas.size());

	glm::mat4 aux = glm::transpose(instance.transform);

	VkTransformMatrixKHR transform = {
		aux[0].x, aux[0].y, aux[0].z, aux[0].w,
		aux[1].x, aux[1].y, aux[1].z, aux[1].w,
		aux[2].x, aux[2].y, aux[2].z, aux[2].w,
	};

	VkAccelerationStructureInstanceKHR vkInst{};
	vkInst.transform								= transform;
	vkInst.instanceCustomIndex						= instance.instanceId;
	vkInst.mask										= instance.mask;
	vkInst.instanceShaderBindingTableRecordOffset	= instance.hitGroupId;
	vkInst.flags									= instance.flags;
	vkInst.accelerationStructureReference			= _bottomLevelAS[instance.blasId].deviceAddress;

	return vkInst;
}

void Renderer::create_rt_descriptors()
{
	std::vector<VkDescriptorPoolSize> poolSize = {
		{VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1},
		{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10}
	};

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = vkinit::descriptor_pool_create_info(poolSize, 1);
	VK_CHECK(vkCreateDescriptorPool(*device, &descriptorPoolCreateInfo, nullptr, &_rtDescriptorPool));

	// First set:
	//	binding 0 = AS
	//	binding 1 = storage image
	//	binding 2 = Camera data
	//  binding 3 = Vertex buffer
	//  binding 4 = Index buffer
	//  binding 5 = matrices buffer
	//  binding 6 = light buffer

	const uint32_t nInstances = VulkanEngine::engine->_renderables.size();		
	const int nLights = VulkanEngine::engine->_lights.size();
	lightBuffer = VulkanEngine::engine->create_buffer(sizeof(uboLight) * nLights, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	VkDescriptorSetLayoutBinding accelerationStructureLayoutBinding = vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 0);
	VkDescriptorSetLayoutBinding resultImageLayoutBinding			= vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 1);
	VkDescriptorSetLayoutBinding uniformBufferBinding				= vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 2);
	VkDescriptorSetLayoutBinding vertexBufferBinding				= vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 3, nInstances);
	VkDescriptorSetLayoutBinding indexBufferBinding					= vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 4, nInstances);
	VkDescriptorSetLayoutBinding matrixBufferBinding				= vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 5, nInstances);
	VkDescriptorSetLayoutBinding lightBufferBinding					= vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 6);

	std::vector<VkDescriptorSetLayoutBinding> bindings({
		accelerationStructureLayoutBinding,
		resultImageLayoutBinding,
		uniformBufferBinding,
		vertexBufferBinding,
		indexBufferBinding,
		matrixBufferBinding,
		lightBufferBinding
		});

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
	descriptorSetLayoutCreateInfo.sType			= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.bindingCount	= static_cast<uint32_t>(bindings.size());
	descriptorSetLayoutCreateInfo.pBindings		= bindings.data();
	VK_CHECK(vkCreateDescriptorSetLayout(*device, &descriptorSetLayoutCreateInfo, nullptr, &_rtDescriptorSetLayout));

	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = vkinit::descriptor_set_allocate_info(_rtDescriptorPool, &_rtDescriptorSetLayout, 1);
	VK_CHECK(vkAllocateDescriptorSets(*device, &descriptorSetAllocateInfo, &_rtDescriptorSet));

	VkWriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo{};
	descriptorAccelerationStructureInfo.sType						= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
	descriptorAccelerationStructureInfo.accelerationStructureCount	= 1;
	descriptorAccelerationStructureInfo.pAccelerationStructures		= &_topLevelAS.handle;

	VkWriteDescriptorSet accelerationStructureWrite{};
	accelerationStructureWrite.sType			= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	accelerationStructureWrite.pNext			= &descriptorAccelerationStructureInfo;
	accelerationStructureWrite.dstSet			= _rtDescriptorSet;
	accelerationStructureWrite.dstBinding		= 0;
	accelerationStructureWrite.descriptorCount	= 1;
	accelerationStructureWrite.descriptorType	= VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

	VkDescriptorImageInfo storageImageDescriptor{};
	storageImageDescriptor.imageView			= _rtImage.imageView;
	storageImageDescriptor.imageLayout			= VK_IMAGE_LAYOUT_GENERAL;

	VkDescriptorBufferInfo _rtDescriptorBufferInfo{};
	_rtDescriptorBufferInfo.buffer				= VulkanEngine::engine->rtCameraBuffer._buffer;
	_rtDescriptorBufferInfo.offset				= 0;
	_rtDescriptorBufferInfo.range				= sizeof(RTCameraData);

	std::vector<VkDescriptorBufferInfo> vertexDescInfo;
	std::vector<VkDescriptorBufferInfo> indexDescInfo;
	std::vector<VkDescriptorBufferInfo> matrixDescInfo;
	for (Object* obj : VulkanEngine::engine->_renderables)
	{
		obj->vBuffer = VulkanEngine::engine->create_buffer(sizeof(rtVertexAttribute) * obj->mesh->_vertices.size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

		std::vector<rtVertexAttribute> vAttr;
		vAttr.reserve(obj->mesh->_vertices.size());
		for (Vertex& v : obj->mesh->_vertices) {
			vAttr.push_back({ {v.normal.x, v.normal.y, v.normal.z, 1}, {v.color.x, v.color.y, v.color.z, 1} });
		}

		void* vdata;
		vmaMapMemory(VulkanEngine::engine->_allocator, obj->vBuffer._allocation, &vdata);
		memcpy(vdata, vAttr.data(), sizeof(rtVertexAttribute) * obj->mesh->_vertices.size());
		vmaUnmapMemory(VulkanEngine::engine->_allocator, obj->vBuffer._allocation);

		VkDescriptorBufferInfo vertexBufferDescriptor{};
		vertexBufferDescriptor.buffer	= obj->vBuffer._buffer;
		vertexBufferDescriptor.offset	= 0;
		vertexBufferDescriptor.range	= sizeof(rtVertexAttribute) * obj->mesh->_vertices.size();
		vertexDescInfo.push_back(vertexBufferDescriptor);

		VkDescriptorBufferInfo indexBufferDescriptor{};
		indexBufferDescriptor.buffer	= obj->mesh->_indexBuffer._buffer;
		indexBufferDescriptor.offset	= 0;
		indexBufferDescriptor.range		= VK_WHOLE_SIZE;
		indexDescInfo.push_back(indexBufferDescriptor);

		obj->matrixBuffer = VulkanEngine::engine->create_buffer(sizeof(glm::mat4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
		void* data;
		vmaMapMemory(VulkanEngine::engine->_allocator, obj->matrixBuffer._allocation, &data);
		memcpy(data, &obj->m_matrix, sizeof(glm::mat4));
		vmaUnmapMemory(VulkanEngine::engine->_allocator, obj->matrixBuffer._allocation);

		VkDescriptorBufferInfo matrixBufferDescriptor{};
		matrixBufferDescriptor.buffer	= obj->matrixBuffer._buffer;
		matrixBufferDescriptor.offset	= 0;
		matrixBufferDescriptor.range	= VK_WHOLE_SIZE;
		matrixDescInfo.push_back(matrixBufferDescriptor);
	}

	VkDescriptorBufferInfo lightBufferInfo;
	lightBufferInfo.buffer = lightBuffer._buffer;
	lightBufferInfo.offset = 0;
	lightBufferInfo.range = sizeof(uboLight) * nLights;

	VkWriteDescriptorSet resultImageWrite	= vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, _rtDescriptorSet, &storageImageDescriptor, 1);
	VkWriteDescriptorSet uniformBufferWrite = vkinit::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, _rtDescriptorSet, &_rtDescriptorBufferInfo, 2);
	VkWriteDescriptorSet vertexBufferWrite	= vkinit::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, _rtDescriptorSet, vertexDescInfo.data(), 3, nInstances);
	VkWriteDescriptorSet indexBufferWrite	= vkinit::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, _rtDescriptorSet, indexDescInfo.data(), 4, nInstances);
	VkWriteDescriptorSet matrixBufferWrite	= vkinit::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, _rtDescriptorSet, matrixDescInfo.data(), 5, nInstances);
	VkWriteDescriptorSet lightsBufferWrite	= vkinit::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, _rtDescriptorSet, &lightBufferInfo, 6);

	std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
		accelerationStructureWrite,
		resultImageWrite,
		uniformBufferWrite,
		vertexBufferWrite,
		indexBufferWrite,
		matrixBufferWrite,
		lightsBufferWrite
	};

	vkUpdateDescriptorSets(*device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, VK_NULL_HANDLE);

	VulkanEngine::engine->_mainDeletionQueue.push_function([=]() {
		vkDestroyDescriptorSetLayout(*device, _rtDescriptorSetLayout, nullptr);
		vkDestroyDescriptorPool(*device, _rtDescriptorPool, nullptr);
		for (Object* obj : VulkanEngine::engine->_renderables) {
			vmaDestroyBuffer(VulkanEngine::engine->_allocator, obj->matrixBuffer._buffer, obj->matrixBuffer._allocation);
			vmaDestroyBuffer(VulkanEngine::engine->_allocator, obj->vBuffer._buffer, obj->vBuffer._allocation);
		}
		vmaDestroyBuffer(VulkanEngine::engine->_allocator, lightBuffer._buffer, lightBuffer._allocation);
		});
}

void Renderer::init_raytracing_pipeline()
{
	// Setup ray tracing shader groups
	std::vector<VkPipelineShaderStageCreateInfo> shaderStages = {};

	// Ray generation group
	VkShaderModule rayGenModule;
	{
		shaderStages.push_back(VulkanEngine::engine->load_shader_stage("data/shaders/raygen.rgen.spv", &rayGenModule, VK_SHADER_STAGE_RAYGEN_BIT_KHR));
		VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
		shaderGroup.sType				= VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
		shaderGroup.type				= VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
		shaderGroup.generalShader		= 0;
		shaderGroup.closestHitShader	= VK_SHADER_UNUSED_KHR;
		shaderGroup.anyHitShader		= VK_SHADER_UNUSED_KHR;
		shaderGroup.intersectionShader	= VK_SHADER_UNUSED_KHR;
		shaderGroups.push_back(shaderGroup);
	}

	// Miss group
	VkShaderModule missModule;
	{
		shaderStages.push_back(VulkanEngine::engine->load_shader_stage("data/shaders/miss.rmiss.spv", &missModule, VK_SHADER_STAGE_MISS_BIT_KHR));
		VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
		shaderGroup.sType				= VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
		shaderGroup.type				= VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
		shaderGroup.generalShader		= static_cast<uint32_t>(shaderStages.size()) - 1;
		shaderGroup.closestHitShader	= VK_SHADER_UNUSED_KHR;
		shaderGroup.anyHitShader		= VK_SHADER_UNUSED_KHR;
		shaderGroup.intersectionShader	= VK_SHADER_UNUSED_KHR;
		shaderGroups.push_back(shaderGroup);
	}

	// Shadow miss
	VkShaderModule shadowModule;
	{
		shaderStages.push_back(VulkanEngine::engine->load_shader_stage("data/shaders/shadow.rmiss.spv", &shadowModule, VK_SHADER_STAGE_MISS_BIT_KHR));
		VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
		shaderGroup.sType				= VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
		shaderGroup.type				= VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
		shaderGroup.generalShader		= static_cast<uint32_t>(shaderStages.size()) - 1;
		shaderGroup.closestHitShader	= VK_SHADER_UNUSED_KHR;
		shaderGroup.anyHitShader		= VK_SHADER_UNUSED_KHR;
		shaderGroup.intersectionShader	= VK_SHADER_UNUSED_KHR;
		shaderGroups.push_back(shaderGroup);
	}

	// Hit group
	VkShaderModule hitModule;
	{
		shaderStages.push_back(VulkanEngine::engine->load_shader_stage("data/shaders/closesthit.rchit.spv", &hitModule, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR));
		VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
		shaderGroup.sType				= VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
		shaderGroup.type				= VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
		shaderGroup.generalShader		= VK_SHADER_UNUSED_KHR;
		shaderGroup.closestHitShader	= static_cast<uint32_t>(shaderStages.size()) - 1;;
		shaderGroup.anyHitShader		= VK_SHADER_UNUSED_KHR;
		shaderGroup.intersectionShader	= VK_SHADER_UNUSED_KHR;
		shaderGroups.push_back(shaderGroup);
	}

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
	pipelineLayoutCreateInfo.sType				= VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount		= 1;
	pipelineLayoutCreateInfo.pSetLayouts		= &_rtDescriptorSetLayout;
	VK_CHECK(vkCreatePipelineLayout(*device, &pipelineLayoutCreateInfo, nullptr, &_rtPipelineLayout));

	// Create RT pipeline 
	VkRayTracingPipelineCreateInfoKHR rtPipelineCreateInfo{};
	rtPipelineCreateInfo.sType							= VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
	rtPipelineCreateInfo.stageCount						= static_cast<uint32_t>(shaderStages.size());
	rtPipelineCreateInfo.pStages						= shaderStages.data();
	rtPipelineCreateInfo.groupCount						= static_cast<uint32_t>(shaderGroups.size());
	rtPipelineCreateInfo.pGroups						= shaderGroups.data();
	rtPipelineCreateInfo.maxPipelineRayRecursionDepth	= 2;
	rtPipelineCreateInfo.layout							= _rtPipelineLayout;

	VK_CHECK(vkCreateRayTracingPipelinesKHR(*device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rtPipelineCreateInfo, nullptr, &_rtPipeline));

	vkDestroyShaderModule(*device, rayGenModule, nullptr);
	vkDestroyShaderModule(*device, hitModule, nullptr);
	vkDestroyShaderModule(*device, missModule, nullptr);
	vkDestroyShaderModule(*device, shadowModule, nullptr);

	VulkanEngine::engine->_mainDeletionQueue.push_function([=]() {
		vkDestroyPipeline(*device, _rtPipeline, nullptr);
		vkDestroyPipelineLayout(*device, _rtPipelineLayout, nullptr);
		});
}

void Renderer::create_shader_binding_table()
{
	const uint32_t groupCount = static_cast<uint32_t>(shaderGroups.size());	// 4 shaders: raygen, miss, shadowmiss and hit
	const uint32_t handleSize = VulkanEngine::engine->_rtProperties.shaderGroupHandleSize;	// Size of a programm identifier
	const uint32_t handleAlignment = VulkanEngine::engine->_rtProperties.shaderGroupHandleAlignment;
	const uint32_t sbtSize = groupCount * handleSize;

	std::vector<uint8_t> shaderHandleStorage(sbtSize);
	VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(VulkanEngine::engine->_device, _rtPipeline, 0, groupCount, sbtSize, shaderHandleStorage.data()));

	const VkBufferUsageFlags bufferUsageFlags = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	raygenShaderBindingTable		= VulkanEngine::engine->create_buffer(handleSize, bufferUsageFlags, VMA_MEMORY_USAGE_CPU_TO_GPU);
	missShaderBindingTable			= VulkanEngine::engine->create_buffer(handleSize * 2, bufferUsageFlags, VMA_MEMORY_USAGE_CPU_TO_GPU);
	hitShaderBindingTable			= VulkanEngine::engine->create_buffer(handleSize, bufferUsageFlags, VMA_MEMORY_USAGE_CPU_TO_GPU);

	void* rayGenData, *missData, *shadowData, *hitData;
	vmaMapMemory(VulkanEngine::engine->_allocator, raygenShaderBindingTable._allocation, &rayGenData);
	memcpy(rayGenData, shaderHandleStorage.data(), handleSize);
	vmaMapMemory(VulkanEngine::engine->_allocator, missShaderBindingTable._allocation, &missData);
	memcpy(missData, shaderHandleStorage.data() + handleAlignment, handleSize * 2);
	vmaMapMemory(VulkanEngine::engine->_allocator, hitShaderBindingTable._allocation, &hitData);
	memcpy(hitData, shaderHandleStorage.data() + handleAlignment * 3, handleSize);

	vmaUnmapMemory(VulkanEngine::engine->_allocator, raygenShaderBindingTable._allocation);
	vmaUnmapMemory(VulkanEngine::engine->_allocator, missShaderBindingTable._allocation);
	vmaUnmapMemory(VulkanEngine::engine->_allocator, hitShaderBindingTable._allocation);

	VulkanEngine::engine->_mainDeletionQueue.push_function([=]() {
		vmaDestroyBuffer(VulkanEngine::engine->_allocator, raygenShaderBindingTable._buffer, raygenShaderBindingTable._allocation);
		vmaDestroyBuffer(VulkanEngine::engine->_allocator, missShaderBindingTable._buffer, missShaderBindingTable._allocation);
		vmaDestroyBuffer(VulkanEngine::engine->_allocator, hitShaderBindingTable._buffer, hitShaderBindingTable._allocation);
		});
}

void Renderer::build_raytracing_command_buffers()
{
	VkCommandBufferBeginInfo cmdBufInfo = vkinit::command_buffer_begin_info(0);

	VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	
	VkCommandBuffer& cmd = get_current_frame()._mainCommandBuffer;

	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBufInfo));

	VkStridedDeviceAddressRegionKHR raygenShaderSbtEntry{};
	raygenShaderSbtEntry.deviceAddress	= VulkanEngine::engine->getBufferDeviceAddress(raygenShaderBindingTable._buffer);
	raygenShaderSbtEntry.stride			= VulkanEngine::engine->_rtProperties.shaderGroupHandleSize;
	raygenShaderSbtEntry.size			= VulkanEngine::engine->_rtProperties.shaderGroupHandleSize;
	
	VkStridedDeviceAddressRegionKHR missShaderSbtEntry{};
	missShaderSbtEntry.deviceAddress	= VulkanEngine::engine->getBufferDeviceAddress(missShaderBindingTable._buffer);
	missShaderSbtEntry.stride			= VulkanEngine::engine->_rtProperties.shaderGroupHandleSize;
	missShaderSbtEntry.size				= VulkanEngine::engine->_rtProperties.shaderGroupHandleSize * 2;
	
	VkStridedDeviceAddressRegionKHR hitShaderSbtEntry{};
	hitShaderSbtEntry.deviceAddress		= VulkanEngine::engine->getBufferDeviceAddress(hitShaderBindingTable._buffer);
	hitShaderSbtEntry.stride			= VulkanEngine::engine->_rtProperties.shaderGroupHandleSize;
	hitShaderSbtEntry.size				= VulkanEngine::engine->_rtProperties.shaderGroupHandleSize;

	VkStridedDeviceAddressRegionKHR callableShaderSbtEntry{};

	uint32_t width = VulkanEngine::engine->_windowExtent.width, height = VulkanEngine::engine->_windowExtent.height;

	// Copy ray tracing output to swapchain image
	//VulkanEngine::engine->immediate_submit([&](VkCommandBuffer cmd)
	//	{
			VkImage& image = VulkanEngine::engine->_swapchainImages[VulkanEngine::engine->_indexSwapchainImage];

			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _rtPipeline);
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _rtPipelineLayout, 0, 1, &_rtDescriptorSet, 0, nullptr);

			vkCmdTraceRaysKHR(
				cmd,
				&raygenShaderSbtEntry,
				&missShaderSbtEntry,
				&hitShaderSbtEntry,
				&callableShaderSbtEntry,
				width,
				height,
				1
			);

			// Prepare current swap chain image as transfer destination
			VkImageMemoryBarrier imageMemoryBarrierSwapChain{};
			imageMemoryBarrierSwapChain.sType				= VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			imageMemoryBarrierSwapChain.pNext				= nullptr;
			imageMemoryBarrierSwapChain.image				= image;
			imageMemoryBarrierSwapChain.oldLayout			= VK_IMAGE_LAYOUT_UNDEFINED;
			imageMemoryBarrierSwapChain.newLayout			= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			imageMemoryBarrierSwapChain.subresourceRange	= subresourceRange;
			imageMemoryBarrierSwapChain.srcAccessMask		= 0;
			imageMemoryBarrierSwapChain.dstAccessMask		= VK_ACCESS_TRANSFER_WRITE_BIT;

			vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrierSwapChain);

			// Prepare ray tracing output image as transfer source
			VkImageMemoryBarrier imageMemoryBarrierOutput{};
			imageMemoryBarrierOutput.sType				= VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			imageMemoryBarrierOutput.pNext				= nullptr;
			imageMemoryBarrierOutput.image				= _rtImage.image._image;
			imageMemoryBarrierOutput.oldLayout			= VK_IMAGE_LAYOUT_GENERAL;
			imageMemoryBarrierOutput.newLayout			= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			imageMemoryBarrierOutput.subresourceRange	= subresourceRange;
			imageMemoryBarrierOutput.srcAccessMask		= VK_ACCESS_TRANSFER_READ_BIT;

			vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrierOutput);

			VkImageCopy copyRegion{};
			copyRegion.srcSubresource	= { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
			copyRegion.srcOffset		= { 0, 0, 0 };
			copyRegion.dstSubresource	= { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
			copyRegion.dstOffset		= { 0, 0, 0 };
			copyRegion.extent			= { width, height, 1 };
			vkCmdCopyImage(cmd, _rtImage.image._image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

			// Return swapchain image back to present
			imageMemoryBarrierSwapChain.oldLayout		= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			imageMemoryBarrierSwapChain.newLayout		= VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			imageMemoryBarrierSwapChain.srcAccessMask	= VK_ACCESS_TRANSFER_WRITE_BIT;
			imageMemoryBarrierSwapChain.dstAccessMask	= 0;

			vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrierSwapChain);

			// Return output image back to general
			imageMemoryBarrierOutput.oldLayout			= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			imageMemoryBarrierOutput.newLayout			= VK_IMAGE_LAYOUT_GENERAL;
			imageMemoryBarrierOutput.srcAccessMask		= VK_ACCESS_TRANSFER_READ_BIT;
			imageMemoryBarrierOutput.dstAccessMask		= 0;

			vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrierOutput);

		//});

	VK_CHECK(vkEndCommandBuffer(cmd));
}